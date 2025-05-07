#include <hive/chain/sync_block_writer.hpp>

#include <hive/chain/block_flow_control.hpp>
#include <hive/chain/block_storage_interface.hpp>
#include <hive/chain/fork_database.hpp>
#include <hive/chain/full_block.hpp>
#include <hive/chain/witness_objects.hpp>

#include <boost/scope_exit.hpp>

namespace hive { namespace chain {

sync_block_writer::sync_block_writer( block_storage_i& bs,
                                      database& db, application& app )
  : _block_storage( bs ), _reader( _fork_db, _block_storage ),
    _db( db ), _app( app )
{}

const block_read_i& sync_block_writer::get_block_reader()
{
  return _reader;
}

void sync_block_writer::store_block( uint32_t current_irreversible_block_num,
  uint32_t state_head_block_number )
{
  const auto fork_head = _fork_db.head();
  if (fork_head)
    FC_ASSERT(fork_head->get_block_num() == state_head_block_number,
              "Fork Head Block Number: ${fork_head}, Chain Head Block Number: ${chain_head}",
              ("fork_head", fork_head->get_block_num())("chain_head", state_head_block_number));

  // output to block log based on new last irreverisible block num
  std::shared_ptr<full_block_type> tmp_head = _block_storage.head_block();
  uint32_t blocklog_head_num = tmp_head ? tmp_head->get_block_num() : 0;
  vector<item_ptr> blocks_to_write;
  if( blocklog_head_num < current_irreversible_block_num )
  {
    // Check for all blocks that we want to write out to the block log but don't write any
    // unless we are certain they all exist in the fork db
    while( blocklog_head_num < current_irreversible_block_num )
    {
      item_ptr block_ptr = _fork_db.fetch_block_on_main_branch_by_number( blocklog_head_num + 1 );
      FC_ASSERT( block_ptr, "Current fork in the fork database does not contain the last_irreversible_block" );
      blocks_to_write.push_back( block_ptr );
      blocklog_head_num++;
    }

    for( auto block_itr = blocks_to_write.begin(); block_itr != blocks_to_write.end(); ++block_itr )
      _block_storage.append( block_itr->get()->full_block, _is_at_live_sync );

    if( not blocks_to_write.empty() )
      _db.set_last_irreversible_block_data( blocks_to_write.rbegin()->get()->full_block );

    _block_storage.flush_head_storage();
  }

  // This deletes blocks from the fork db
  _fork_db.set_max_size( state_head_block_number - current_irreversible_block_num + 1 );
}

void sync_block_writer::pop_block()
{
  _fork_db.pop_block();
}

bool sync_block_writer::push_block(const std::shared_ptr<full_block_type>& full_block, 
  const block_flow_control& block_ctrl,
  uint32_t state_head_block_num,
  block_id_type state_head_block_id,
  const uint32_t skip,
  apply_block_t apply_block_extended,
  pop_block_t pop_block_extended )
{
  std::vector<std::shared_ptr<full_block_type>> blocks;

  if (true) //if fork checking enabled
  {
    const item_ptr new_head = _fork_db.push_block(full_block);
    block_ctrl.on_fork_db_insert();
    // Inlined here former _maybe_warn_multiple_production( new_head->get_block_num() );
    {
      uint32_t height = new_head->get_block_num();
      const auto blocks = _fork_db.fetch_block_by_number(height);
      if (blocks.size() > 1)
      {
        vector<std::pair<account_name_type, fc::time_point_sec>> witness_time_pairs;
        witness_time_pairs.reserve(blocks.size());
        for (const auto& b : blocks)
          witness_time_pairs.push_back(std::make_pair(b->get_block_header().witness, b->get_block_header().timestamp));

        ilog("Encountered block num collision at block ${height} due to a fork, witnesses are: ${witness_time_pairs}", (height)(witness_time_pairs));
      }
    }

    // If the new head block is actually older one, the new block is on a shorter fork
    // (or duplicate), so don't validate it.
    if (new_head->get_block_num() <= state_head_block_num)
    {
      block_ctrl.on_fork_ignore();
      return false;
    }

    //if new_head indirectly builds off the current head_block
    // then there's no fork switch, we're just linking in previously unlinked blocks to the main branch
    for (item_ptr block = new_head;
         block->get_block_num() > state_head_block_num;
         block = block->prev.lock())
    {
      blocks.push_back(block->full_block);
      if (block->get_block_num() == 1) //prevent crash backing up to null in for-loop
        break;
    }
    //we've found a longer fork, so do a fork switch to pop back to the common block of the two forks
    if (blocks.back()->get_block_header().previous != state_head_block_id)
    {
      block_ctrl.on_fork_apply();
      ilog("calling switch_forks() from push_block()");
      switch_forks( new_head->get_block_id(), new_head->get_block_num(),
                    skip, &block_ctrl, state_head_block_id, state_head_block_num,
                    apply_block_extended, pop_block_extended );
      _app.status.save_fork( new_head->get_block_num(), new_head->get_block_id().str());
      return true;
    }
  }
  else //fork checking not enabled, just try to push the new block
  {
    blocks.push_back(full_block);
    // even though we've skipped fork db, we still need to notify flow control in proper order
    block_ctrl.on_fork_db_insert();
  }

  //we are building off our head block, try to add the block(s)
  block_ctrl.on_fork_normal();
  for (auto iter = blocks.crbegin(); iter != blocks.crend(); ++iter)
  {
    try
    {
      bool is_pushed_block = (*iter)->get_block_id() == block_ctrl.get_full_block()->get_block_id();
      if( blocks.size() > 1 )
        _fork_db.set_head( _fork_db.fetch_block((*iter)->get_block_id(), true) );
      apply_block_extended( 
        // if we've linked in a chain of multiple blocks, we need to keep the fork_db's head block in sync
        // with what we're applying.  If we're only appending a single block, the forkdb's head block
        // should already be correct
        *iter,
        skip,
        is_pushed_block ? &block_ctrl : nullptr);
    }
    catch (const fc::exception& e)
    {
      elog("Failed to push new block:\n${e}", ("e", e.to_detail_string()));
      // remove failed block, and all blocks on the fork after it, from the fork database
      for (; iter != blocks.crend(); ++iter)
        _fork_db.remove((*iter)->get_block_id());
      throw;
    }
  }
  return false;
}

void sync_block_writer::switch_forks( const block_id_type& new_head_block_id, uint32_t new_head_block_num,
  uint32_t skip, const block_flow_control* pushed_block_ctrl,
  const block_id_type original_head_block_id, const uint32_t original_head_block_number,
  apply_block_t apply_block_extended, pop_block_t pop_block_extended )
{
  BOOST_SCOPE_EXIT(void) { ilog("Done fork switch"); } BOOST_SCOPE_EXIT_END
  ilog("Switching to fork: ${id}", ("id", new_head_block_id));
  ilog("Before switching, head_block_id is ${original_head_block_id} head_block_number ${original_head_block_number}", (original_head_block_id)(original_head_block_number));
  const auto [new_branch, old_branch] = _fork_db.fetch_branch_from(new_head_block_id, original_head_block_id);

  ilog("Destination branch block ids:");
  std::for_each(new_branch.begin(), new_branch.end(), [](const item_ptr& item) {
    ilog(" - ${id}", ("id", item->get_block_id()));
  });
  const block_id_type common_block_id = new_branch.back()->previous_id();
  const uint32_t common_block_number = new_branch.back()->get_block_num() - 1;

  ilog(" - common_block_id ${common_block_id} common_block_number ${common_block_number} (block before first block in branch, should be common)", (common_block_id)(common_block_number));

  uint32_t current_head_block_num = original_head_block_number;
  if (old_branch.size())
  {
    ilog("Source branch block ids:");
    std::for_each(old_branch.begin(), old_branch.end(), [](const item_ptr& item) {
      ilog(" - ${id}", ("id", item->get_block_id()));
    });

    // pop blocks until we hit the common ancestor block
    current_head_block_num = pop_block_extended( old_branch.back()->previous_id() );
    ilog("Done popping blocks");
  }

  _db.notify_switch_fork( current_head_block_num );

  // push all blocks on the new fork
  for (auto ritr = new_branch.crbegin(); ritr != new_branch.crend(); ++ritr)
  {
    ilog("pushing block #${block_num} from new fork, id ${id}", ("block_num", (*ritr)->get_block_num())("id", (*ritr)->get_block_id()));
    std::shared_ptr<fc::exception> delayed_exception_to_avoid_yield_in_catch;
    try
    {
      // when we are handling block that triggered fork switch, we want to release related promise so P2P
      // can broadcast the block; it should happen even if some other block later causes reversal of the
      // fork switch (the block was good after all)
      bool is_pushed_block = ( pushed_block_ctrl != nullptr ) && ( ( *ritr )->full_block->get_block_id() == pushed_block_ctrl->get_full_block()->get_block_id() );
      if( *ritr )
        _fork_db.set_head( *ritr );
      apply_block_extended( ( *ritr )->full_block,
                            skip,
                            is_pushed_block ? pushed_block_ctrl : nullptr );
    }
    catch (const fc::exception& e)
    {
      delayed_exception_to_avoid_yield_in_catch = e.dynamic_copy_exception();
    }
    if (delayed_exception_to_avoid_yield_in_catch)
    {
      wlog("exception thrown while switching forks ${e}", ("e", delayed_exception_to_avoid_yield_in_catch->to_detail_string()));

      // remove the rest of new_branch from the fork_db, those blocks are invalid
      while (ritr != new_branch.rend())
      {
        _fork_db.remove((*ritr)->get_block_id());
        ++ritr;
      }

      // our fork switch has failed.  That could mean that we are now on a shorter fork than we started on,
      // and we should switch back to the original fork because longer == good.  But it's also possible that
      // the bad block was late in the fork, and even without the bad blocks the new fork is longer so we
      // should stay.  And a third possibility is that the fork is shorter, but one of the blocks on the fork
      // became irreversible, so switching back is no longer an option.
      // Note: database method calls to get_last_irreversible_block_num & head_block_num have been replaced by
      //       the ones from fork_db below:
      //if (get_last_irreversible_block_num() < common_block_number && head_block_num() < original_head_block_number)
      if( _fork_db.get_last_irreversible_block_num() < common_block_number &&
          _fork_db.get_head()->get_block_num() < original_head_block_number )
      {
        // pop all blocks from the bad fork
        uint32_t new_head_block_num = pop_block_extended( common_block_id );
        ilog(" - reverting to previous chain, done popping blocks");
        _db.notify_switch_fork( new_head_block_num );

        // restore any popped blocks from the good fork
        if (old_branch.size())
        {
          ilog("restoring blocks from original fork");
          for (auto ritr = old_branch.crbegin(); ritr != old_branch.crend(); ++ritr)
          {
            ilog(" - restoring block ${id}", ("id", (*ritr)->get_block_id()));
            if( *ritr )
              _fork_db.set_head( *ritr );
            apply_block_extended( (*ritr)->full_block,
                                  skip,
                                  nullptr );
          }
          ilog("done restoring blocks from original fork");
        }
      }
      delayed_exception_to_avoid_yield_in_catch->dynamic_rethrow_exception();
    }
  }
  ilog("done pushing blocks from new fork");
} // switch_forks

std::optional<block_write_i::new_last_irreversible_block_t> 
sync_block_writer::find_new_last_irreversible_block(
  const std::vector<const witness_object*>& scheduled_witness_objects,
  const std::map<account_name_type, block_id_type>& last_fast_approved_block_by_witness,
  const unsigned witnesses_required_for_irreversiblity,
  const uint32_t old_last_irreversible ) const
{
  new_last_irreversible_block_t result;
  // during our search for a new irreversible block, if we find a
  // candidate better than the current last_irreversible_block,
  // store it here:
  item_ptr new_last_irreversible_block;
  item_ptr new_head_block;

  // for each witness in the upcoming schedule, they may (and likely will) have voted on blocks
  // both by sending fast-confirm transactions and by generating blocks that implicitly vote on
  // other blocks by building off of them.  we only care about the highest block number they
  // have "voted" for, regardless of method.  If they fast-confirm one block, then generate
  // a block with a higher block_num, we'll say they voted for the higher block number; the one
  // they generated.
  // create a map of each block_id that was the best vote for at least one witness, mapped
  // to the number of witnesses directly voting for it
  // start with the fast-confirms broadcast by each witness
  const std::map<account_name_type, block_id_type> last_block_generated_by_witness = 
    _fork_db.get_last_block_generated_by_each_witness();
  std::map<block_id_type, uint32_t> number_of_approvals_by_block_id;
  for (const witness_object* witness_obj : scheduled_witness_objects)
  {
    const auto fast_approval_iter = last_fast_approved_block_by_witness.find(witness_obj->owner);
    const auto last_block_iter = last_block_generated_by_witness.find(witness_obj->owner);
    std::optional<block_id_type> best_block_id_for_this_witness;
    if (fast_approval_iter != last_fast_approved_block_by_witness.end())
    {
      if (last_block_iter != last_block_generated_by_witness.end()) // they have cast a fast-confirm vote and produced a block, choose the most recent
        best_block_id_for_this_witness = block_header::num_from_id(fast_approval_iter->second) > block_header::num_from_id(last_block_iter->second) ?
                                         fast_approval_iter->second : last_block_iter->second;
      else // no generated blocks, but they have cast votes
        best_block_id_for_this_witness = fast_approval_iter->second;
    }
    else if (last_block_iter != last_block_generated_by_witness.end()) // they produced a block, but have not cast any votes
      best_block_id_for_this_witness = last_block_iter->second;
    if (best_block_id_for_this_witness)
      ++number_of_approvals_by_block_id[*best_block_id_for_this_witness];
  }

  // walk over each fork in the forkdb
  std::vector<item_ptr> heads = _fork_db.fetch_heads();
  for (const item_ptr& possible_head : heads)
  {
    // dlog("Considering possible head ${block_id}", ("block_id", possible_head->get_block_id()));
    // keep track of all witnesses approving this block
    uint32_t number_of_witnesses_approving_this_block = 0;
    item_ptr this_block = possible_head;

    // walk backwards over blocks on this fork
    while (this_block &&
           this_block->get_block_num() > old_last_irreversible &&
           (!new_last_irreversible_block || // we don't yet have a candidate
            this_block == new_last_irreversible_block || // this is our candidate, but we're coming at it from a different fork
            this_block->get_block_num() > new_last_irreversible_block->get_block_num())) // it's a higher block number than our current candidate
    {
      // dlog("Considering block ${block_id}", ("block_id", this_block->get_block_id()));
      number_of_witnesses_approving_this_block += number_of_approvals_by_block_id[this_block->get_block_id()];
      // dlog("Has ${number_of_witnesses_approving_this_block} witnesses approving", (number_of_witnesses_approving_this_block));

      if (number_of_witnesses_approving_this_block >= witnesses_required_for_irreversiblity)
      {
        // dlog("Block ${num} can be made irreversible, ${number_of_witnesses_approving_this_block} witnesses approve it",
        //      ("num", this_block->get_block_num())(number_of_witnesses_approving_this_block));
        if (!new_last_irreversible_block ||
            possible_head->get_block_num() > new_head_block->get_block_num())
        {
          new_head_block = possible_head;
          new_last_irreversible_block = this_block;
          result.new_head_block = possible_head->full_block;
          result.new_last_irreversible_block_num = new_last_irreversible_block->get_block_num();
        }
        break;
      }
      else
      {
        // dlog("Can't make block ${num} irreversible, only ${witnesses_approving_this_block} out of a required ${witnesses_required_for_irreversiblity} approve it",
        //      ("num", this_block->get_block_num())(number_of_witnesses_approving_this_block)(witnesses_required_for_irreversiblity));
      }
      this_block = this_block->prev.lock();
    }
  }

  if (!new_last_irreversible_block)
  {
    // dlog("Leaving update_last_irreversible_block without making any new blocks irreversible");
    return std::optional<new_last_irreversible_block_t>();
  }

  // dlog("Found a new last irreversible block: ${new_last_irreversible_block_num}", ("new_last_irreversible_block_num", new_last_irreversible_block->get_block_num()));
  const item_ptr main_branch_block = 
    _fork_db.fetch_block_on_main_branch_by_number(new_last_irreversible_block->get_block_num());

  result.found_on_another_fork = ( new_last_irreversible_block != main_branch_block );

  return std::optional< new_last_irreversible_block_t >( result );
} // find_new_last_irreversible_block

void sync_block_writer::on_reindex_start()
{
  _fork_db.reset(); // override effect of fork_db.start_block() call in open()
}

void sync_block_writer::on_reindex_end( const std::shared_ptr<full_block_type>& end_block )
{
  _fork_db.start_block( end_block );
}

void sync_block_writer::on_state_independent_data_initialized()
{
  // Get fork db in sync with block log.
  auto head = _block_storage.head_block();
  if( head )
    _fork_db.start_block( head );
}

void sync_block_writer::close()
{
  _fork_db.reset();
}

} } //hive::chain
