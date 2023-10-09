#include <hive/chain/full_database.hpp>

#include <boost/scope_exit.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <appbase/application.hpp>
#include <hive/chain/database_exceptions.hpp>
#include <hive/chain/signal_wrapper.hpp>

namespace hive { namespace chain {

void full_database::state_dependent_open( const open_args& args )
{
  load_state_initial_data(args, [this](int block_num) { return _block_log.read_block_by_num(block_num); });
}

void full_database::state_independent_open( const open_args& args )
{
  database::state_independent_open( args );
}


void full_database::open( const open_args& args )
{
  open_block_log(args);
  database::open(args);
}

void full_database::open_block_log(const open_args& args)
{
  with_write_lock([&]()
  {
    _block_log.open(args.data_dir / "block_log");
    _block_log.set_compression(args.enable_block_log_compression);
    _block_log.set_compression_level(args.block_log_compression_level);
  });
}

uint32_t full_database::reindex_internal( const open_args& args, const std::shared_ptr<full_block_type>& start_block )
{
  uint64_t skip_flags = skip_validate_invariants | skip_block_log;
  if (args.validate_during_replay)
    ulog("Doing full validation during replay at user request");
  else
  {
    skip_flags |= skip_witness_signature |
      skip_transaction_signatures |
      skip_transaction_dupe_check |
      skip_tapos_check |
      skip_merkle_check |
      skip_witness_schedule_check |
      skip_authority_check |
      skip_validate; /// no need to validate operations
  }

  uint32_t last_block_num = _block_log.head()->get_block_num();
  if( args.stop_replay_at > 0 && args.stop_replay_at < last_block_num )
    last_block_num = args.stop_replay_at;

  bool rat = fc::enable_record_assert_trip;
  bool as = fc::enable_assert_stacktrace;
  fc::enable_record_assert_trip = true; //enable detailed backtrace from FC_ASSERT (that should not ever be triggered during replay)
  fc::enable_assert_stacktrace = true;

  BOOST_SCOPE_EXIT( this_ ) { this_->clear_tx_status(); } BOOST_SCOPE_EXIT_END
  set_tx_status( TX_STATUS_BLOCK );

  std::shared_ptr<full_block_type> last_applied_block;
  const auto process_block = [&](const std::shared_ptr<full_block_type>& full_block) {
    const uint32_t current_block_num = full_block->get_block_num();

    if (current_block_num % 100000 == 0)
    {
      std::ostringstream percent_complete_stream;
      percent_complete_stream << std::fixed << std::setprecision(2) << double(current_block_num) * 100 / last_block_num;
      ulog("   ${current_block_num} of ${last_block_num} blocks = ${percent_complete}%   (${free_memory_megabytes}MB shared memory free)",
           ("percent_complete", percent_complete_stream.str())
           (current_block_num)(last_block_num)
           ("free_memory_megabytes", get_free_memory() >> 20));
    }

    apply_block(full_block, skip_flags);
    last_applied_block = full_block;

    return !appbase::app().is_interrupt_request();
  };

  const uint32_t start_block_number = start_block->get_block_num();
  process_block(start_block);

  if (start_block_number < last_block_num)
    _block_log.for_each_block(start_block_number + 1, last_block_num, process_block, block_log::for_each_purpose::replay);

  if (appbase::app().is_interrupt_request())
    ilog("Replaying is interrupted on user request. Last applied: (block number: ${n}, id: ${id})",
         ("n", last_applied_block->get_block_num())("id", last_applied_block->get_block_id()));

  fc::enable_record_assert_trip = rat; //restore flag
  fc::enable_assert_stacktrace = as;

  return last_applied_block->get_block_num();
}

bool full_database::is_reindex_complete( uint64_t* head_block_num_in_blocklog, uint64_t* head_block_num_in_db ) const
{
  std::shared_ptr<full_block_type> head = _block_log.head();
  uint32_t head_block_num_origin = head ? head->get_block_num() : 0;
  uint32_t head_block_num_state = head_block_num();

  if( head_block_num_in_blocklog ) //if head block number requested
    *head_block_num_in_blocklog = head_block_num_origin;

  if( head_block_num_in_db ) //if head block number in database requested
    *head_block_num_in_db = head_block_num_state;

  if( head_block_num_state > head_block_num_origin )
  elog( "Incorrect number of blocks in `block_log` vs `state`. { \"block_log-head\": ${head_block_num_origin}, \"state-head\": ${head_block_num_state} }",
      ( head_block_num_origin )(head_block_num_state ) );

  return head_block_num_origin == head_block_num_state;
}

uint32_t full_database::reindex( const open_args& args )
{
  reindex_notification note( args );

  BOOST_SCOPE_EXIT(this_,&note) {
    HIVE_TRY_NOTIFY(this_->_post_reindex_signal, note);
  } BOOST_SCOPE_EXIT_END

  try
  {
    ilog( "Reindexing Blockchain" );

    if( appbase::app().is_interrupt_request() )
      return 0;

    uint32_t _head_block_num = head_block_num();

    std::shared_ptr<full_block_type> _head = _block_log.head();
    if( _head )
    {
      if( args.stop_replay_at == 0 )
        note.max_block_number = _head->get_block_num();
      else
        note.max_block_number = std::min( args.stop_replay_at, _head->get_block_num() );
    }
    else
      note.max_block_number = 0;//anyway later an assert is triggered

    note.force_replay = args.force_replay || _head_block_num == 0;
    note.validate_during_replay = args.validate_during_replay;

    HIVE_TRY_NOTIFY(_pre_reindex_signal, note);

    _fork_db.reset();    // override effect of _fork_db.start_block() call in open()

    auto start_time = fc::time_point::now();
    HIVE_ASSERT( _head, block_log_exception, "No blocks in block log. Cannot reindex an empty chain." );

    ilog( "Replaying blocks..." );

    with_write_lock( [&]()
    {
      std::shared_ptr<full_block_type> start_block;

      bool replay_required = true;

      if( _head_block_num > 0 )
      {
        if( args.stop_replay_at == 0 || args.stop_replay_at > _head_block_num )
          start_block = _block_log.read_block_by_num( _head_block_num + 1 );

        if( !start_block )
        {
          start_block = _block_log.read_block_by_num( _head_block_num );
          FC_ASSERT( start_block, "Head block number for state: ${h} but for `block_log` this block doesn't exist", ( "h", _head_block_num ) );

          replay_required = false;
        }
      }
      else
      {
        start_block = _block_log.read_block_by_num( 1 );
      }

      if( replay_required )
      {
        auto _last_block_number = start_block->get_block_num();
        if( _last_block_number && !args.force_replay )
          ilog("Resume of replaying. Last applied block: ${n}", ( "n", _last_block_number - 1 ) );

        note.last_block_number = reindex_internal( args, start_block );
      }
      else
      {
        note.last_block_number = start_block->get_block_num();
      }

      set_revision( head_block_num() );

      //get_index< account_index >().indices().print_stats();
    });

    FC_ASSERT( _block_log.head()->get_block_num(), "this should never happen" );
    _fork_db.start_block( _block_log.head() );

    auto end_time = fc::time_point::now();
    ilog("Done reindexing, elapsed time: ${elapsed_time} sec",
         ("elapsed_time", double((end_time - start_time).count()) / 1000000.0));

    note.reindex_success = true;

    return note.last_block_number;
  }
  FC_CAPTURE_AND_RETHROW( (args.data_dir)(args.shared_mem_dir) )

}

void full_database::close_chainbase(bool rewind)
{
  database::close_chainbase(rewind);
  _block_log.close();
}

//no chainbase lock required
bool full_database::is_known_block(const block_id_type& id)const
{ try {
  if (_fork_db.fetch_block(id))
    return true;

  auto requested_block_num = protocol::block_header::num_from_id(id);
  auto read_block_id = _block_log.read_block_id_by_num(requested_block_num);

  return read_block_id != block_id_type() && read_block_id == id;
} FC_CAPTURE_AND_RETHROW() }

//no chainbase lock required, but fork database read lock is required
bool full_database::is_known_block_unlocked(const block_id_type& id)const
{ try {
  if (_fork_db.fetch_block_unlocked(id, true /* only search linked blocks */))
    return true;

  auto requested_block_num = protocol::block_header::num_from_id(id);
  auto read_block_id = _block_log.read_block_id_by_num(requested_block_num);

  return read_block_id != block_id_type() && read_block_id == id;
} FC_CAPTURE_AND_RETHROW() }

//no chainbase lock required
block_id_type full_database::find_block_id_for_num( uint32_t block_num )const
{
  try
  {
    if( block_num == 0 )
      return block_id_type();
/*
    // Reversible blocks are *usually* in the TAPOS buffer.  Since this
    // is the fastest check, we do it first.
    block_summary_object::id_type bsid( block_num & 0xFFFF );
    const block_summary_object* bs = find< block_summary_object, by_id >( bsid );
    if( bs != nullptr )
    {
      if( protocol::block_header::num_from_id(bs->block_id) == block_num )
        return bs->block_id;
    }
*/

    // See if fork DB has the item
    shared_ptr<fork_item> fitem = _fork_db.fetch_block_on_main_branch_by_number( block_num );
    if( fitem )
      return fitem->get_block_id();

    // Next we check if block_log has it. Irreversible blocks are here.
    return _block_log.read_block_id_by_num(block_num);
  }
  FC_CAPTURE_AND_RETHROW( (block_num) )
}

//no chainbase lock required
block_id_type full_database::get_block_id_for_num( uint32_t block_num )const
{
  block_id_type bid = find_block_id_for_num( block_num );
  if (bid == block_id_type())
    FC_THROW_EXCEPTION(fc::key_not_found_exception, "block number not found");
  return bid;
}

//no chainbase lock required
std::shared_ptr<full_block_type> full_database::fetch_block_by_id( const block_id_type& id )const
{ try {
  shared_ptr<fork_item> fork_item = _fork_db.fetch_block( id );
  if (fork_item)
    return fork_item->full_block;

  std::shared_ptr<full_block_type> block_from_block_log = _block_log.read_block_by_num( protocol::block_header::num_from_id( id ) );
  if( block_from_block_log && block_from_block_log->get_block_id() == id )
    return block_from_block_log;
  return std::shared_ptr<full_block_type>();
} FC_CAPTURE_AND_RETHROW() }

//no chainbase lock required
std::shared_ptr<full_block_type> full_database::fetch_block_by_number( uint32_t block_num, fc::microseconds wait_for_microseconds )const
{ try {
  shared_ptr<fork_item> forkdb_item = _fork_db.fetch_block_on_main_branch_by_number(block_num, wait_for_microseconds);
  if (forkdb_item)
    return forkdb_item->full_block;

  return _block_log.read_block_by_num(block_num);
} FC_LOG_AND_RETHROW() }

//no chainbase lock required
std::vector<std::shared_ptr<full_block_type>> full_database::fetch_block_range( const uint32_t starting_block_num, const uint32_t count, fc::microseconds wait_for_microseconds )
{ try {
  // for debugging, put the head block back so it should straddle the last irreversible
  // const uint32_t starting_block_num = head_block_num() - 30;
  FC_ASSERT(starting_block_num > 0, "Invalid starting block number");
  FC_ASSERT(count > 0, "Why ask for zero blocks?");
  FC_ASSERT(count <= 1000, "You can only ask for 1000 blocks at a time");
  idump((starting_block_num)(count));

  vector<fork_item> fork_items = _fork_db.fetch_block_range_on_main_branch_by_number( starting_block_num, count, wait_for_microseconds );
  idump((fork_items.size()));
  if (!fork_items.empty())
    idump((fork_items.front().get_block_num()));

  // if the fork database returns some blocks, it means:
  // - that the last block in the range [starting_block_num, starting_block_num + count - 1]
  // - any block before the first block it returned should be in the block log
  uint32_t remaining_count = fork_items.empty() ? count : fork_items.front().get_block_num() - starting_block_num;
  idump((remaining_count));
  std::vector<std::shared_ptr<full_block_type>> result;

  if (remaining_count)
    result = _block_log.read_block_range_by_num(starting_block_num, remaining_count);

  idump((result.size()));
  if (!result.empty())
    idump((result.front()->get_block_num())(result.back()->get_block_num()));
  result.reserve(result.size() + fork_items.size());
  for (fork_item& item : fork_items)
    result.push_back(item.full_block);

  return result;
} FC_LOG_AND_RETHROW() }

boost::signals2::connection full_database::add_pre_reindex_handler(const reindex_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  return connect_impl<true>(_pre_reindex_signal, func, plugin, group, "reindex");
}

boost::signals2::connection full_database::add_post_reindex_handler(const reindex_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  return connect_impl<false>(_post_reindex_signal, func, plugin, group, "reindex");
}

void full_database::migrate_irreversible_state_perform(uint32_t old_last_irreversible)
{
  migrate_irreversible_state_to_blocklog(old_last_irreversible);
  database::migrate_irreversible_state_perform(old_last_irreversible);
}

void full_database::migrate_irreversible_state_to_blocklog(uint32_t old_last_irreversible)
{
  if( !( get_node_skip_flags() & skip_block_log ) )
  {
    // output to block log based on new last irreverisible block num
    std::shared_ptr<full_block_type> tmp_head = _block_log.head();
    uint32_t blocklog_head_num = tmp_head ? tmp_head->get_block_num() : 0;
    vector<item_ptr> blocks_to_write;

    if( blocklog_head_num < get_last_irreversible_block_num() )
    {
      // Check for all blocks that we want to write out to the block log but don't write any
      // unless we are certain they all exist in the fork db
      while( blocklog_head_num < get_last_irreversible_block_num() )
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
  }
}

//safe to call without chainbase lock
std::vector<block_id_type> full_database::get_blockchain_synopsis(const block_id_type& reference_point, uint32_t number_of_blocks_after_reference_point)
{
  fc::optional<uint32_t> block_number_needed_from_block_log;
  std::vector<block_id_type> synopsis = _fork_db.get_blockchain_synopsis(reference_point, number_of_blocks_after_reference_point, block_number_needed_from_block_log);

  if (block_number_needed_from_block_log)
  {
    uint32_t reference_point_block_num = protocol::block_header::num_from_id(reference_point);
    auto read_block_id = _block_log.read_block_id_by_num(*block_number_needed_from_block_log);

    if (reference_point_block_num == *block_number_needed_from_block_log)
    {
      // we're getting this block from the database because it's the reference point,
      // not because it's the last irreversible.
      // We can only do this if the reference point really is in the blockchain
      if (read_block_id == reference_point)
        synopsis.insert(synopsis.begin(), reference_point);
      else
      {
        wlog("Unable to generate a usable synopsis because the peer we're generating it for forked too long ago "
             "(our chains diverge before block #${reference_point_block_num}",
             (reference_point_block_num));
        // TODO: get the right type of exception here
        //FC_THROW_EXCEPTION(graphene::net::block_older_than_undo_history, "Peer is on a fork I'm unable to switch to");
        FC_THROW("Peer is on a fork I'm unable to switch to");
      }
    }
    else
    {
      synopsis.insert(synopsis.begin(), read_block_id);
    }
  }
  return synopsis;
}

std::deque<block_id_type>::const_iterator full_database::find_first_item_not_in_blockchain(const std::deque<block_id_type>& item_hashes_received)
{
  return _fork_db.with_read_lock([&](){
    return std::partition_point(item_hashes_received.begin(), item_hashes_received.end(), [&](const block_id_type& block_id) {
      return is_known_block_unlocked(block_id);
    });
  });
}

// requires forkdb read lock, does not require chainbase lock
bool full_database::is_included_block_unlocked(const block_id_type& block_id)
{ try {
  uint32_t block_num = block_header::num_from_id(block_id);
  if (block_num == 0)
    return block_id == block_id_type();

  // See if fork DB has the item
  shared_ptr<fork_item> fitem = _fork_db.fetch_block_on_main_branch_by_number_unlocked(block_num);
  if (fitem)
    return block_id == fitem->get_block_id();

  // Next we check if block_log has it. Irreversible blocks are here.
  auto read_block_id = _block_log.read_block_id_by_num(block_num);
  return block_id == read_block_id;
} FC_CAPTURE_AND_RETHROW() }

// used by the p2p layer, get_block_ids takes a blockchain synopsis provided by a peer, and generates
// a sequential list of block ids that builds off of the last item in the synopsis that we have in
// common
// no chainbase lock required
std::vector<block_id_type> full_database::get_block_ids(const std::vector<block_id_type>& blockchain_synopsis, uint32_t& remaining_item_count, uint32_t limit)
{
  uint32_t first_block_num_in_reply;
  uint32_t last_block_num_in_reply;
  uint32_t last_block_from_block_log_in_reply;
  shared_ptr<fork_item> head;
  uint32_t head_block_num;
  vector<block_id_type> result;

  // get and hold a fork database lock so a fork switch can't happen while we're in the middle of creating
  // this list of block ids
  _fork_db.with_read_lock([&]() {
    remaining_item_count = 0;
    head = _fork_db.head_unlocked();
    if (!head)
      return;
    head_block_num = head->get_block_num();

    block_id_type last_known_block_id;
    if (blockchain_synopsis.empty() ||
        (blockchain_synopsis.size() == 1 && blockchain_synopsis[0] == block_id_type()))
    {
      // peer has sent us an empty synopsis meaning they have no blocks.
      // A bug in old versions would cause them to send a synopsis containing block 000000000
      // when they had an empty blockchain, so pretend they sent the right thing here.
      // do nothing, leave last_known_block_id set to zero
    }
    else
    {
      bool found_a_block_in_synopsis = false;
      for (const block_id_type& block_id_in_synopsis : boost::adaptors::reverse(blockchain_synopsis))
        if (block_id_in_synopsis == block_id_type() || is_included_block_unlocked(block_id_in_synopsis))
        {
          last_known_block_id = block_id_in_synopsis;
          found_a_block_in_synopsis = true;
          break;
        }

      if (!found_a_block_in_synopsis)
        FC_THROW_EXCEPTION(internal_peer_is_on_an_unreachable_fork, "Unable to provide a list of blocks starting at any of the blocks in peer's synopsis");
    }

    // the list will be composed of block ids from the block_log first, followed by ones from the fork database.
    // when building our reply, we'll fill in the ones from the fork_database first, so we can release the
    // fork_db lock, then we'll grab the ids from the block_log at our leisure.
    first_block_num_in_reply = block_header::num_from_id(last_known_block_id);
    if (first_block_num_in_reply == 0)
      ++first_block_num_in_reply;
    last_block_num_in_reply = std::min(head_block_num, first_block_num_in_reply + limit - 1);
    uint32_t result_size = last_block_num_in_reply - first_block_num_in_reply + 1;

    result.resize(result_size);

    uint32_t oldest_block_num_in_forkdb = _fork_db.get_oldest_block_num_unlocked();
    last_block_from_block_log_in_reply = std::min(oldest_block_num_in_forkdb - 1, last_block_num_in_reply);

    uint32_t first_block_num_from_fork_db_in_reply = std::max(oldest_block_num_in_forkdb, first_block_num_in_reply);
    //idump((first_block_num_in_reply)(last_block_from_block_log_in_reply)(first_block_num_from_fork_db_in_reply)(last_block_num_in_reply));

    for (uint32_t block_num = first_block_num_from_fork_db_in_reply;
         block_num <= last_block_num_in_reply;
         ++block_num)
    {
      shared_ptr<fork_item> item_from_forkdb = _fork_db.fetch_block_on_main_branch_by_number_unlocked(block_num);
      assert(item_from_forkdb);
      uint32_t index_in_result = block_num - first_block_num_in_reply;
      result[index_in_result] = item_from_forkdb->get_block_id();
    }
  }); // drop the forkdb lock

  if (!head)
  {
    remaining_item_count = 0;
    return result;
  }

  for (uint32_t block_num = first_block_num_in_reply;
       block_num <= last_block_from_block_log_in_reply;
       ++block_num)
  {
    uint32_t index_in_result = block_num - first_block_num_in_reply;
    result[index_in_result] = _block_log.read_block_id_by_num(block_num);
  }

  if (!result.empty() && block_header::num_from_id(result.back()) < head_block_num)
    remaining_item_count = head_block_num - last_block_num_in_reply;
  else
    remaining_item_count = 0;

  return result;
}

}}
