#include <hive/chain/sync_block_writer.hpp>

#include <hive/chain/block_flow_control.hpp>
#include <hive/chain/block_log.hpp>
#include <hive/chain/fork_database.hpp>
#include <hive/chain/full_block.hpp>

#include <boost/scope_exit.hpp>

namespace hive { namespace chain {

sync_block_writer::sync_block_writer( 
  block_log& block_log, fork_database& fork_db )
  : _reader( fork_db, block_log ), _block_log( block_log ), _fork_db( fork_db )
{}

block_read_i& sync_block_writer::get_block_reader()
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
  std::shared_ptr<full_block_type> tmp_head = _block_log.head();
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
      _block_log.append( block_itr->get()->full_block, _is_at_live_sync );

    _block_log.flush();
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
  pop_block_t pop_block_extended,
  notify_switch_fork_t notify_switch_fork,
  external_notify_switch_fork_t external_notify_switch_fork )
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
                    apply_block_extended, pop_block_extended, notify_switch_fork );
      external_notify_switch_fork( new_head->get_block_id().str(), new_head->get_block_num() );
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
  apply_block_t apply_block_extended, pop_block_t pop_block_extended, notify_switch_fork_t notify_switch_fork )
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

  notify_switch_fork( current_head_block_num );

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
        notify_switch_fork( new_head_block_num );

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

} } //hive::chain
