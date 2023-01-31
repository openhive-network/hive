#include <hive/chain/hive_fwd.hpp>

#include <appbase/application.hpp>

#include <hive/protocol/hive_operations.hpp>
#include <hive/protocol/get_config.hpp>
#include <hive/protocol/transaction_util.hpp>

#include <hive/chain/block_summary_object.hpp>
#include <hive/chain/compound.hpp>
#include <hive/chain/custom_operation_interpreter.hpp>
#include <hive/chain/database.hpp>
#include <hive/chain/database_exceptions.hpp>
#include <hive/chain/db_with.hpp>
#include <hive/chain/evaluator_registry.hpp>
#include <hive/chain/global_property_object.hpp>
#include <hive/chain/optional_action_evaluator.hpp>
#include <hive/chain/pending_required_action_object.hpp>
#include <hive/chain/pending_optional_action_object.hpp>
#include <hive/chain/required_action_evaluator.hpp>
#include <hive/chain/smt_objects.hpp>
#include <hive/chain/hive_evaluator.hpp>
#include <hive/chain/hive_objects.hpp>
#include <hive/chain/transaction_object.hpp>
#include <hive/chain/shared_db_merkle.hpp>
#include <hive/chain/witness_schedule.hpp>
#include <hive/chain/blockchain_worker_thread_pool.hpp>

#include <hive/chain/util/asset.hpp>
#include <hive/chain/util/reward.hpp>
#include <hive/chain/util/uint256.hpp>
#include <hive/chain/util/reward.hpp>
#include <hive/chain/util/manabar.hpp>
#include <hive/chain/util/rd_setup.hpp>
#include <hive/chain/util/nai_generator.hpp>
#include <hive/chain/util/dhf_processor.hpp>
#include <hive/chain/util/delayed_voting.hpp>

#include <fc/smart_ref_impl.hpp>
#include <fc/uint128.hpp>

#include <fc/container/deque.hpp>

#include <fc/io/fstream.hpp>

#include <boost/scope_exit.hpp>
#include <boost/range/adaptor/reversed.hpp>

#include <iostream>

#include <cstdint>
#include <deque>
#include <fstream>
#include <functional>

#include <stdlib.h>



void inside_apply_block_play_json(  const hive::protocol::signed_block& input_block ,   const uint32_t block_num)
{

  //ilog("MTLK Block: ${block_num}. Data : ${block}", ("block_num", block_num)("block", input_block));

  if(false)//block_num == 209120)
  {

    // ilog("MTLK Block: ${block_num}. Data : ${block}", ("block_num", block_num)("block", input_block));

    std::string json_filename;

    {
        std::ostringstream json_filename_stream;

        json_filename_stream << "/home/dev/jsons/block";
        json_filename_stream << std::setw(6) << std::setfill('0') <<  block_num ;
        json_filename_stream << ".json";
        json_filename = json_filename_stream.str();

    }

    {
        auto write_block = input_block;
        fc::variant v;
        fc::to_variant( write_block, v );
        auto s = fc::json::to_string( v );
       // ilog("SIZE: ${siz}", ("siz", s.size()));
        
        //ilog("MTLK variant: ${s}", ("s", s));

        fc::json::save_to_file( v, json_filename);
    }

    // {

    //     hive::protocol::signed_block read_block;

    //     {
    //         fc::variant vr = fc::json::from_file(json_filename);
    //         fc::from_variant(vr, read_block);
    //     }

    //     fc::variant wv;
    //     fc::to_variant( read_block, wv );
        
    //     fc::json::save_to_file( wv, json_filename + '2');
    //     int a = 0;
    //     a=a;
    // }


  }
}


long next_hf_time()
{
  // current "next hardfork" is HF28
  long hfTime =

#ifdef IS_TEST_NET
    1675818000; // Wednesday, February 8, 2023 1:00:00 AM
#else
    1682026620; // Thursday, April 20, 2023 9:37:00 PM
#endif /// IS_TEST_NET

  const char* value = getenv("HIVE_HF28_TIME");
  if (value)
  {
    hfTime = atol(value);
    ilog("HIVE_HF28_TIME has been specified through environment variable as ${value}, long value: ${hfTime}", (value)(hfTime));
  }

  return hfTime;
}

namespace hive { namespace chain {

struct object_schema_repr
{
  std::pair< uint16_t, uint16_t > space_type;
  std::string type;
};

struct operation_schema_repr
{
  std::string id;
  std::string type;
};

struct db_schema
{
  std::map< std::string, std::string > types;
  std::vector< object_schema_repr > object_types;
  std::string operation_type;
  std::vector< operation_schema_repr > custom_operation_types;
};

} }

FC_REFLECT( hive::chain::object_schema_repr, (space_type)(type) )
FC_REFLECT( hive::chain::operation_schema_repr, (id)(type) )
FC_REFLECT( hive::chain::db_schema, (types)(object_types)(operation_type)(custom_operation_types) )

namespace hive { namespace chain {

struct reward_fund_context
{
  uint128_t   recent_claims = 0;
  asset       reward_balance = asset( 0, HIVE_SYMBOL );
  share_type  hive_awarded = 0;
};

class database_impl
{
  public:
    database_impl( database& self );

    database&                                       _self;
    evaluator_registry< operation >                 _evaluator_registry;
    evaluator_registry< required_automated_action > _req_action_evaluator_registry;
    evaluator_registry< optional_automated_action > _opt_action_evaluator_registry;
    std::map<account_name_type, block_id_type>      _last_fast_approved_block_by_witness;
    bool                                            _last_pushed_block_was_before_checkpoint = false; // just used for logging
};

database_impl::database_impl( database& self )
  : _self(self), _evaluator_registry(self), _req_action_evaluator_registry(self), _opt_action_evaluator_registry(self) {}

database::database()
  : _my( new database_impl(*this) ) {}

database::~database()
{
  clear_pending();
}

void database::open( const open_args& args )
{
  try
  {
    init_schema();


    helpers::environment_extension_resources environment_extension(
                                                appbase::app().get_version_string(),
                                                appbase::app().get_plugins_names(),
                                                []( const std::string& message ){ wlog( message.c_str() ); }
                                              );
    
    chainbase::database::open( args.shared_mem_dir, args.chainbase_flags, args.shared_file_size, args.database_cfg, &environment_extension, args.dont_use_blocklog ? : args.force_replay );

    if(args.dont_use_blocklog)
      return;

    initialize_state_independent_data(args);
    load_state_initial_data(args);

  }
  FC_CAPTURE_LOG_AND_RETHROW( (args.data_dir)(args.shared_mem_dir)(args.shared_file_size) )
}

void database::initialize_state_independent_data(const open_args& args)
{
  initialize_indexes();
  initialize_evaluators();
  initialize_irreversible_storage();

  if(!find< dynamic_global_property_object >())
  {
    with_write_lock([&]()
    {
      init_genesis(args.initial_supply, args.hbd_initial_supply);
    });
  }

  _benchmark_dumper.set_enabled(args.benchmark_is_enabled);
  if( _benchmark_dumper.is_enabled() &&
      ( !_pre_apply_operation_signal.empty() || !_post_apply_operation_signal.empty() ) )
  {
    wlog( "BENCHMARK will run into nested measurements - data on operations that emit vops will be lost!!!" );
  }

  if(!args.dont_use_blocklog)
  {
    with_write_lock([&]()
    {
      _block_log.open(args.data_dir / "block_log");
      _block_log.set_compression(args.enable_block_log_compression);
      _block_log.set_compression_level(args.block_log_compression_level);
    });
  }
  
  _shared_file_full_threshold = args.shared_file_full_threshold;
  _shared_file_scale_rate = args.shared_file_scale_rate;

  /// Initialize all static (state independent) specific to hardforks
  init_hardforks();
}

void database::load_state_initial_data(const open_args& args)
{
  uint32_t hb = head_block_num();
  uint32_t last_irreversible_block = get_last_irreversible_block_num();

  FC_ASSERT(hb >= last_irreversible_block);

  ilog("Loaded a blockchain database holding a state specific to head block: ${hb} and last irreversible block: ${last_irreversible_block}", (hb)("last_irreversible_block", decorate_number_with_upticks(last_irreversible_block)));

  // Rewind all undo state. This should return us to the state at the last irreversible block.
  with_write_lock([&]() {
    ilog("Attempting to rewind all undo state...");

    undo_all();

    ilog("Rewind undo state done.");

    uint32_t new_hb = head_block_num();
    notify_switch_fork( new_hb );


    FC_ASSERT(new_hb >= last_irreversible_block);

    FC_ASSERT(get_last_irreversible_block_num() == last_irreversible_block, "Undo operation should not touch irreversible block value");

    ilog("Blockchain state database is AT IRREVERSIBLE state specific to head block: ${hb} and LIB: ${lb}",
         ("hb", decorate_number_with_upticks(head_block_num()))("lb", decorate_number_with_upticks(this->get_last_irreversible_block_num())));

    if (args.chainbase_flags & chainbase::skip_env_check)
    {
      set_revision(head_block_num());
    }
    else
    {
      FC_ASSERT(revision() == head_block_num(), "Chainbase revision does not match head block num.",
                ("rev", revision())("head_block", head_block_num()));
      if (args.do_validate_invariants)
        validate_invariants();
    }
  });

  if (head_block_num())
  {
    std::shared_ptr<full_block_type> head_block = _block_log.read_block_by_num(head_block_num());
    // This assertion should be caught and a reindex should occur
    FC_ASSERT(head_block && head_block->get_block_id() == head_block_id(),
    "Chain state {\"block-number\": ${block_number1} \"id\":\"${block_hash1}\"} does not match block log {\"block-number\": ${block_number2} \"id\":\"${block_hash2}\"}. Please reindex blockchain.",
    ("block_number1", head_block_num())("block_hash1", head_block_id())("block_number2", head_block ? head_block->get_block_num() : 0)("block_hash2", head_block ? head_block->get_block_id() : block_id_type()));

    _fork_db.start_block(head_block);
  }

  with_read_lock([&]() {
    const auto& hardforks = get_hardfork_property_object();
    FC_ASSERT(hardforks.last_hardfork <= HIVE_NUM_HARDFORKS, "Chain knows of more hardforks than configuration", ("hardforks.last_hardfork", hardforks.last_hardfork)("HIVE_NUM_HARDFORKS", HIVE_NUM_HARDFORKS));
    FC_ASSERT(_hardfork_versions.versions[hardforks.last_hardfork] <= HIVE_BLOCKCHAIN_VERSION, "Blockchain version is older than last applied hardfork");
    FC_ASSERT(HIVE_BLOCKCHAIN_HARDFORK_VERSION >= HIVE_BLOCKCHAIN_VERSION);
    FC_ASSERT(HIVE_BLOCKCHAIN_HARDFORK_VERSION == _hardfork_versions.versions[HIVE_NUM_HARDFORKS], "Blockchain version mismatch", (HIVE_BLOCKCHAIN_HARDFORK_VERSION)(_hardfork_versions.versions[HIVE_NUM_HARDFORKS]));
  });

#ifdef USE_ALTERNATE_CHAIN_ID
  /// Leave the chain-id passed to cmdline option.
#else
  with_read_lock([&]() {
    const auto& hardforks = get_hardfork_property_object();
    if(hardforks.last_hardfork >= HIVE_HARDFORK_1_24)
    {
      ilog("Loaded blockchain which had already processed hardfork 24, setting Hive chain id");
      set_chain_id(HIVE_CHAIN_ID);
    }
  });
#endif /// IS_TEST_NET
}


uint32_t database::reindex_internal( const open_args& args, const std::shared_ptr<full_block_type>& start_block )
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

    if (current_block_num % 10000 == 0)
    {
      std::ostringstream percent_complete_stream;
      percent_complete_stream << std::fixed << std::setprecision(2) << double(current_block_num) * 100 / last_block_num;
      ulog("   ${current_block_num} of ${last_block_num} blocks = ${percent_complete}%   (${free_memory_megabytes}MB shared memory free)",
           ("percent_complete", percent_complete_stream.str())
           ("current_block_num", decorate_number_with_upticks(current_block_num))
           ("last_block_num", decorate_number_with_upticks(last_block_num))
           ("free_memory_megabytes", decorate_number_with_upticks(get_free_memory() >> 20)));
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
         ("n", decorate_number_with_upticks(last_applied_block->get_block_num()))("id", last_applied_block->get_block_id()));

  fc::enable_record_assert_trip = rat; //restore flag
  fc::enable_assert_stacktrace = as;

  return last_applied_block->get_block_num();
}

bool database::is_reindex_complete( uint64_t* head_block_num_in_blocklog, uint64_t* head_block_num_in_db ) const
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

uint32_t database::reindex( const open_args& args )
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
        {
          start_block = _block_log.read_block_by_num( _head_block_num + 1 );

          static auto enable = false;
          if(enable)
          {
            std::ifstream json_file("/home/dev/jsons/block.json");
            if(json_file)
            {
              std::stringstream raw_data_stream;
              raw_data_stream << json_file.rdbuf();
              std::string raw_data = raw_data_stream.str();

            fc::variant v = fc::json::from_string( raw_data );

              hive::protocol::operation op;
              fc::from_variant( v, op );
              int a = 0;
              a=a;
            }
          }

        }

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
          ilog("Resume of replaying. Last applied block: ${n}", ( "n", decorate_number_with_upticks(_last_block_number - 1 )) );

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

void database::wipe( const fc::path& data_dir, const fc::path& shared_mem_dir, bool include_blocks)
{
  if( get_is_open() )
    close();
  chainbase::database::wipe( shared_mem_dir );
  if( include_blocks )
  {
    fc::remove_all( data_dir / "block_log" );
    fc::remove_all( data_dir / "block_log.index" );
  }
}

void database::close(bool rewind)
{
  try
  {
    if(get_is_open() == false)
      wlog("database::close method is MISUSED since it is NOT opened atm...");

    ilog( "Closing database" );

    // Since pop_block() will move tx's in the popped blocks into pending,
    // we have to clear_pending() after we're done popping to get a clean
    // DB state (issue #336).
    clear_pending();

    chainbase::database::flush();

    auto lib = this->get_last_irreversible_block_num();

    ilog("Database flushed at last irreversible block: ${b}", ("b", decorate_number_with_upticks(lib)));

    chainbase::database::close();

    _block_log.close();

    _fork_db.reset();

    ilog( "Database is closed" );
  }
  FC_CAPTURE_AND_RETHROW()
}

//no chainbase lock required
bool database::is_known_block(const block_id_type& id)const
{ try {
  if (_fork_db.fetch_block(id))
    return true;

  auto requested_block_num = protocol::block_header::num_from_id(id);
  auto read_block_id = _block_log.read_block_id_by_num(requested_block_num);

  return read_block_id != block_id_type() && read_block_id == id;
} FC_CAPTURE_AND_RETHROW() }

//no chainbase lock required, but fork database read lock is required
bool database::is_known_block_unlocked(const block_id_type& id)const
{ try {
  if (_fork_db.fetch_block_unlocked(id, true /* only search linked blocks */))
    return true;

  auto requested_block_num = protocol::block_header::num_from_id(id);
  auto read_block_id = _block_log.read_block_id_by_num(requested_block_num);

  return read_block_id != block_id_type() && read_block_id == id;
} FC_CAPTURE_AND_RETHROW() }

/**
  * Only return true *if* the transaction has not expired or been invalidated. If this
  * method is called with a VERY old transaction we will return false, they should
  * query things by blocks if they are that old.
  */
bool database::is_known_transaction( const transaction_id_type& id )const
{ try {
  const auto& trx_idx = get_index<transaction_index>().indices().get<by_trx_id>();
  return trx_idx.find( id ) != trx_idx.end();
} FC_CAPTURE_AND_RETHROW() }

//no chainbase lock required
block_id_type database::find_block_id_for_num( uint32_t block_num )const
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
block_id_type database::get_block_id_for_num( uint32_t block_num )const
{
  block_id_type bid = find_block_id_for_num( block_num );
  if (bid == block_id_type())
    FC_THROW_EXCEPTION(fc::key_not_found_exception, "block number not found");
  return bid;
}

//no chainbase lock required
std::shared_ptr<full_block_type> database::fetch_block_by_id( const block_id_type& id )const
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
std::shared_ptr<full_block_type> database::fetch_block_by_number( uint32_t block_num, fc::microseconds wait_for_microseconds )const
{ try {
  shared_ptr<fork_item> forkdb_item = _fork_db.fetch_block_on_main_branch_by_number(block_num, wait_for_microseconds);
  if (forkdb_item)
    return forkdb_item->full_block;

  return _block_log.read_block_by_num(block_num);
} FC_LOG_AND_RETHROW() }

//no chainbase lock required
std::vector<std::shared_ptr<full_block_type>> database::fetch_block_range( const uint32_t starting_block_num, const uint32_t count, fc::microseconds wait_for_microseconds )
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

chain_id_type database::get_chain_id() const
{
  return hive_chain_id;
}

chain_id_type database::get_old_chain_id() const
{
#ifdef USE_ALTERNATE_CHAIN_ID
  return hive_chain_id; /// In testnet always use the chain-id passed as hived option
#else
  return OLD_CHAIN_ID;
#endif /// IS_TEST_NET
}

chain_id_type database::get_new_chain_id() const
{
#ifdef USE_ALTERNATE_CHAIN_ID
  return hive_chain_id; /// In testnet always use the chain-id passed as hived option
#else
  return HIVE_CHAIN_ID;
#endif /// IS_TEST_NET
}

void database::set_chain_id( const chain_id_type& chain_id )
{
  hive_chain_id = chain_id;

  idump( (hive_chain_id) );
#ifdef USE_ALTERNATE_CHAIN_ID
  set_chain_id_for_transaction_signature_validation(chain_id);
#endif
}

const witness_object& database::get_witness( const account_name_type& name ) const
{ try {
  return get< witness_object, by_name >( name );
} FC_CAPTURE_AND_RETHROW( (name) ) }

const witness_object* database::find_witness( const account_name_type& name ) const
{
  return find< witness_object, by_name >( name );
}

std::string database::get_treasury_name( uint32_t hardfork ) const
{
  if( hardfork >= HIVE_HARDFORK_1_24_TREASURY_RENAME )
    return NEW_HIVE_TREASURY_ACCOUNT;
  else
    return OBSOLETE_TREASURY_ACCOUNT;
}

bool database::is_treasury( const account_name_type& name )const
{
  return ( name == NEW_HIVE_TREASURY_ACCOUNT ) || ( name == OBSOLETE_TREASURY_ACCOUNT );
}

const account_object& database::get_account( const account_id_type id )const
{ try {
  return get< account_object, by_id >( id );
} FC_CAPTURE_AND_RETHROW( (id) ) }

const account_object* database::find_account( const account_id_type& id )const
{
  return find< account_object, by_id >( id );
}

const account_object& database::get_account( const account_name_type& name )const
{ try {
  return get< account_object, by_name >( name );
} FC_CAPTURE_AND_RETHROW( (name) ) }

const account_object* database::find_account( const account_name_type& name )const
{
  return find< account_object, by_name >( name );
}

const comment_object& database::get_comment( comment_id_type comment_id )const try
{
  return get< comment_object, by_id >( comment_id );
}
FC_CAPTURE_AND_RETHROW( (comment_id) )

const comment_object& database::get_comment( const account_id_type& author, const shared_string& permlink )const
{ try {
  return get< comment_object, by_permlink >( comment_object::compute_author_and_permlink_hash( author, to_string( permlink ) ) );
} FC_CAPTURE_AND_RETHROW( (author)(permlink) ) }

const comment_object* database::find_comment( const account_id_type& author, const shared_string& permlink )const
{
  return find< comment_object, by_permlink >( comment_object::compute_author_and_permlink_hash( author, to_string( permlink ) ) );
}

const comment_object& database::get_comment( const account_name_type& author, const shared_string& permlink )const
{ try {
  return get_comment( get_account(author).get_id(), permlink );
} FC_CAPTURE_AND_RETHROW( (author)(permlink) ) }

const comment_object* database::find_comment( const account_name_type& author, const shared_string& permlink )const
{
  const account_object* acc = find_account(author);
  if(acc == nullptr) return nullptr;
  return find_comment( acc->get_id(), permlink );
}

#ifndef ENABLE_STD_ALLOCATOR

const comment_object& database::get_comment( const account_id_type& author, const string& permlink )const
{ try {
  return get< comment_object, by_permlink >( comment_object::compute_author_and_permlink_hash( author, permlink ) );
} FC_CAPTURE_AND_RETHROW( (author)(permlink) ) }

const comment_object* database::find_comment( const account_id_type& author, const string& permlink )const
{
  return find< comment_object, by_permlink >( comment_object::compute_author_and_permlink_hash( author, permlink ) );
}

const comment_object& database::get_comment( const account_name_type& author, const string& permlink )const
{ try {
  return get_comment( get_account(author).get_id(), permlink );
} FC_CAPTURE_AND_RETHROW( (author)(permlink) ) }

const comment_object* database::find_comment( const account_name_type& author, const string& permlink )const
{
  const account_object* acc = find_account(author);
  if(acc == nullptr) return nullptr;
  return find_comment( acc->get_id(), permlink );
}

#endif

const escrow_object& database::get_escrow( const account_name_type& name, uint32_t escrow_id )const
{ try {
  return get< escrow_object, by_from_id >( boost::make_tuple( name, escrow_id ) );
} FC_CAPTURE_AND_RETHROW( (name)(escrow_id) ) }

const escrow_object* database::find_escrow( const account_name_type& name, uint32_t escrow_id )const
{
  return find< escrow_object, by_from_id >( boost::make_tuple( name, escrow_id ) );
}

const limit_order_object& database::get_limit_order( const account_name_type& name, uint32_t orderid )const
{ try {
  if( !has_hardfork( HIVE_HARDFORK_0_6__127 ) )
    orderid = orderid & 0x0000FFFF;

  return get< limit_order_object, by_account >( boost::make_tuple( name, orderid ) );
} FC_CAPTURE_AND_RETHROW( (name)(orderid) ) }

const limit_order_object* database::find_limit_order( const account_name_type& name, uint32_t orderid )const
{
  if( !has_hardfork( HIVE_HARDFORK_0_6__127 ) )
    orderid = orderid & 0x0000FFFF;

  return find< limit_order_object, by_account >( boost::make_tuple( name, orderid ) );
}

const savings_withdraw_object& database::get_savings_withdraw( const account_name_type& owner, uint32_t request_id )const
{ try {
  return get< savings_withdraw_object, by_from_rid >( boost::make_tuple( owner, request_id ) );
} FC_CAPTURE_AND_RETHROW( (owner)(request_id) ) }

const savings_withdraw_object* database::find_savings_withdraw( const account_name_type& owner, uint32_t request_id )const
{
  return find< savings_withdraw_object, by_from_rid >( boost::make_tuple( owner, request_id ) );
}

const dynamic_global_property_object&database::get_dynamic_global_properties() const
{ try {
  return get< dynamic_global_property_object >();
} FC_CAPTURE_AND_RETHROW() }

const node_property_object& database::get_node_properties() const
{
  return _node_property_object;
}

const feed_history_object& database::get_feed_history()const
{ try {
  return get< feed_history_object >();
} FC_CAPTURE_AND_RETHROW() }

const witness_schedule_object& database::get_witness_schedule_object()const
{ try {
  return get< witness_schedule_object >();
} FC_CAPTURE_AND_RETHROW() }

const witness_schedule_object& database::get_future_witness_schedule_object() const
{
  try
  {
    return get<witness_schedule_object>(witness_schedule_object::id_type(1));
  }
  catch (const std::out_of_range&)
  {
    FC_THROW_EXCEPTION(fc::key_not_found_exception, "Future witness schedule does not exist");
  }
}

const hardfork_property_object& database::get_hardfork_property_object()const
{ try {
  return get< hardfork_property_object >();
} FC_CAPTURE_AND_RETHROW() }

const comment_object& database::get_comment_for_payout_time( const comment_object& comment )const
{
  if( has_hardfork( HIVE_HARDFORK_0_17__769 ) || comment.is_root() )
    return comment;
  else
    return get< comment_object >( find_comment_cashout_ex( comment )->get_root_id() );
}

const time_point_sec database::calculate_discussion_payout_time( const comment_object& comment )const
{
  const comment_object& _comment = get_comment_for_payout_time( comment );

  const comment_cashout_object* comment_cashout = find_comment_cashout( _comment );
  if( comment_cashout )
    return comment_cashout->get_cashout_time();
  else
    return time_point_sec::maximum();
}

const time_point_sec database::calculate_discussion_payout_time( const comment_object& comment, const comment_cashout_object& comment_cashout )const
{
  const comment_object& _comment = get_comment_for_payout_time( comment );

  if( _comment.get_id() == comment.get_id() )
  {
    return comment_cashout.get_cashout_time();
  }
  else
  {
    const comment_cashout_object* _comment_cashout = find_comment_cashout( _comment );
    if( _comment_cashout )
      return _comment_cashout->get_cashout_time();
    else
      return time_point_sec::maximum();
  }
}

const reward_fund_object& database::get_reward_fund() const
{
  return get< reward_fund_object, by_name >( HIVE_POST_REWARD_FUND_NAME );
}

const comment_cashout_object* database::find_comment_cashout( const comment_object& comment ) const
{
  return find_comment_cashout( comment.get_id() );
}

const comment_cashout_object* database::find_comment_cashout( comment_id_type comment_id ) const
{
  const auto& idx = get_index< comment_cashout_index, by_id >();
  comment_cashout_object::id_type ccid( comment_id );
  auto found = idx.find( ccid );

  if( found == idx.end() )
    return nullptr;
  else
    return &( *found );
}

const comment_cashout_ex_object* database::find_comment_cashout_ex( const comment_object& comment ) const
{
  return find_comment_cashout_ex( comment.get_id() );
}

const comment_cashout_ex_object* database::find_comment_cashout_ex( comment_id_type comment_id ) const
{
  if( has_hardfork( HIVE_HARDFORK_0_19 ) )
    return nullptr;

  const auto& idx = get_index< comment_cashout_ex_index, by_id >();
  comment_cashout_ex_object::id_type ccid( comment_id );
  auto found = idx.find( ccid );

  FC_ASSERT( found != idx.end() );
  return &( *found );
}

const comment_object& database::get_comment( const comment_cashout_object& comment_cashout ) const
{
  const auto& idx = get_index< comment_index >().indices().get< by_id >();
  auto found = idx.find( comment_cashout.get_comment_id() );

  FC_ASSERT( found != idx.end() );

  return *found;
}

void database::remove_old_cashouts()
{
  // Remove all cashout extras
  auto& comment_cashout_ex_idx = get_mutable_index< comment_cashout_ex_index >();
  comment_cashout_ex_idx.clear();

  // Remove regular cashouts for paid comments
  const auto& idx = get_index< comment_cashout_index, by_cashout_time >();
  auto itr = idx.find( fc::time_point_sec::maximum() );

  while( itr != idx.end() )
  {
    const auto& current = *itr;
    ++itr;
    remove( current );
  }
}

asset database::get_effective_vesting_shares( const account_object& account, asset_symbol_type vested_symbol )const
{
  if( vested_symbol == VESTS_SYMBOL )
    return asset( account.get_effective_vesting_shares( false ), VESTS_SYMBOL );

#ifdef HIVE_ENABLE_SMT
  FC_ASSERT( vested_symbol.space() == asset_symbol_type::smt_nai_space );
  FC_ASSERT( vested_symbol.is_vesting() );

FC_TODO( "Update the code below when delegation is modified to support SMTs." )
  const account_regular_balance_object* bo = find< account_regular_balance_object, by_owner_liquid_symbol >(
    boost::make_tuple( account.get_id(), vested_symbol.get_paired_symbol() ) );
  if( bo == nullptr )
    return asset( 0, vested_symbol );

  return bo->vesting;
#else
  FC_ASSERT( false, "Invalid symbol" );
#endif
}

uint32_t database::witness_participation_rate()const
{
  const dynamic_global_property_object& dpo = get_dynamic_global_properties();
  return uint64_t(HIVE_100_PERCENT) * fc::uint128_popcount(dpo.recent_slots_filled) / 128;
}

void database::add_checkpoints( const flat_map< uint32_t, block_id_type >& checkpts )
{
  for( const auto& i : checkpts )
    _checkpoints[i.first] = i.second;
  if (!_checkpoints.empty())
    blockchain_worker_thread_pool::get_instance().set_last_checkpoint(_checkpoints.rbegin()->first);
}

bool database::before_last_checkpoint()const
{
  return (_checkpoints.size() > 0) && (_checkpoints.rbegin()->first >= head_block_num());
}

/**
  * Push block "may fail" in which case every partial change is unwound.  After
  * push block is successful the block is appended to the chain database on disk.
  *
  * @return true if we switched forks as a result of this push.
  */

//na krotkim blocklogu zobacz jak to push blok chodzi

bool database::push_block( const block_flow_control& block_ctrl, uint32_t skip )
{
  const std::shared_ptr<full_block_type>& full_block = block_ctrl.get_full_block();
  const signed_block& new_block = full_block->get_block();

  uint32_t block_num = full_block->get_block_num();
  if( _checkpoints.size() && _checkpoints.rbegin()->second != block_id_type() )
  {
    auto itr = _checkpoints.find( block_num );
    if( itr != _checkpoints.end() )
      FC_ASSERT(full_block->get_block_id() == itr->second, "Block did not match checkpoint", ("checkpoint", *itr)("block_id", full_block->get_block_id()));

    if( _checkpoints.rbegin()->first >= block_num )
    {
      skip = skip_witness_signature
          | skip_transaction_signatures
          | skip_transaction_dupe_check
          /*| skip_fork_db Fork db cannot be skipped or else blocks will not be written out to block log */
          | skip_block_size_check
          | skip_tapos_check
          | skip_authority_check
          /* | skip_merkle_check While blockchain is being downloaded, txs need to be validated against block headers */
          | skip_undo_history_check
          | skip_witness_schedule_check
          | skip_validate
          | skip_validate_invariants
          ;
      if (!_my->_last_pushed_block_was_before_checkpoint)
      {
        // log something to let the node operator know that checkpoints are in force
        ilog("checkpoints enabled, doing reduced validation until final checkpoint, block ${block_num}, id ${block_id}",
             ("block_num", _checkpoints.rbegin()->first)("block_id", _checkpoints.rbegin()->second));
        _my->_last_pushed_block_was_before_checkpoint = true;
      }
    }
    else if (_my->_last_pushed_block_was_before_checkpoint)
    {
      ilog("final checkpoint reached, resuming normal block validation");
      _my->_last_pushed_block_was_before_checkpoint = false;
    }
  }

  bool result;
  detail::with_skip_flags( *this, skip, [&]()
  {
    detail::without_pending_transactions( *this, block_ctrl, std::move(_pending_tx), [&]()
    {
      try
      {
        result = _push_block( block_ctrl );
        block_ctrl.on_end_of_apply_block();
        notify_finish_push_block( full_block );
      }
      FC_CAPTURE_AND_RETHROW((new_block))

      check_free_memory( false, full_block->get_block_num() );
    });
  });

  //fc::time_point end_time = fc::time_point::now();
  //fc::microseconds dt = end_time - begin_time;
  //if( ( new_block.block_num() % 10000 ) == 0 )
  //   ilog( "push_block ${b} took ${t} microseconds", ("b", new_block.block_num())("t", dt.count()) );
  return result;
}

void database::_maybe_warn_multiple_production( uint32_t height )const
{
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

void database::switch_forks(const item_ptr new_head)
{
  uint32_t skip = get_node_properties().skip_flags;

  BOOST_SCOPE_EXIT(void) { ilog("Done fork switch"); } BOOST_SCOPE_EXIT_END
  ilog("Switching to fork: ${id}", ("id", new_head->get_block_id()));
  const block_id_type original_head_block_id = head_block_id();
  const uint32_t original_head_block_number = head_block_num();
  ilog("Before switching, head_block_id is ${original_head_block_id} head_block_number ${original_head_block_number}", (original_head_block_id)(original_head_block_number));
  const auto [new_branch, old_branch] = _fork_db.fetch_branch_from(new_head->get_block_id(), original_head_block_id);

  ilog("Destination branch block ids:");
  std::for_each(new_branch.begin(), new_branch.end(), [](const item_ptr& item) {
    ilog(" - ${id}", ("id", item->get_block_id()));
  });
  const block_id_type common_block_id = new_branch.back()->previous_id();
  const uint32_t common_block_number = new_branch.back()->get_block_num() - 1;

  ilog(" - common_block_id ${common_block_id} common_block_number ${common_block_number} (block before first block in branch, should be common)", (common_block_id)(common_block_number));

  if (old_branch.size())
  {
    ilog("Source branch block ids:");
    std::for_each(old_branch.begin(), old_branch.end(), [](const item_ptr& item) {
      ilog(" - ${id}", ("id", item->get_block_id()));
    });

    try
    {
      // pop blocks until we hit the common ancestor block
      while (head_block_id() != old_branch.back()->previous_id())
      {
        const block_id_type id_being_popped = head_block_id();
        ilog(" - Popping block ${id_being_popped}", (id_being_popped));
        pop_block();
        ilog(" - Popped block ${id_being_popped}", (id_being_popped));
      }
      ilog("Done popping blocks");
    }
    FC_LOG_AND_RETHROW()
  }

  notify_switch_fork(head_block_num());

  // push all blocks on the new fork
  for (auto ritr = new_branch.crbegin(); ritr != new_branch.crend(); ++ritr)
  {
    ilog("pushing block #${block_num} from new fork, id ${id}", ("block_num", (*ritr)->get_block_num())("id", (*ritr)->get_block_id()));
    std::shared_ptr<fc::exception> delayed_exception_to_avoid_yield_in_catch;
    try
    {
      BOOST_SCOPE_EXIT(this_) { this_->clear_tx_status(); } BOOST_SCOPE_EXIT_END
      // we have to treat blocks from fork as not validated
      set_tx_status(database::TX_STATUS_P2P_BLOCK);
      _fork_db.set_head(*ritr);
      auto session = start_undo_session();
      apply_block((*ritr)->full_block, skip);
      session.push();
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
      if (get_last_irreversible_block_num() < common_block_number && head_block_num() < original_head_block_number)
      {
        try
        {
          // pop all blocks from the bad fork
          while (head_block_id() != common_block_id)
          {
            ilog(" - reverting to previous chain, popping block ${id}", ("id", head_block_id()));
            pop_block();
          }
          ilog(" - reverting to previous chain, done popping blocks");
        }
        FC_LOG_AND_RETHROW()
        notify_switch_fork(head_block_num());

        // restore any popped blocks from the good fork
        if (old_branch.size())
        {
          ilog("restoring blocks from original fork");
          for (auto ritr = old_branch.crbegin(); ritr != old_branch.crend(); ++ritr)
          {
            ilog(" - restoring block ${id}", ("id", (*ritr)->get_block_id()));
            BOOST_SCOPE_EXIT(this_) { this_->clear_tx_status(); } BOOST_SCOPE_EXIT_END
            // even though those blocks were already processed before, it is safer to treat them as completely new,
            // especially since alternative would be to treat them as replayed blocks, but that would be misleading
            // since replayed blocks are already irreversible, while these are clearly reversible
            set_tx_status(database::TX_STATUS_P2P_BLOCK);
            _fork_db.set_head(*ritr);
            auto session = start_undo_session();
            apply_block((*ritr)->full_block, skip);
            session.push();
          }
          ilog("done restoring blocks from original fork");
        }
      }
      delayed_exception_to_avoid_yield_in_catch->dynamic_rethrow_exception();
    }
  }
  ilog("done pushing blocks from new fork");
  hive::notify("switching forks", "id", new_head->get_block_id().str(), "num", new_head->get_block_num());
}

bool database::_push_block(const block_flow_control& block_ctrl)
{ try {
  const std::shared_ptr<full_block_type>& full_block = block_ctrl.get_full_block();

  const uint32_t skip = get_node_properties().skip_flags;
  std::vector<std::shared_ptr<full_block_type>> blocks;

  if (!(skip & skip_fork_db)) //if fork checking enabled
  {
    const item_ptr new_head = _fork_db.push_block(full_block);
    block_ctrl.on_fork_db_insert();
    _maybe_warn_multiple_production( new_head->get_block_num() );

    // if the new head block is at a lower height than our head block,
    // it is on a shorter fork, so don't validate it
    if (new_head->get_block_num() <= head_block_num())
    {
      block_ctrl.on_fork_ignore();
      return false;
    }

    //if new_head indirectly builds off the current head_block
    // then there's no fork switch, we're just linking in previously unlinked blocks to the main branch
    for (item_ptr block = new_head;
         block->get_block_num() > head_block_num();
         block = block->prev.lock())
    {
      blocks.push_back(block->full_block);
      if (block->get_block_num() == 1) //prevent crash backing up to null in for-loop
        break;
    }
    //we've found a longer fork, so do a fork switch to pop back to the common block of the two forks
    if (blocks.back()->get_block_header().previous != head_block_id())
    {
      block_ctrl.on_fork_apply();
      ilog("calling switch_forks() from _push_block()");
      switch_forks(new_head);
      return true;
    }
  }
  else //fork checking not enabled, just try to push the new block
    blocks.push_back(full_block);

  //we are building off our head block, try to add the block(s)
  block_ctrl.on_fork_normal();
  for (auto iter = blocks.crbegin(); iter != blocks.crend(); ++iter)
  {
    try
    {
      BOOST_SCOPE_EXIT(this_) { this_->clear_tx_status(); } BOOST_SCOPE_EXIT_END;
      set_tx_status(database::TX_STATUS_P2P_BLOCK);

      // if we've linked in a chain of multiple blocks, we need to keep the fork_db's head block in sync
      // with what we're applying.  If we're only appending a single block, the forkdb's head block
      // should already be correct
      if (blocks.size() > 1)
        _fork_db.set_head(_fork_db.fetch_block((*iter)->get_block_id(), true));

      auto session = start_undo_session();
      apply_block(*iter, skip);
      session.push();
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
} FC_CAPTURE_AND_RETHROW() }

bool is_fast_confirm_transaction(const std::shared_ptr<full_transaction_type>& full_transaction)
{
  const signed_transaction& trx = full_transaction->get_transaction();
  return trx.operations.size() == 1 && trx.operations.front().which() == operation::tag<witness_block_approve_operation>::value;
}

/**
  * Attempts to push the transaction into the pending queue
  *
  * When called to push a locally generated transaction, set the skip_block_size_check bit on the skip argument. This
  * will allow the transaction to be pushed even if it causes the pending block size to exceed the maximum block size.
  * Although the transaction will probably not propagate further now, as the peers are likely to have their pending
  * queues full as well, it will be kept in the queue to be propagated later when a new block flushes out the pending
  * queues.
  */
void database::push_transaction( const std::shared_ptr<full_transaction_type>& full_transaction, uint32_t skip )
{
  const signed_transaction& trx = full_transaction->get_transaction(); // just for the rethrow
  try
  {
    if (is_fast_confirm_transaction(full_transaction))
    {
      // fast-confirm transactions are just processed in memory, they're not added to the blockchain
      process_fast_confirm_transaction(full_transaction);
      return;
    }

    size_t trx_size = full_transaction->get_transaction_size();
    //ABW: why is that limit related to block size and not HIVE_MAX_TRANSACTION_SIZE?
    //DLN: the block size is dynamically voted by witnesses, so this code ensures that the transaction
    //can fit into the currently voted block size.
    auto trx_size_limit = get_dynamic_global_properties().maximum_block_size - 256;
    FC_ASSERT(trx_size <= trx_size_limit, "Transaction too large - size = ${trx_size}, limit ${trx_size_limit}",
              (trx_size)(trx_size_limit));

    detail::with_skip_flags(*this, skip, [&]()
    {
      BOOST_SCOPE_EXIT( this_ ) { this_->clear_tx_status(); } BOOST_SCOPE_EXIT_END
      set_tx_status( TX_STATUS_UNVERIFIED );
      _push_transaction(full_transaction);
    });
  }
  FC_CAPTURE_AND_RETHROW((trx))
}

void database::_push_transaction(const std::shared_ptr<full_transaction_type>& full_transaction)
{
  // If this is the first transaction pushed after applying a block, start a new undo session.
  // This allows us to quickly rewind to the clean state of the head block, in case a new block arrives.
  if (!_pending_tx_session.valid())
    _pending_tx_session = start_undo_session();

  // Create a temporary undo session as a child of _pending_tx_session.
  // The temporary session will be discarded by the destructor if
  // _apply_transaction fails.  If we make it to merge(), we
  // apply the changes.

  auto temp_session = start_undo_session();
  _apply_transaction(full_transaction);
  _pending_tx.push_back(full_transaction);

  notify_changed_objects();
  // The transaction applied successfully. Merge its changes into the pending block session.
  temp_session.squash();
}

/**
  * Removes the most recent block from the database and
  * undoes any changes it made.
  */
void database::pop_block()
{
  try
  {
    try
    {
      _pending_tx_session.reset();
    }
    FC_CAPTURE_AND_RETHROW()

    block_id_type head_id = head_block_id();

    /// save the head block so we can recover its transactions
    std::shared_ptr<full_block_type> full_head_block;
    try
    {
      full_head_block = fetch_block_by_id(head_id);
    }
    FC_CAPTURE_AND_RETHROW()

    HIVE_ASSERT(full_head_block, pop_empty_chain, "there are no blocks to pop");

    try
    {
      _fork_db.pop_block();
    }
    FC_CAPTURE_AND_RETHROW()
    try
    {
      undo();
    }
    FC_CAPTURE_AND_RETHROW()

    try
    {
      _popped_tx.insert(_popped_tx.begin(), full_head_block->get_full_transactions().begin(), full_head_block->get_full_transactions().end());
    }
    FC_CAPTURE_AND_RETHROW()
  }
  FC_CAPTURE_AND_RETHROW()
}

void database::clear_pending()
{
  try
  {
    assert( _pending_tx.empty() || _pending_tx_session.valid() );
    _pending_tx.clear();
    _pending_tx_session.reset();
  }
  FC_CAPTURE_AND_RETHROW()
}

void database::push_virtual_operation( const operation& op )
{
  FC_ASSERT( is_virtual_operation( op ) );
  _current_op_in_trx++;
  operation_notification note = create_operation_notification( op );
  notify_pre_apply_operation( note );
  notify_post_apply_operation( note );
}

void database::pre_push_virtual_operation( const operation& op )
{
  FC_ASSERT( is_virtual_operation( op ) );
  _current_op_in_trx++;
  operation_notification note = create_operation_notification( op );
  notify_pre_apply_operation( note );
}

void database::post_push_virtual_operation( const operation& op, const fc::optional<uint64_t>& op_in_trx )
{
  FC_ASSERT( is_virtual_operation( op ) );
  operation_notification note = create_operation_notification( op );
  if(op_in_trx.valid()) note.op_in_trx = *op_in_trx;
  notify_post_apply_operation( note );
}

void database::notify_pre_apply_operation( const operation_notification& note )
{
  HIVE_TRY_NOTIFY( _pre_apply_operation_signal, note )
}

struct action_validate_visitor
{
  typedef void result_type;

  template< typename Action >
  void operator()( const Action& a )const
  {
    a.validate();
  }
};

void database::push_required_action( const required_automated_action& a, time_point_sec execution_time )
{
  FC_ASSERT( execution_time >= head_block_time(), "Cannot push required action to execute in the past. head_block_time: ${h} execution_time: ${e}",
    ("h", head_block_time())("e", execution_time) );

  static const action_validate_visitor validate_visitor;
  a.visit( validate_visitor );

  create< pending_required_action_object >( [&]( pending_required_action_object& pending_action )
  {
    pending_action.action = a;
    pending_action.execution_time = execution_time;
  });
}

void database::push_required_action( const required_automated_action& a )
{
  push_required_action( a, head_block_time() );
}

void database::push_optional_action( const optional_automated_action& a, time_point_sec execution_time )
{
  FC_ASSERT( execution_time >= head_block_time(), "Cannot push optional action to execute in the past. head_block_time: ${h} execution_time: ${e}",
    ("h", head_block_time())("e", execution_time) );

  static const action_validate_visitor validate_visitor;
  a.visit( validate_visitor );

  create< pending_optional_action_object >( [&]( pending_optional_action_object& pending_action )
  {
    pending_action.action = a;
    pending_action.execution_time = execution_time;
  });
}

void database::push_optional_action( const optional_automated_action& a )
{
  push_optional_action( a, head_block_time() );
}

void database::notify_pre_apply_required_action( const required_action_notification& note )
{
  HIVE_TRY_NOTIFY( _pre_apply_required_action_signal, note );
}

void database::notify_post_apply_required_action( const required_action_notification& note )
{
  HIVE_TRY_NOTIFY( _post_apply_required_action_signal, note );
}

void database::notify_pre_apply_optional_action( const optional_action_notification& note )
{
  HIVE_TRY_NOTIFY( _pre_apply_optional_action_signal, note );
}

void database::notify_post_apply_optional_action( const optional_action_notification& note )
{
  HIVE_TRY_NOTIFY( _post_apply_optional_action_signal, note );
}

void database::notify_post_apply_operation( const operation_notification& note )
{
  HIVE_TRY_NOTIFY( _post_apply_operation_signal, note )
}

void database::notify_pre_apply_block( const block_notification& note )
{
  HIVE_TRY_NOTIFY( _pre_apply_block_signal, note )
}

void database::notify_irreversible_block( uint32_t block_num )
{
  HIVE_TRY_NOTIFY( _on_irreversible_block, block_num )
}

void database::notify_switch_fork( uint32_t block_num )
{
  HIVE_TRY_NOTIFY( _switch_fork_signal, block_num )
}

void database::notify_post_apply_block( const block_notification& note )
{
  HIVE_TRY_NOTIFY( _post_apply_block_signal, note )
}

void database::notify_fail_apply_block( const block_notification& note )
{
  HIVE_TRY_NOTIFY( _fail_apply_block_signal, note )
}

void database::notify_pre_apply_transaction( const transaction_notification& note )
{
  HIVE_TRY_NOTIFY( _pre_apply_transaction_signal, note )
}

void database::notify_post_apply_transaction( const transaction_notification& note )
{
  HIVE_TRY_NOTIFY( _post_apply_transaction_signal, note )
}

void database::notify_prepare_snapshot_data_supplement(const prepare_snapshot_supplement_notification& note)
{
  HIVE_TRY_NOTIFY(_prepare_snapshot_supplement_signal, note)
}

void database::notify_load_snapshot_data_supplement(const load_snapshot_supplement_notification& note)
{
  HIVE_TRY_NOTIFY(_load_snapshot_supplement_signal, note)
}

void database::notify_comment_reward(const comment_reward_notification& note)
{
  HIVE_TRY_NOTIFY(_comment_reward_signal, note)
}

void database::notify_end_of_syncing()
{
  HIVE_TRY_NOTIFY(_end_of_syncing_signal)
}

void database::notify_pre_apply_custom_operation( const custom_operation_notification& note )
{
  HIVE_TRY_NOTIFY( _pre_apply_custom_operation_signal, note )
}

void database::notify_post_apply_custom_operation( const custom_operation_notification& note )
{
  HIVE_TRY_NOTIFY( _post_apply_custom_operation_signal, note )
}

void database::notify_finish_push_block( const block_notification& note )
{
  HIVE_TRY_NOTIFY( _finish_push_block_signal, note )
}

account_name_type database::get_scheduled_witness( uint32_t slot_num )const
{
  const dynamic_global_property_object& dpo = get_dynamic_global_properties();
  const witness_schedule_object& wso = get_witness_schedule_object();
  uint64_t current_aslot = dpo.current_aslot + slot_num;
  return wso.current_shuffled_witnesses[ current_aslot % wso.num_scheduled_witnesses ];
}

fc::time_point_sec database::get_slot_time(uint32_t slot_num)const
{
  if( slot_num == 0 )
    return fc::time_point_sec();

  auto interval = HIVE_BLOCK_INTERVAL;
  const dynamic_global_property_object& dpo = get_dynamic_global_properties();

  if( head_block_num() == 0 )
  {
    // n.b. first block is at genesis_time plus one block interval
    fc::time_point_sec genesis_time = dpo.time;
    return genesis_time + slot_num * interval;
  }

  int64_t head_block_abs_slot = head_block_time().sec_since_epoch() / interval;
  fc::time_point_sec head_slot_time( head_block_abs_slot * interval );

  // "slot 0" is head_slot_time
  // "slot 1" is head_slot_time,
  //   plus maint interval if head block is a maint block
  //   plus block interval if head block is not a maint block
  return head_slot_time + (slot_num * interval);
}

uint32_t database::get_slot_at_time(fc::time_point_sec when)const
{
  fc::time_point_sec first_slot_time = get_slot_time( 1 );
  if( when < first_slot_time )
    return 0;
  return (when - first_slot_time).to_seconds() / HIVE_BLOCK_INTERVAL + 1;
}

/**
  *  Converts HIVE into HBD and adds it to to_account while reducing the HIVE supply
  *  by HIVE and increasing the HBD supply by the specified amount.
  */
std::pair< asset, asset > database::create_hbd( const account_object& to_account, asset hive, bool to_reward_balance )
{
  std::pair< asset, asset > assets( asset( 0, HBD_SYMBOL ), asset( 0, HIVE_SYMBOL ) );

  try
  {
    if( hive.amount == 0 )
      return assets;

    const auto& median_price = get_feed_history().current_median_history;
    const auto& gpo = get_dynamic_global_properties();

    if( !median_price.is_null() )
    {
      auto to_hbd = ( gpo.hbd_print_rate * hive.amount ) / HIVE_100_PERCENT;
      auto to_hive = hive.amount - to_hbd;

      auto hbd = asset( to_hbd, HIVE_SYMBOL ) * median_price;

      if( to_reward_balance )
      {
        adjust_reward_balance( to_account, hbd );
        adjust_reward_balance( to_account, asset( to_hive, HIVE_SYMBOL ) );
      }
      else
      {
        adjust_balance( to_account, hbd );
        adjust_balance( to_account, asset( to_hive, HIVE_SYMBOL ) );
      }

      adjust_supply( asset( -to_hbd, HIVE_SYMBOL ) );
      adjust_supply( hbd );
      assets.first = hbd;
      assets.second = asset( to_hive, HIVE_SYMBOL );
    }
    else
    {
      adjust_balance( to_account, hive );
      assets.second = hive;
    }
  }
  FC_CAPTURE_LOG_AND_RETHROW( (to_account.get_name())(hive) )

  return assets;
}

asset calculate_vesting( database& db, const asset& liquid, bool to_reward_balance )
{
  auto calculate_new_vesting = [ liquid ] ( price vesting_share_price ) -> asset
  {
    /**
      *  The ratio of total_vesting_shares / total_vesting_fund_hive should not
      *  change as the result of the user adding funds
      *
      *  V / C  = (V+Vn) / (C+Cn)
      *
      *  Simplifies to Vn = (V * Cn ) / C
      *
      *  If Cn equals o.amount, then we must solve for Vn to know how many new vesting shares
      *  the user should receive.
      *
      *  128 bit math is requred due to multiplying of 64 bit numbers. This is done in asset and price.
      */
    asset new_vesting = liquid * ( vesting_share_price );
    return new_vesting;
  };

#ifdef HIVE_ENABLE_SMT
  if( liquid.symbol.space() == asset_symbol_type::smt_nai_space )
  {
    FC_ASSERT( liquid.symbol.is_vesting() == false );
    // Get share price.
    const auto& smt = db.get< smt_token_object, by_symbol >( liquid.symbol );
    FC_ASSERT( smt.allow_voting == to_reward_balance, "No voting - no rewards" );
    price vesting_share_price = to_reward_balance ? smt.get_reward_vesting_share_price() : smt.get_vesting_share_price();
    // Calculate new vesting from provided liquid using share price.
    return calculate_new_vesting( vesting_share_price );
  }
#endif

  FC_ASSERT( liquid.symbol == HIVE_SYMBOL );
  // ^ A novelty, needed but risky in case someone managed to slip HBD/TESTS here in blockchain history.
  // Get share price.
  const auto& cprops = db.get_dynamic_global_properties();
  price vesting_share_price = to_reward_balance ? cprops.get_reward_vesting_share_price() : cprops.get_vesting_share_price();
  // Calculate new vesting from provided liquid using share price.
  return calculate_new_vesting( vesting_share_price );
}

asset database::adjust_account_vesting_balance(const account_object& to_account, const asset& liquid, bool to_reward_balance, Before&& before_vesting_callback )
{
  try
  {
#ifdef HIVE_ENABLE_SMT
    if( liquid.symbol.space() == asset_symbol_type::smt_nai_space )
    {
      asset new_vesting = calculate_vesting( *this, liquid, to_reward_balance );
      before_vesting_callback( new_vesting );
      // Add new vesting to owner's balance.
      if( to_reward_balance )
        adjust_reward_balance( to_account, liquid, new_vesting );
      else
        adjust_balance( to_account, new_vesting );
      // Update global vesting pool numbers.
      const auto& smt = get< smt_token_object, by_symbol >( liquid.symbol );
      modify( smt, [&]( smt_token_object& smt_object )
      {
        if( to_reward_balance )
        {
          smt_object.pending_rewarded_vesting_shares += new_vesting.amount;
          smt_object.pending_rewarded_vesting_smt += liquid.amount;
        }
        else
        {
          smt_object.total_vesting_fund_smt += liquid.amount;
          smt_object.total_vesting_shares += new_vesting.amount;
        }
      } );

      // NOTE that SMT vesting does not impact witness voting.

      return new_vesting;
    }
#endif
    asset new_vesting = calculate_vesting( *this, liquid, to_reward_balance );
    before_vesting_callback( new_vesting );
    // Add new vesting to owner's balance.

    const auto& cprops = get_dynamic_global_properties();

    if( to_reward_balance )
    {
      adjust_reward_balance( to_account, liquid, new_vesting );
    }
    else
    {
      if( has_hardfork( HIVE_HARDFORK_0_20__2539 ) )
      {
        modify( to_account, [&]( account_object& a )
        {
          util::update_manabar( cprops, a, new_vesting.amount.value );
        });
      }

      adjust_balance( to_account, new_vesting );
    }
    // Update global vesting pool numbers.
    modify( cprops, [&]( dynamic_global_property_object& props )
    {
      if( to_reward_balance )
      {
        props.pending_rewarded_vesting_shares += new_vesting;
        props.pending_rewarded_vesting_hive += liquid;
      }
      else
      {
        props.total_vesting_fund_hive += liquid;
        props.total_vesting_shares += new_vesting;
      }
    } );

    return new_vesting;
  }
  FC_CAPTURE_AND_RETHROW( (to_account.get_name())(liquid) )
}

// Create vesting, then a caller-supplied callback after determining how many shares to create, but before
// we modify the database.
// This allows us to implement virtual op pre-notifications in the Before function.
template< typename Before >
asset create_vesting2( database& db, const account_object& to_account, const asset& liquid, bool to_reward_balance, Before&& before_vesting_callback )
{
  try
  {
    asset new_vesting = db.adjust_account_vesting_balance( to_account, liquid, to_reward_balance, std::forward<Before>( before_vesting_callback ) );

    // Update witness voting numbers.
    if( !to_reward_balance )
      db.adjust_proxied_witness_votes( to_account, new_vesting.amount );

    return new_vesting;
  }
  FC_CAPTURE_AND_RETHROW( (to_account.get_name())(liquid) )
}

/**
  * @param to_account - the account to receive the new vesting shares
  * @param liquid     - HIVE or liquid SMT to be converted to vesting shares
  */
asset database::create_vesting( const account_object& to_account, const asset& liquid, bool to_reward_balance )
{
  return create_vesting2( *this, to_account, liquid, to_reward_balance, []( asset vests_created ) {} );
}

fc::sha256 database::get_pow_target()const
{
  const auto& dgp = get_dynamic_global_properties();
  fc::sha256 target;
  target._hash[0] = -1;
  target._hash[1] = -1;
  target._hash[2] = -1;
  target._hash[3] = -1;
  target = target >> ((dgp.num_pow_witnesses/4)+4);
  return target;
}

uint32_t database::get_pow_summary_target()const
{
  const dynamic_global_property_object& dgp = get_dynamic_global_properties();
  if( dgp.num_pow_witnesses >= 1004 )
    return 0;

  if( has_hardfork( HIVE_HARDFORK_0_16__551 ) )
    return (0xFE00 - 0x0040 * dgp.num_pow_witnesses ) << 0x10;
  else
    return (0xFC00 - 0x0040 * dgp.num_pow_witnesses) << 0x10;
}

void database::adjust_proxied_witness_votes( const account_object& a,
                        const std::array< share_type, HIVE_MAX_PROXY_RECURSION_DEPTH+1 >& delta,
                        int depth )
{
  if( a.has_proxy() )
  {
    /// nested proxies are not supported, vote will not propagate
    if( depth >= HIVE_MAX_PROXY_RECURSION_DEPTH )
      return;

    const auto& proxy = get_account( a.get_proxy() );

    modify( proxy, [&]( account_object& a )
    {
      for( int i = HIVE_MAX_PROXY_RECURSION_DEPTH - depth - 1; i >= 0; --i )
      {
        a.proxied_vsf_votes[i+depth] += delta[i];
      }
    } );

    adjust_proxied_witness_votes( proxy, delta, depth + 1 );
  }
  else
  {
    share_type total_delta = 0;
    for( int i = HIVE_MAX_PROXY_RECURSION_DEPTH - depth; i >= 0; --i )
      total_delta += delta[i];
    adjust_witness_votes( a, total_delta );
  }
}

void database::adjust_proxied_witness_votes( const account_object& a, share_type delta, int depth )
{
  if( a.has_proxy() )
  {
    /// nested proxies are not supported, vote will not propagate
    if( depth >= HIVE_MAX_PROXY_RECURSION_DEPTH )
      return;

    const auto& proxy = get_account( a.get_proxy() );

    modify( proxy, [&]( account_object& a )
    {
      a.proxied_vsf_votes[depth] += delta;
    } );

    adjust_proxied_witness_votes( proxy, delta, depth + 1 );
  }
  else
  {
    adjust_witness_votes( a, delta );
  }
}

void database::nullify_proxied_witness_votes( const account_object& a )
{
  std::array<share_type, HIVE_MAX_PROXY_RECURSION_DEPTH + 1> delta;
  delta[ 0 ] = -a.get_direct_governance_vote_power();
  for( int i = 0; i < HIVE_MAX_PROXY_RECURSION_DEPTH; ++i )
    delta[ i + 1 ] = -a.proxied_vsf_votes[ i ];
  adjust_proxied_witness_votes( a, delta );
}

void database::adjust_witness_votes( const account_object& a, const share_type& delta )
{
  const auto& vidx = get_index< witness_vote_index >().indices().get< by_account_witness >();
  auto itr = vidx.lower_bound( boost::make_tuple( a.get_name(), account_name_type() ) );
  while( itr != vidx.end() && itr->account == a.get_name() )
  {
    adjust_witness_vote( get< witness_object, by_name >(itr->witness), delta );
    ++itr;
  }
}

void database::adjust_witness_vote( const witness_object& witness, share_type delta )
{
  const witness_schedule_object& wso = has_hardfork(HIVE_HARDFORK_1_27_FIX_TIMESHARE_WITNESS_SCHEDULING) ?
                                       get_witness_schedule_object_for_irreversibility() : get_witness_schedule_object();
  modify( witness, [&]( witness_object& w )
  {
    auto delta_pos = w.votes.value * (wso.current_virtual_time - w.virtual_last_update);
    w.virtual_position += delta_pos;

    w.virtual_last_update = wso.current_virtual_time;
    w.votes += delta;
    FC_ASSERT( w.votes <= get_dynamic_global_properties().total_vesting_shares.amount, "", ("w.votes", w.votes)("props",get_dynamic_global_properties().total_vesting_shares) );

    if( has_hardfork( HIVE_HARDFORK_0_2 ) )
      w.virtual_scheduled_time = w.virtual_last_update + (HIVE_VIRTUAL_SCHEDULE_LAP_LENGTH2 - w.virtual_position)/(w.votes.value+1);
    else
      w.virtual_scheduled_time = w.virtual_last_update + (HIVE_VIRTUAL_SCHEDULE_LAP_LENGTH - w.virtual_position)/(w.votes.value+1);

    /** witnesses with a low number of votes could overflow the time field and end up with a scheduled time in the past */
    if( has_hardfork( HIVE_HARDFORK_0_4 ) )
    {
      if( w.virtual_scheduled_time < wso.current_virtual_time )
        w.virtual_scheduled_time = fc::uint128_max_value();
    }
  } );
}

void database::clear_witness_votes( const account_object& a )
{
  const auto& vidx = get_index< witness_vote_index >().indices().get<by_account_witness>();
  auto itr = vidx.lower_bound( boost::make_tuple( a.get_name(), account_name_type() ) );
  while( itr != vidx.end() && itr->account == a.get_name() )
  {
    const auto& current = *itr;
    ++itr;
    remove(current);
  }

  if( has_hardfork( HIVE_HARDFORK_0_6__104 ) )
    modify( a, [&](account_object& acc )
    {
      acc.witnesses_voted_for = 0;
    });
}

bool database::collect_account_total_balance( const account_object& account, asset* total_hive, asset* total_hbd,
  asset* total_vests, asset* vesting_shares_hive_value )
{
  const auto& gpo = get_dynamic_global_properties();

  *vesting_shares_hive_value = account.get_vesting() * gpo.get_vesting_share_price();
  *total_hive = *vesting_shares_hive_value;
  *total_vests = account.get_vesting();
  *total_hive += account.get_vest_rewards_as_hive();
  *total_vests += account.get_vest_rewards();

  *total_hive += account.get_balance();
  *total_hive += account.get_savings();
  *total_hive += account.get_rewards();

  *total_hbd = account.get_hbd_balance();
  *total_hbd += account.get_hbd_savings();
  *total_hbd += account.get_hbd_rewards();

  return ( total_hive->amount.value != 0 ) || ( total_hbd->amount.value != 0 ) || ( total_vests->amount.value != 0 );
}

void database::clear_null_account_balance()
{
  if( !has_hardfork( HIVE_HARDFORK_0_14__327 ) ) return;

  const auto& null_account = get_account( HIVE_NULL_ACCOUNT );
  asset total_hive, total_hbd, total_vests, vesting_shares_hive_value;

  if( !( collect_account_total_balance( null_account, &total_hive, &total_hbd, &total_vests, &vesting_shares_hive_value ) ) )
    return;

  operation vop_op = clear_null_account_balance_operation();
  clear_null_account_balance_operation& vop = vop_op.get< clear_null_account_balance_operation >();
  if( total_hive.amount.value > 0 )
    vop.total_cleared.push_back( total_hive );
  if( total_vests.amount.value > 0 )
    vop.total_cleared.push_back( total_vests );
  if( total_hbd.amount.value > 0 )
    vop.total_cleared.push_back( total_hbd );
  pre_push_virtual_operation( vop_op );

  /////////////////////////////////////////////////////////////////////////////////////

  if( null_account.get_balance().amount > 0 )
  {
    adjust_balance( null_account, -null_account.get_balance() );
  }

  if( null_account.get_savings().amount > 0 )
  {
    adjust_savings_balance( null_account, -null_account.get_savings() );
  }

  if( null_account.get_hbd_balance().amount > 0 )
  {
    adjust_balance( null_account, -null_account.get_hbd_balance() );
  }

  if( null_account.get_hbd_savings().amount > 0 )
  {
    adjust_savings_balance( null_account, -null_account.get_hbd_savings() );
  }

  if( null_account.get_vesting().amount > 0 )
  {
    const auto& gpo = get_dynamic_global_properties();

    modify( gpo, [&]( dynamic_global_property_object& g )
    {
      g.total_vesting_shares -= null_account.get_vesting();
      g.total_vesting_fund_hive -= vesting_shares_hive_value;
    });

    modify( null_account, [&]( account_object& a )
    {
      a.vesting_shares.amount = 0;
      a.sum_delayed_votes = 0;
      a.delayed_votes.clear();
    });
  }

  if( null_account.get_rewards().amount > 0 )
  {
    adjust_reward_balance( null_account, -null_account.get_rewards() );
  }

  if( null_account.get_hbd_rewards().amount > 0 )
  {
    adjust_reward_balance( null_account, -null_account.get_hbd_rewards() );
  }

  if( null_account.get_vest_rewards().amount > 0 )
  {
    const auto& gpo = get_dynamic_global_properties();

    modify( gpo, [&]( dynamic_global_property_object& g )
    {
      g.pending_rewarded_vesting_shares -= null_account.get_vest_rewards();
      g.pending_rewarded_vesting_hive -= null_account.get_vest_rewards_as_hive();
    });

    modify( null_account, [&]( account_object& a )
    {
      a.reward_vesting_hive.amount = 0;
      a.reward_vesting_balance.amount = 0;
    });
  }

  //////////////////////////////////////////////////////////////

  if( total_hive.amount > 0 )
    adjust_supply( -total_hive );

  if( total_hbd.amount > 0 )
    adjust_supply( -total_hbd );

  post_push_virtual_operation( vop_op );
}

void database::consolidate_treasury_balance()
{
  if( !has_hardfork( HIVE_HARDFORK_1_24_TREASURY_RENAME ) )
    return;

  auto treasury_name = get_treasury_name();
  auto old_treasury_name = get_treasury_name( HIVE_HARDFORK_1_24_TREASURY_RENAME - 1 );

  const auto& old_treasury_account = get_account( old_treasury_name );
  asset total_hive, total_hbd, total_vests, vesting_shares_hive_value;

  if( treasury_name == old_treasury_name || //ABW: once we actually include change of treasury in HF24 that part of condition can be removed
    !( collect_account_total_balance( old_treasury_account, &total_hive, &total_hbd, &total_vests, &vesting_shares_hive_value ) ) )
    return;

  const auto& treasury_account = get_treasury();

  operation vop_op = consolidate_treasury_balance_operation();
  consolidate_treasury_balance_operation& vop = vop_op.get< consolidate_treasury_balance_operation >();
  if( total_hive.amount.value > 0 )
    vop.total_moved.push_back( total_hive );
  if( total_vests.amount.value > 0 )
    vop.total_moved.push_back( total_vests );
  if( total_hbd.amount.value > 0 )
    vop.total_moved.push_back( total_hbd );
  pre_push_virtual_operation( vop_op );

  /////////////////////////////////////////////////////////////////////////////////////

  if( old_treasury_account.get_balance().amount > 0 )
  {
    adjust_balance( treasury_account, old_treasury_account.get_balance() );
    adjust_balance( old_treasury_account, -old_treasury_account.get_balance() );
  }

  if( old_treasury_account.get_savings().amount > 0 )
  {
    adjust_savings_balance( treasury_account, old_treasury_account.get_savings() );
    adjust_savings_balance( old_treasury_account, -old_treasury_account.get_savings() );
  }

  if( old_treasury_account.get_hbd_balance().amount > 0 )
  {
    adjust_balance( treasury_account, old_treasury_account.get_hbd_balance() );
    adjust_balance( old_treasury_account, -old_treasury_account.get_hbd_balance() );
  }

  if( old_treasury_account.get_hbd_savings().amount > 0 )
  {
    adjust_savings_balance( treasury_account, old_treasury_account.get_hbd_savings() );
    adjust_savings_balance( old_treasury_account, -old_treasury_account.get_hbd_savings() );
  }

  if( old_treasury_account.get_vesting().amount > 0 )
  {
    //note that if we wanted to move vests in vested form it would complicate delayed_votes part;
    //not that treasury could gain anything from vests anyway, so it is better to liquify them
    adjust_balance( treasury_account, vesting_shares_hive_value );

    const auto& gpo = get_dynamic_global_properties();
    modify( gpo, [&]( dynamic_global_property_object& g )
    {
      g.total_vesting_shares -= old_treasury_account.get_vesting();
      g.total_vesting_fund_hive -= vesting_shares_hive_value;
    } );

    modify( old_treasury_account, [&]( account_object& a )
    {
      a.vesting_shares.amount = 0;
      a.sum_delayed_votes = 0;
      a.delayed_votes.clear();
    } );
  }

  if( old_treasury_account.get_rewards().amount > 0 )
  {
    adjust_reward_balance( treasury_account, old_treasury_account.get_rewards() );
    adjust_reward_balance( old_treasury_account, -old_treasury_account.get_rewards() );
  }

  if( old_treasury_account.get_hbd_rewards().amount > 0 )
  {
    adjust_reward_balance( treasury_account, old_treasury_account.get_hbd_rewards() );
    adjust_reward_balance( old_treasury_account, -old_treasury_account.get_hbd_rewards() );
  }

  if( old_treasury_account.get_vest_rewards().amount > 0 )
  {
    //see above handling of regular vests
    adjust_balance( treasury_account, old_treasury_account.get_vest_rewards_as_hive() );

    const auto& gpo = get_dynamic_global_properties();
    modify( gpo, [ & ]( dynamic_global_property_object& g )
    {
      g.pending_rewarded_vesting_shares -= old_treasury_account.get_vest_rewards();
      g.pending_rewarded_vesting_hive -= old_treasury_account.get_vest_rewards_as_hive();
    } );

    modify( old_treasury_account, [&]( account_object& a )
    {
      a.reward_vesting_hive.amount = 0;
      a.reward_vesting_balance.amount = 0;
    } );
  }

  //////////////////////////////////////////////////////////////

  post_push_virtual_operation( vop_op );
}

void database::lock_account( const account_object& account )
{
  auto* account_auth = find< account_authority_object, by_account >( account.get_name() );
  if( account_auth == nullptr )
  {
    create< account_authority_object >( [&]( account_authority_object& auth )
    {
      auth.account = account.get_name();
      auth.owner.weight_threshold = 1;
      auth.active.weight_threshold = 1;
      auth.posting.weight_threshold = 1;
    } );
  }
  else
  {
    modify( *account_auth, []( account_authority_object& auth )
    {
      auth.owner.weight_threshold = 1;
      auth.owner.clear();

      auth.active.weight_threshold = 1;
      auth.active.clear();

      auth.posting.weight_threshold = 1;
      auth.posting.clear();
    } );
  }

  modify( account, []( account_object& a )
  {
    a.set_recovery_account( a );
    a.memo_key = public_key_type();
  } );

  auto rec_req = find< account_recovery_request_object, by_account >( account.get_name() );
  if( rec_req )
    remove( *rec_req );

  auto change_request = find< change_recovery_account_request_object, by_account >( account.get_name() );
  if( change_request )
    remove( *change_request );
}

void database::restore_accounts( const std::set< std::string >& restored_accounts )
{
  const auto& hardforks = get_hardfork_property_object();

  const auto& treasury_account = get_treasury();
  auto treasury_name = get_treasury_name();

  for( auto& name : restored_accounts )
  {
    auto found = hardforks.h23_balances.find( name );

    if( found == hardforks.h23_balances.end() )
    {
      #ifndef IS_TEST_NET
        ilog( "The account ${acc} hadn't removed balances, balances can't be restored", ( "acc", name ) );
      #endif
      continue;
    }

    const auto* account_ptr = find_account( name );
    if( account_ptr == nullptr )
    {
      ilog( "The account ${acc} doesn't exist at all, balances can't be restored", ( "acc", name ) );
      continue;
    }

    adjust_balance( treasury_account, -found->second.hbd_balance );
    adjust_balance( treasury_account, -found->second.balance );

    adjust_balance( *account_ptr, found->second.hbd_balance );
    adjust_balance( *account_ptr, found->second.balance );

    operation vop = hardfork_hive_restore_operation( name, treasury_name, found->second.hbd_balance, found->second.balance );
    push_virtual_operation( vop );

    ilog( "Balances ${hbd} and ${hive} for the account ${acc} were restored", ( "hbd", found->second.hbd_balance )( "hive", found->second.balance )( "acc", name ) );
  }
}

void database::gather_balance( const std::string& name, const asset& balance, const asset& hbd_balance )
{
  modify( get_hardfork_property_object(), [&]( hardfork_property_object& hfp )
  {
    hfp.h23_balances.emplace( std::make_pair( name, hf23_item{ balance, hbd_balance } ) );
  } );
}

void database::clear_accounts( const std::set< std::string >& cleared_accounts )
{
  auto treasury_name = get_treasury_name();
  for( const auto& account_name : cleared_accounts )
  {
    const auto* account_ptr = find_account( account_name );
    if( account_ptr == nullptr )
      continue;

    clear_account( *account_ptr );
  }
}

void database::clear_account( const account_object& account )
{
  const auto& account_name = account.get_name();
  FC_ASSERT( account_name != get_treasury_name(), "Can't clear treasury account" );

  const auto& treasury_account = get_treasury();
  const auto& cprops = get_dynamic_global_properties();

  hardfork_hive_operation vop( account_name, treasury_account.get_name() );

  asset total_transferred_hive = asset( 0, HIVE_SYMBOL );
  asset total_transferred_hbd = asset( 0, HBD_SYMBOL );
  asset total_converted_vests = asset( 0, VESTS_SYMBOL );
  asset total_hive_from_vests = asset( 0, HIVE_SYMBOL );

  if( account.get_vesting().amount > 0 )
  {
    // Collect delegations and their delegatees to capture all affected accounts before delegations are deleted
    std::vector< std::pair< const vesting_delegation_object&, const account_object& > > delegations;

    const auto& delegation_idx = get_index< vesting_delegation_index, by_delegation >();
    auto delegation_itr = delegation_idx.lower_bound( account.get_id() );
    while( delegation_itr != delegation_idx.end() && delegation_itr->get_delegator() == account.get_id() )
    {
      delegations.emplace_back( *delegation_itr, get_account( delegation_itr->get_delegatee() ) );
      vop.other_affected_accounts.emplace_back( delegations.back().second.get_name() );
      ++delegation_itr;
    }

    // emit vop with other_affected_accounts filled but before any change happened on the accounts
    pre_push_virtual_operation( vop );

    // Remove all delegations
    asset freed_delegations = asset( 0, VESTS_SYMBOL );

    for( auto& delegation_pair : delegations )
    {
      const auto& delegation = delegation_pair.first;
      const auto& delegatee = delegation_pair.second;

      modify( delegatee, [&]( account_object& a )
      {
        util::update_manabar( cprops, a );
        a.received_vesting_shares -= delegation.get_vesting();
        freed_delegations += delegation.get_vesting();

        a.voting_manabar.use_mana( delegation.get_vesting().amount.value );

        a.downvote_manabar.use_mana(
          fc::uint128_to_int64((uint128_t(delegation.get_vesting().amount.value) * cprops.downvote_pool_percent) /
          HIVE_100_PERCENT));
      } );

      remove( delegation );
    }

    // Remove pending expired delegations
    const auto& exp_delegation_idx = get_index< vesting_delegation_expiration_index, by_account_expiration >();
    auto exp_delegation_itr = exp_delegation_idx.lower_bound( account.get_id() );
    while( exp_delegation_itr != exp_delegation_idx.end() && exp_delegation_itr->get_delegator() == account.get_id() )
    {
      auto& delegation = *exp_delegation_itr;
      ++exp_delegation_itr;

      freed_delegations += delegation.get_vesting();
      remove( delegation );
    }

    auto vests_to_convert = account.get_vesting();
    auto converted_hive = vests_to_convert * cprops.get_vesting_share_price();
    total_converted_vests += account.get_vesting();
    total_hive_from_vests += asset( converted_hive, HIVE_SYMBOL );

    adjust_proxied_witness_votes( account, -account.get_vesting().amount );

    modify( account, [&]( account_object& a )
    {
      util::update_manabar( cprops, a );
      a.voting_manabar.current_mana = 0;
      a.downvote_manabar.current_mana = 0;
      a.vesting_shares = asset( 0, VESTS_SYMBOL );
      //FC_ASSERT( a.delegated_vesting_shares == freed_delegations, "Inconsistent amount of delegations" );
      a.delegated_vesting_shares = asset( 0, VESTS_SYMBOL );
      a.vesting_withdraw_rate.amount = 0;
      a.next_vesting_withdrawal = fc::time_point_sec::maximum();
      a.to_withdraw.amount = 0;
      a.withdrawn.amount = 0;

      if( has_hardfork( HIVE_HARDFORK_1_24 ) )
      {
        a.delayed_votes.clear();
        a.sum_delayed_votes = 0;
      }
    } );

    adjust_balance( treasury_account, asset( converted_hive, HIVE_SYMBOL ) );
    modify( cprops, [&]( dynamic_global_property_object& o )
    {
      o.total_vesting_fund_hive -= converted_hive;
      o.total_vesting_shares -= vests_to_convert;
    } );
  }
  else
  {
    // just emit empty vop, since there was nothing to fill it with so far
    pre_push_virtual_operation( vop );
  }

  // Remove pending escrows (return balance to account - compare with expire_escrow_ratification())
  const auto& escrow_idx = get_index< chain::escrow_index, chain::by_from_id >();
  auto escrow_itr = escrow_idx.lower_bound( account_name );
  while( escrow_itr != escrow_idx.end() && escrow_itr->from == account_name )
  {
    auto& escrow = *escrow_itr;
    ++escrow_itr;

    adjust_balance( account, escrow.get_hive_balance() );
    adjust_balance( account, escrow.get_hbd_balance() );
    adjust_balance( account, escrow.get_fee() );

    modify( account, []( account_object& a )
    {
      a.pending_escrow_transfers--;
    } );

    remove( escrow );
  }

  // Remove open limit orders (return balance to account - compare with clear_expired_orders())
  const auto& order_idx = get_index< chain::limit_order_index, chain::by_account >();
  auto order_itr = order_idx.lower_bound( account_name );
  while( order_itr != order_idx.end() && order_itr->seller == account_name )
  {
    auto& order = *order_itr;
    ++order_itr;

    cancel_order( order, true );
  }

  // Remove pending convert requests (return balance to account)
  const auto& request_idx = get_index< chain::convert_request_index, chain::by_owner >();
  auto request_itr = request_idx.lower_bound( account.get_id() );
  while( request_itr != request_idx.end() && request_itr->get_owner() == account.get_id() )
  {
    auto& request = *request_itr;
    ++request_itr;

    adjust_balance( account, request.get_convert_amount() );
    remove( request );
  }

  // Make sure there are no pending collateralized convert requests
  // (if we decided to handle them anyway, in case we wanted to reuse this routine outsede HF23 code,
  // we should most likely destroy collateral balance instead of putting it into treasury)
  const auto& collateralized_request_idx = get_index< chain::collateralized_convert_request_index, chain::by_owner >();
  auto collateralized_request_itr = collateralized_request_idx.lower_bound( account.get_id() );
  FC_ASSERT( collateralized_request_itr == collateralized_request_idx.end() || collateralized_request_itr->get_owner() != account.get_id(),
    "Collateralized convert requests not handled by clear_account" );

  // Remove ongoing saving withdrawals (return/pass balance to account)
  const auto& withdraw_from_idx = get_index< savings_withdraw_index, by_from_rid >();
  auto withdraw_from_itr = withdraw_from_idx.lower_bound( account_name );
  while( withdraw_from_itr != withdraw_from_idx.end() && withdraw_from_itr->from == account_name )
  {
    auto& withdrawal = *withdraw_from_itr;
    ++withdraw_from_itr;

    adjust_balance( account, withdrawal.amount );
    modify( account, []( account_object& a )
    {
      a.savings_withdraw_requests--;
    } );

    remove( withdrawal );
  }

  const auto& withdraw_to_idx = get_index< savings_withdraw_index, by_to_complete >();
  auto withdraw_to_itr = withdraw_to_idx.lower_bound( account_name );
  while( withdraw_to_itr != withdraw_to_idx.end() && withdraw_to_itr->to == account_name )
  {
    auto& withdrawal = *withdraw_to_itr;
    ++withdraw_to_itr;

    adjust_balance( account, withdrawal.amount );
    modify( get_account( withdrawal.from ), []( account_object& a )
    {
      a.savings_withdraw_requests--;
    } );

    remove( withdrawal );
  }

  // Touch HBD balances (to be sure all interests are added to balances)
  if( has_hardfork( HIVE_HARDFORK_1_24 ) )
  {
    adjust_balance( account, asset( 0, HBD_SYMBOL ) );
    adjust_savings_balance( account, asset( 0, HBD_SYMBOL ) );
    adjust_reward_balance( account, asset( 0, HBD_SYMBOL ) );
  }

  // Remove remaining savings balances
  total_transferred_hive += account.get_savings();
  total_transferred_hbd += account.get_hbd_savings();
  adjust_balance( treasury_account, account.get_savings() );
  adjust_savings_balance( account, -account.get_savings() );
  adjust_balance( treasury_account, account.get_hbd_savings() );
  adjust_savings_balance( account, -account.get_hbd_savings() );

  // Remove HBD and HIVE balances
  total_transferred_hive += account.get_balance();
  total_transferred_hbd += account.get_hbd_balance();
  adjust_balance( treasury_account, account.get_balance() );
  adjust_balance( account, -account.get_balance() );
  adjust_balance( treasury_account, account.get_hbd_balance() );
  adjust_balance( account, -account.get_hbd_balance() );

  // Transfer reward balances
  total_transferred_hive += account.get_rewards();
  total_transferred_hbd += account.get_hbd_rewards();
  adjust_balance( treasury_account, account.get_rewards() );
  adjust_reward_balance( account, -account.get_rewards() );
  adjust_balance( treasury_account, account.get_hbd_rewards() );
  adjust_reward_balance( account, -account.get_hbd_rewards() );

  // Convert and transfer vesting rewards
  adjust_balance( treasury_account, account.get_vest_rewards_as_hive() );
  total_converted_vests += account.get_vest_rewards();
  total_hive_from_vests += account.get_vest_rewards_as_hive();

  modify( get_dynamic_global_properties(), [&]( dynamic_global_property_object& gpo )
  {
    gpo.pending_rewarded_vesting_shares -= account.get_vest_rewards();
    gpo.pending_rewarded_vesting_hive -= account.get_vest_rewards_as_hive();
  } );

  modify( account, []( account_object &a )
  {
    a.reward_vesting_balance = asset( 0, VESTS_SYMBOL );
    a.reward_vesting_hive = asset( 0, HIVE_SYMBOL );
  } );

  vop.hbd_transferred = total_transferred_hbd;
  vop.hive_transferred = total_transferred_hive;
  vop.vests_converted = total_converted_vests;
  vop.total_hive_from_vests = total_hive_from_vests;

  gather_balance( account_name, vop.hive_transferred, vop.hbd_transferred );

  post_push_virtual_operation( vop );
}

void database::process_proposals( const block_notification& note )
{
  if( has_hardfork( HIVE_PROPOSALS_HARDFORK ) )
  {
    dhf_processor dhf( *this );
    dhf.run( note );
  }
}

void database::process_delayed_voting( const block_notification& note )
{
  if( has_hardfork( HIVE_HARDFORK_1_24 ) )
  {
    delayed_voting dv( *this );
    dv.run( note.get_block_timestamp() );
  }
}

/**
  *  Iterates over all recurrent transfers with a due date date before
  *  the head block time and then executes the transfers
  */
void database::process_recurrent_transfers()
{
  if( !has_hardfork( HIVE_HARDFORK_1_25 ) )
    return;

  auto now = head_block_time();
  const auto& recurrent_transfers_by_date = get_index< recurrent_transfer_index, by_trigger_date >();
  auto itr = recurrent_transfers_by_date.begin();

  // uint16_t is okay because we stop at 1000, if the limit changes, make sure to check if it fits in the integer.
  uint16_t processed_transfers = 0;
  if( _benchmark_dumper.is_enabled() )
    _benchmark_dumper.begin();
  while( itr != recurrent_transfers_by_date.end() && itr->get_trigger_date() <= now )
  {
    // Since this is an intensive process, we don't want to process too many recurrent transfers in a single block
    if (processed_transfers >= HIVE_MAX_RECURRENT_TRANSFERS_PER_BLOCK)
    {
      ilog("Reached max processed recurrent transfers this block");
      break;
    }

    auto &current_recurrent_transfer = *itr;
    ++itr;

    const auto &from_account = get_account(current_recurrent_transfer.from_id);
    const auto &to_account = get_account(current_recurrent_transfer.to_id);
    asset available = get_balance(from_account, current_recurrent_transfer.amount.symbol);
    FC_ASSERT(current_recurrent_transfer.remaining_executions > 0);
    const auto remaining_executions = current_recurrent_transfer.remaining_executions -1;
    bool remove_recurrent_transfer = false;

    // If we have enough money, we proceed with the transfer
    if (available >= current_recurrent_transfer.amount)
    {
      adjust_balance(from_account, -current_recurrent_transfer.amount);
      adjust_balance(to_account, current_recurrent_transfer.amount);

      // No need to update the object if we know that we will remove it
      if (remaining_executions == 0)
      {
        remove_recurrent_transfer = true;
      }
      else
      {
        modify(current_recurrent_transfer, [&](recurrent_transfer_object &rt)
        {
          rt.consecutive_failures = 0; // reset the consecutive failures counter
          rt.update_next_trigger_date();
          rt.remaining_executions = remaining_executions;
        });
      }

      push_virtual_operation(fill_recurrent_transfer_operation(from_account.get_name(), to_account.get_name(), current_recurrent_transfer.amount, to_string(current_recurrent_transfer.memo), remaining_executions));
    }
    else
    {
      uint8_t consecutive_failures = current_recurrent_transfer.consecutive_failures + 1;

      if (consecutive_failures < HIVE_MAX_CONSECUTIVE_RECURRENT_TRANSFER_FAILURES)
      {
        // No need to update the object if we know that we will remove it
        if (remaining_executions == 0)
        {
          remove_recurrent_transfer = true;
        }
        else
        {
          modify(current_recurrent_transfer, [&](recurrent_transfer_object &rt)
          {
            ++rt.consecutive_failures;
            rt.update_next_trigger_date();
            rt.remaining_executions = remaining_executions;
          });
        }
        // false means the recurrent transfer was not deleted
        push_virtual_operation(failed_recurrent_transfer_operation(from_account.get_name(), to_account.get_name(), current_recurrent_transfer.amount, consecutive_failures, to_string(current_recurrent_transfer.memo), remaining_executions, false));
      }
      else
      {
        // if we had too many consecutive failures, remove the recurrent payment object
        remove_recurrent_transfer = true;
        // true means the recurrent transfer was deleted
        push_virtual_operation(failed_recurrent_transfer_operation(from_account.get_name(), to_account.get_name(), current_recurrent_transfer.amount, consecutive_failures, to_string(current_recurrent_transfer.memo), remaining_executions, true));
      }
    }

    if (remove_recurrent_transfer)
    {
      remove( current_recurrent_transfer );
      modify(from_account, [&](account_object& a )
      {
        FC_ASSERT( a.open_recurrent_transfers > 0 );
        a.open_recurrent_transfers--;
      });
    }

    processed_transfers++;
  }
  if( _benchmark_dumper.is_enabled() && processed_transfers )
    _benchmark_dumper.end( "processing", "hive::protocol::recurrent_transfer_operation", processed_transfers );
}

void database::remove_proposal_votes_for_accounts_without_voting_rights()
{
  std::vector<account_name_type> voters;

  const auto& proposal_votes_idx = get_index< proposal_vote_index, by_voter_proposal >();

  auto itr = proposal_votes_idx.begin();
  while( itr != proposal_votes_idx.end() )
  {
    voters.push_back( itr->voter );
    ++itr;
  }

  //Lack of voters.
  if( voters.empty() )
    return;

  std::vector<account_name_type> accounts;

  for( auto& voter : voters )
  {
    const auto& account = get_account( voter );
    if( !account.can_vote )
      accounts.push_back( account.get_name() );
  }

  //Lack of voters who declined voting rights.
  if( accounts.empty() )
    return;

  /*
    For every account set a request to remove proposal votes.
    Current time is set, because we want to start removing proposal votes as soon as possible.
  */
  const auto& request_idx = get_index< decline_voting_rights_request_index, by_account >();

  for( auto& account : accounts )
  {
    auto found = request_idx.find( account );
    if( found !=request_idx.end() )
    {
      /*
        Before HF28 it was possible to create `decline_voting_rights` operation again, even if an account had `can_vote` set to false.
        In this case `effective_date` must be changed otherwise a new object is created.
      */
      modify( *found, [&]( decline_voting_rights_request_object& req )
      {
        req.effective_date = head_block_time();
      });
    }
    else
    {
      create< decline_voting_rights_request_object >( [&]( decline_voting_rights_request_object& req )
      {
        req.account = account;
        req.effective_date = head_block_time();
      });
    }
  }
}

/**
  * This method updates total_reward_shares2 on DGPO, and children_rshares2 on comments, when a comment's rshares2 changes
  * from old_rshares2 to new_rshares2.  Maintaining invariants that children_rshares2 is the sum of all descendants' rshares2,
  * and dgpo.total_reward_shares2 is the total number of rshares2 outstanding.
  */
void database::adjust_rshares2( fc::uint128_t old_rshares2, fc::uint128_t new_rshares2 )
{
  const auto& dgpo = get_dynamic_global_properties();
  modify( dgpo, [&]( dynamic_global_property_object& p )
  {
    p.total_reward_shares2 -= old_rshares2;
    p.total_reward_shares2 += new_rshares2;
  } );
}

void database::update_owner_authority( const account_object& account, const authority& owner_authority )
{
  if( head_block_num() >= HIVE_OWNER_AUTH_HISTORY_TRACKING_START_BLOCK_NUM )
  {
    create< owner_authority_history_object >( account, get< account_authority_object, by_account >( account.get_name() ).owner, head_block_time() );
  }

  modify( get< account_authority_object, by_account >( account.get_name() ), [&]( account_authority_object& auth )
  {
    auth.owner = owner_authority;
    auth.previous_owner_update = auth.last_owner_update;
    auth.last_owner_update = head_block_time();
  });
}

void database::process_vesting_withdrawals()
{
  const auto& widx = get_index< account_index, by_next_vesting_withdrawal >();
  const auto& didx = get_index< withdraw_vesting_route_index, by_withdraw_route >();
  auto current = widx.begin();

  const auto& cprops = get_dynamic_global_properties();

  int count = 0;
  if( _benchmark_dumper.is_enabled() )
    _benchmark_dumper.begin();
  while( current != widx.end() && current->next_vesting_withdrawal <= head_block_time() )
  {
    const auto& from_account = *current; ++current; ++count;

    share_type to_withdraw = from_account.get_next_vesting_withdrawal();
    if( !has_hardfork( HIVE_HARDFORK_1_28_FIX_POWER_DOWN ) && to_withdraw < from_account.vesting_withdraw_rate.amount )
      to_withdraw = from_account.to_withdraw.amount % from_account.vesting_withdraw_rate.amount;
    // see history of first (and so far the only) power down of 'gil' account: https://hiveblocks.com/@gil
    // the situation was caused by HF1, where vesting_withdraw_rate changed from 9615 before split to 9615.384615
    // (instead of correct 9615.000000); that is the source of nonequivalence between taking all the rest of power down
    // (0.769270 VESTS) and modulo of "all % weekly rate" (0.000040 VESTS);
    // it is possible that other accounts were also affected in similar way, 'gil' was just the first where the difference
    // occurred
    if( to_withdraw > from_account.get_vesting().amount )
    {
      elog( "NOTIFYALERT! somehow account was scheduled to power down more than it has on balance (${s} vs ${h})",
        ( "s", to_withdraw )( "h", from_account.get_vesting().amount ) );
      to_withdraw = from_account.get_vesting().amount;
    }

    optional< delayed_voting > dv;
    delayed_voting::opt_votes_update_data_items _votes_update_data_items;

    if( has_hardfork( HIVE_HARDFORK_1_24 ) )
    {
      dv = delayed_voting( *this );
      _votes_update_data_items = delayed_voting::votes_update_data_items();
    }

    share_type vests_deposited = 0;

    auto process_routing = [ &, this ]( bool auto_vest_mode )
    {
      for( auto itr = didx.upper_bound( boost::make_tuple( from_account.get_name(), account_name_type() ) );
        itr != didx.end() && itr->from_account == from_account.get_name();
        ++itr )
      {
        if( !( auto_vest_mode ^ itr->auto_vest ) )
        {
          share_type to_deposit = fc::uint128_to_uint64( ( fc::uint128_t ( to_withdraw.value ) * itr->percent ) / HIVE_100_PERCENT );
          vests_deposited += to_deposit;

          if( to_deposit > 0 )
          {
            const auto& to_account = get< account_object, by_name >( itr->to_account );

            asset vests = asset( to_deposit, VESTS_SYMBOL );
            asset routed = auto_vest_mode ? vests : ( vests * cprops.get_vesting_share_price() );
            operation vop = fill_vesting_withdraw_operation( from_account.get_name(), to_account.get_name(), vests, routed );

            pre_push_virtual_operation( vop );

            modify( to_account, [&]( account_object& a )
            {
              if( auto_vest_mode )
                a.vesting_shares += routed;
              else
                a.balance += routed;
            });

            if( auto_vest_mode )
            {
              if( has_hardfork( HIVE_HARDFORK_1_24 ) )
              {
                FC_ASSERT( dv.valid(), "The object processing `delayed votes` must exist" );

                dv->add_votes( _votes_update_data_items,
                          to_account.get_id() == from_account.get_id()/*withdraw_executor*/,
                          routed.amount.value/*val*/,
                          to_account/*account*/
                        );
              }
              else
              {
                adjust_proxied_witness_votes( to_account, to_deposit );
              }
            }
            else
            {
              modify( cprops, [&]( dynamic_global_property_object& o )
              {
                o.total_vesting_fund_hive     -= routed;
                o.total_vesting_shares.amount -= to_deposit;
              });
            }

            post_push_virtual_operation( vop );
          }
        }
      }
    };

    // Do two passes, the first for VESTS, the second for HIVE. Try to maintain as much accuracy for VESTS as possible.
    process_routing( true/*auto_vest_mode*/ );
    process_routing( false/*auto_vest_mode*/ );

    share_type to_convert = to_withdraw - vests_deposited;
    FC_ASSERT( to_convert >= 0, "Deposited more vests than were supposed to be withdrawn" );

    auto converted_hive = asset( to_convert, VESTS_SYMBOL ) * cprops.get_vesting_share_price();
    operation vop = fill_vesting_withdraw_operation( from_account.get_name(), from_account.get_name(), asset( to_convert, VESTS_SYMBOL ), converted_hive );
    //note: it has to be generated even if to_convert is zero because we've accumulated change on from_account
    //and only now we are going to update that account's VESTS (see issue #337)
    pre_push_virtual_operation( vop );

    if( has_hardfork( HIVE_HARDFORK_1_24 ) )
    {
      FC_ASSERT( dv.valid(), "The object processing `delayed votes` must exist" );

      dv->add_votes( _votes_update_data_items,
                true/*withdraw_executor*/,
                -to_withdraw.value/*val*/,
                from_account/*account*/
              );
    }

    modify( from_account, [&]( account_object& a )
    {
      a.vesting_shares.amount -= to_withdraw;
      a.balance += converted_hive;
      a.withdrawn.amount += to_withdraw;

      if( a.get_total_vesting_withdrawal() <= 0 || a.get_vesting().amount == 0 )
      {
        a.vesting_withdraw_rate.amount = 0;
        a.next_vesting_withdrawal = fc::time_point_sec::maximum();
        //to_withdraw/withdrawn should also be reset here
      }
      else
      {
        a.next_vesting_withdrawal += fc::seconds( HIVE_VESTING_WITHDRAW_INTERVAL_SECONDS );
      }
    });

    modify( cprops, [&]( dynamic_global_property_object& o )
    {
      o.total_vesting_fund_hive -= converted_hive;
      o.total_vesting_shares.amount -= to_convert;
    });

    if( has_hardfork( HIVE_HARDFORK_1_24 ) )
    {
      FC_ASSERT( dv.valid(), "The object processing `delayed votes` must exist" );

      fc::optional< ushare_type > leftover = dv->update_votes( _votes_update_data_items, head_block_time() );
      FC_ASSERT( leftover.valid(), "Something went wrong" );
      if( leftover.valid() && ( *leftover ) > 0 )
        adjust_proxied_witness_votes( from_account, -( ( *leftover ).value ) );
    }
    else
    {
      if( to_withdraw > 0 )
        adjust_proxied_witness_votes( from_account, -to_withdraw );
    }

    post_push_virtual_operation( vop );
  }
  if( _benchmark_dumper.is_enabled() && count )
    _benchmark_dumper.end( "processing", "hive::protocol::withdraw_vesting_operation", count );
}

/**
  *  This method will iterate through all comment_vote_objects and give them
  *  (max_rewards * weight) / c.total_vote_weight.
  *
  *  @returns unclaimed rewards.
  */
share_type database::pay_curators( const comment_object& comment, const comment_cashout_object& comment_cashout, share_type& max_rewards )
{
  struct cmp
  {
    bool operator()( const comment_vote_object* obj, const comment_vote_object* obj2 ) const
    {
      if( obj->get_weight() == obj2->get_weight() )
        return obj->get_voter() < obj2->get_voter();
      else
        return obj->get_weight() > obj2->get_weight();
    }
  };

  try
  {
    share_type unclaimed_rewards = max_rewards;

    if( !comment_cashout.allows_curation_rewards() )
    {
      unclaimed_rewards = 0;
      max_rewards = 0;
    }
    else if( comment_cashout.get_total_vote_weight() > 0 )
    {
      uint128_t total_weight( comment_cashout.get_total_vote_weight() );

      const auto& cvidx = get_index<comment_vote_index, by_comment_voter>();
      auto itr = cvidx.lower_bound( comment.get_id() );

      std::set< const comment_vote_object*, cmp > proxy_set;
      while( itr != cvidx.end() && itr->get_comment() == comment.get_id() )
      {
        proxy_set.insert( &( *itr ) );
        ++itr;
      }

      const auto& comment_author_name = get_account( comment_cashout.get_author_id() ).get_name();
      for( auto& item : proxy_set )
      { try {
        uint128_t weight( item->get_weight() );
        auto claim = fc::uint128_to_uint64( ( max_rewards.value * weight ) / total_weight );
        if( claim > 0 ) // min_amt is non-zero satoshis
        {
          unclaimed_rewards -= claim;
          const auto& voter = get( item->get_voter() );
          operation vop = curation_reward_operation( voter.get_name(), asset(0, VESTS_SYMBOL), comment_author_name, to_string( comment_cashout.get_permlink() ), has_hardfork( HIVE_HARDFORK_0_17__659 ) );
          create_vesting2( *this, voter, asset( claim, HIVE_SYMBOL ), has_hardfork( HIVE_HARDFORK_0_17__659 ),
            [&]( const asset& reward )
            {
              vop.get< curation_reward_operation >().reward = reward;
              pre_push_virtual_operation( vop );
            } );

            modify( voter, [&]( account_object& a )
            {
              a.curation_rewards.amount += claim;
            });
          post_push_virtual_operation( vop );
        }
      } FC_CAPTURE_AND_RETHROW( (*item) ) }
    }
    max_rewards -= unclaimed_rewards;

    return unclaimed_rewards;
  } FC_CAPTURE_AND_RETHROW( (max_rewards) )
}

share_type database::cashout_comment_helper( util::comment_reward_context& ctx, const comment_object& comment,
  const comment_cashout_object& comment_cashout, const comment_cashout_ex_object* comment_cashout_ex, bool forward_curation_remainder )
{
  try
  {
    share_type claimed_reward = 0;

    if( comment_cashout.get_net_rshares() > 0 )
    {
      ctx.rshares = comment_cashout.get_net_rshares();
      ctx.max_hbd = comment_cashout.get_max_accepted_payout();
      if( comment_cashout_ex )
        ctx.reward_weight = comment_cashout_ex->get_reward_weight();
      else
        ctx.reward_weight = HIVE_100_PERCENT;

      if( has_hardfork( HIVE_HARDFORK_0_17__774 ) )
      {
        const auto& rf = get_reward_fund();
        ctx.reward_curve = rf.author_reward_curve;
        ctx.content_constant = rf.content_constant;
      }

      const share_type reward = util::get_rshare_reward( ctx );
      uint128_t reward_tokens = uint128_t( reward.value );
      share_type curation_tokens;
      share_type author_tokens;

      if( reward_tokens > 0 )
      {
        curation_tokens = fc::uint128_to_uint64( ( reward_tokens * get_curation_rewards_percent() ) / HIVE_100_PERCENT );
        author_tokens = fc::uint128_to_uint64(reward_tokens) - curation_tokens;

        share_type curation_remainder = pay_curators( comment, comment_cashout, curation_tokens );

        if( forward_curation_remainder )
          author_tokens += curation_remainder;

        share_type total_beneficiary = 0;
        claimed_reward = author_tokens + curation_tokens;
        const auto& author = get_account( comment_cashout.get_author_id() );
        const auto& comment_author = author.get_name();

        for( auto& b : comment_cashout.get_beneficiaries() )
        {
          const auto& beneficiary = get_account( b.account_id );

          auto benefactor_tokens = ( author_tokens * b.weight ) / HIVE_100_PERCENT;
          auto benefactor_vesting_hive = benefactor_tokens;
          auto vop = comment_benefactor_reward_operation( beneficiary.get_name(), comment_author, to_string( comment_cashout.get_permlink() ),
            asset( 0, HBD_SYMBOL ), asset( 0, HIVE_SYMBOL ), asset( 0, VESTS_SYMBOL ), has_hardfork( HIVE_HARDFORK_0_17__659 ) );

          if( has_hardfork( HIVE_HARDFORK_0_21__3343 ) && is_treasury( beneficiary.get_name() ) )
          {
            benefactor_vesting_hive = 0;
            vop.hbd_payout = asset( benefactor_tokens, HIVE_SYMBOL ) * get_feed_history().current_median_history;
            vop.payout_must_be_claimed = false;
            adjust_balance( get_treasury(), vop.hbd_payout );
            adjust_supply( asset( -benefactor_tokens, HIVE_SYMBOL ) );
            adjust_supply( vop.hbd_payout );
          }
          else if( has_hardfork( HIVE_HARDFORK_0_20__2022 ) )
          {
            auto benefactor_hbd_hive = ( benefactor_tokens * comment_cashout.get_percent_hbd() ) / ( 2 * HIVE_100_PERCENT ) ;
            benefactor_vesting_hive  = benefactor_tokens - benefactor_hbd_hive;
            auto hbd_payout          = create_hbd( beneficiary, asset( benefactor_hbd_hive, HIVE_SYMBOL ), true );

            vop.hbd_payout   = hbd_payout.first; // HBD portion
            vop.hive_payout = hbd_payout.second; // HIVE portion
          }

          create_vesting2( *this, beneficiary, asset( benefactor_vesting_hive, HIVE_SYMBOL ), has_hardfork( HIVE_HARDFORK_0_17__659 ),
          [&]( const asset& reward )
          {
            vop.vesting_payout = reward;
            pre_push_virtual_operation( vop );
          });

          post_push_virtual_operation( vop );
          total_beneficiary += benefactor_tokens;
        }

        author_tokens -= total_beneficiary;

        auto hbd_hive     = ( author_tokens * comment_cashout.get_percent_hbd() ) / ( 2 * HIVE_100_PERCENT ) ;
        auto vesting_hive = author_tokens - hbd_hive;

        auto hbd_payout = create_hbd( author, asset( hbd_hive, HIVE_SYMBOL ), has_hardfork( HIVE_HARDFORK_0_17__659 ) );

        /*
          Total payout for curators is calculated due to the performance in third party softwares(f.e. `hivemind`).
          During payments calculations it's enough to catch `author_reward_operation`, without adding all values from `curation_reward_operation`.
        */
        auto curators_vesting_payout = calculate_vesting( *this, asset( curation_tokens, HIVE_SYMBOL ), has_hardfork( HIVE_HARDFORK_0_17__659 ) );

        operation vop = author_reward_operation( comment_author, to_string( comment_cashout.get_permlink() ), hbd_payout.first, hbd_payout.second, asset( 0, VESTS_SYMBOL ),
                                                curators_vesting_payout, has_hardfork( HIVE_HARDFORK_0_17__659 ) );

        create_vesting2( *this, author, asset( vesting_hive, HIVE_SYMBOL ), has_hardfork( HIVE_HARDFORK_0_17__659 ),
          [&]( const asset& vesting_payout )
          {
            vop.get< author_reward_operation >().vesting_payout = vesting_payout;
            pre_push_virtual_operation( vop );
          } );
        post_push_virtual_operation( vop );

        asset payout = hbd_payout.first + to_hbd( hbd_payout.second + asset( vesting_hive, HIVE_SYMBOL ) );
        asset curator_payout = to_hbd( asset( curation_tokens, HIVE_SYMBOL ) );
        asset beneficiary_payout = to_hbd( asset( total_beneficiary, HIVE_SYMBOL ) );
        if( !has_hardfork( HIVE_HARDFORK_0_19 ) )
        {
          modify( *comment_cashout_ex, [&]( comment_cashout_ex_object& c_ex )
          {
            c_ex.add_payout_values( payout, curator_payout, beneficiary_payout );
            payout = c_ex.get_total_payout();
            curator_payout = c_ex.get_curator_payout();
            beneficiary_payout = c_ex.get_beneficiary_payout();
          } );
        }
        vop = comment_reward_operation( comment_author, to_string( comment_cashout.get_permlink() ),
          to_hbd( asset( claimed_reward, HIVE_SYMBOL ) ), author_tokens, payout, curator_payout, beneficiary_payout );
        pre_push_virtual_operation( vop );
        post_push_virtual_operation( vop );

        modify( author, [&]( account_object& a )
        {
          a.posting_rewards.amount += author_tokens;
        });
      }

      notify_comment_reward( { reward, author_tokens, curation_tokens } );

      if( !has_hardfork( HIVE_HARDFORK_0_17__774 ) )
        adjust_rshares2( util::evaluate_reward_curve( comment_cashout.get_net_rshares() ), 0 );
    }

    if( !has_hardfork( HIVE_HARDFORK_0_19 ) ) // paid comment is removed after HF19, so no point in modification
    {
      modify( comment_cashout, [&]( comment_cashout_object& c )
      {
        /**
        * A payout is only made for positive rshares, negative rshares hang around
        * for the next time this post might get an upvote.
        */
        int64_t negative_rshares = c.get_net_rshares() < 0 ? c.get_net_rshares() : 0;
        c.on_payout();
        c.accumulate_vote_rshares( negative_rshares, 0 );

        if( has_hardfork( HIVE_HARDFORK_0_17__769 ) )
        {
          c.set_cashout_time();
        }
        else if( comment.is_root() )
        {
          if( has_hardfork( HIVE_HARDFORK_0_12__177 ) && !comment_cashout_ex->was_paid() )
            c.set_cashout_time( head_block_time() + HIVE_SECOND_CASHOUT_WINDOW );
          else
            c.set_cashout_time();
        }
      } );
      modify( *comment_cashout_ex, [&]( comment_cashout_ex_object& c_ex )
      {
        c_ex.on_payout( head_block_time() );
      } );
    }

    if( has_hardfork( HIVE_HARDFORK_0_17__769 ) || calculate_discussion_payout_time( comment, comment_cashout ) == fc::time_point_sec::maximum() )
    {
      push_virtual_operation( comment_payout_update_operation( get_account( comment_cashout.get_author_id() ).get_name(), to_string( comment_cashout.get_permlink() ) ) );
    }

    const auto& vote_idx = get_index< comment_vote_index, by_comment_voter >();
    auto vote_itr = vote_idx.lower_bound( comment.get_id() );
    while( vote_itr != vote_idx.end() && vote_itr->get_comment() == comment.get_id() )
    {
      const auto& cur_vote = *vote_itr;
      ++vote_itr;
      remove( cur_vote );
    }

    return claimed_reward;
  } FC_CAPTURE_AND_RETHROW( (comment)(comment_cashout)(ctx) )
}

void database::process_comment_cashout()
{
  /// don't allow any content to get paid out until the website is ready to launch
  /// and people have had a week to start posting.  The first cashout will be the biggest because it
  /// will represent 2+ months of rewards.
  if( !has_hardfork( HIVE_FIRST_CASHOUT_TIME ) )
    return;

  const auto& gpo = get_dynamic_global_properties();
  auto _now = head_block_time();
  util::comment_reward_context ctx;
  ctx.current_hive_price = get_feed_history().current_median_history;

  vector< reward_fund_context > funds;
  vector< share_type > hive_awarded;
  const auto& reward_idx = get_index< reward_fund_index, by_id >();

  // Decay recent rshares of each fund
  for( auto itr = reward_idx.begin(); itr != reward_idx.end(); ++itr )
  {
    // Add all reward funds to the local cache and decay their recent rshares
    modify( *itr, [&]( reward_fund_object& rfo )
    {
      fc::microseconds decay_time;

      if( has_hardfork( HIVE_HARDFORK_0_19__1051 ) )
        decay_time = HIVE_RECENT_RSHARES_DECAY_TIME_HF19;
      else
        decay_time = HIVE_RECENT_RSHARES_DECAY_TIME_HF17;

      if( ( _now - rfo.last_update ) <= decay_time || !has_hardfork( HIVE_HARDFORK_1_26_CLAIM_UNDERFLOW ) ) //TODO: remove hardfork part after HF26
        rfo.recent_claims -= ( rfo.recent_claims * ( _now - rfo.last_update ).to_seconds() ) / decay_time.to_seconds();
      else
        rfo.recent_claims = 0; // this should never happen - requires chain to be inactive for more than decay_time
      rfo.last_update = _now;
    });

    reward_fund_context rf_ctx;
    rf_ctx.recent_claims = itr->recent_claims;
    rf_ctx.reward_balance = itr->reward_balance;

    // The index is by ID, so the ID should be the current size of the vector (0, 1, 2, etc...)
    assert( funds.size() == itr->get_id().get_value() );

    funds.push_back( rf_ctx );
  }

  const auto& cidx        = get_index< comment_cashout_index, by_cashout_time >();
  const auto& com_by_root = get_index< comment_cashout_ex_index, by_root >();

  auto _current = cidx.begin();
  // add all rshares about to be cashed out to the reward funds. This ensures equal satoshi per rshare payment
  if( has_hardfork( HIVE_HARDFORK_0_17__771 ) )
  {
    while( _current != cidx.end() && _current->get_cashout_time() <= _now )
    {
      if( _current->get_net_rshares() > 0 )
      {
        const auto& rf = get_reward_fund();
        funds[ rf.get_id() ].recent_claims += util::evaluate_reward_curve( _current->get_net_rshares(), rf.author_reward_curve, rf.content_constant );
      }

      ++_current;
    }

    _current = cidx.begin();
  }

  bool forward_curation_remainder = !has_hardfork( HIVE_HARDFORK_0_20__1877 );
  /*
    * Payout all comments
    *
    * Each payout follows a similar pattern, but for a different reason.
    * Cashout comment helper does not know about the reward fund it is paying from.
    * The helper only does token allocation based on curation rewards and the HBD
    * global %, etc.
    *
    * Each context is used by get_rshare_reward to determine what part of each budget
    * the comment is entitled to. Prior to hardfork 17, all payouts are done against
    * the global state updated each payout. After the hardfork, each payout is done
    * against a reward fund state that is snapshotted before all payouts in the block.
    */
  int count = 0;
  if( _benchmark_dumper.is_enabled() )
    _benchmark_dumper.begin();
  while( _current != cidx.end() && _current->get_cashout_time() <= _now )
  {
    if( has_hardfork( HIVE_HARDFORK_0_17__771 ) )
    {
      auto fund_id = get_reward_fund().get_id();
      ctx.total_reward_shares2 = funds[ fund_id ].recent_claims;
      ctx.total_reward_fund_hive = funds[ fund_id ].reward_balance;

      const comment_object& _comment = get_comment( *_current );
      funds[ fund_id ].hive_awarded += cashout_comment_helper( ctx, _comment, *_current,
        find_comment_cashout_ex( _comment ), forward_curation_remainder );
      ++count;
    }
    else
    {
      comment_id_type root_id = find_comment_cashout_ex( _current->get_comment_id() )->get_root_id();
      auto itr = com_by_root.lower_bound( root_id );
      while( itr != com_by_root.end() && itr->get_root_id() == root_id )
      {
        const auto& comment_cashout_ex = *itr; ++itr;
        ctx.total_reward_shares2 = gpo.total_reward_shares2;
        ctx.total_reward_fund_hive = gpo.get_total_reward_fund_hive();

        const comment_object& comment = get_comment( comment_cashout_ex.get_comment_id() );
        const comment_cashout_object* comment_cashout = find_comment_cashout( comment_cashout_ex.get_comment_id() );
        FC_ASSERT( comment_cashout );
        auto reward = cashout_comment_helper( ctx, comment, *comment_cashout, &comment_cashout_ex );
        ++count;

        if( reward > 0 )
        {
          modify( get_dynamic_global_properties(), [&]( dynamic_global_property_object& p )
          {
            p.total_reward_fund_hive.amount -= reward;
          });
        }
      }
    }

    if( has_hardfork( HIVE_HARDFORK_0_19 ) )
      remove( *_current );
    _current = cidx.begin();
  }
  if( _benchmark_dumper.is_enabled() && count )
    _benchmark_dumper.end( "processing", "hive::protocol::comment_operation", count );

  // Write the cached fund state back to the database
  if( funds.size() )
  {
    for( size_t i = 0; i < funds.size(); i++ )
    {
      modify( get< reward_fund_object, by_id >( reward_fund_object::id_type( i ) ), [&]( reward_fund_object& rfo )
      {
        rfo.recent_claims = funds[ i ].recent_claims;
        rfo.reward_balance -= asset( funds[ i ].hive_awarded, HIVE_SYMBOL );
      });
    }
  }
}

/**
  *  Overall the network has an inflation rate of 102% of virtual hive per year
  *  90% of inflation is directed to vesting shares
  *  10% of inflation is directed to subjective proof of work voting
  *  1% of inflation is directed to liquidity providers
  *  1% of inflation is directed to block producers
  *
  *  This method pays out vesting and reward shares every block, and liquidity shares once per day.
  *  This method does not pay out witnesses.
  */
void database::process_funds()
{
  const auto& props = get_dynamic_global_properties();
  const auto& wso = get_witness_schedule_object();
  const auto& feed = get_feed_history();

  if( has_hardfork( HIVE_HARDFORK_0_16__551) )
  {
    /**
      * At block 7,000,000 have a 9.5% instantaneous inflation rate, decreasing to 0.95% at a rate of 0.01%
      * every 250k blocks. This narrowing will take approximately 20.5 years and will complete on block 220,750,000
      */
    int64_t start_inflation_rate = int64_t( HIVE_INFLATION_RATE_START_PERCENT );
    int64_t inflation_rate_adjustment = int64_t( head_block_num() / HIVE_INFLATION_NARROWING_PERIOD );
    int64_t inflation_rate_floor = int64_t( HIVE_INFLATION_RATE_STOP_PERCENT );

    // below subtraction cannot underflow int64_t because inflation_rate_adjustment is <2^32
    int64_t current_inflation_rate = std::max( start_inflation_rate - inflation_rate_adjustment, inflation_rate_floor );

    auto new_hive = ( props.virtual_supply.amount * current_inflation_rate ) / ( int64_t( HIVE_100_PERCENT ) * int64_t( HIVE_BLOCKS_PER_YEAR ) );
    auto content_reward = ( new_hive * props.content_reward_percent ) / HIVE_100_PERCENT;
    if( has_hardfork( HIVE_HARDFORK_0_17__774 ) )
      content_reward = pay_reward_funds( content_reward );
    auto vesting_reward = ( new_hive * props.vesting_reward_percent ) / HIVE_100_PERCENT;
    auto dhf_new_funds = ( new_hive * props.proposal_fund_percent ) / HIVE_100_PERCENT;
    auto witness_reward = new_hive - content_reward - vesting_reward - dhf_new_funds;

    const auto& cwit = get_witness( props.current_witness );
    witness_reward *= HIVE_MAX_WITNESSES;

    if( cwit.schedule == witness_object::timeshare )
      witness_reward *= wso.timeshare_weight;
    else if( cwit.schedule == witness_object::miner )
      witness_reward *= wso.miner_weight;
    else if( cwit.schedule == witness_object::elected )
      witness_reward *= wso.elected_weight;
    else
    {
      push_virtual_operation( system_warning_operation( FC_LOG_MESSAGE( warn,
        "Encountered unknown witness type for witness: ${w}", ( "w", cwit.owner ) ).get_message() ) );
    }

    witness_reward /= wso.witness_pay_normalization_factor;

    auto new_hbd = asset( 0, HBD_SYMBOL );

    if( dhf_new_funds.value )
    {
      new_hbd = asset( dhf_new_funds, HIVE_SYMBOL ) * feed.current_median_history;
      adjust_balance( get_treasury_name(), new_hbd );
    }

    new_hive = content_reward + vesting_reward + witness_reward;

    modify( props, [&]( dynamic_global_property_object& p )
    {
      p.total_vesting_fund_hive += asset( vesting_reward, HIVE_SYMBOL );
      if( !has_hardfork( HIVE_HARDFORK_0_17__774 ) )
        p.total_reward_fund_hive += asset( content_reward, HIVE_SYMBOL );
      p.current_supply      += asset( new_hive, HIVE_SYMBOL );
      p.current_hbd_supply  += new_hbd;
      p.virtual_supply      += asset( new_hive + dhf_new_funds, HIVE_SYMBOL );
      p.dhf_interval_ledger += new_hbd;
    });

    operation vop = producer_reward_operation( cwit.owner, asset( 0, VESTS_SYMBOL ) );
    create_vesting2( *this, get_account( cwit.owner ), asset( witness_reward, HIVE_SYMBOL ), false,
      [&]( const asset& vesting_shares )
      {
        vop.get< producer_reward_operation >().vesting_shares = vesting_shares;
        pre_push_virtual_operation( vop );
      } );
    post_push_virtual_operation( vop );
  }
  else
  {
    auto content_reward = get_content_reward();
    auto curate_reward = get_curation_reward();
    auto witness_pay = get_producer_reward();
    auto vesting_reward = content_reward + curate_reward + witness_pay;

    content_reward = content_reward + curate_reward;

    if( props.head_block_number < HIVE_START_VESTING_BLOCK )
      vesting_reward.amount = 0;
    else
      vesting_reward.amount.value *= 9;

    modify( props, [&]( dynamic_global_property_object& p )
    {
        p.total_vesting_fund_hive += vesting_reward;
        p.total_reward_fund_hive  += content_reward;
        p.current_supply += content_reward + witness_pay + vesting_reward;
        p.virtual_supply += content_reward + witness_pay + vesting_reward;
    } );
  }
}

void database::process_savings_withdraws()
{
  const auto& idx = get_index< savings_withdraw_index >().indices().get< by_complete_from_rid >();
  auto itr = idx.begin();

  int count = 0;
  if( _benchmark_dumper.is_enabled() )
    _benchmark_dumper.begin();
  while( itr != idx.end() )
  {
    if( itr->complete > head_block_time() )
      break;
    adjust_balance( get_account( itr->to ), itr->amount );

    modify( get_account( itr->from ), [&]( account_object& a )
    {
      a.savings_withdraw_requests--;
    });

    push_virtual_operation( fill_transfer_from_savings_operation( itr->from, itr->to, itr->amount, itr->request_id, to_string( itr->memo) ) );

    remove( *itr );
    itr = idx.begin();
    ++count;
  }
  if( _benchmark_dumper.is_enabled() && count )
    _benchmark_dumper.end( "processing", "hive::protocol::transfer_from_savings_operation", count );
}

void database::process_subsidized_accounts()
{
  const witness_schedule_object& wso = get_witness_schedule_object();
  const dynamic_global_property_object& gpo = get_dynamic_global_properties();

  // Update global pool.
  modify( gpo, [&]( dynamic_global_property_object& g )
  {
    g.available_account_subsidies = rd_apply( wso.account_subsidy_rd, g.available_account_subsidies );
  } );

  // Update per-witness pool for current witness.
  const witness_object& current_witness = get_witness( gpo.current_witness );
  if( current_witness.schedule == witness_object::elected )
  {
    modify( current_witness, [&]( witness_object& w )
    {
      w.available_witness_account_subsidies = rd_apply( wso.account_subsidy_witness_rd, w.available_witness_account_subsidies );
    } );
  }
}

asset database::get_liquidity_reward()const
{
  if( has_hardfork( HIVE_HARDFORK_0_12__178 ) )
    return asset( 0, HIVE_SYMBOL );

  const auto& props = get_dynamic_global_properties();
  static_assert( HIVE_LIQUIDITY_REWARD_PERIOD_SEC == 60*60, "this code assumes a 1 hour time interval" ); // NOLINT(misc-redundant-expression)
  asset percent( protocol::calc_percent_reward_per_hour< HIVE_LIQUIDITY_APR_PERCENT >( props.virtual_supply.amount ), HIVE_SYMBOL );
  return std::max( percent, HIVE_MIN_LIQUIDITY_REWARD );
}

asset database::get_content_reward()const
{
  const auto& props = get_dynamic_global_properties();
  static_assert( HIVE_BLOCK_INTERVAL == 3, "this code assumes a 3-second time interval" );
  asset percent( protocol::calc_percent_reward_per_block< HIVE_CONTENT_APR_PERCENT >( props.virtual_supply.amount ), HIVE_SYMBOL );
  return std::max( percent, HIVE_MIN_CONTENT_REWARD );
}

asset database::get_curation_reward()const
{
  const auto& props = get_dynamic_global_properties();
  static_assert( HIVE_BLOCK_INTERVAL == 3, "this code assumes a 3-second time interval" );
  asset percent( protocol::calc_percent_reward_per_block< HIVE_CURATE_APR_PERCENT >( props.virtual_supply.amount ), HIVE_SYMBOL);
  return std::max( percent, HIVE_MIN_CURATE_REWARD );
}

asset database::get_producer_reward()
{
  const auto& props = get_dynamic_global_properties();
  static_assert( HIVE_BLOCK_INTERVAL == 3, "this code assumes a 3-second time interval" );
  asset percent( protocol::calc_percent_reward_per_block< HIVE_PRODUCER_APR_PERCENT >( props.virtual_supply.amount ), HIVE_SYMBOL);
  auto pay = std::max( percent, HIVE_MIN_PRODUCER_REWARD );
  const auto& witness_account = get_account( props.current_witness );

  /// pay witness in vesting shares
  if( props.head_block_number >= HIVE_START_MINER_VOTING_BLOCK || (witness_account.get_vesting().amount.value == 0) )
  {
    // const auto& witness_obj = get_witness( props.current_witness );
    operation vop = producer_reward_operation( witness_account.get_name(), asset( 0, VESTS_SYMBOL ) );
    create_vesting2( *this, witness_account, pay, false,
      [&]( const asset& vesting_shares )
      {
        vop.get< producer_reward_operation >().vesting_shares = vesting_shares;
        pre_push_virtual_operation( vop );
      } );
    post_push_virtual_operation( vop );
  }
  else
  {
    modify( get_account( witness_account.get_name() ), [&]( account_object& a )
    {
      a.balance += pay;
    } );
    push_virtual_operation( producer_reward_operation( witness_account.get_name(), pay ) );
  }

  return pay;
}

asset database::get_pow_reward()const
{
  const auto& props = get_dynamic_global_properties();

#ifndef IS_TEST_NET
  /// 0 block rewards until at least HIVE_MAX_WITNESSES have produced a POW
  if( props.num_pow_witnesses < HIVE_MAX_WITNESSES && props.head_block_number < HIVE_START_VESTING_BLOCK )
    return asset( 0, HIVE_SYMBOL );
#endif

  static_assert( HIVE_BLOCK_INTERVAL == 3, "this code assumes a 3-second time interval" );
  static_assert( HIVE_MAX_WITNESSES == 21, "this code assumes 21 per round" );
  asset percent( calc_percent_reward_per_round< HIVE_POW_APR_PERCENT >( props.virtual_supply.amount ), HIVE_SYMBOL);
  return std::max( percent, HIVE_MIN_POW_REWARD );
}


void database::pay_liquidity_reward()
{
#ifdef IS_TEST_NET
  if( !liquidity_rewards_enabled )
    return;
#endif

  if( (head_block_num() % HIVE_LIQUIDITY_REWARD_BLOCKS) == 0 )
  {
    auto reward = get_liquidity_reward();

    if( reward.amount == 0 )
      return;

    const auto& ridx = get_index< liquidity_reward_balance_index >().indices().get< by_volume_weight >();
    auto itr = ridx.begin();
    if( itr != ridx.end() && itr->volume_weight() > 0 )
    {
      adjust_supply( reward, true );
      adjust_balance( get(itr->owner), reward );
      modify( *itr, [&]( liquidity_reward_balance_object& obj )
      {
        obj.hive_volume = 0;
        obj.hbd_volume  = 0;
        obj.last_update = head_block_time();
        obj.weight      = 0;
      } );

      push_virtual_operation( liquidity_reward_operation( get(itr->owner).get_name(), reward ) );
    }
  }
}

uint16_t database::get_curation_rewards_percent() const
{
  if( has_hardfork( HIVE_HARDFORK_0_17__774 ) )
    return get_reward_fund().percent_curation_rewards;
  else if( has_hardfork( HIVE_HARDFORK_0_8__116 ) )
    return HIVE_1_PERCENT * 25;
  else
    return HIVE_1_PERCENT * 50;
}

share_type database::pay_reward_funds( const share_type& reward )
{
  const auto& reward_idx = get_index< reward_fund_index, by_id >();
  share_type used_rewards = 0;

  for( auto itr = reward_idx.begin(); itr != reward_idx.end(); ++itr )
  {
    // reward is a per block reward and the percents are 16-bit. This should never overflow
    auto r = ( reward * itr->percent_content_rewards ) / HIVE_100_PERCENT;

    modify( *itr, [&]( reward_fund_object& rfo )
    {
      rfo.reward_balance += asset( r, HIVE_SYMBOL );
    });

    used_rewards += r;

    // Sanity check to ensure we aren't printing more HIVE than has been allocated through inflation
    FC_ASSERT( used_rewards <= reward );
  }

  return used_rewards;
}

/**
  *  Iterates over all [collateralized] conversion requests with a conversion date before
  *  the head block time and then converts them to/from HIVE/HBD at the
  *  current median price feed history price times the premium/fee
  *  Collateralized requests might also return excess collateral.
  */
void database::process_conversions()
{
  auto now = head_block_time();
  const auto& fhistory = get_feed_history();
  if( fhistory.current_median_history.is_null() )
    return;

  asset net_hbd( 0, HBD_SYMBOL );
  asset net_hive( 0, HIVE_SYMBOL );

  //regular requests
  int count = 0;
  if( _benchmark_dumper.is_enabled() )
    _benchmark_dumper.begin();
  {
    const auto& request_by_date = get_index< convert_request_index, by_conversion_date >();
    auto itr = request_by_date.begin();

    while( itr != request_by_date.end() && itr->get_conversion_date() <= now )
    {
      auto amount_to_issue = itr->get_convert_amount() * fhistory.current_median_history;
      const auto& owner = get_account( itr->get_owner() );

      adjust_balance( owner, amount_to_issue );

      net_hbd  -= itr->get_convert_amount();
      net_hive += amount_to_issue;

      push_virtual_operation( fill_convert_request_operation( owner.get_name(), itr->get_request_id(),
        itr->get_convert_amount(), amount_to_issue ) );

      remove( *itr );
      itr = request_by_date.begin();

      ++count;
    }
  }
  if( _benchmark_dumper.is_enabled() && count )
    _benchmark_dumper.end( "processing", "hive::protocol::convert_operation", count );

  //collateralized requests
  count = 0;
  if( _benchmark_dumper.is_enabled() )
    _benchmark_dumper.begin();
  {
    const auto& request_by_date = get_index< collateralized_convert_request_index, by_conversion_date >();
    auto itr = request_by_date.begin();

    while( itr != request_by_date.end() && itr->get_conversion_date() <= now )
    {
      const auto& owner = get_account( itr->get_owner() );

      //calculate how much HIVE we'd need for already created HBD at current corrected price
      //note that we are using market median price instead of minimal as it was during immediate part of this
      //conversion or current median price as is used in other conversions; the former is by design - we are supposed
      //to use median price here, minimal during immediate conversion was to prevent gaming; the latter is for rare
      //case, when this conversion happens when median price does not reflect market conditions when hard limit was
      //hit - see update_median_feed()
      auto required_hive = multiply_with_fee( itr->get_converted_amount(), fhistory.market_median_history,
        HIVE_COLLATERALIZED_CONVERSION_FEE, HIVE_SYMBOL );
      auto excess_collateral = itr->get_collateral_amount() - required_hive;
      if( excess_collateral.amount < 0 )
      {
        push_virtual_operation( system_warning_operation( FC_LOG_MESSAGE( warn,
          "Insufficient collateral on conversion ${id} by ${o} - shortfall of ${ec}",
          ( "id", itr->get_request_id() )( "o", owner.get_name() )( "ec", -excess_collateral ) ).get_message() ) );

        required_hive = itr->get_collateral_amount();
        excess_collateral.amount = 0;
      }
      else
      {
        adjust_balance( owner, excess_collateral );
      }

      net_hive -= required_hive;
      //note that HBD was created immediately, so we don't need to correct its supply here
      push_virtual_operation( fill_collateralized_convert_request_operation( owner.get_name(), itr->get_request_id(),
        required_hive, itr->get_converted_amount(), excess_collateral ) );

      remove( *itr );
      itr = request_by_date.begin();

      ++count;
    }
    if( _benchmark_dumper.is_enabled() && count )
      _benchmark_dumper.end( "processing", "hive::protocol::collateralized_convert_operation", count );
  }

  //correct global supply (if needed)
  if( net_hive.amount.value | net_hbd.amount.value )
  {
    modify( get_dynamic_global_properties(), [&]( dynamic_global_property_object& p )
    {
      p.current_supply += net_hive;
      p.current_hbd_supply += net_hbd;
      p.virtual_supply += net_hive;
      p.virtual_supply += net_hbd * fhistory.current_median_history;
    } );
  }
}

asset database::to_hbd( const asset& hive )const
{
  return util::to_hbd( get_feed_history().current_median_history, hive );
}

asset database::to_hive( const asset& hbd )const
{
  return util::to_hive( get_feed_history().current_median_history, hbd );
}

void database::account_recovery_processing()
{
  // Clear expired recovery requests
  const auto& rec_req_idx = get_index< account_recovery_request_index, by_expiration >();
  auto rec_req = rec_req_idx.begin();

  while( rec_req != rec_req_idx.end() && rec_req->get_expiration_time() <= head_block_time() )
  {
    remove( *rec_req );
    rec_req = rec_req_idx.begin();
  }

  // Clear invalid historical authorities
  const auto& hist_idx = get_index< owner_authority_history_index >().indices(); //by id
  auto hist = hist_idx.begin();

  while( hist != hist_idx.end() && time_point_sec( hist->last_valid_time + HIVE_OWNER_AUTH_RECOVERY_PERIOD ) < head_block_time() )
  {
    remove( *hist );
    hist = hist_idx.begin();
  }

  // Apply effective recovery_account changes
  const auto& change_req_idx = get_index< change_recovery_account_request_index, by_effective_date >();
  auto change_req = change_req_idx.begin();

  while( change_req != change_req_idx.end() && change_req->get_execution_time() <= head_block_time() )
  {
    const auto& account = get_account( change_req->get_account_to_recover() );
    account_name_type old_recovery_account_name;
    if( account.has_recovery_account() )
      old_recovery_account_name = get_account( account.get_recovery_account() ).get_name();
    const auto& new_recovery_account = get_account( change_req->get_recovery_account() );
    modify( account, [&]( account_object& a )
    {
      a.set_recovery_account( new_recovery_account );
    });

    push_virtual_operation(changed_recovery_account_operation( account.get_name(), old_recovery_account_name, new_recovery_account.get_name() ));

    remove( *change_req );
    change_req = change_req_idx.begin();
  }
}

void database::expire_escrow_ratification()
{
  const auto& escrow_idx = get_index< escrow_index >().indices().get< by_ratification_deadline >();
  auto escrow_itr = escrow_idx.lower_bound( false );

  while( escrow_itr != escrow_idx.end() && !escrow_itr->is_approved() && escrow_itr->ratification_deadline <= head_block_time() )
  {
    const auto& old_escrow = *escrow_itr;
    ++escrow_itr;

    adjust_balance( old_escrow.from, old_escrow.get_hive_balance() );
    adjust_balance( old_escrow.from, old_escrow.get_hbd_balance() );
    adjust_balance( old_escrow.from, old_escrow.get_fee() );

    push_virtual_operation( escrow_rejected_operation( old_escrow.from, old_escrow.to, old_escrow.agent, old_escrow.escrow_id,
      old_escrow.get_hbd_balance(), old_escrow.get_hive_balance(), old_escrow.get_fee() ) );

    modify( get_account( old_escrow.from ), []( account_object& a )
    {
      a.pending_escrow_transfers--;
    } );

    remove( old_escrow );
  }
}

void database::process_decline_voting_rights()
{
  const auto& request_idx = get_index< decline_voting_rights_request_index >().indices().get< by_effective_date >();
  auto itr = request_idx.begin();

  int count = 0;
  if( _benchmark_dumper.is_enabled() )
    _benchmark_dumper.begin();

  const auto& proposal_votes = get_index< proposal_vote_index, by_voter_proposal >();
  remove_guard obj_perf( get_remove_threshold() );

  while( itr != request_idx.end() && itr->effective_date <= head_block_time() )
  {
    const auto& account = get< account_object, by_name >( itr->account );

    if( !has_hardfork( HIVE_HARDFORK_1_28 ) || dhf_helper::remove_proposal_votes( account, proposal_votes, *this, obj_perf ) )
    {
      nullify_proxied_witness_votes( account );
      clear_witness_votes( account );

      if( account.has_proxy() )
        push_virtual_operation( proxy_cleared_operation( account.get_name(), get_account( account.get_proxy() ).get_name()) );

      push_virtual_operation( declined_voting_rights_operation( itr->account ) );

      modify( account, [&]( account_object& a )
      {
        a.can_vote = false;
        a.clear_proxy();
      });

      remove( *itr );
      itr = request_idx.begin();
      ++count;
    }
    else
    {
      ilog("Threshold exceeded while deleting proposal votes for account ${account}.",
        ("account", account.get_name())); // to be continued in next block
      break;
    }
  }
  if( _benchmark_dumper.is_enabled() && count )
    _benchmark_dumper.end( "processing", "hive::protocol::decline_voting_rights_operation", count );
}

time_point_sec database::head_block_time()const
{
  return get_dynamic_global_properties().time;
}

uint32_t database::head_block_num()const
{
  return get_dynamic_global_properties().head_block_number;
}

block_id_type database::head_block_id()const
{
  return get_dynamic_global_properties().head_block_id;
}

//safe to call without chainbase lock
time_point_sec database::head_block_time_from_fork_db(fc::microseconds wait_for_microseconds)const
{
  return _fork_db.head_block_time(wait_for_microseconds);
}

//safe to call without chainbase lock
uint32_t database::head_block_num_from_fork_db(fc::microseconds wait_for_microseconds)const
{
  return _fork_db.head_block_num(wait_for_microseconds);
}

//safe to call without chainbase lock
block_id_type database::head_block_id_from_fork_db(fc::microseconds wait_for_microseconds)const
{
  return _fork_db.head_block_id(wait_for_microseconds);
}

node_property_object& database::node_properties()
{
  return _node_property_object;
}

uint32_t database::get_last_irreversible_block_num() const
{
  //ilog("getting last_irreversible_block_num irreversible is ${l}", ("l", irreversible_object->last_irreversible_block_num));
  //ilog("getting last_irreversible_block_num head is ${l}", ("l", head_block_num()));
  return irreversible_object->last_irreversible_block_num;
}

void database::set_last_irreversible_block_num(uint32_t block_num)
{
  //dlog("setting last_irreversible_block_num previous ${l}", ("l", irreversible_object->last_irreversible_block_num));
  FC_ASSERT(block_num >= irreversible_object->last_irreversible_block_num, "Irreversible block can only move forward. Old: ${o}, new: ${n}",
    ("o", irreversible_object->last_irreversible_block_num)("n", block_num));

  irreversible_object->last_irreversible_block_num = block_num;
  //dlog("setting last_irreversible_block_num new ${l}", ("l", irreversible_object->last_irreversible_block_num));
}

void database::initialize_evaluators()
{
  _my->_evaluator_registry.register_evaluator< vote_evaluator                           >();
  _my->_evaluator_registry.register_evaluator< comment_evaluator                        >();
  _my->_evaluator_registry.register_evaluator< comment_options_evaluator                >();
  _my->_evaluator_registry.register_evaluator< delete_comment_evaluator                 >();
  _my->_evaluator_registry.register_evaluator< transfer_evaluator                       >();
  _my->_evaluator_registry.register_evaluator< transfer_to_vesting_evaluator            >();
  _my->_evaluator_registry.register_evaluator< withdraw_vesting_evaluator               >();
  _my->_evaluator_registry.register_evaluator< set_withdraw_vesting_route_evaluator     >();
  _my->_evaluator_registry.register_evaluator< account_create_evaluator                 >();
  _my->_evaluator_registry.register_evaluator< account_update_evaluator                 >();
  _my->_evaluator_registry.register_evaluator< account_update2_evaluator                >();
  _my->_evaluator_registry.register_evaluator< witness_update_evaluator                 >();
  _my->_evaluator_registry.register_evaluator< account_witness_vote_evaluator           >();
  _my->_evaluator_registry.register_evaluator< account_witness_proxy_evaluator          >();
  _my->_evaluator_registry.register_evaluator< custom_evaluator                         >();
  _my->_evaluator_registry.register_evaluator< custom_binary_evaluator                  >();
  _my->_evaluator_registry.register_evaluator< custom_json_evaluator                    >();
  _my->_evaluator_registry.register_evaluator< pow_evaluator                            >();
  _my->_evaluator_registry.register_evaluator< pow2_evaluator                           >();
  _my->_evaluator_registry.register_evaluator< feed_publish_evaluator                   >();
  _my->_evaluator_registry.register_evaluator< convert_evaluator                        >();
  _my->_evaluator_registry.register_evaluator< collateralized_convert_evaluator         >();
  _my->_evaluator_registry.register_evaluator< limit_order_create_evaluator             >();
  _my->_evaluator_registry.register_evaluator< limit_order_create2_evaluator            >();
  _my->_evaluator_registry.register_evaluator< limit_order_cancel_evaluator             >();
  _my->_evaluator_registry.register_evaluator< claim_account_evaluator                  >();
  _my->_evaluator_registry.register_evaluator< create_claimed_account_evaluator         >();
  _my->_evaluator_registry.register_evaluator< request_account_recovery_evaluator       >();
  _my->_evaluator_registry.register_evaluator< recover_account_evaluator                >();
  _my->_evaluator_registry.register_evaluator< change_recovery_account_evaluator        >();
  _my->_evaluator_registry.register_evaluator< escrow_transfer_evaluator                >();
  _my->_evaluator_registry.register_evaluator< escrow_approve_evaluator                 >();
  _my->_evaluator_registry.register_evaluator< escrow_dispute_evaluator                 >();
  _my->_evaluator_registry.register_evaluator< escrow_release_evaluator                 >();
  _my->_evaluator_registry.register_evaluator< transfer_to_savings_evaluator            >();
  _my->_evaluator_registry.register_evaluator< transfer_from_savings_evaluator          >();
  _my->_evaluator_registry.register_evaluator< cancel_transfer_from_savings_evaluator   >();
  _my->_evaluator_registry.register_evaluator< decline_voting_rights_evaluator          >();
  _my->_evaluator_registry.register_evaluator< reset_account_evaluator                  >();
  _my->_evaluator_registry.register_evaluator< set_reset_account_evaluator              >();
  _my->_evaluator_registry.register_evaluator< claim_reward_balance_evaluator           >();
#ifdef HIVE_ENABLE_SMT
  _my->_evaluator_registry.register_evaluator< claim_reward_balance2_evaluator          >();
#endif
  _my->_evaluator_registry.register_evaluator< account_create_with_delegation_evaluator >();
  _my->_evaluator_registry.register_evaluator< delegate_vesting_shares_evaluator        >();
  _my->_evaluator_registry.register_evaluator< witness_set_properties_evaluator         >();

#ifdef HIVE_ENABLE_SMT
  _my->_evaluator_registry.register_evaluator< smt_setup_evaluator                      >();
  _my->_evaluator_registry.register_evaluator< smt_setup_emissions_evaluator            >();
  _my->_evaluator_registry.register_evaluator< smt_set_setup_parameters_evaluator       >();
  _my->_evaluator_registry.register_evaluator< smt_set_runtime_parameters_evaluator     >();
  _my->_evaluator_registry.register_evaluator< smt_create_evaluator                     >();
  _my->_evaluator_registry.register_evaluator< smt_contribute_evaluator                 >();
#endif

  _my->_evaluator_registry.register_evaluator< create_proposal_evaluator                >();
  _my->_evaluator_registry.register_evaluator< update_proposal_evaluator                >();
  _my->_evaluator_registry.register_evaluator< update_proposal_votes_evaluator          >();
  _my->_evaluator_registry.register_evaluator< remove_proposal_evaluator                >();
  _my->_evaluator_registry.register_evaluator< recurrent_transfer_evaluator             >();
  _my->_evaluator_registry.register_evaluator< witness_block_approve_evaluator          >();


#ifdef IS_TEST_NET
  _my->_req_action_evaluator_registry.register_evaluator< example_required_evaluator    >();

  _my->_opt_action_evaluator_registry.register_evaluator< example_optional_evaluator    >();
#endif
}


void database::register_custom_operation_interpreter( std::shared_ptr< custom_operation_interpreter > interpreter )
{
  FC_ASSERT( interpreter );
  bool inserted = _custom_operation_interpreters.emplace( interpreter->get_custom_id(), interpreter ).second;
  // This assert triggering means we're mis-configured (multiple registrations of custom JSON evaluator for same ID)
  FC_ASSERT( inserted );
}

std::shared_ptr< custom_operation_interpreter > database::get_custom_json_evaluator( const custom_id_type& id )
{
  auto it = _custom_operation_interpreters.find( id );
  if( it != _custom_operation_interpreters.end() )
    return it->second;
  return std::shared_ptr< custom_operation_interpreter >();
}

void initialize_core_indexes( database& db );

void database::initialize_indexes()
{
  initialize_core_indexes( *this );
  _plugin_index_signal();
}

void database::initialize_irreversible_storage()
{
  auto s = get_segment_manager();

  irreversible_object = s->find_or_construct<irreversible_object_type>( "irreversible" )();
}

void database::resetState(const open_args& args)
{
  wipe(args.data_dir, args.shared_mem_dir, false);
  open(args);
}

const std::string& database::get_json_schema()const
{
  return _json_schema;
}

void database::init_schema()
{
  /*done_adding_indexes();

  db_schema ds;

  std::vector< std::shared_ptr< abstract_schema > > schema_list;

  std::vector< object_schema > object_schemas;
  get_object_schemas( object_schemas );

  for( const object_schema& oschema : object_schemas )
  {
    ds.object_types.emplace_back();
    ds.object_types.back().space_type.first = oschema.space_id;
    ds.object_types.back().space_type.second = oschema.type_id;
    oschema.schema->get_name( ds.object_types.back().type );
    schema_list.push_back( oschema.schema );
  }

  std::shared_ptr< abstract_schema > operation_schema = get_schema_for_type< operation >();
  operation_schema->get_name( ds.operation_type );
  schema_list.push_back( operation_schema );

  for( const std::pair< std::string, std::shared_ptr< custom_operation_interpreter > >& p : _custom_operation_interpreters )
  {
    ds.custom_operation_types.emplace_back();
    ds.custom_operation_types.back().id = p.first;
    schema_list.push_back( p.second->get_operation_schema() );
    schema_list.back()->get_name( ds.custom_operation_types.back().type );
  }

  graphene::db::add_dependent_schemas( schema_list );
  std::sort( schema_list.begin(), schema_list.end(),
    []( const std::shared_ptr< abstract_schema >& a,
        const std::shared_ptr< abstract_schema >& b )
    {
      return a->id < b->id;
    } );
  auto new_end = std::unique( schema_list.begin(), schema_list.end(),
    []( const std::shared_ptr< abstract_schema >& a,
        const std::shared_ptr< abstract_schema >& b )
    {
      return a->id == b->id;
    } );
  schema_list.erase( new_end, schema_list.end() );

  for( std::shared_ptr< abstract_schema >& s : schema_list )
  {
    std::string tname;
    s->get_name( tname );
    FC_ASSERT( ds.types.find( tname ) == ds.types.end(), "types with different ID's found for name ${tname}", ("tname", tname) );
    std::string ss;
    s->get_str_schema( ss );
    ds.types.emplace( tname, ss );
  }

  _json_schema = fc::json::to_string( ds );
  return;*/
}

void database::init_genesis( uint64_t init_supply, uint64_t hbd_init_supply )
{
  try
  {
    struct auth_inhibitor
    {
      auth_inhibitor(database& db) : db(db), old_flags(db.node_properties().skip_flags)
      { db.node_properties().skip_flags |= skip_authority_check; }
      ~auth_inhibitor()
      { db.node_properties().skip_flags = old_flags; }
    private:
      database& db;
      uint32_t old_flags;
    } inhibitor(*this);

    // Create blockchain accounts
    public_key_type      init_public_key(HIVE_INIT_PUBLIC_KEY);

    create< account_object >( HIVE_MINER_ACCOUNT, HIVE_GENESIS_TIME );
    create< account_authority_object >( [&]( account_authority_object& auth )
    {
      auth.account = HIVE_MINER_ACCOUNT;
      auth.owner.weight_threshold = 1;
      auth.active.weight_threshold = 1;
    });

    create< account_object >( HIVE_NULL_ACCOUNT, HIVE_GENESIS_TIME );
    create< account_authority_object >( [&]( account_authority_object& auth )
    {
      auth.account = HIVE_NULL_ACCOUNT;
      auth.owner.weight_threshold = 1;
      auth.active.weight_threshold = 1;
    });

#ifdef IS_TEST_NET
    create< account_object >( OBSOLETE_TREASURY_ACCOUNT, HIVE_GENESIS_TIME );
    create< account_object >( NEW_HIVE_TREASURY_ACCOUNT, HIVE_GENESIS_TIME );
#endif

    create< account_object >( HIVE_TEMP_ACCOUNT, HIVE_GENESIS_TIME );
    create< account_authority_object >( [&]( account_authority_object& auth )
    {
      auth.account = HIVE_TEMP_ACCOUNT;
      auth.owner.weight_threshold = 0;
      auth.active.weight_threshold = 0;
    });

    const auto init_witness = [&]( const account_name_type& account_name )
    {
      create< account_object >( account_name, HIVE_GENESIS_TIME, init_public_key );

      create< account_authority_object >( [&]( account_authority_object& auth )
      {
        auth.account = account_name;
        auth.owner.add_authority( init_public_key, 1 );
        auth.owner.weight_threshold = 1;
        auth.active  = auth.owner;
        auth.posting = auth.active;
      });

      create< witness_object >( [&]( witness_object& w )
      {
        w.owner        = account_name;
        w.signing_key  = init_public_key;
        w.schedule = witness_object::miner;
      } );
    };

    for( int i = 0; i < HIVE_NUM_INIT_MINERS; ++i )
      init_witness( HIVE_INIT_MINER_NAME + ( i ? fc::to_string( i ) : std::string() ) );

#ifdef USE_ALTERNATE_CHAIN_ID
    for( const auto& witness : configuration_data.get_init_witnesses() )
      init_witness( witness );
#endif

    // "steem" account was used as system account even before it was officially created; that bug
    // didn't have effect on the blockchain by chance, but caused problems during optimizations, so
    // now the account is officially created as system account (with all the same features it had -
    // there is not much difference between mined account and genesis one, just creation time);
    // the following transaction mined that account officially:
    // https://hiveblocks.com/tx/46ddcba847f2297d13e32be07d72d15c530a7271
    {
      const char* STEEM_ACCOUNT_NAME = "steem";
      auto STEEM_PUBLIC_KEY = public_key_type( HIVE_ADDRESS_PREFIX"65wH1LZ7BfSHcK69SShnqCAH5xdoSZpGkUjmzHJ5GCuxEK9V5G" );
      create< account_object >( STEEM_ACCOUNT_NAME, STEEM_PUBLIC_KEY, HIVE_GENESIS_TIME, true, nullptr, true, asset( 0, VESTS_SYMBOL ) );
      create< account_authority_object >( [&]( account_authority_object& auth )
      {
        auth.account = STEEM_ACCOUNT_NAME;
#ifdef USE_ALTERNATE_CHAIN_ID
        auth.owner = authority( 1, STEEM_PUBLIC_KEY, 1, init_public_key, 1 );
#else
        auth.owner = authority( 1, STEEM_PUBLIC_KEY, 1 );
#endif
        auth.active = auth.owner;
        auth.posting = auth.owner;
      } );
    }

    create< dynamic_global_property_object >( HIVE_INIT_MINER_NAME, asset( init_supply, HIVE_SYMBOL ), asset( hbd_init_supply, HBD_SYMBOL ) );
    // feed initial token supply to first miner
    modify( get_account( HIVE_INIT_MINER_NAME ), [&]( account_object& a )
    {
      a.balance = asset( init_supply, HIVE_SYMBOL );
      a.hbd_balance = asset( hbd_init_supply, HBD_SYMBOL );
    } );

#ifdef IS_TEST_NET
    create< feed_history_object >( [&]( feed_history_object& o )
    {
      o.current_median_history = price( asset( 1, HBD_SYMBOL ), asset( 1, HIVE_SYMBOL ) );
      o.market_median_history = o.current_median_history;
      o.current_min_history = o.current_median_history;
      o.current_max_history = o.current_median_history;
    });
#else
    // Nothing to do
    create< feed_history_object >( [&]( feed_history_object& o ) {});
#endif

    for( int i = 0; i < 0x10000; i++ )
      create< block_summary_object >( [&]( block_summary_object& ) {});
    create< hardfork_property_object >( [&](hardfork_property_object& hpo )
    {
      hpo.processed_hardforks.push_back( HIVE_GENESIS_TIME );
    } );

    // Create witness scheduler
    create< witness_schedule_object >( [&]( witness_schedule_object& wso )
    {
      FC_TODO( "Copied from witness_schedule.cpp, do we want to abstract this to a separate function?" );
      wso.current_shuffled_witnesses[0] = HIVE_INIT_MINER_NAME;
      util::rd_system_params account_subsidy_system_params;
      account_subsidy_system_params.resource_unit = HIVE_ACCOUNT_SUBSIDY_PRECISION;
      account_subsidy_system_params.decay_per_time_unit_denom_shift = HIVE_RD_DECAY_DENOM_SHIFT;
      util::rd_user_params account_subsidy_user_params;
      account_subsidy_user_params.budget_per_time_unit = wso.median_props.account_subsidy_budget;
      account_subsidy_user_params.decay_per_time_unit = wso.median_props.account_subsidy_decay;

      util::rd_user_params account_subsidy_per_witness_user_params;
      int64_t w_budget = wso.median_props.account_subsidy_budget;
      w_budget = (w_budget * HIVE_WITNESS_SUBSIDY_BUDGET_PERCENT) / HIVE_100_PERCENT;
      w_budget = std::min( w_budget, int64_t(std::numeric_limits<int32_t>::max()) );
      uint64_t w_decay = wso.median_props.account_subsidy_decay;
      w_decay = (w_decay * HIVE_WITNESS_SUBSIDY_DECAY_PERCENT) / HIVE_100_PERCENT;
      w_decay = std::min( w_decay, uint64_t(std::numeric_limits<uint32_t>::max()) );

      account_subsidy_per_witness_user_params.budget_per_time_unit = int32_t(w_budget);
      account_subsidy_per_witness_user_params.decay_per_time_unit = uint32_t(w_decay);

      util::rd_setup_dynamics_params( account_subsidy_user_params, account_subsidy_system_params, wso.account_subsidy_rd );
      util::rd_setup_dynamics_params( account_subsidy_per_witness_user_params, account_subsidy_system_params, wso.account_subsidy_witness_rd );
    } );

#ifdef HIVE_ENABLE_SMT
    create< nai_pool_object >( [&]( nai_pool_object& npo ) {} );
#endif
  }
  FC_CAPTURE_AND_RETHROW()
}

void database::notify_changed_objects()
{
  try
  {
    /*vector< chainbase::generic_id > ids;
    get_changed_ids( ids );
    HIVE_TRY_NOTIFY( changed_objects, ids )*/
    /*
    if( _undo_db.enabled() )
    {
      const auto& head_undo = _undo_db.head();
      vector<object_id_type> changed_ids;  changed_ids.reserve(head_undo.old_values.size());
      for( const auto& item : head_undo.old_values ) changed_ids.push_back(item.first);
      for( const auto& item : head_undo.new_ids ) changed_ids.push_back(item);
      vector<const object*> removed;
      removed.reserve( head_undo.removed.size() );
      for( const auto& item : head_undo.removed )
      {
        changed_ids.push_back( item.first );
        removed.emplace_back( item.second.get() );
      }
      HIVE_TRY_NOTIFY( changed_objects, changed_ids )
    }
    */
  }
  FC_CAPTURE_AND_RETHROW()

}

void database::set_flush_interval( uint32_t flush_blocks )
{
  _flush_blocks = flush_blocks;
  _next_flush_block = 0;
}

//////////////////// private methods ////////////////////

void database::apply_block(const std::shared_ptr<full_block_type>& full_block, uint32_t skip)
{ try {
  //fc::time_point begin_time = fc::time_point::now();

  detail::with_skip_flags( *this, skip, [&]()
  {
    _apply_block(full_block);
  } );

  /*try
  {
  /// check invariants
  if( is_in_control() || !( skip & skip_validate_invariants ) )
    validate_invariants();
  }
  FC_CAPTURE_AND_RETHROW( (next_block) );*/

  uint32_t block_num = full_block->get_block_num();

  //fc::time_point end_time = fc::time_point::now();
  //fc::microseconds dt = end_time - begin_time;
  if( _flush_blocks != 0 )
  {
    if( _next_flush_block == 0 )
    {
      uint32_t lep = block_num + 1 + _flush_blocks * 9 / 10;
      uint32_t rep = block_num + 1 + _flush_blocks;

      // use time_point::now() as RNG source to pick block randomly between lep and rep
      uint32_t span = rep - lep;
      uint32_t x = lep;
      if( span > 0 )
      {
        uint64_t now = uint64_t( fc::time_point::now().time_since_epoch().count() );
        x += now % span;
      }
      _next_flush_block = x;
      //ilog( "Next flush scheduled at block ${b}", ("b", x) );
    }

    if( _next_flush_block == block_num )
    {
      _next_flush_block = 0;
      //ilog( "Flushing database shared memory at block ${b}", ("b", block_num) );
      chainbase::database::flush();
    }
  }

} FC_CAPTURE_AND_RETHROW((full_block->get_block())) }

void database::check_free_memory( bool force_print, uint32_t current_block_num )
{
  uint64_t free_mem = get_free_memory();
  uint64_t max_mem = get_max_memory();

  if( BOOST_UNLIKELY( _shared_file_full_threshold != 0 && _shared_file_scale_rate != 0 && free_mem < fc::uint128_to_uint64( ( uint128_t( HIVE_100_PERCENT - _shared_file_full_threshold ) * max_mem ) / HIVE_100_PERCENT ) ) )
  {
    uint64_t new_max = fc::uint128_to_uint64( uint128_t( max_mem * _shared_file_scale_rate ) / HIVE_100_PERCENT ) + max_mem;

    wlog( "Memory is almost full, increasing to ${mem}M", ("mem", new_max / (1024*1024)) );

    resize( new_max );

    uint32_t free_mb = uint32_t( get_free_memory() / (1024*1024) );
    wlog( "Free memory is now ${free}M", ("free", free_mb) );
    _last_free_gb_printed = free_mb / 1024;
  }
  else
  {
    uint32_t free_gb = uint32_t( free_mem / (1024*1024*1024) );
    if( BOOST_UNLIKELY( force_print || (free_gb < _last_free_gb_printed) || (free_gb > _last_free_gb_printed+1) ) )
    {
      ilog( "Free memory is now ${n}G. Current block number: ${block}", ("n", free_gb)("block",current_block_num) );
      _last_free_gb_printed = free_gb;
    }

    if( BOOST_UNLIKELY( free_gb == 0 ) )
    {
      uint32_t free_mb = uint32_t( free_mem / (1024*1024) );

  #ifdef IS_TEST_NET
    if( !disable_low_mem_warning )
  #endif
      if( free_mb <= 100 && head_block_num() % 10 == 0 )
        elog( "Free memory is now ${n}M. Increase shared file size immediately!" , ("n", free_mb) );
    }
  }
}




void database::_apply_block(const std::shared_ptr<full_block_type>& full_block)
{
  const signed_block& block = full_block->get_block();
  const uint32_t block_num = full_block->get_block_num();
  block_notification note(full_block);



   inside_apply_block_play_json(block, block_num);

  // if(block_num % 10000 == 333)
  // {
  //   ilog("MTLK Block: ${block_num}. Data : ${block}", ("block_num", block_num)("block", block));

  //   fc::variant v;
  //   fc::to_variant( block, v );
  //   auto s = fc::json::to_string( v );
  //   ilog("MTLK variant: ${s}", ("s", s));

  //   fc::json::save_to_file( v, "temp.json");

  // }


  try {
  notify_pre_apply_block( note );

  BOOST_SCOPE_EXIT( this_ )
  {
    this_->_currently_processing_block_id.reset();
  } BOOST_SCOPE_EXIT_END
  _currently_processing_block_id = full_block->get_block_id();

  uint32_t skip = get_node_properties().skip_flags;
  _current_block_num    = block_num;
  _current_trx_in_block = 0;

  if( BOOST_UNLIKELY( block_num == 1 ) )
  {
    process_genesis_accounts();

    // For every existing before the head_block_time (genesis time), apply the hardfork
    // This allows the test net to launch with past hardforks and apply the next harfork when running

    uint32_t n;
    for( n=0; n<HIVE_NUM_HARDFORKS; n++ )
    {
      if( _hardfork_versions.times[n+1] > block.timestamp )
        break;
    }

    if( n > 0 )
    {
      ilog( "Processing ${n} genesis hardforks", ("n", n) );
      set_hardfork( n, true );
#ifdef IS_TEST_NET
      if( n < HIVE_NUM_HARDFORKS )
      {
        ilog( "Next hardfork scheduled for ${t} (current block timestamp ${c})",
          ( "t", _hardfork_versions.times[ n + 1 ] )( "c", block.timestamp ) );
      }
#endif

      const hardfork_property_object& hardfork_state = get_hardfork_property_object();
      FC_ASSERT( hardfork_state.current_hardfork_version == _hardfork_versions.versions[n], "Unexpected genesis hardfork state" );

      const auto& witness_idx = get_index<witness_index>().indices().get<by_id>();
      vector<witness_id_type> wit_ids_to_update;
      for( auto it=witness_idx.begin(); it!=witness_idx.end(); ++it )
        wit_ids_to_update.push_back( it->get_id() );

      for( witness_id_type wit_id : wit_ids_to_update )
      {
        modify( get( wit_id ), [&]( witness_object& wit )
        {
          wit.running_version = _hardfork_versions.versions[n];
          wit.hardfork_version_vote = _hardfork_versions.versions[n];
          wit.hardfork_time_vote = _hardfork_versions.times[n];
        } );
      }
    }
  }

  if( !( skip & skip_merkle_check ) )
  {
    if( _benchmark_dumper.is_enabled() )
      _benchmark_dumper.begin();

    auto merkle_root = full_block->get_merkle_root();

    try
    {
      FC_ASSERT(block.transaction_merkle_root == merkle_root, "Merkle check failed",
                (block.transaction_merkle_root)(merkle_root)(block)("id", full_block->get_block_id()));
    }
    catch( fc::assert_exception& e )
    { //don't throw error if this is a block with a known bad merkle root
      const auto& merkle_map = get_shared_db_merkle();
      auto itr = merkle_map.find( block_num );
      if( itr == merkle_map.end() || itr->second != merkle_root )
        throw e;
    }

    if( _benchmark_dumper.is_enabled() )
      _benchmark_dumper.end( "block", "merkle check" );
  }

  const witness_object& signing_witness = validate_block_header(skip, full_block);

  const auto& gprops = get_dynamic_global_properties();
  uint32_t block_size = full_block->get_uncompressed_block().raw_size;
  if( has_hardfork( HIVE_HARDFORK_0_12 ) )
    FC_ASSERT(block_size <= gprops.maximum_block_size, "Block size is larger than voted on block size", (block_num)(block_size)("max",gprops.maximum_block_size));

  if( block_size < HIVE_MIN_BLOCK_SIZE )
    elog("Block size is too small", (block_num)(block_size)("min", HIVE_MIN_BLOCK_SIZE));

  /// modify current witness so transaction evaluators can know who included the transaction,
  /// this is mostly for POW operations which must pay the current_witness
  modify( gprops, [&]( dynamic_global_property_object& dgp ){
    dgp.current_witness = block.witness;
  });

  required_automated_actions req_actions;
  optional_automated_actions opt_actions;
  /// parse witness version reporting
  process_header_extensions( block, req_actions, opt_actions );

  if( has_hardfork( HIVE_HARDFORK_0_5__54 ) ) // Cannot remove after hardfork
  {
    const auto& witness = get_witness( block.witness );
    const auto& hardfork_state = get_hardfork_property_object();
    FC_ASSERT(witness.running_version >= hardfork_state.current_hardfork_version,
              "Block produced by witness that is not running current hardfork",
              (witness)(block.witness)(hardfork_state));
  }

  for( const std::shared_ptr<full_transaction_type>& trx : full_block->get_full_transactions() )
  {
    /* We do not need to push the undo state for each transaction
      * because they either all apply and are valid or the
      * entire block fails to apply.  We only need an "undo" state
      * for transactions when validating broadcast transactions or
      * when building a block.
      */
    apply_transaction( trx, skip );
    ++_current_trx_in_block;
  }

  _current_trx_in_block = -1;
  _current_op_in_trx = 0;

  update_global_dynamic_data(block);
  update_signing_witness(signing_witness, block);

  uint32_t old_last_irreversible = update_last_irreversible_block(true);

  create_block_summary(full_block);
  clear_expired_transactions();
  clear_expired_orders();
  clear_expired_delegations();

  update_witness_schedule(*this);

  update_median_feed();
  update_virtual_supply(); //accommodate potentially new price

  clear_null_account_balance();
  consolidate_treasury_balance();
  process_funds();
  process_conversions();
  process_comment_cashout();
  process_vesting_withdrawals();
  process_savings_withdraws();
  process_subsidized_accounts();
  pay_liquidity_reward();
  update_virtual_supply(); //cover changes in HBD supply from above processes

  account_recovery_processing();
  expire_escrow_ratification();
  process_decline_voting_rights();
  process_proposals( note ); //new HBD converted here does not count towards limit
  process_delayed_voting( note );
  remove_expired_governance_votes();

  process_recurrent_transfers();

  generate_required_actions();
  generate_optional_actions();

  process_required_actions( req_actions );
  process_optional_actions( opt_actions );

  process_hardforks();

  // notify observers that the block has been applied
  notify_post_apply_block( note );

  notify_changed_objects();

  // This moves newly irreversible blocks from the fork db to the block log
  // and commits irreversible state to the database. This should always be the
  // last call of applying a block because it is the only thing that is not
  // reversible.
  migrate_irreversible_state(old_last_irreversible);
} FC_CAPTURE_CALL_LOG_AND_RETHROW( std::bind( &database::notify_fail_apply_block, this, note ), (block_num) ) }

struct process_header_visitor
{
  process_header_visitor( const std::string& witness, required_automated_actions& req_actions, optional_automated_actions& opt_actions, database& db ) :
    _witness( witness ),
    _req_actions( req_actions ),
    _opt_actions( opt_actions ),
    _db( db ) {}

  typedef void result_type;

  const std::string& _witness;
  required_automated_actions& _req_actions;
  optional_automated_actions& _opt_actions;
  database& _db;

  void operator()( const void_t& obj ) const
  {
    //Nothing to do.
  }

  void operator()( const version& reported_version ) const
  {
    const auto& signing_witness = _db.get_witness( _witness );
    //idump( (next_block.witness)(signing_witness.running_version)(reported_version) );

    if( reported_version != signing_witness.running_version )
    {
      _db.modify( signing_witness, [&]( witness_object& wo )
      {
        wo.running_version = reported_version;
      });
    }
  }

  void operator()( const hardfork_version_vote& hfv ) const
  {
    const auto& signing_witness = _db.get_witness( _witness );
    //idump( (next_block.witness)(signing_witness.running_version)(hfv) );

    if( hfv.hf_version != signing_witness.hardfork_version_vote || hfv.hf_time != signing_witness.hardfork_time_vote )
      _db.modify( signing_witness, [&]( witness_object& wo )
      {
        wo.hardfork_version_vote = hfv.hf_version;
        wo.hardfork_time_vote = hfv.hf_time;
      });
  }

  void operator()( const required_automated_actions& req_actions ) const
  {
    FC_ASSERT( _db.has_hardfork( HIVE_SMT_HARDFORK ), "Automated actions are not enabled until SMT hardfork." );
    std::copy( req_actions.begin(), req_actions.end(), std::back_inserter( _req_actions ) );
  }

FC_TODO( "Remove when optional automated actions are created" )
#ifdef IS_TEST_NET
  void operator()( const optional_automated_actions& opt_actions ) const
  {
    FC_ASSERT( _db.has_hardfork( HIVE_SMT_HARDFORK ), "Automated actions are not enabled until SMT hardfork." );
    std::copy( opt_actions.begin(), opt_actions.end(), std::back_inserter( _opt_actions ) );
  }
#endif
};

void database::process_header_extensions( const signed_block& next_block, required_automated_actions& req_actions, optional_automated_actions& opt_actions )
{
  process_header_visitor _v( next_block.witness, req_actions, opt_actions, *this );

  for( const auto& e : next_block.extensions )
    e.visit( _v );
}

void database::process_genesis_accounts()
{
  /*
    This method is evaluated after processing all transactions,
    so parameters of vop notification (trx_in_block = -1)
    matches system pattern (vops generated by chain).

    Calling this method before processing transaction makes
    notifications simillar to theese produced by normal operations,
    which is invalid, because there is no such operation
    to refer (no access to genesis [0] block).
  */

  // create virtual operations for accounts created in genesis
  const int32_t trx_in_block_prev{ _current_trx_in_block };
  _current_trx_in_block = -1;
  const auto& account_idx = get_index< chain::account_index >().indices().get< chain::by_id >();
  std::for_each(
      account_idx.begin()
    , account_idx.end()
    , [&]( const account_object& obj ){
        push_virtual_operation(
          account_created_operation( obj.get_name(), obj.get_name(), asset(0, VESTS_SYMBOL), asset(0, VESTS_SYMBOL) ) );
      }
  );
  _current_trx_in_block = trx_in_block_prev;
}

void database::update_median_feed()
{
try {
  if( (head_block_num() % HIVE_FEED_INTERVAL_BLOCKS) != 0 )
    return;

  auto now = head_block_time();
  const witness_schedule_object& wso = get_witness_schedule_object();
  vector<price> feeds; feeds.reserve( wso.num_scheduled_witnesses );
  for( int i = 0; i < wso.num_scheduled_witnesses; i++ )
  {
    const auto& wit = get_witness( wso.current_shuffled_witnesses[i] );
    if( has_hardfork( HIVE_HARDFORK_0_19__822 ) )
    {
      if( now < wit.get_last_hbd_exchange_update() + HIVE_MAX_FEED_AGE_SECONDS
        && !wit.get_hbd_exchange_rate().is_null() )
      {
        feeds.push_back( wit.get_hbd_exchange_rate() );
      }
    }
    else if( wit.get_last_hbd_exchange_update() < now + HIVE_MAX_FEED_AGE_SECONDS &&
        !wit.get_hbd_exchange_rate().is_null() )
    {
      feeds.push_back( wit.get_hbd_exchange_rate() );
    }
  }

  if( feeds.size() >= HIVE_MIN_FEEDS )
  {
    std::sort( feeds.begin(), feeds.end() );
    auto median_feed = feeds[feeds.size()/2];

    modify( get_feed_history(), [&]( feed_history_object& fho )
    {
      fho.price_history.push_back( median_feed );
      size_t hive_feed_history_window = HIVE_FEED_HISTORY_WINDOW_PRE_HF_16;
      if( has_hardfork( HIVE_HARDFORK_0_16__551) )
        hive_feed_history_window = HIVE_FEED_HISTORY_WINDOW;

      if( fho.price_history.size() > hive_feed_history_window )
        fho.price_history.pop_front();

      if( fho.price_history.size() )
      {
        std::vector< price > copy( fho.price_history.begin(), fho.price_history.end() );
        std::sort( copy.begin(), copy.end() );
        fho.current_median_history = copy[copy.size()/2];
        fho.market_median_history = fho.current_median_history;
        fho.current_min_history = copy.front();
        fho.current_max_history = copy.back();

#ifdef IS_TEST_NET
        if( skip_price_feed_limit_check )
          return;
#endif
        if( has_hardfork( HIVE_HARDFORK_0_14__230 ) )
        {
          // This block limits the effective median price to force HBD to remain at or
          // below HIVE_HBD_HARD_LIMIT of the combined market cap of HIVE and HBD.
          // The reason is to prevent individual with a lot of HBD to use sharp decline
          // in HIVE price to make in-chain-but-out-of-market conversion to HIVE and take
          // over the blockchain
          //
          // For example (for 10% hard limit), if we have 500 HIVE and 100 HBD, the price is
          // limited to 900 HBD / 500 HIVE which works out to be $1.80. At this price, 500 HIVE
          // would be valued at 500 * $1.80 = $900. 100 HBD is by definition always $100,
          // so the combined market cap is $900 + $100 = $1000.
          //
          // Generalized formula:
          // With minimal price we want existing amount of HBD (X) to be HIVE_HBD_HARD_LIMIT (L) of
          // combined market cap (CMC), meaning the existing amount of HIVE (Y) has to be 100%-L of CMC.
          // X + Y*price = CMC, X = L*CMC, Y*price = (100%-L)*CMC
          // (100% - L)*CMC = (100% - L)*X/L = (100%/L - 1)*X therefore minimal price is
          // (100%/L - 1)*X HBD per Y HIVE
          // the above has one big problem - accuracy; f.e. with L = 30% the price will be the same
          // as for L = 33%; for better accuracy we can express the price as (100%-L)*X HBD per L*Y HIVE

          const auto& dgpo = get_dynamic_global_properties();
          auto hbd_supply = dgpo.get_current_hbd_supply();
          if( has_hardfork( HIVE_HARDFORK_1_25_HBD_HARD_CAP ) )
            hbd_supply -= get_treasury().get_hbd_balance();
          if( hbd_supply.amount > 0 )
          {
            uint16_t limit = HIVE_HBD_HARD_LIMIT_PRE_HF26;
            if( has_hardfork( HIVE_HARDFORK_1_26_HBD_HARD_CAP ) )
              limit = HIVE_HBD_HARD_LIMIT;
            static_assert( ( HIVE_HBD_HARD_LIMIT % HIVE_1_PERCENT ) == 0, "Hard cap has to be expressed in full percentage points" );
            limit /= HIVE_1_PERCENT; //ABW: this is just to have two more levels of magnitude bigger margin;
              //even without it we can still fit within 64bit value, even though numbers used here are pretty big
            price min_price( asset( ( HIVE_100_PERCENT/HIVE_1_PERCENT - limit ) * hbd_supply.amount, HBD_SYMBOL ),
                             asset( limit * dgpo.get_current_supply().amount, HIVE_SYMBOL ) );

            /*
            ilog( "GREP${daily}: ${block}, ${minfeed}, ${medfeed}, ${maxfeed}, ${minprice}, ${medprice}, ${maxprice}, ${capprice}, ${debt}, ${hbdinfl}, ${hivesup}, ${virtsup}, ${hbdsup}",
              ( "daily", ( ( head_block_num() % ( 24 * HIVE_FEED_INTERVAL_BLOCKS ) ) == 0 ) ? "24" : "" )( "block", head_block_num() )
              ( "minfeed", double( feeds.front().base.amount.value ) / double( feeds.front().quote.amount.value ) )
              ( "medfeed", double( median_feed.base.amount.value ) / double( median_feed.quote.amount.value ) )
              ( "maxfeed", double( feeds.back().base.amount.value ) / double( feeds.back().quote.amount.value ) )
              ( "minprice", double( fho.current_min_history.base.amount.value ) / double( fho.current_min_history.quote.amount.value ) )
              ( "medprice", double( fho.current_median_history.base.amount.value ) / double( fho.current_median_history.quote.amount.value ) )
              ( "maxprice", double( fho.current_max_history.base.amount.value ) / double( fho.current_max_history.quote.amount.value ) )
              ( "capprice", double( min_price.base.amount.value ) / double( min_price.quote.amount.value ) )
              ( "debt", double( calculate_HBD_percent() ) / 100.0 )
              ( "hbdinfl", double( dgpo.get_hbd_interest_rate() ) / 100.0 )
              ( "hivesup", dgpo.get_current_supply().amount.value )
              ( "virtsup", dgpo.virtual_supply.amount.value )
              ( "hbdsup", hbd_supply.amount.value )
            );
            */

            if( min_price > fho.current_median_history )
            {
              push_virtual_operation( system_warning_operation( FC_LOG_MESSAGE( warn,
                "HIVE price corrected upward due to ${limit}% HBD cutoff rule, from ${actual} to ${corrected}",
                ( "limit", limit )( "actual", fho.current_median_history )( "corrected", min_price )).get_message()));

              fho.current_median_history = min_price;
            }
            //should we also correct current_min_history? note that we MUST NOT change market_median_history
            if( min_price > fho.current_max_history )
              fho.current_max_history = min_price;
          }
        }
      }
    });
  }
} FC_CAPTURE_AND_RETHROW() }

void database::apply_transaction(const std::shared_ptr<full_transaction_type>& trx, uint32_t skip)
{
  detail::with_skip_flags( *this, skip, [&]() { _apply_transaction(trx); });
}

void database::validate_transaction(const std::shared_ptr<full_transaction_type>& full_transaction, uint32_t skip)
{
  const signed_transaction& trx = full_transaction->get_transaction();

  //Skip all manner of expiration and TaPoS checking if we're on block 1; It's impossible that the transaction is
  //expired, and TaPoS makes no sense as no blocks exist.
  if (BOOST_LIKELY(head_block_num() > 0))
  {
    fc::time_point_sec now = head_block_time();

    HIVE_ASSERT(trx.expiration <= now + fc::seconds(HIVE_MAX_TIME_UNTIL_EXPIRATION), transaction_expiration_exception,
                "", (trx.expiration)(now)("max_til_exp", HIVE_MAX_TIME_UNTIL_EXPIRATION));
    if (has_hardfork(HIVE_HARDFORK_0_9)) // Simple solution to pending trx bug when now == trx.expiration
      HIVE_ASSERT(now < trx.expiration, transaction_expiration_exception, "", (now)(trx.expiration));
    else
      HIVE_ASSERT(now <= trx.expiration, transaction_expiration_exception, "", (now)(trx.expiration));

    if (!(skip & skip_tapos_check))
    {
      if (_benchmark_dumper.is_enabled())
        _benchmark_dumper.begin();

      block_summary_object::id_type bsid(trx.ref_block_num);
      const auto& tapos_block_summary = get<block_summary_object>(bsid);
      //Verify TaPoS block summary has correct ID prefix, and that this block's time is not past the expiration
      HIVE_ASSERT(trx.ref_block_prefix == tapos_block_summary.block_id._hash[1], transaction_tapos_exception,
                  "", (trx.ref_block_prefix)("tapos_block_summary", tapos_block_summary.block_id._hash[1]));

      if (_benchmark_dumper.is_enabled())
        _benchmark_dumper.end("transaction", "tapos check");
    }
  }

  if (!(skip & skip_validate)) /* issue #505 explains why this skip_flag is disabled */
  {
    if (_benchmark_dumper.is_enabled())
    {
      std::string name;
      full_transaction->validate([&](const operation& op, bool post) {
        if (!post)
        {
          name = _my->_evaluator_registry.get_evaluator(op).get_name(op);
          _benchmark_dumper.begin();
        }
        else
        {
          _benchmark_dumper.end("validate", name);
        }
      });
    }
    else
    {
      full_transaction->validate();
    }

    if (!has_hardfork(HIVE_HARDFORK_1_26_ENABLE_NEW_SERIALIZATION))
      HIVE_ASSERT(full_transaction->is_legacy_pack(), hive::protocol::transaction_auth_exception, "legacy serialization must be used until hardfork 26");
  }

  if (!(skip & (skip_transaction_signatures | skip_authority_check)))
  {
    auto get_active  =    [&]( const string& name ) { return authority( get< account_authority_object, by_account >( name ).active ); };
    auto get_owner   =    [&]( const string& name ) { return authority( get< account_authority_object, by_account >( name ).owner );  };
    auto get_posting =    [&]( const string& name ) { return authority( get< account_authority_object, by_account >( name ).posting );  };
    auto get_witness_key = [&]( const string& name ) { try { return get_witness( name ).signing_key; } FC_CAPTURE_AND_RETHROW((name)) };

    try
    {
      if( _benchmark_dumper.is_enabled() )
        _benchmark_dumper.begin();

      const flat_set<public_key_type>& signature_keys = full_transaction->get_signature_keys();
      const required_authorities_type& required_authorities = full_transaction->get_required_authorities();

      hive::protocol::verify_authority(required_authorities,
                                       signature_keys,
                                       get_active,
                                       get_owner,
                                       get_posting,
                                       get_witness_key,
                                       HIVE_MAX_SIG_CHECK_DEPTH,
                                       has_hardfork(HIVE_HARDFORK_0_20) ? HIVE_MAX_AUTHORITY_MEMBERSHIP : 0,
                                       has_hardfork(HIVE_HARDFORK_0_20) ? HIVE_MAX_SIG_CHECK_ACCOUNTS : 0,
                                       false,
                                       flat_set<account_name_type>(),
                                       flat_set<account_name_type>(),
                                       flat_set<account_name_type>());

      if (_benchmark_dumper.is_enabled())
        _benchmark_dumper.end("transaction", "verify_authority", trx.signatures.size());
    }
    catch (protocol::tx_missing_active_auth& e)
    {
      if (get_shared_db_merkle().find(head_block_num() + 1) == get_shared_db_merkle().end())
        throw e;
    }
  }
}

void database::_apply_transaction(const std::shared_ptr<full_transaction_type>& full_transaction)
{ try {
  if( _current_tx_status == TX_STATUS_NONE )
  {
    wlog( "Missing tx processing indicator" );
    // make sure to call set_tx_status() with proper status when your call can lead here
  }

  transaction_notification note( full_transaction );
  BOOST_SCOPE_EXIT(this_) { this_->_current_trx_id = transaction_id_type(); } BOOST_SCOPE_EXIT_END
  _current_trx_id = full_transaction->get_transaction_id();
  const transaction_id_type& trx_id = full_transaction->get_transaction_id();

  uint32_t skip = get_node_properties().skip_flags;

  if( !( skip & skip_transaction_dupe_check ) )
  {
    auto& trx_idx = get_index<transaction_index>();
    FC_ASSERT(trx_idx.indices().get<by_trx_id>().find(trx_id) == trx_idx.indices().get<by_trx_id>().end(),
              "Duplicate transaction check failed", (trx_id));
  }

  const signed_transaction& trx = full_transaction->get_transaction();

  validate_transaction(full_transaction, skip);

  //Insert transaction into unique transactions database.
  if( !(skip & skip_transaction_dupe_check) )
  {
    if( _benchmark_dumper.is_enabled() )
      _benchmark_dumper.begin();

    create<transaction_object>([&](transaction_object& transaction) {
      transaction.trx_id = trx_id;
      transaction.expiration = trx.expiration;
    });

    if( _benchmark_dumper.is_enabled() )
      _benchmark_dumper.end( "transaction", "dupe check" );
  }

  notify_pre_apply_transaction( note );

  //Finally process the operations
  _current_op_in_trx = 0;
  for (const auto& op : trx.operations)
  { try {
    apply_operation(op);
    ++_current_op_in_trx;
  } FC_CAPTURE_AND_RETHROW( (op) ) }

  notify_post_apply_transaction( note );

} FC_CAPTURE_AND_RETHROW( (full_transaction->get_transaction()) ) }


struct applied_operation_info_controller
{
  applied_operation_info_controller(const struct operation_notification** storage, const operation_notification& note) :
    _storage(storage)
    {
    *_storage = &note;
    }

  ~applied_operation_info_controller()
  {
    *_storage = nullptr;
  }

private:
  const struct operation_notification** _storage = nullptr;
};

void database::apply_operation(const operation& op)
{
  operation_notification note = create_operation_notification( op );

  applied_operation_info_controller ctrlr(&_current_applied_operation_info, note);

  notify_pre_apply_operation( note );

  std::string name;
  if( _benchmark_dumper.is_enabled() )
  {
    name = _my->_evaluator_registry.get_evaluator( op ).get_name( op );
    _benchmark_dumper.begin();
  }

  _my->_evaluator_registry.get_evaluator( op ).apply( op );

  if( _benchmark_dumper.is_enabled() )
    _benchmark_dumper.end( name );

  notify_post_apply_operation( note );
}

struct action_equal_visitor
{
  typedef bool result_type;

  const required_automated_action& action_a;

  action_equal_visitor( const required_automated_action& a ) : action_a( a ) {}

  template< typename Action >
  bool operator()( const Action& action_b )const
  {
    if( action_a.which() != required_automated_action::tag< Action >::value ) return false;

    return action_a.get< Action >() == action_b;
  }
};

void database::process_required_actions( const required_automated_actions& actions )
{
  const auto& pending_action_idx = get_index< pending_required_action_index, by_id >();
  auto actions_itr = actions.begin();
  uint64_t total_actions_size = 0;

  while( true )
  {
    auto pending_itr = pending_action_idx.begin();

    if( actions_itr == actions.end() )
    {
      // We're done processing actions in the block.
      if( pending_itr != pending_action_idx.end() && pending_itr->execution_time <= head_block_time() )
      {
        total_actions_size += fc::raw::pack_size( pending_itr->action );
        const auto& gpo = get_dynamic_global_properties();
        uint64_t required_actions_partition_size = ( gpo.maximum_block_size * gpo.required_actions_partition_percent ) / HIVE_100_PERCENT;
        FC_ASSERT( total_actions_size > required_actions_partition_size,
          "Expected action was not included in block. total_actions_size: ${as}, required_actions_partition_action: ${rs}, pending_action: ${pa}",
          ("as", total_actions_size)
          ("rs", required_actions_partition_size)
          ("pa", *pending_itr) );
      }
      break;
    }

    FC_ASSERT( pending_itr != pending_action_idx.end(),
      "Block included required action that does not exist in queue" );

    action_equal_visitor equal_visitor( pending_itr->action );
    FC_ASSERT( actions_itr->visit( equal_visitor ),
      "Unexpected action included. Expected: ${e} Observed: ${o}",
      ("e", pending_itr->action)("o", *actions_itr) );

    apply_required_action( *actions_itr );

    total_actions_size += fc::raw::pack_size( *actions_itr );

    remove( *pending_itr );
    ++actions_itr;
  }
}

void database::apply_required_action( const required_automated_action& a )
{
  required_action_notification note( a );
  notify_pre_apply_required_action( note );

  _my->_req_action_evaluator_registry.get_evaluator( a ).apply( a );

  notify_post_apply_required_action( note );
}

void database::process_optional_actions( const optional_automated_actions& actions )
{
  if( !has_hardfork( HIVE_SMT_HARDFORK ) ) return;

  static const action_validate_visitor validate_visitor;

  for( auto actions_itr = actions.begin(); actions_itr != actions.end(); ++actions_itr )
  {
    actions_itr->visit( validate_visitor );

    // There is no execution check because we don't have a good way of indexing into local
    // optional actions from those contained in a block. It is the responsibility of the
    // action evaluator to prevent early execution.
    apply_optional_action( *actions_itr );
  }

  // This expiration is based on the timestamp of the last irreversible block. For historical
  // blocks, generation of optional actions should be disabled and the expiration can be skipped.
  // For reindexing of the first 2 million blocks, this unnecessary read consumes almost 30%
  // of runtime.
  FC_TODO( "Optimize expiration for reindex." );

  // Clear out "expired" optional_actions. If the block when an optional action was generated
  // has become irreversible then a super majority of witnesses have chosen to not include it
  // and it is safe to delete.
  const auto& pending_action_idx = get_index< pending_optional_action_index, by_execution >();
  auto pending_itr = pending_action_idx.begin();
  std::shared_ptr<full_block_type> lib = fetch_block_by_number(get_last_irreversible_block_num());

  // This is always valid when running on mainnet because there are irreversible blocks
  // Testnet and unit tests, not so much. Could be ifdeffed with IS_TEST_NET, but seems
  // like a reasonable check and will be optimized via speculative execution.
  if (lib)
  {
    while (pending_itr != pending_action_idx.end() && pending_itr->execution_time <= lib->get_block_header().timestamp)
    {
      remove( *pending_itr );
      pending_itr = pending_action_idx.begin();
    }
  }
}

void database::apply_optional_action( const optional_automated_action& a )
{
  optional_action_notification note( a );
  notify_pre_apply_optional_action( note );

  _my->_opt_action_evaluator_registry.get_evaluator( a ).apply( a );

  notify_post_apply_optional_action( note );
}

template <typename TFunction> struct fcall {};

template <typename TResult, typename... TArgs>
struct fcall<TResult(TArgs...)>
{
  using TNotification = std::function<TResult(TArgs...)>;

  fcall() = default;
  fcall(const TNotification& func, util::advanced_benchmark_dumper& dumper,
    const abstract_plugin& plugin, const std::string& context, const std::string& item_name)
    : _func(func), _benchmark_dumper(dumper), _context(context), _name(item_name) {}

  void operator () (TArgs&&... args)
  {
    if (_benchmark_dumper.is_enabled())
      _benchmark_dumper.begin();

    _func(std::forward<TArgs>(args)...);

    if (_benchmark_dumper.is_enabled())
      _benchmark_dumper.end( _context, _name );
  }

private:
  TNotification                    _func;
  util::advanced_benchmark_dumper& _benchmark_dumper;
  std::string                      _context;
  std::string                      _name;
};

template <typename TResult, typename... TArgs>
struct fcall<std::function<TResult(TArgs...)>>
  : public fcall<TResult(TArgs...)>
{
  typedef fcall<TResult(TArgs...)> TBase;
  using TBase::TBase;
};

template <bool IS_PRE_OPERATION, typename TSignal, typename TNotification>
boost::signals2::connection database::connect_impl( TSignal& signal, const TNotification& func,
  const abstract_plugin& plugin, int32_t group, const std::string& item_name )
{
  fcall<TNotification> fcall_wrapper( func, _benchmark_dumper, plugin,
    util::advanced_benchmark_dumper::generate_context_desc<IS_PRE_OPERATION>( plugin.get_name() ), item_name );

  return signal.connect(group, fcall_wrapper);
}

template< bool IS_PRE_OPERATION >
boost::signals2::connection database::any_apply_operation_handler_impl( const apply_operation_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  std::string context = util::advanced_benchmark_dumper::generate_context_desc< IS_PRE_OPERATION >( plugin.get_name() );
  auto complex_func = [this, func, &plugin, context]( const operation_notification& o )
  {
    std::string name;

    if (_benchmark_dumper.is_enabled())
    {
      if( _my->_evaluator_registry.is_evaluator( o.op ) )
        name = _my->_evaluator_registry.get_evaluator( o.op ).get_name( o.op );
      else
        name = util::advanced_benchmark_dumper::get_virtual_operation_name();

      _benchmark_dumper.begin();
    }

    func( o );

    if (_benchmark_dumper.is_enabled())
      _benchmark_dumper.end( context, name );
  };

  if( IS_PRE_OPERATION )
    return _pre_apply_operation_signal.connect(group, complex_func);
  else
    return _post_apply_operation_signal.connect(group, complex_func);
}

boost::signals2::connection database::add_pre_apply_required_action_handler( const apply_required_action_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  return connect_impl<true>(_pre_apply_required_action_signal, func, plugin, group, "required_action");
}

boost::signals2::connection database::add_post_apply_required_action_handler( const apply_required_action_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  return connect_impl<false>(_post_apply_required_action_signal, func, plugin, group, "required_action");
}

boost::signals2::connection database::add_pre_apply_optional_action_handler( const apply_optional_action_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  return connect_impl<true>(_pre_apply_optional_action_signal, func, plugin, group, "optional_action");
}

boost::signals2::connection database::add_post_apply_optional_action_handler( const apply_optional_action_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  return connect_impl<false>(_post_apply_optional_action_signal, func, plugin, group, "optional_action");
}

boost::signals2::connection database::add_pre_apply_operation_handler( const apply_operation_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  return any_apply_operation_handler_impl< true/*IS_PRE_OPERATION*/ >( func, plugin, group );
}

boost::signals2::connection database::add_post_apply_operation_handler( const apply_operation_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  return any_apply_operation_handler_impl< false/*IS_PRE_OPERATION*/ >( func, plugin, group );
}

boost::signals2::connection database::add_pre_apply_transaction_handler( const apply_transaction_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  return connect_impl<true>(_pre_apply_transaction_signal, func, plugin, group, "transaction");
}

boost::signals2::connection database::add_post_apply_transaction_handler( const apply_transaction_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  return connect_impl<false>(_post_apply_transaction_signal, func, plugin, group, "transaction");
}

boost::signals2::connection database::add_pre_apply_custom_operation_handler ( const apply_custom_operation_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  return connect_impl< true/*IS_PRE_OPERATION*/ >(_pre_apply_custom_operation_signal, func, plugin, group, "custom");
}

boost::signals2::connection database::add_post_apply_custom_operation_handler( const apply_custom_operation_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  return connect_impl< false/*IS_PRE_OPERATION*/ >(_post_apply_custom_operation_signal, func, plugin, group, "custom");
}

boost::signals2::connection database::add_pre_apply_block_handler( const apply_block_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  return connect_impl<true>(_pre_apply_block_signal, func, plugin, group, "block");
}

boost::signals2::connection database::add_post_apply_block_handler( const apply_block_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  return connect_impl<false>(_post_apply_block_signal, func, plugin, group, "block");
}

boost::signals2::connection database::add_fail_apply_block_handler( const apply_block_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  return connect_impl<false>(_fail_apply_block_signal, func, plugin, group, "failed block");
}

boost::signals2::connection database::add_irreversible_block_handler( const irreversible_block_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  return connect_impl<false>(_on_irreversible_block, func, plugin, group, "irreversible");
}

boost::signals2::connection database::add_switch_fork_handler( const switch_fork_handler_t& func,
                                                                      const abstract_plugin& plugin, int32_t group )
{
  return connect_impl<false>(_switch_fork_signal, func, plugin, group, "switch_fork");
}

boost::signals2::connection database::add_pre_reindex_handler(const reindex_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  return connect_impl<true>(_pre_reindex_signal, func, plugin, group, "reindex");
}

boost::signals2::connection database::add_post_reindex_handler(const reindex_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  return connect_impl<false>(_post_reindex_signal, func, plugin, group, "reindex");
}

boost::signals2::connection database::add_finish_push_block_handler( const push_block_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  return connect_impl<false>(_finish_push_block_signal, func, plugin, group, "block");
}

boost::signals2::connection database::add_generate_optional_actions_handler(const generate_optional_actions_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  return connect_impl<true>(_generate_optional_actions_signal, func, plugin, group, "generate_optional_actions");
}

boost::signals2::connection database::add_prepare_snapshot_handler(const prepare_snapshot_handler_t& func, const abstract_plugin& plugin, int32_t group)
{
  return connect_impl<true>(_prepare_snapshot_signal, func, plugin, group, "prepare_snapshot");
}

boost::signals2::connection database::add_snapshot_supplement_handler(const prepare_snapshot_data_supplement_handler_t& func, const abstract_plugin& plugin, int32_t group)
{
  return connect_impl<true>(_prepare_snapshot_supplement_signal, func, plugin, group, "prepare_snapshot_data_supplement");
}

boost::signals2::connection database::add_snapshot_supplement_handler(const load_snapshot_data_supplement_handler_t& func, const abstract_plugin& plugin, int32_t group)
{
  return connect_impl<true>(_load_snapshot_supplement_signal, func, plugin, group, "load_snapshot_data_supplement");
}

boost::signals2::connection database::add_comment_reward_handler(const comment_reward_notification_handler_t& func, const abstract_plugin& plugin, int32_t group)
{
  return connect_impl<true>(_comment_reward_signal, func, plugin, group, "comment_reward");
}

boost::signals2::connection database::add_end_of_syncing_handler(const end_of_syncing_notification_handler_t& func, const abstract_plugin& plugin, int32_t group)
{
  return connect_impl<false>(_end_of_syncing_signal, func, plugin, group, "->syncing_end");
}

const witness_object& database::validate_block_header( uint32_t skip, const std::shared_ptr<full_block_type>& full_block )const
{ try {
  const signed_block_header& next_block_header = full_block->get_block_header();
  FC_ASSERT( head_block_id() == next_block_header.previous, "", ("head_block_id", head_block_id())("next.prev", next_block_header.previous) );
  FC_ASSERT( head_block_time() < next_block_header.timestamp, "", ("head_block_time", head_block_time())("next", next_block_header.timestamp)("blocknum", full_block->get_block_num()) );
  const witness_object& witness = get_witness( next_block_header.witness );

  if (!(skip & skip_witness_signature))
    FC_ASSERT(witness.signing_key == full_block->get_signing_key());

  if( !(skip&skip_witness_schedule_check) )
  {
    uint32_t slot_num = get_slot_at_time( next_block_header.timestamp );
    FC_ASSERT( slot_num > 0 );

    string scheduled_witness = get_scheduled_witness( slot_num );

    FC_ASSERT(witness.owner == scheduled_witness, "Witness produced block at wrong time",
              ("block witness", next_block_header.witness)(scheduled_witness)(slot_num));
  }

  return witness;
} FC_CAPTURE_AND_RETHROW() }

void database::create_block_summary(const std::shared_ptr<full_block_type>& full_block)
{ try {
  block_summary_object::id_type bsid( full_block->get_block_num() & 0xffff );
  modify( get< block_summary_object >( bsid ), [&](block_summary_object& p) {
      p.block_id = full_block->get_block_id();
  });
} FC_CAPTURE_AND_RETHROW() }

void database::update_global_dynamic_data( const signed_block& b )
{ try {
  const dynamic_global_property_object& _dgp = get_dynamic_global_properties();

  uint32_t missed_blocks = get_slot_at_time( b.timestamp );
;
  assert( missed_blocks != 0 );
  missed_blocks--;
  if( head_block_num() != 0 )
  {
    for( uint32_t i = 0; i < missed_blocks; ++i )
    {
      const auto& witness_missed = get_witness( get_scheduled_witness( i + 1 ) );
      if(  witness_missed.owner != b.witness )
      {
        modify( witness_missed, [&]( witness_object& w )
        {
          w.total_missed++;
          if( has_hardfork( HIVE_HARDFORK_0_14__278 ) && !has_hardfork( HIVE_HARDFORK_0_20__SP190 ) )
          {
            if( head_block_num() - w.last_confirmed_block_num  > HIVE_WITNESS_SHUTDOWN_THRESHOLD )
            {
              w.signing_key = public_key_type();
#ifdef USE_ALTERNATE_CHAIN_ID
              if( configuration_data.get_generate_missed_block_operations() )
#endif
                push_virtual_operation( shutdown_witness_operation( w.owner ) );
            }
          }
#ifdef USE_ALTERNATE_CHAIN_ID
          if( configuration_data.get_generate_missed_block_operations() )
#endif
            push_virtual_operation( producer_missed_operation( w.owner ) );
        } );
      }
    }
  }

  // dynamic global properties updating
  modify( _dgp, [&]( dynamic_global_property_object& dgp )
  {
    // This is constant time assuming 100% participation. It is O(B) otherwise (B = Num blocks between update)
    for( uint32_t i = 0; i < missed_blocks + 1; i++ )
    {
      dgp.participation_count -= fc::uint128_high_bits(dgp.recent_slots_filled) & 0x8000000000000000ULL ? 1 : 0;
      dgp.recent_slots_filled = ( dgp.recent_slots_filled << 1 ) + ( i == 0 ? 1 : 0 );
      dgp.participation_count += ( i == 0 ? 1 : 0 );
    }

    dgp.head_block_number = b.block_num();
    // Following FC_ASSERT should never fail, as _currently_processing_block_id is always set by caller
    FC_ASSERT( _currently_processing_block_id.valid() );
    dgp.head_block_id = *_currently_processing_block_id;
    dgp.time = b.timestamp;
    dgp.current_aslot += missed_blocks+1;
  } );

  if( !(get_node_properties().skip_flags & skip_undo_history_check) )
  {
    HIVE_ASSERT( _dgp.head_block_number - get_last_irreversible_block_num() < HIVE_MAX_UNDO_HISTORY, undo_database_exception,
                 "The database does not have enough undo history to support a blockchain with so many missed blocks. "
                 "Please add a checkpoint if you would like to continue applying blocks beyond this point.",
                 ("last_irreversible_block_num", get_last_irreversible_block_num())("head", _dgp.head_block_number)
                 ("max_undo", HIVE_MAX_UNDO_HISTORY) );
  }
} FC_CAPTURE_AND_RETHROW() }

uint16_t database::calculate_HBD_percent()
{
  auto median_price = get_feed_history().current_median_history;
  if( median_price.is_null() )
    return 0;

  const auto& dgpo = get_dynamic_global_properties();
  auto hbd_supply = dgpo.get_current_hbd_supply();
  auto virtual_supply = dgpo.virtual_supply;
  if( has_hardfork( HIVE_HARDFORK_1_24 ) )
  {
    // Removing the hbd in the treasury from the debt ratio calculations
    hbd_supply -= get_treasury().get_hbd_balance();
    if( hbd_supply.amount < 0 )
      hbd_supply = asset( 0, HBD_SYMBOL );
    virtual_supply = hbd_supply * median_price + dgpo.get_current_supply();
  }

  auto hbd_as_hive = fc::uint128_t( ( hbd_supply * median_price ).amount.value );
  hbd_as_hive *= HIVE_100_PERCENT;
  if( has_hardfork( HIVE_HARDFORK_0_21 ) )
    hbd_as_hive += virtual_supply.amount.value / 2; // added rounding
  return uint16_t( fc::uint128_to_uint64( hbd_as_hive / virtual_supply.amount.value ) );
}

void database::update_virtual_supply()
{ try {
  modify( get_dynamic_global_properties(), [&]( dynamic_global_property_object& dgp )
  {
    auto median_price = get_feed_history().current_median_history;
    dgp.virtual_supply = dgp.current_supply
      + ( median_price.is_null() ? asset( 0, HIVE_SYMBOL ) : dgp.get_current_hbd_supply() * median_price );

    if( !median_price.is_null() && has_hardfork( HIVE_HARDFORK_0_14__230 ) )
    {
      uint16_t percent_hbd = calculate_HBD_percent();

      if( percent_hbd >= dgp.hbd_stop_percent )
        dgp.hbd_print_rate = 0;
      else if( percent_hbd <= dgp.hbd_start_percent )
        dgp.hbd_print_rate = HIVE_100_PERCENT;
      else
        dgp.hbd_print_rate = ( ( dgp.hbd_stop_percent - percent_hbd ) * HIVE_100_PERCENT ) / ( dgp.hbd_stop_percent - dgp.hbd_start_percent );
    }
  });
} FC_CAPTURE_AND_RETHROW() }

void database::update_signing_witness(const witness_object& signing_witness, const signed_block& new_block)
{ try {
  const dynamic_global_property_object& dpo = get_dynamic_global_properties();
  uint64_t new_block_aslot = dpo.current_aslot + get_slot_at_time( new_block.timestamp );

  modify( signing_witness, [&]( witness_object& _wit )
  {
    _wit.last_aslot = new_block_aslot;
    _wit.last_confirmed_block_num = new_block.block_num();
  } );
} FC_CAPTURE_AND_RETHROW() }

const witness_schedule_object& database::get_witness_schedule_object_for_irreversibility() const
{
  if (has_hardfork(HIVE_HARDFORK_1_26_FUTURE_WITNESS_SCHEDULE))
    return get_future_witness_schedule_object();
  else
    return get_witness_schedule_object();
}

void database::process_fast_confirm_transaction(const std::shared_ptr<full_transaction_type>& full_transaction)
{ try {
  FC_ASSERT(has_hardfork(HIVE_HARDFORK_1_26_FAST_CONFIRMATION), "Fast confirmation transactions not valid until HF26");
  // fast-confirm transactions are processed outside of the normal transaction processing flow,
  // so we need to explicitly call validation here.
  // Skip the tapos check for these transactions -- a witness could be on a different fork from
  // ours (so their tapos would reference a block not in our chain), but we still want to know.
  // If we have that block in our fork database and a supermajority of witnesses approve it, we
  // want to try to switch to it.
  validate_transaction(full_transaction, skip_tapos_check);

  signed_transaction trx = full_transaction->get_transaction();

  const witness_block_approve_operation& block_approve_op = trx.operations.front().get<witness_block_approve_operation>();
  // dlog("Processing fast-confirm transaction from witness ${witness}", ("witness", block_approve_op.witness));

  const witness_schedule_object& wso_for_irreversibility = get_witness_schedule_object_for_irreversibility();
  bool witness_is_scheduled = std::any_of(wso_for_irreversibility.current_shuffled_witnesses.begin(),
                                          wso_for_irreversibility.current_shuffled_witnesses.begin() + wso_for_irreversibility.num_scheduled_witnesses,
                                          [&](const account_name_type& witness_account_name) {
                                            return block_approve_op.witness == get_witness(witness_account_name).owner;
                                          });
  if (!witness_is_scheduled)
  {
    ilog("Received a fast-confirm from witness ${witness} who is not on the schedule, ignoring", ("witness", block_approve_op.witness));
    return;
  }

  const uint32_t new_approved_block_number = block_header::num_from_id( block_approve_op.block_id );
  if (auto iter = _my->_last_fast_approved_block_by_witness.find(block_approve_op.witness);
      iter != _my->_last_fast_approved_block_by_witness.end())
  {
    const uint32_t previous_approved_block_number = block_header::num_from_id(iter->second);
    if (new_approved_block_number <= previous_approved_block_number)
    {
      ilog("Received a fast-confirm from witness ${witness} for block #${new_approved_block_number}, "
           "but we already have a fast-confirm for a block #${previous_approved_block_number} which is at least as new, ignoring",
           ("witness", block_approve_op.witness)
           (previous_approved_block_number)(new_approved_block_number));
      return;
    }
  }
  _my->_last_fast_approved_block_by_witness[block_approve_op.witness] = block_approve_op.block_id;
  dlog("Accepted fast-confirm from witness ${witness} for block #${new_approved_block_number} (${id})", ("witness", block_approve_op.witness)
       (new_approved_block_number)("id", block_approve_op.block_id));
  // ddump((_my->_last_fast_approved_block_by_witness));

  uint32_t old_last_irreversible_block = update_last_irreversible_block(false);
  migrate_irreversible_state(old_last_irreversible_block);
} FC_CAPTURE_AND_RETHROW() }

uint32_t database::update_last_irreversible_block(const bool currently_applying_a_block)
{ try {
  uint32_t old_last_irreversible = get_last_irreversible_block_num();
  /**
    * Prior to voting taking over, we must be more conservative...
    */
  if (head_block_num() < HIVE_START_MINER_VOTING_BLOCK)
  {
    if (head_block_num() > HIVE_MAX_WITNESSES)
    {
      uint32_t new_last_irreversible_block_num = head_block_num() - HIVE_MAX_WITNESSES;

      if (new_last_irreversible_block_num > get_last_irreversible_block_num())
        set_last_irreversible_block_num(new_last_irreversible_block_num);
    }
    return old_last_irreversible;
  }

  uint32_t head_block_number = head_block_num();
  if (old_last_irreversible == head_block_number)
  {
    //dlog("Head-block is already irreversible, nothing to do");
    return old_last_irreversible;
  }
  // ddump((head_block_number)(old_last_irreversible));

  // we'll need to know who the current scheduled witnesses are, because their opinions are
  // the only ones that matter
  std::vector<const witness_object*> scheduled_witness_objects;
  const witness_schedule_object& wso_for_irreversibility = get_witness_schedule_object_for_irreversibility();
  std::transform(wso_for_irreversibility.current_shuffled_witnesses.begin(),
                 wso_for_irreversibility.current_shuffled_witnesses.begin() + wso_for_irreversibility.num_scheduled_witnesses,
                 std::back_inserter(scheduled_witness_objects),
                 [&](const account_name_type& witness_account_name) {
                   return &get_witness(witness_account_name);
                 });

  const size_t offset = (HIVE_100_PERCENT - HIVE_IRREVERSIBLE_THRESHOLD) * scheduled_witness_objects.size() / HIVE_100_PERCENT;

  if (get_node_properties().skip_flags & skip_block_log)
  {
    // if we're doing a replay where we're not pushing blocks to the fork_db, use the old algorithm to compute last_irreversible
    // because the new algorithm requires the fork_db to have the blocks
    std::nth_element(scheduled_witness_objects.begin(), scheduled_witness_objects.begin() + offset, scheduled_witness_objects.end(),
                     [](const witness_object* a, const witness_object* b) { return a->last_confirmed_block_num < b->last_confirmed_block_num; });
    uint32_t new_last_irreversible_block_num = scheduled_witness_objects[offset]->last_confirmed_block_num;
    if (new_last_irreversible_block_num > old_last_irreversible)
      set_last_irreversible_block_num(new_last_irreversible_block_num);
    return old_last_irreversible;
  }

  // the forkdb is active, so we can use the new algorithm.  Start by getting a list of witnesses by name
  std::set<account_name_type> scheduled_witnesses;
  std::transform(scheduled_witness_objects.begin(), scheduled_witness_objects.end(),
                 std::inserter(scheduled_witnesses, scheduled_witnesses.end()),
                 [&](const witness_object* witness_obj) {
                   return witness_obj->owner;
                 });
  const unsigned witnesses_required_for_irreversiblity = scheduled_witnesses.size() - offset;

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
  const std::map<account_name_type, block_id_type> last_block_generated_by_witness = _fork_db.get_last_block_generated_by_each_witness();
  std::map<block_id_type, uint32_t> number_of_approvals_by_block_id;
  for (const witness_object* witness_obj : scheduled_witness_objects)
  {
    const auto fast_approval_iter = _my->_last_fast_approved_block_by_witness.find(witness_obj->owner);
    const auto last_block_iter = last_block_generated_by_witness.find(witness_obj->owner);
    std::optional<block_id_type> best_block_id_for_this_witness;
    if (fast_approval_iter != _my->_last_fast_approved_block_by_witness.end())
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
    return old_last_irreversible;
  }

  // dlog("Found a new last irreversible block: ${new_last_irreversible_block_num}", ("new_last_irreversible_block_num", new_last_irreversible_block->get_block_num()));
  const item_ptr main_branch_block = _fork_db.fetch_block_on_main_branch_by_number(new_last_irreversible_block->get_block_num());
  if (new_last_irreversible_block != main_branch_block)
  {
    // we found a new last irreversible block on another fork
    if (new_head_block->get_block_num() < head_block_num())
    {
      // we need to switch to the fork containing our new last irreversible block candidate, but
      // that fork is shorter than our current head block, so our head block number would decrease
      // as a result.  There may be other places in the code that assume the head block number
      // never decreases.  Since we're not sure, just postpone the fork switch until we get
      // enough blocks on the candidate fork to at least equal our current head block
      return old_last_irreversible;
    }

    if (currently_applying_a_block)
    {
      // we shouldn't ever trigger a fork switch when this function is called near the end of
      // _apply_block().  If it ever happens, we'll just delay the fork switch until the next
      // time we get a new block or a fast-confirm operation.
      return old_last_irreversible;
    }

    try
    {
      detail::without_pending_transactions(*this, existing_block_flow_control(new_head_block->full_block), std::move(_pending_tx), [&]() {
        try
        {
          dlog("calling switch_forks() from update_last_irreversible_block()");
          switch_forks(new_head_block);
          // when we switch forks, irreversibility will be re-evaluated at the end of every block pushed
          // on the new fork, so we don't need to mark the block as irreversible here
          return old_last_irreversible;
        }
        FC_CAPTURE_AND_RETHROW()
      });
    }
    catch (const fc::exception&)
    {
      return old_last_irreversible;
    }
  }

  // don't set last_irreversible_block to a block not yet processed (could happen during a fork switch)
  set_last_irreversible_block_num(std::min(new_last_irreversible_block->get_block_num(), head_block_num()));

  // clean up any fast-confirms that are no longer relevant to reversible blocks
  for (auto iter = _my->_last_fast_approved_block_by_witness.begin(); iter != _my->_last_fast_approved_block_by_witness.end();)
    if (block_header::num_from_id(iter->second) <= get_last_irreversible_block_num())
      iter = _my->_last_fast_approved_block_by_witness.erase(iter);
    else
      ++iter;

  return old_last_irreversible;
} FC_CAPTURE_AND_RETHROW() }

void database::migrate_irreversible_state(uint32_t old_last_irreversible)
{
  // This method should happen atomically. We cannot prevent unclean shutdown in the middle
  // of the call, but all side effects happen at the end to minize the chance that state
  // invariants will be violated.
  try
  {
    const dynamic_global_property_object& dpo = get_dynamic_global_properties();

    const auto fork_head = _fork_db.head();
    if (fork_head)
      FC_ASSERT(fork_head->get_block_num() == dpo.head_block_number, "Fork Head Block Number: ${fork_head}, Chain Head Block Number: ${chain_head}",
                ("fork_head", fork_head->get_block_num())("chain_head", dpo.head_block_number));

    if( !( get_node_properties().skip_flags & skip_block_log ) )
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
          _block_log.append( block_itr->get()->full_block );

        _block_log.flush();
      }
    }

    // This deletes blocks from the fork db
    //edump((dpo.head_block_number)(get_last_irreversible_block_num()));
    _fork_db.set_max_size( dpo.head_block_number - get_last_irreversible_block_num() + 1 );

    // This deletes undo state
    commit( get_last_irreversible_block_num() );

    if (old_last_irreversible < get_last_irreversible_block_num())
    {
      //ilog("Updating last irreversible block to: ${b}. Old last irreversible was: ${ob}.",
      //  ("b", get_last_irreversible_block_num())("ob", old_last_irreversible));

      for (uint32_t i = old_last_irreversible + 1; i <= get_last_irreversible_block_num(); ++i)
        notify_irreversible_block(i);
    }

  }
  FC_CAPTURE_CALL_LOG_AND_RETHROW( [](){
                                          elog( "An error occured during migrating an irreversible state. The node will be closed." );
                                          appbase::app().generate_interrupt_request();
                                       }, (old_last_irreversible) )
}


bool database::apply_order( const limit_order_object& new_order_object )
{
  auto order_id = new_order_object.get_id();

  const auto& limit_price_idx = get_index<limit_order_index>().indices().get<by_price>();

  auto max_price = ~new_order_object.sell_price;
  auto limit_itr = limit_price_idx.lower_bound(max_price.max());
  auto limit_end = limit_price_idx.upper_bound(max_price);

  bool finished = false;
  while( !finished && limit_itr != limit_end )
  {
    auto old_limit_itr = limit_itr;
    ++limit_itr;
    // match returns 2 when only the old order was fully filled. In this case, we keep matching; otherwise, we stop.
    finished = ( match(new_order_object, *old_limit_itr, old_limit_itr->sell_price) & 0x1 );
  }

  return find< limit_order_object >( order_id ) == nullptr;
}

int database::match( const limit_order_object& new_order, const limit_order_object& old_order, const price& match_price )
{
  bool has_hf_20__1815 = has_hardfork( HIVE_HARDFORK_0_20__1815 );

FC_TODO( " Remove if(), do assert unconditionally after HF20 occurs" )
  if( has_hf_20__1815 )
  {
    HIVE_ASSERT( new_order.sell_price.quote.symbol == old_order.sell_price.base.symbol,
      order_match_exception, "error matching orders: ${new_order} ${old_order} ${match_price}",
      ("new_order", new_order)("old_order", old_order)("match_price", match_price) );
    HIVE_ASSERT( new_order.sell_price.base.symbol  == old_order.sell_price.quote.symbol,
      order_match_exception, "error matching orders: ${new_order} ${old_order} ${match_price}",
      ("new_order", new_order)("old_order", old_order)("match_price", match_price) );
    HIVE_ASSERT( new_order.for_sale > 0 && old_order.for_sale > 0,
      order_match_exception, "error matching orders: ${new_order} ${old_order} ${match_price}",
      ("new_order", new_order)("old_order", old_order)("match_price", match_price) );
    HIVE_ASSERT( match_price.quote.symbol == new_order.sell_price.base.symbol,
      order_match_exception, "error matching orders: ${new_order} ${old_order} ${match_price}",
      ("new_order", new_order)("old_order", old_order)("match_price", match_price) );
    HIVE_ASSERT( match_price.base.symbol == old_order.sell_price.base.symbol,
      order_match_exception, "error matching orders: ${new_order} ${old_order} ${match_price}",
      ("new_order", new_order)("old_order", old_order)("match_price", match_price) );
  }

  auto new_order_for_sale = new_order.amount_for_sale();
  auto old_order_for_sale = old_order.amount_for_sale();

  asset new_order_pays, new_order_receives, old_order_pays, old_order_receives;

  if( new_order_for_sale <= old_order_for_sale * match_price )
  {
    old_order_receives = new_order_for_sale;
    new_order_receives  = new_order_for_sale * match_price;
  }
  else
  {
    //This line once read: assert( old_order_for_sale < new_order_for_sale * match_price );
    //This assert is not always true -- see trade_amount_equals_zero in operation_tests.cpp
    //Although new_order_for_sale is greater than old_order_for_sale * match_price, old_order_for_sale == new_order_for_sale * match_price
    //Removing the assert seems to be safe -- apparently no asset is created or destroyed.
    new_order_receives = old_order_for_sale;
    old_order_receives = old_order_for_sale * match_price;
  }

  old_order_pays = new_order_receives;
  new_order_pays = old_order_receives;

FC_TODO( " Remove if(), do assert unconditionally after HF20 occurs" )
  if( has_hf_20__1815 )
  {
    HIVE_ASSERT( new_order_pays == new_order.amount_for_sale() ||
              old_order_pays == old_order.amount_for_sale(),
      order_match_exception, "error matching orders: ${new_order} ${old_order} ${match_price}",
      ("new_order", new_order)("old_order", old_order)("match_price", match_price) );
  }

  auto age = head_block_time() - old_order.created;
  if( !has_hardfork( HIVE_HARDFORK_0_12__178 ) &&
      ( (age >= HIVE_MIN_LIQUIDITY_REWARD_PERIOD_SEC && !has_hardfork( HIVE_HARDFORK_0_10__149)) ||
      (age >= HIVE_MIN_LIQUIDITY_REWARD_PERIOD_SEC_HF10 && has_hardfork( HIVE_HARDFORK_0_10__149) ) ) )
  {
    if( old_order_receives.symbol == HIVE_SYMBOL )
    {
      adjust_liquidity_reward( get_account( old_order.seller ), old_order_receives, false );
      adjust_liquidity_reward( get_account( new_order.seller ), -old_order_receives, false );
    }
    else
    {
      adjust_liquidity_reward( get_account( old_order.seller ), new_order_receives, true );
      adjust_liquidity_reward( get_account( new_order.seller ), -new_order_receives, true );
    }
  }

  push_virtual_operation( fill_order_operation( new_order.seller, new_order.orderid, new_order_pays, old_order.seller, old_order.orderid, old_order_pays ) );

  int result = 0;
  result |= fill_order( new_order, new_order_pays, new_order_receives );
  result |= fill_order( old_order, old_order_pays, old_order_receives ) << 1;

FC_TODO( " Remove if(), do assert unconditionally after HF20 occurs" )
  if( has_hf_20__1815 )
  {
    HIVE_ASSERT( result != 0,
      order_match_exception, "error matching orders: ${new_order} ${old_order} ${match_price}",
      ("new_order", new_order)("old_order", old_order)("match_price", match_price) );
  }
  return result;
}


void database::adjust_liquidity_reward( const account_object& owner, const asset& volume, bool is_hbd )
{
  const auto& ridx = get_index< liquidity_reward_balance_index >().indices().get< by_owner >();
  auto itr = ridx.find( owner.get_id() );
  if( itr != ridx.end() )
  {
    modify<liquidity_reward_balance_object>( *itr, [&]( liquidity_reward_balance_object& r )
    {
      if( head_block_time() - r.last_update >= HIVE_LIQUIDITY_TIMEOUT_SEC )
      {
        r.hbd_volume = 0;
        r.hive_volume = 0;
        r.weight = 0;
      }

      if( is_hbd )
        r.hbd_volume += volume.amount.value;
      else
        r.hive_volume += volume.amount.value;

      r.update_weight( has_hardfork( HIVE_HARDFORK_0_10__141 ) );
      r.last_update = head_block_time();
    } );
  }
  else
  {
    create<liquidity_reward_balance_object>( [&](liquidity_reward_balance_object& r )
    {
      r.owner = owner.get_id();
      if( is_hbd )
        r.hbd_volume = volume.amount.value;
      else
        r.hive_volume = volume.amount.value;

      r.update_weight( has_hardfork( HIVE_HARDFORK_0_9__141 ) );
      r.last_update = head_block_time();
    } );
  }
}


bool database::fill_order( const limit_order_object& order, const asset& pays, const asset& receives )
{
  try
  {
    HIVE_ASSERT( order.amount_for_sale().symbol == pays.symbol,
      order_fill_exception, "error filling orders: ${order} ${pays} ${receives}",
      ("order", order)("pays", pays)("receives", receives) );
    HIVE_ASSERT( pays.symbol != receives.symbol,
      order_fill_exception, "error filling orders: ${order} ${pays} ${receives}",
      ("order", order)("pays", pays)("receives", receives) );

    adjust_balance( order.seller, receives );

    if( pays == order.amount_for_sale() )
    {
      remove( order );
      return true;
    }
    else
    {
FC_TODO( " Remove if(), do assert unconditionally after HF20 occurs" )
      if( has_hardfork( HIVE_HARDFORK_0_20__1815 ) )
      {
        HIVE_ASSERT( pays < order.amount_for_sale(),
          order_fill_exception, "error filling orders: ${order} ${pays} ${receives}",
          ("order", order)("pays", pays)("receives", receives) );
      }

      modify( order, [&]( limit_order_object& b )
      {
        b.for_sale -= pays.amount;
      } );
      /**
        *  There are times when the AMOUNT_FOR_SALE * SALE_PRICE == 0 which means that we
        *  have hit the limit where the seller is asking for nothing in return.  When this
        *  happens we must refund any balance back to the seller, it is too small to be
        *  sold at the sale price.
        */
      if( order.amount_to_receive().amount == 0 )
      {
        cancel_order(order);
        return true;
      }
      return false;
    }
  }
  FC_CAPTURE_AND_RETHROW( (order)(pays)(receives) )
}

void database::cancel_order( const limit_order_object& order, bool suppress_vop )
{
  auto amount_back = order.amount_for_sale();

  adjust_balance( order.seller, amount_back );
  if( !suppress_vop )
    push_virtual_operation(limit_order_cancelled_operation(order.seller, order.orderid, amount_back));

  remove(order);
}


void database::clear_expired_transactions()
{
  //Look for expired transactions in the deduplication list, and remove them.
  //Transactions must have expired by at least two forking windows in order to be removed.
  auto& transaction_idx = get_index< transaction_index >();
  const auto& dedupe_index = transaction_idx.indices().get< by_expiration >();
  while( ( !dedupe_index.empty() ) && ( head_block_time() > dedupe_index.begin()->expiration ) )
    remove( *dedupe_index.begin() );
}

void database::clear_expired_orders()
{
  auto now = head_block_time();
  const auto& orders_by_exp = get_index<limit_order_index>().indices().get<by_expiration>();
  auto itr = orders_by_exp.begin();
  while( itr != orders_by_exp.end() && itr->expiration < now )
  {
    cancel_order( *itr );
    itr = orders_by_exp.begin();
  }
}

void database::clear_expired_delegations()
{
  auto now = head_block_time();
  const auto& delegations_by_exp = get_index< vesting_delegation_expiration_index, by_expiration >();
  auto itr = delegations_by_exp.begin();
  const auto& gpo = get_dynamic_global_properties();

  while( itr != delegations_by_exp.end() && itr->get_expiration_time() < now )
  {
    auto& delegator = get_account( itr->get_delegator() );
    operation vop = return_vesting_delegation_operation( delegator.get_name(), itr->get_vesting() );
    try{
    pre_push_virtual_operation( vop );

    modify( delegator, [&]( account_object& a )
    {
      if( has_hardfork( HIVE_HARDFORK_0_20__2539 ) )
      {
        util::update_manabar( gpo, a, itr->get_vesting().amount.value );
      }

      a.delegated_vesting_shares -= itr->get_vesting();
    });

    post_push_virtual_operation( vop );

    remove( *itr );
    itr = delegations_by_exp.begin();
  } FC_CAPTURE_AND_RETHROW( (vop) ) }
}
#ifdef HIVE_ENABLE_SMT
template< typename smt_balance_object_type, typename modifier_type >
void database::adjust_smt_balance( const account_object& owner, const asset& delta, modifier_type&& modifier )
{
  asset_symbol_type liquid_symbol = delta.symbol.is_vesting() ? delta.symbol.get_paired_symbol() : delta.symbol;
  const smt_balance_object_type* bo = find< smt_balance_object_type, by_owner_liquid_symbol >( boost::make_tuple( owner.get_id(), liquid_symbol ) );
  // Note that SMT related code, being post-20-hf needs no hf-guard to do balance checks.
  if( bo == nullptr )
  {
    // No balance object related to the SMT means '0' balance. Check delta to avoid creation of negative balance.
    FC_ASSERT( delta.amount.value >= 0, "Insufficient SMT ${smt} funds", ("smt", delta.symbol) );
    // No need to create object with '0' balance (see comment above).
    if( delta.amount.value == 0 )
      return;

    auto& new_balance_object = create< smt_balance_object_type >( owner, liquid_symbol );
    bo = &new_balance_object;
  }

  modify( *bo, std::forward<modifier_type>( modifier ) );
  if( bo->is_empty() )
  {
    // Zero balance is the same as non object balance at all.
    // Remove balance object if both liquid and vesting balances are zero.
    remove( *bo );
  }
}
#endif

void database::modify_balance( const account_object& a, const asset& delta, bool check_balance )
{
  const bool trace_balance_change = false; //a.get_name() == "X";
  std::string op_context;

  if(trace_balance_change)
  {
    if(_current_applied_operation_info != nullptr)
      op_context = fc::json::to_string(_current_applied_operation_info->op);
    else
      op_context = "No operation context";
  }

  modify( a, [&]( account_object& acnt )
  {
    switch( delta.symbol.asset_num )
    {
      case HIVE_ASSET_NUM_HIVE:
      {
        auto b = acnt.balance;
        acnt.balance += delta;
        if(trace_balance_change)
          ilog("${a} HIVE balance changed to ${nb} (previous: ${b} ) at block: ${block}. Operation context: ${c}", ("a", a.get_name())("b", b.amount)("nb", acnt.balance.amount)("block", _current_block_num)("c", op_context));

        if( check_balance )
        {
          FC_ASSERT( acnt.get_balance().amount.value >= 0, "Insufficient HIVE funds" );
        }
        break;
      }
      case HIVE_ASSET_NUM_HBD:
      {
        /// Starting from HF 25 HBD interest will be paid only from saving balance.
        if( has_hardfork(HIVE_HARDFORK_1_25) == false && a.hbd_seconds_last_update != head_block_time() )
        {
          acnt.hbd_seconds += fc::uint128_t(a.get_hbd_balance().amount.value) * (head_block_time() - a.hbd_seconds_last_update).to_seconds();
          acnt.hbd_seconds_last_update = head_block_time();
          if( acnt.hbd_seconds > 0 &&
              (acnt.hbd_seconds_last_update - acnt.hbd_last_interest_payment).to_seconds() > HIVE_HBD_INTEREST_COMPOUND_INTERVAL_SEC )
          {
            auto interest = acnt.hbd_seconds / HIVE_SECONDS_PER_YEAR;
            interest *= get_dynamic_global_properties().get_hbd_interest_rate();
            interest /= HIVE_100_PERCENT;
            asset interest_paid(fc::uint128_to_uint64(interest), HBD_SYMBOL);
            acnt.hbd_balance += interest_paid;
            acnt.hbd_seconds = 0;
            acnt.hbd_last_interest_payment = head_block_time();

            if(interest > 0)
              push_virtual_operation( interest_operation( a.get_name(), interest_paid, true ) );

            modify( get_dynamic_global_properties(), [&]( dynamic_global_property_object& props)
            {
              props.current_hbd_supply += interest_paid;
              props.virtual_supply += interest_paid * get_feed_history().current_median_history;
            } );
          }
        }

        auto b = acnt.hbd_balance;
        acnt.hbd_balance += delta;

        if(trace_balance_change)
          ilog("${a} HBD balance changed to ${nb} (previous: ${b} ) at block: ${block}. Operation context: ${c}", ("a", a.get_name())("b", b.amount)("nb", acnt.hbd_balance.amount)("block", _current_block_num)("c", op_context));

        if( check_balance )
        {
          FC_ASSERT( acnt.get_hbd_balance().amount.value >= 0, "Insufficient HBD funds" );
        }
        break;
      }
      case HIVE_ASSET_NUM_VESTS:
        acnt.vesting_shares += delta;
        if( check_balance )
        {
          FC_ASSERT( acnt.get_vesting().amount.value >= 0, "Insufficient VESTS funds" );
        }
        break;
      default:
        FC_ASSERT( false, "invalid symbol" );
    }
  } );
}

void database::modify_reward_balance( const account_object& a, const asset& value_delta, const asset& share_delta, bool check_balance )
{
  modify( a, [&]( account_object& acnt )
  {
    switch( value_delta.symbol.asset_num )
    {
      case HIVE_ASSET_NUM_HIVE:
        if( share_delta.amount.value == 0 )
        {
          acnt.reward_hive_balance += value_delta;
          if( check_balance )
          {
            FC_ASSERT( acnt.get_rewards().amount.value >= 0, "Insufficient reward HIVE funds" );
          }
        }
        else
        {
          acnt.reward_vesting_hive += value_delta;
          acnt.reward_vesting_balance += share_delta;
          if( check_balance )
          {
            FC_ASSERT( acnt.get_vest_rewards().amount.value >= 0, "Insufficient reward VESTS funds" );
          }
        }
        break;
      case HIVE_ASSET_NUM_HBD:
        FC_ASSERT( share_delta.amount.value == 0 );
        acnt.reward_hbd_balance += value_delta;
        if( check_balance )
        {
          FC_ASSERT( acnt.get_hbd_rewards().amount.value >= 0, "Insufficient reward HBD funds" );
        }
        break;
      default:
        FC_ASSERT( false, "invalid symbol" );
    }
  });
}

void database::adjust_balance( const account_object& a, const asset& delta )
{
  if ( delta.amount < 0 )
  {
    asset available = get_balance( a, delta.symbol );
    FC_ASSERT( available >= -delta,
      "Account ${acc} does not have sufficient funds for balance adjustment. Required: ${r}, Available: ${a}",
        ("acc", a.get_name())("r", delta)("a", available) );
  }

  bool check_balance = has_hardfork( HIVE_HARDFORK_0_20__1811 );

#ifdef HIVE_ENABLE_SMT
  if( delta.symbol.space() == asset_symbol_type::smt_nai_space )
  {
    // No account object modification for SMT balance, hence separate handling here.
    // Note that SMT related code, being post-20-hf needs no hf-guard to do balance checks.
    adjust_smt_balance< account_regular_balance_object >( a, delta, [&]( account_regular_balance_object& bo )
    {
      if( delta.symbol.is_vesting() )
      {
        bo.vesting += delta;
        // Check result to avoid negative balance storing.
        FC_ASSERT( bo.vesting.amount >= 0, "Insufficient SMT ${smt} funds", ( "smt", delta.symbol ) ); //ABW: redundant with check at start
      }
      else
      {
        bo.liquid += delta;
        // Check result to avoid negative balance storing.
        FC_ASSERT( bo.liquid.amount >= 0, "Insufficient SMT ${smt} funds", ( "smt", delta.symbol ) ); //ABW: redundant with check at start
      }
    } );
  }
  else
#endif
  {
    modify_balance( a, delta, check_balance );
  }
}

void database::adjust_savings_balance( const account_object& a, const asset& delta )
{
  bool check_balance = has_hardfork( HIVE_HARDFORK_0_20__1811 );

  modify( a, [&]( account_object& acnt )
  {
    switch( delta.symbol.asset_num )
    {
      case HIVE_ASSET_NUM_HIVE:
        acnt.savings_balance += delta;
        if( check_balance )
        {
          FC_ASSERT( acnt.get_savings().amount.value >= 0, "Insufficient savings HIVE funds" );
        }
        break;
      case HIVE_ASSET_NUM_HBD:
        if( a.savings_hbd_seconds_last_update != head_block_time() )
        {
          acnt.savings_hbd_seconds += fc::uint128_t(a.get_hbd_savings().amount.value) * (head_block_time() - a.savings_hbd_seconds_last_update).to_seconds();
          acnt.savings_hbd_seconds_last_update = head_block_time();

          if( acnt.savings_hbd_seconds > 0 &&
              (acnt.savings_hbd_seconds_last_update - acnt.savings_hbd_last_interest_payment).to_seconds() > HIVE_HBD_INTEREST_COMPOUND_INTERVAL_SEC )
          {
            auto interest = acnt.savings_hbd_seconds / HIVE_SECONDS_PER_YEAR;
            interest *= get_dynamic_global_properties().get_hbd_interest_rate();
            interest /= HIVE_100_PERCENT;
            asset interest_paid(fc::uint128_to_uint64(interest), HBD_SYMBOL);
            acnt.savings_hbd_balance += interest_paid;
            acnt.savings_hbd_seconds = 0;
            acnt.savings_hbd_last_interest_payment = head_block_time();

            if(interest > 0)
              push_virtual_operation( interest_operation( a.get_name(), interest_paid, false ) );

            modify( get_dynamic_global_properties(), [&]( dynamic_global_property_object& props)
            {
              props.current_hbd_supply += interest_paid;
              props.virtual_supply += interest_paid * get_feed_history().current_median_history;
            } );
          }
        }
        acnt.savings_hbd_balance += delta;
        if( check_balance )
        {
          FC_ASSERT( acnt.get_hbd_savings().amount.value >= 0, "Insufficient savings HBD funds" );
        }
        break;
      default:
        FC_ASSERT( !"invalid symbol" );
    }
  } );
}

void database::adjust_reward_balance( const account_object& a, const asset& value_delta,
                          const asset& share_delta /*= asset(0,VESTS_SYMBOL)*/ )
{
  bool check_balance = has_hardfork( HIVE_HARDFORK_0_20__1811 );
  FC_ASSERT( value_delta.symbol.is_vesting() == false && share_delta.symbol.is_vesting() );

#ifdef HIVE_ENABLE_SMT
  // No account object modification for SMT balance, hence separate handling here.
  // Note that SMT related code, being post-20-hf needs no hf-guard to do balance checks.
  if( value_delta.symbol.space() == asset_symbol_type::smt_nai_space )
  {
    adjust_smt_balance< account_rewards_balance_object >( a, value_delta, [&]( account_rewards_balance_object& bo )
    {
      if( share_delta.amount.value != 0 )
      {
        bo.pending_vesting_value += value_delta;
        bo.pending_vesting_shares += share_delta;
        // Check result to avoid negative balance storing.
        FC_ASSERT( bo.pending_vesting_value.amount >= 0, "Insufficient SMT ${smt} funds", ( "smt", value_delta.symbol ) );
      }
      else
      {
        bo.pending_liquid += value_delta;
        // Check result to avoid negative balance storing.
        FC_ASSERT( bo.pending_liquid.amount >= 0, "Insufficient SMT ${smt} funds", ( "smt", value_delta.symbol ) );
      }
    } );
  }
  else
#endif
  {
    modify_reward_balance( a, value_delta, share_delta, check_balance );
  }
}

void database::adjust_supply( const asset& delta, bool adjust_vesting )
{
#ifdef HIVE_ENABLE_SMT
  if( delta.symbol.space() == asset_symbol_type::smt_nai_space )
  {
    const auto& smt = get< smt_token_object, by_symbol >( delta.symbol );
    auto smt_new_supply = smt.current_supply + delta.amount;
    FC_ASSERT( smt_new_supply >= 0 );
    modify( smt, [smt_new_supply]( smt_token_object& smt )
    {
      smt.current_supply = smt_new_supply;
    });
    return;
  }
#endif

  bool check_supply = has_hardfork( HIVE_HARDFORK_0_20__1811 );

  const auto& props = get_dynamic_global_properties();
  if( props.head_block_number < HIVE_BLOCKS_PER_DAY*7 )
    adjust_vesting = false;

  modify( props, [&]( dynamic_global_property_object& props )
  {
    switch( delta.symbol.asset_num )
    {
      case HIVE_ASSET_NUM_HIVE:
      {
        asset new_vesting( (adjust_vesting && delta.amount > 0) ? delta.amount * 9 : 0, HIVE_SYMBOL );
        props.current_supply += delta + new_vesting;
        props.virtual_supply += delta + new_vesting;
        props.total_vesting_fund_hive += new_vesting;
        if( check_supply )
        {
          FC_ASSERT( props.current_supply.amount.value >= 0 );
        }
        break;
      }
      case HIVE_ASSET_NUM_HBD:
        props.current_hbd_supply += delta;
        props.virtual_supply = props.get_current_hbd_supply() * get_feed_history().current_median_history + props.current_supply;
        if( check_supply )
        {
          FC_ASSERT( props.get_current_hbd_supply().amount.value >= 0 );
        }
        break;
      default:
        FC_ASSERT( false, "invalid symbol" );
    }
  } );
}


asset database::get_balance( const account_object& a, asset_symbol_type symbol )const
{
  switch( symbol.asset_num )
  {
    case HIVE_ASSET_NUM_HIVE:
      return a.get_balance();
    case HIVE_ASSET_NUM_HBD:
      return a.get_hbd_balance();
    default:
    {
#ifdef HIVE_ENABLE_SMT
      FC_ASSERT( symbol.space() == asset_symbol_type::smt_nai_space, "Invalid symbol: ${s}", ("s", symbol) );
      auto key = boost::make_tuple( a.get_id(), symbol.is_vesting() ? symbol.get_paired_symbol() : symbol );
      const account_regular_balance_object* arbo = find< account_regular_balance_object, by_owner_liquid_symbol >( key );
      if( arbo == nullptr )
      {
        return asset(0, symbol);
      }
      else
      {
        return symbol.is_vesting() ? arbo->vesting : arbo->liquid;
      }
#else
      FC_ASSERT( false, "Invalid symbol: ${s}", ("s", symbol) );
#endif
    }
  }
}

asset database::get_savings_balance( const account_object& a, asset_symbol_type symbol )const
{
  switch( symbol.asset_num )
  {
    case HIVE_ASSET_NUM_HIVE:
      return a.get_savings();
    case HIVE_ASSET_NUM_HBD:
      return a.get_hbd_savings();
    default: // Note no savings balance for SMT per comments in issue 1682.
      FC_ASSERT( !"invalid symbol" );
  }
}

void database::generate_required_actions()
{

}

void database::generate_optional_actions()
{
  static const generate_optional_actions_notification note;
  HIVE_TRY_NOTIFY( _generate_optional_actions_signal, note );
}

void database::init_hardforks()
{
  _hardfork_versions.times[ 0 ] = fc::time_point_sec( HIVE_GENESIS_TIME );
  _hardfork_versions.versions[ 0 ] = hardfork_version( 0, 0 );
  FC_ASSERT( HIVE_HARDFORK_0_1 == 1, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_1 ] = fc::time_point_sec( HIVE_HARDFORK_0_1_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_1 ] = HIVE_HARDFORK_0_1_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_2 == 2, "Invlaid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_2 ] = fc::time_point_sec( HIVE_HARDFORK_0_2_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_2 ] = HIVE_HARDFORK_0_2_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_3 == 3, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_3 ] = fc::time_point_sec( HIVE_HARDFORK_0_3_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_3 ] = HIVE_HARDFORK_0_3_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_4 == 4, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_4 ] = fc::time_point_sec( HIVE_HARDFORK_0_4_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_4 ] = HIVE_HARDFORK_0_4_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_5 == 5, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_5 ] = fc::time_point_sec( HIVE_HARDFORK_0_5_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_5 ] = HIVE_HARDFORK_0_5_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_6 == 6, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_6 ] = fc::time_point_sec( HIVE_HARDFORK_0_6_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_6 ] = HIVE_HARDFORK_0_6_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_7 == 7, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_7 ] = fc::time_point_sec( HIVE_HARDFORK_0_7_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_7 ] = HIVE_HARDFORK_0_7_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_8 == 8, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_8 ] = fc::time_point_sec( HIVE_HARDFORK_0_8_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_8 ] = HIVE_HARDFORK_0_8_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_9 == 9, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_9 ] = fc::time_point_sec( HIVE_HARDFORK_0_9_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_9 ] = HIVE_HARDFORK_0_9_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_10 == 10, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_10 ] = fc::time_point_sec( HIVE_HARDFORK_0_10_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_10 ] = HIVE_HARDFORK_0_10_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_11 == 11, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_11 ] = fc::time_point_sec( HIVE_HARDFORK_0_11_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_11 ] = HIVE_HARDFORK_0_11_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_12 == 12, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_12 ] = fc::time_point_sec( HIVE_HARDFORK_0_12_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_12 ] = HIVE_HARDFORK_0_12_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_13 == 13, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_13 ] = fc::time_point_sec( HIVE_HARDFORK_0_13_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_13 ] = HIVE_HARDFORK_0_13_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_14 == 14, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_14 ] = fc::time_point_sec( HIVE_HARDFORK_0_14_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_14 ] = HIVE_HARDFORK_0_14_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_15 == 15, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_15 ] = fc::time_point_sec( HIVE_HARDFORK_0_15_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_15 ] = HIVE_HARDFORK_0_15_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_16 == 16, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_16 ] = fc::time_point_sec( HIVE_HARDFORK_0_16_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_16 ] = HIVE_HARDFORK_0_16_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_17 == 17, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_17 ] = fc::time_point_sec( HIVE_HARDFORK_0_17_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_17 ] = HIVE_HARDFORK_0_17_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_18 == 18, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_18 ] = fc::time_point_sec( HIVE_HARDFORK_0_18_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_18 ] = HIVE_HARDFORK_0_18_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_19 == 19, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_19 ] = fc::time_point_sec( HIVE_HARDFORK_0_19_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_19 ] = HIVE_HARDFORK_0_19_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_20 == 20, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_20 ] = fc::time_point_sec( HIVE_HARDFORK_0_20_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_20 ] = HIVE_HARDFORK_0_20_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_21 == 21, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_21 ] = fc::time_point_sec( HIVE_HARDFORK_0_21_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_21 ] = HIVE_HARDFORK_0_21_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_22 == 22, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_22 ] = fc::time_point_sec( HIVE_HARDFORK_0_22_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_22 ] = HIVE_HARDFORK_0_22_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_23 == 23, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_23 ] = fc::time_point_sec( HIVE_HARDFORK_0_23_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_23 ] = HIVE_HARDFORK_0_23_VERSION;
  FC_ASSERT( HIVE_HARDFORK_1_24 == 24, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_1_24 ] = fc::time_point_sec( HIVE_HARDFORK_1_24_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_1_24 ] = HIVE_HARDFORK_1_24_VERSION;
  FC_ASSERT( HIVE_HARDFORK_1_25 == 25, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_1_25 ] = fc::time_point_sec( HIVE_HARDFORK_1_25_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_1_25 ] = HIVE_HARDFORK_1_25_VERSION;
  FC_ASSERT( HIVE_HARDFORK_1_26 == 26, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_1_26 ] = fc::time_point_sec( HIVE_HARDFORK_1_26_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_1_26 ] = HIVE_HARDFORK_1_26_VERSION;
  FC_ASSERT( HIVE_HARDFORK_1_27 == 27, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_1_27 ] = fc::time_point_sec( HIVE_HARDFORK_1_27_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_1_27 ] = HIVE_HARDFORK_1_27_VERSION;
#if defined(USE_ALTERNATE_CHAIN_ID)
  FC_ASSERT( HIVE_HARDFORK_1_28 == 28, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_1_28 ] = fc::time_point_sec( HIVE_HARDFORK_1_28_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_1_28 ] = HIVE_HARDFORK_1_28_VERSION;
#elif defined(IS_TEST_NET) && defined(HIVE_ENABLE_SMT)
  FC_ASSERT( HIVE_HARDFORK_1_29 == 29, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_1_29 ] = fc::time_point_sec( HIVE_HARDFORK_1_29_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_1_29 ] = HIVE_HARDFORK_1_29_VERSION;
#endif
}

void database::process_hardforks()
{
  try
  {
    // If there are upcoming hardforks and the next one is later, do nothing
    const auto& hardforks = get_hardfork_property_object();

    if( has_hardfork( HIVE_HARDFORK_0_5__54 ) )
    {
      while( _hardfork_versions.versions[ hardforks.last_hardfork ] < hardforks.next_hardfork
        && hardforks.next_hardfork_time <= head_block_time() )
      {
        if( hardforks.last_hardfork < HIVE_NUM_HARDFORKS ) {
          apply_hardfork( hardforks.last_hardfork + 1 );
        }
        else
          throw unknown_hardfork_exception();
      }
    }
    else
    {
      while( hardforks.last_hardfork < HIVE_NUM_HARDFORKS
          && _hardfork_versions.times[ hardforks.last_hardfork + 1 ] <= head_block_time()
          && hardforks.last_hardfork < HIVE_HARDFORK_0_5__54 )
      {
        apply_hardfork( hardforks.last_hardfork + 1 );
      }
    }
  }
  FC_CAPTURE_AND_RETHROW()
}

bool database::has_hardfork( uint32_t hardfork )const
{
  return get_hardfork_property_object().processed_hardforks.size() > hardfork;
}

uint32_t database::get_hardfork()const
{
  return get_hardfork_property_object().processed_hardforks.size() - 1;
}

void database::set_hardfork( uint32_t hardfork, bool apply_now )
{
  auto const& hardforks = get_hardfork_property_object();

  for( uint32_t i = hardforks.last_hardfork + 1; i <= hardfork && i <= HIVE_NUM_HARDFORKS; i++ )
  {
    if( i <= HIVE_HARDFORK_0_5__54 )
      _hardfork_versions.times[i] = head_block_time();
    else
    {
      modify( hardforks, [&]( hardfork_property_object& hpo )
      {
        hpo.next_hardfork = _hardfork_versions.versions[i];
        hpo.next_hardfork_time = head_block_time();
      } );
    }

    if( apply_now )
      apply_hardfork( i );
  }
}

void database::apply_hardfork( uint32_t hardfork )
{
  if( _log_hardforks )
    elog( "HARDFORK ${hf} at block ${b}", ("hf", hardfork)("b", decorate_number_with_upticks(head_block_num())) );
  operation hardfork_vop = hardfork_operation( hardfork );

  pre_push_virtual_operation( hardfork_vop );
  const auto _op_in_trx = _current_op_in_trx;

  switch( hardfork )
  {
    case HIVE_HARDFORK_0_1:
      perform_vesting_share_split( 1000000 );
      break;
    case HIVE_HARDFORK_0_2:
      retally_witness_votes();
      break;
    case HIVE_HARDFORK_0_3:
      retally_witness_votes();
      break;
    case HIVE_HARDFORK_0_4:
      reset_virtual_schedule_time(*this);
      break;
    case HIVE_HARDFORK_0_5:
      break;
    case HIVE_HARDFORK_0_6:
      retally_witness_vote_counts();
      break;
    case HIVE_HARDFORK_0_7:
      break;
    case HIVE_HARDFORK_0_8:
      retally_witness_vote_counts(true);
      break;
    case HIVE_HARDFORK_0_9:
      {
        for( const std::string& acc : hardfork9::get_compromised_accounts() )
        {
          const account_object* account = find_account( acc );
          if( account == nullptr )
            continue;

          wlog("Setting key: ${k} as an owner authority for account: ${a}", ("k", HIVE_HF_9_COMPROMISED_ACCOUNTS_PUBLIC_KEY_STR)("a", acc));

          update_owner_authority( *account, authority( 1, public_key_type(HIVE_HF_9_COMPROMISED_ACCOUNTS_PUBLIC_KEY_STR), 1 ) );

          modify( get< account_authority_object, by_account >( account->get_name() ), [&]( account_authority_object& auth )
          {
            auth.active  = authority( 1, public_key_type(HIVE_HF_9_COMPROMISED_ACCOUNTS_PUBLIC_KEY_STR), 1 );
            auth.posting = authority( 1, public_key_type(HIVE_HF_9_COMPROMISED_ACCOUNTS_PUBLIC_KEY_STR), 1 );
          });
        }
      }
      break;
    case HIVE_HARDFORK_0_10:
      retally_liquidity_weight();
      break;
    case HIVE_HARDFORK_0_11:
      break;
    case HIVE_HARDFORK_0_12:
      {
        const auto& comment_idx = get_index< comment_cashout_index >().indices();

        for( auto itr = comment_idx.begin(); itr != comment_idx.end(); ++itr )
        {
          // At the hardfork time, all new posts with no votes get their cashout time set to +12 hrs from head block time.
          // All posts with a payout get their cashout time set to +30 days. This hardfork takes place within 30 days
          // initial payout so we don't have to handle the case of posts that should be frozen that aren't
          const comment_object& comment = get_comment( *itr );
          const comment_cashout_ex_object* c_ex = find_comment_cashout_ex( comment );
          if( comment.is_root() )
          {
            // Post has not been paid out and has no votes (cashout_time == 0 === net_rshares == 0, under current semantics)
            if( !c_ex->was_paid() && itr->get_cashout_time() == fc::time_point_sec::maximum() )
            {
              modify( *itr, [&]( comment_cashout_object & c )
              {
                c.set_cashout_time( head_block_time() + HIVE_CASHOUT_WINDOW_SECONDS_PRE_HF17 );
              });
            }
            // Has been paid out, needs to be on second cashout window
            else if( c_ex->was_paid() )
            {
              modify( *itr, [&]( comment_cashout_object& c )
              {
                c.set_cashout_time( c_ex->get_last_payout() + HIVE_SECOND_CASHOUT_WINDOW );
              });
            }
          }
        }

        modify( get< account_authority_object, by_account >( HIVE_MINER_ACCOUNT ), [&]( account_authority_object& auth )
        {
          auth.posting = authority();
          auth.posting.weight_threshold = 1;
        });

        modify( get< account_authority_object, by_account >( HIVE_NULL_ACCOUNT ), [&]( account_authority_object& auth )
        {
          auth.posting = authority();
          auth.posting.weight_threshold = 1;
        });

        modify( get< account_authority_object, by_account >( HIVE_TEMP_ACCOUNT ), [&]( account_authority_object& auth )
        {
          auth.posting = authority();
          auth.posting.weight_threshold = 1;
        });
      }
      break;
    case HIVE_HARDFORK_0_13:
      break;
    case HIVE_HARDFORK_0_14:
      break;
    case HIVE_HARDFORK_0_15:
      break;
    case HIVE_HARDFORK_0_16:
      {
        modify( get_feed_history(), [&]( feed_history_object& fho )
        {
          while( fho.price_history.size() > HIVE_FEED_HISTORY_WINDOW )
            fho.price_history.pop_front();
        });
      }
      break;
    case HIVE_HARDFORK_0_17:
      {
        static_assert(
          HIVE_MAX_VOTED_WITNESSES_HF0 + HIVE_MAX_MINER_WITNESSES_HF0 + HIVE_MAX_RUNNER_WITNESSES_HF0 == HIVE_MAX_WITNESSES,
          "HF0 witness counts must add up to HIVE_MAX_WITNESSES" );
        static_assert(
          HIVE_MAX_VOTED_WITNESSES_HF17 + HIVE_MAX_MINER_WITNESSES_HF17 + HIVE_MAX_RUNNER_WITNESSES_HF17 == HIVE_MAX_WITNESSES,
          "HF17 witness counts must add up to HIVE_MAX_WITNESSES" );

        modify( get_witness_schedule_object(), [&]( witness_schedule_object& wso )
        {
          wso.max_voted_witnesses = HIVE_MAX_VOTED_WITNESSES_HF17;
          wso.max_miner_witnesses = HIVE_MAX_MINER_WITNESSES_HF17;
          wso.max_runner_witnesses = HIVE_MAX_RUNNER_WITNESSES_HF17;
        });

        const auto& gpo = get_dynamic_global_properties();

        auto& post_rf = create< reward_fund_object >( HIVE_POST_REWARD_FUND_NAME, gpo.get_total_reward_fund_hive(), head_block_time()
#ifndef IS_TEST_NET
          , HIVE_HF_17_RECENT_CLAIMS
#endif
          );

        // As a shortcut in payout processing, we use the id as an array index.
        // The IDs must be assigned this way. The assertion is a dummy check to ensure this happens.
        FC_ASSERT( post_rf.get_id() == reward_fund_id_type() );

        modify( gpo, [&]( dynamic_global_property_object& g )
        {
          g.total_reward_fund_hive = asset( 0, HIVE_SYMBOL );
          g.total_reward_shares2 = 0;
        });

        /*
        * For all current comments we will either keep their current cashout time, or extend it to 1 week
        * after creation.
        *
        * We cannot do a simple iteration by cashout time because we are editting cashout time.
        * More specifically, we will be adding an explicit cashout time to all comments with parents.
        * To find all discussions that have not been paid out we fir iterate over posts by cashout time.
        * Before the hardfork these are all root posts. Iterate over all of their children, adding each
        * to a specific list. Next, update payout times for all discussions on the root post. This defines
        * the min cashout time for each child in the discussion. Then iterate over the children and set
        * their cashout time in a similar way, grabbing the root post as their inherent cashout time.
        */
        const auto& comment_idx = get_index< comment_cashout_index, by_cashout_time >();
        const auto& by_root_idx = get_index< comment_cashout_ex_index, by_root >();
        vector< const comment_cashout_object* > root_posts;
        root_posts.reserve( HIVE_HF_17_NUM_POSTS );
        vector< const comment_cashout_object* > replies;
        replies.reserve( HIVE_HF_17_NUM_REPLIES );

        for( auto itr = comment_idx.begin(); itr != comment_idx.end() && itr->get_cashout_time() < fc::time_point_sec::maximum(); ++itr )
        {
          root_posts.push_back( &(*itr) );
          auto root_id = itr->get_comment_id();

          for( auto reply_itr = by_root_idx.lower_bound( root_id ); reply_itr != by_root_idx.end() && reply_itr->get_root_id() == root_id; ++reply_itr )
          {
            const comment_cashout_object* comment_cashout = find_comment_cashout( reply_itr->get_comment_id() );
            replies.push_back( comment_cashout );
          }
        }

        for( const auto& itr : root_posts )
        {
          modify( *itr, [&]( comment_cashout_object& c )
          {
            c.set_cashout_time( std::max( c.get_creation_time() + HIVE_CASHOUT_WINDOW_SECONDS, c.get_cashout_time() ) );
          });
        }

        for( const auto& itr : replies )
        {
          modify( *itr, [&]( comment_cashout_object& c )
          {
            c.set_cashout_time( std::max( calculate_discussion_payout_time( get_comment( c ), c ), c.get_creation_time() + HIVE_CASHOUT_WINDOW_SECONDS ) );
          });
        }
      }
      break;
    case HIVE_HARDFORK_0_18:
      break;
    case HIVE_HARDFORK_0_19:
      {
        modify( get_dynamic_global_properties(), [&]( dynamic_global_property_object& gpo )
        {
          gpo.vote_power_reserve_rate = HIVE_REDUCED_VOTE_POWER_RATE;
        });

        modify( get< reward_fund_object, by_name >( HIVE_POST_REWARD_FUND_NAME ), [&]( reward_fund_object &rfo )
        {
#ifndef IS_TEST_NET
          rfo.recent_claims = HIVE_HF_19_RECENT_CLAIMS;
#endif
          rfo.author_reward_curve = curve_id::linear;
          rfo.curation_reward_curve = curve_id::square_root;
        });

        /* Remove all 0 delegation objects */
        vector< const vesting_delegation_object* > to_remove;
        const auto& delegation_idx = get_index< vesting_delegation_index, by_id >();
        auto delegation_itr = delegation_idx.begin();

        while( delegation_itr != delegation_idx.end() )
        {
          if( delegation_itr->get_vesting().amount == 0 )
            to_remove.push_back( &(*delegation_itr) );

          ++delegation_itr;
        }

        for( const vesting_delegation_object* delegation_ptr: to_remove )
        {
          remove( *delegation_ptr );
        }

        remove_old_cashouts();
      }
      break;
    case HIVE_HARDFORK_0_20:
      {
        modify( get_dynamic_global_properties(), [&]( dynamic_global_property_object& gpo )
        {
          gpo.delegation_return_period = HIVE_DELEGATION_RETURN_PERIOD_HF20;
          gpo.reverse_auction_seconds = HIVE_REVERSE_AUCTION_WINDOW_SECONDS_HF20;
          gpo.hbd_stop_percent = HIVE_HBD_STOP_PERCENT_HF20;
          gpo.hbd_start_percent = HIVE_HBD_START_PERCENT_HF20;
          gpo.available_account_subsidies = 0;
        });

        const auto& wso = get_witness_schedule_object();

        for( const auto& witness : wso.current_shuffled_witnesses )
        {
          // Required check when applying hardfork at genesis
          if( witness != account_name_type() )
          {
            modify( get< witness_object, by_name >( witness ), [&]( witness_object& w )
            {
              w.props.account_creation_fee = asset( w.props.account_creation_fee.amount * HIVE_CREATE_ACCOUNT_WITH_HIVE_MODIFIER, HIVE_SYMBOL );
            });
          }
        }

        modify( wso, [&]( witness_schedule_object& wso )
        {
          wso.median_props.account_creation_fee = asset( wso.median_props.account_creation_fee.amount * HIVE_CREATE_ACCOUNT_WITH_HIVE_MODIFIER, HIVE_SYMBOL );
        });
      }
      break;
    case HIVE_HARDFORK_0_21:
    {
      modify( get_dynamic_global_properties(), [&]( dynamic_global_property_object& gpo )
      {
        gpo.proposal_fund_percent = HIVE_PROPOSAL_FUND_PERCENT_HF21;
        gpo.content_reward_percent = HIVE_CONTENT_REWARD_PERCENT_HF21;
        gpo.downvote_pool_percent = HIVE_DOWNVOTE_POOL_PERCENT_HF21;
        gpo.reverse_auction_seconds = HIVE_REVERSE_AUCTION_WINDOW_SECONDS_HF21;
      });

      const auto treasury_name = get_treasury_name();

      // Create the treasury account if it does not exist
      // This may sometimes happen in the mirrornet, when we do not have the account created upon the HF 21 application or any dependent operation
      if( find_account(treasury_name) == nullptr )
        create< account_object >( treasury_name, head_block_time() );

      lock_account( get_treasury() );

      modify( get< reward_fund_object, by_name >( HIVE_POST_REWARD_FUND_NAME ), [&]( reward_fund_object& rfo )
      {
        rfo.percent_curation_rewards = 50 * HIVE_1_PERCENT;
        rfo.author_reward_curve = convergent_linear;
        rfo.curation_reward_curve = convergent_square_root;
        rfo.content_constant = HIVE_CONTENT_CONSTANT_HF21;
#ifndef  IS_TEST_NET
        rfo.recent_claims = HIVE_HF21_CONVERGENT_LINEAR_RECENT_CLAIMS;
#endif
      });
    }
    break;
    case HIVE_HARDFORK_0_22:
      break;
    case HIVE_HARDFORK_0_23:
    {
      clear_accounts( hardforkprotect::get_steemit_accounts() );

      // Reset TAPOS buffer to avoid replay attack
      auto empty_block_id = block_id_type();
      const auto& bs_idx = get_index< block_summary_index, by_id >();
      for( auto itr = bs_idx.begin(); itr != bs_idx.end(); ++itr )
      {
        modify( *itr, [&](block_summary_object& p) {
          p.block_id = empty_block_id;
        });
      }
      break;
    }
    case HIVE_HARDFORK_1_24:
    {
      restore_accounts( hardforkprotect::get_restored_accounts() );
#ifdef USE_ALTERNATE_CHAIN_ID
      /// Don't change chain_id in testnet build.
#else
      set_chain_id(HIVE_CHAIN_ID);
#endif /// IS_TEST_NET
      break;
    }
    case HIVE_HARDFORK_1_25:
    {
      modify( get< reward_fund_object, by_name >( HIVE_POST_REWARD_FUND_NAME ), [&]( reward_fund_object& rfo )
      {
        rfo.curation_reward_curve = linear;
        rfo.author_reward_curve   = linear;
      });
      modify( get_dynamic_global_properties(), [&]( dynamic_global_property_object& gpo )
      {
        gpo.reverse_auction_seconds = HIVE_REVERSE_AUCTION_WINDOW_SECONDS_HF25;
        gpo.early_voting_seconds    = HIVE_EARLY_VOTING_SECONDS_HF25;
        gpo.mid_voting_seconds      = HIVE_MID_VOTING_SECONDS_HF25;
      });
      break;
    }
    case HIVE_HARDFORK_1_26:
    {
      modify( get_dynamic_global_properties(), [&]( dynamic_global_property_object& gpo )
      {
        gpo.hbd_stop_percent = HIVE_HBD_STOP_PERCENT_HF26;
        gpo.hbd_start_percent = HIVE_HBD_START_PERCENT_HF26;
      } );
      const auto& fwso = create<witness_schedule_object>( [&]( witness_schedule_object& future_witness_schedule )
      {
        future_witness_schedule.copy_values_from( get_witness_schedule_object() );
      } );
      FC_ASSERT( fwso.get_id() == 1, "Unexpected id allocated to future witness schedule object" );
      break;
    }
    case HIVE_HARDFORK_1_28:
    {
      remove_proposal_votes_for_accounts_without_voting_rights();
      break;
    }
    case HIVE_SMT_HARDFORK:
    {
#ifdef HIVE_ENABLE_SMT
      replenish_nai_pool( *this );
#endif
      modify( get_dynamic_global_properties(), [&]( dynamic_global_property_object& gpo )
      {
        gpo.required_actions_partition_percent = 25 * HIVE_1_PERCENT;
      });

      break;
    }
    default:
      break;
  }

  modify( get_hardfork_property_object(), [&]( hardfork_property_object& hfp )
  {
    FC_ASSERT( hardfork == hfp.last_hardfork + 1, "Hardfork being applied out of order", ("hardfork",hardfork)("hfp.last_hardfork",hfp.last_hardfork) );
    FC_ASSERT( hfp.processed_hardforks.size() == hardfork, "Hardfork being applied out of order" );
    hfp.processed_hardforks.push_back( _hardfork_versions.times[ hardfork ] );
    hfp.last_hardfork = hardfork;
    hfp.current_hardfork_version = _hardfork_versions.versions[ hardfork ];
    FC_ASSERT( hfp.processed_hardforks[ hfp.last_hardfork ] == _hardfork_versions.times[ hfp.last_hardfork ], "Hardfork processing failed sanity check..." );
  } );

  if( hardfork == HIVE_HARDFORK_1_24_TREASURY_RENAME )
  {
    const auto treasury_name = get_treasury_name();

    if( find_account(treasury_name) == nullptr )
      create< account_object >( treasury_name, head_block_time() );

    lock_account( get_treasury() );
    //the following routine can only be called effectively after hardfork was marked as applied
    //we could wait for regular call in _apply_block(), however it could hinder future changes, most notably use of treasury in future
    //hardfork code with assumption of nonzero balance
    consolidate_treasury_balance();
  }

  post_push_virtual_operation( hardfork_vop, _op_in_trx );
}

void database::retally_liquidity_weight() {
  const auto& ridx = get_index< liquidity_reward_balance_index >().indices().get< by_owner >();
  for( const auto& i : ridx ) {
    modify( i, []( liquidity_reward_balance_object& o ){
      o.update_weight(true/*HAS HARDFORK10 if this method is called*/);
    });
  }
}

/**
  * Verifies all supply invariantes check out
  */
void database::validate_invariants()const
{
  try
  {
    const auto& account_idx = get_index< account_index, by_name >();
    asset total_supply = asset( 0, HIVE_SYMBOL );
    asset total_hbd = asset( 0, HBD_SYMBOL );
    asset total_vesting = asset( 0, VESTS_SYMBOL );
    asset pending_vesting_hive = asset( 0, HIVE_SYMBOL );
    share_type total_vsf_votes = share_type( 0 );
    ushare_type total_delayed_votes = ushare_type( 0 );

    uint64_t witness_no = 0;
    uint64_t account_no = 0;
    uint64_t convert_no = 0;
    uint64_t collateralized_convert_no = 0;
    uint64_t order_no = 0;
    uint64_t escrow_no = 0;
    uint64_t withdrawal_no = 0;
    uint64_t reward_fund_no = 0;
    uint64_t contribution_no = 0;

    auto& gpo = get_dynamic_global_properties();

    /// verify no witness has too many votes
    const auto& witness_idx = get_index< witness_index >().indices();
    for( auto itr = witness_idx.begin(); itr != witness_idx.end(); ++itr )
    {
      FC_ASSERT( itr->votes <= gpo.total_vesting_shares.amount, "", ("itr",*itr) );
      ++witness_no;
    }

    for( auto itr = account_idx.begin(); itr != account_idx.end(); ++itr )
    {
      total_supply += itr->get_balance();
      total_supply += itr->get_savings();
      total_supply += itr->get_rewards();
      total_hbd += itr->get_hbd_balance();
      total_hbd += itr->get_hbd_savings();
      total_hbd += itr->get_hbd_rewards();
      total_vesting += itr->get_vesting();
      total_vesting += itr->get_vest_rewards();
      pending_vesting_hive += itr->get_vest_rewards_as_hive();
      total_vsf_votes += ( !itr->has_proxy() ?
                      itr->get_governance_vote_power() :
                      ( HIVE_MAX_PROXY_RECURSION_DEPTH > 0 ?
                          itr->proxied_vsf_votes[HIVE_MAX_PROXY_RECURSION_DEPTH - 1] :
                          itr->get_direct_governance_vote_power() ) );
      total_delayed_votes += itr->sum_delayed_votes;
      ushare_type sum_delayed_votes{ 0ul };
      for( auto& dv : itr->delayed_votes )
        sum_delayed_votes += dv.val;
      FC_ASSERT( sum_delayed_votes == itr->sum_delayed_votes, "", ("sum_delayed_votes",sum_delayed_votes)("itr->sum_delayed_votes",itr->sum_delayed_votes) );
      FC_ASSERT( sum_delayed_votes.value <= itr->get_vesting().amount, "", ("sum_delayed_votes",sum_delayed_votes)("itr->vesting_shares.amount",itr->get_vesting().amount)("account",itr->get_name()) );
      ++account_no;
    }

    const auto& convert_request_idx = get_index< convert_request_index >().indices();
    for( auto itr = convert_request_idx.begin(); itr != convert_request_idx.end(); ++itr )
    {
      total_hbd += itr->get_convert_amount();
      ++convert_no;
    }

    const auto& collateralized_convert_request_idx = get_index< collateralized_convert_request_index >().indices();
    for( auto itr = collateralized_convert_request_idx.begin(); itr != collateralized_convert_request_idx.end(); ++itr )
    {
      total_supply += itr->get_collateral_amount();
      // don't collect get_converted_amount() - it is not balance object; that tokens are already on owner's balance
      ++collateralized_convert_no;
    }

    const auto& limit_order_idx = get_index< limit_order_index >().indices();

    for( auto itr = limit_order_idx.begin(); itr != limit_order_idx.end(); ++itr )
    {
      if( itr->sell_price.base.symbol == HIVE_SYMBOL )
      {
        total_supply += asset( itr->for_sale, HIVE_SYMBOL );
      }
      else if ( itr->sell_price.base.symbol == HBD_SYMBOL )
      {
        total_hbd += asset( itr->for_sale, HBD_SYMBOL );
      }
      ++order_no;
    }

    const auto& escrow_idx = get_index< escrow_index >().indices().get< by_id >();

    for( auto itr = escrow_idx.begin(); itr != escrow_idx.end(); ++itr )
    {
      total_supply += itr->get_hive_balance();
      total_hbd += itr->get_hbd_balance();

      if( itr->get_fee().symbol == HIVE_SYMBOL )
        total_supply += itr->get_fee();
      else if( itr->get_fee().symbol == HBD_SYMBOL )
        total_hbd += itr->get_fee();
      else
        FC_ASSERT( false, "found escrow pending fee that is not HBD or HIVE" );
      ++escrow_no;
    }

    const auto& savings_withdraw_idx = get_index< savings_withdraw_index >().indices().get< by_id >();

    for( auto itr = savings_withdraw_idx.begin(); itr != savings_withdraw_idx.end(); ++itr )
    {
      if( itr->amount.symbol == HIVE_SYMBOL )
        total_supply += itr->amount;
      else if( itr->amount.symbol == HBD_SYMBOL )
        total_hbd += itr->amount;
      else
        FC_ASSERT( false, "found savings withdraw that is not HBD or HIVE" );
      ++withdrawal_no;
    }

    const auto& reward_idx = get_index< reward_fund_index, by_id >();

    for( auto itr = reward_idx.begin(); itr != reward_idx.end(); ++itr )
    {
      total_supply += itr->reward_balance;
      ++reward_fund_no;
    }

#ifdef HIVE_ENABLE_SMT
    const auto& smt_contribution_idx = get_index< smt_contribution_index, by_id >();

    for ( auto itr = smt_contribution_idx.begin(); itr != smt_contribution_idx.end(); ++itr )
    {
      total_supply += itr->contribution;
      ++contribution_no;
    }
#endif

    total_supply += gpo.get_total_vesting_fund_hive() + gpo.get_total_reward_fund_hive() + gpo.get_pending_rewarded_vesting_hive();

    FC_ASSERT( gpo.current_supply == total_supply, "", ("gpo.current_supply",gpo.current_supply)("total_supply",total_supply) );
    FC_ASSERT( gpo.get_current_hbd_supply() + gpo.get_init_hbd_supply() == total_hbd, "", ("gpo.current_hbd_supply",gpo.get_current_hbd_supply())("total_hbd",total_hbd) );
    FC_ASSERT( gpo.total_vesting_shares + gpo.pending_rewarded_vesting_shares == total_vesting, "", ("gpo.total_vesting_shares",gpo.total_vesting_shares)("total_vesting",total_vesting) );
    FC_ASSERT( gpo.total_vesting_shares.amount == total_vsf_votes + total_delayed_votes.value, "", ("total_vesting_shares",gpo.total_vesting_shares)("total_vsf_votes + total_delayed_votes",total_vsf_votes + total_delayed_votes.value) );
    FC_ASSERT( gpo.get_pending_rewarded_vesting_hive() == pending_vesting_hive, "", ("pending_rewarded_vesting_hive",gpo.get_pending_rewarded_vesting_hive())("pending_vesting_hive", pending_vesting_hive));

    FC_ASSERT( gpo.virtual_supply >= gpo.current_supply );
    if ( !get_feed_history().current_median_history.is_null() )
    {
      FC_ASSERT( gpo.get_current_hbd_supply()* get_feed_history().current_median_history + gpo.current_supply
        == gpo.virtual_supply, "", ("gpo.current_hbd_supply",gpo.get_current_hbd_supply())("get_feed_history().current_median_history",get_feed_history().current_median_history)("gpo.current_supply",gpo.current_supply)("gpo.virtual_supply",gpo.virtual_supply) );
    }

    ilog( "validate_invariants @${b}:", ( "b", head_block_num() ) );
    ilog( "successful scan of ${p} witnesses, ${a} accounts, ${c} convert requests, ${cc} collateralized convert requests, ${o} limit orders, ${e} escrow transfers, ${w} saving withdrawals, ${r} reward funds and ${s} SMT contributions.",
      ( "p", witness_no )( "a", account_no )( "c", convert_no )( "cc", collateralized_convert_no )( "o", order_no )( "e", escrow_no )( "w", withdrawal_no )( "r", reward_fund_no )( "s", contribution_no ) );
    ilog( "HIVE supply: ${h}", ( "h", gpo.current_supply.amount.value ) );
    ilog( "HBD supply: ${s} ( + ${i} initial )", ( "s", gpo.get_current_hbd_supply().amount.value )( "i", gpo.get_init_hbd_supply().amount.value ) );
    ilog( "virtual supply (HIVE): ${w}", ( "w", gpo.virtual_supply.amount.value ) );
    ilog( "VESTS: ${v} ( + ${p} pending ) worth (HIVE): ${x} ( + ${y} )", ( "v", gpo.total_vesting_shares.amount.value )( "p", gpo.pending_rewarded_vesting_shares.amount.value )( "x", gpo.get_total_vesting_fund_hive().amount.value )( "y", gpo.get_pending_rewarded_vesting_hive().amount.value ) );
  }
  FC_CAPTURE_LOG_AND_RETHROW( (head_block_num()) );
}

#ifdef HIVE_ENABLE_SMT

namespace {
  template <typename index_type, typename lambda>
  void add_from_balance_index(const index_type& balance_idx, lambda callback )
  {
    auto it = balance_idx.begin();
    auto end = balance_idx.end();
    for( ; it != end; ++it )
    {
      const auto& balance = *it;
      callback( balance );
    }
  }
}

/**
  * SMT version of validate_invariants.
  */
void database::validate_smt_invariants()const
{
  try
  {
    // Get total balances.
    typedef struct {
      asset liquid;
      asset vesting;
      asset pending_liquid;
      asset pending_vesting_shares;
      asset pending_vesting_value;
    } TCombinedBalance;
    typedef std::map< asset_symbol_type, TCombinedBalance > TCombinedSupplyMap;
    TCombinedSupplyMap theMap;

    // - Process regular balances, collecting SMT counterparts of 'balance' & 'vesting_shares'.
    const auto& balance_idx = get_index< account_regular_balance_index, by_id >();
    add_from_balance_index( balance_idx, [ &theMap ] ( const account_regular_balance_object& regular )
    {
      asset zero_liquid = asset( 0, regular.liquid.symbol );
      asset zero_vesting = asset( 0, regular.vesting.symbol );
      auto insertInfo = theMap.emplace( regular.liquid.symbol,
        TCombinedBalance( { regular.liquid, regular.vesting, zero_liquid, zero_vesting, zero_liquid } ) );
      if( insertInfo.second == false )
        {
        TCombinedBalance& existing_balance = insertInfo.first->second;
        existing_balance.liquid += regular.liquid;
        existing_balance.vesting += regular.vesting;
        }
    });

    // - Process reward balances, collecting SMT counterparts of 'reward_hive_balance', 'reward_vesting_balance' & 'reward_vesting_hive'.
    const auto& rewards_balance_idx = get_index< account_rewards_balance_index, by_id >();
    add_from_balance_index( rewards_balance_idx, [ &theMap ] ( const account_rewards_balance_object& rewards )
    {
      asset zero_liquid = asset( 0, rewards.pending_vesting_value.symbol );
      asset zero_vesting = asset( 0, rewards.pending_vesting_shares.symbol );
      auto insertInfo = theMap.emplace( rewards.pending_liquid.symbol, TCombinedBalance( { zero_liquid, zero_vesting,
        rewards.pending_liquid, rewards.pending_vesting_shares, rewards.pending_vesting_value } ) );
      if( insertInfo.second == false )
        {
        TCombinedBalance& existing_balance = insertInfo.first->second;
        existing_balance.pending_liquid += rewards.pending_liquid;
        existing_balance.pending_vesting_shares += rewards.pending_vesting_shares;
        existing_balance.pending_vesting_value += rewards.pending_vesting_value;
        }
    });

    // - Market orders
    const auto& limit_order_idx = get_index< limit_order_index >().indices();
    for( auto itr = limit_order_idx.begin(); itr != limit_order_idx.end(); ++itr )
    {
      if( itr->sell_price.base.symbol.space() == asset_symbol_type::smt_nai_space )
      {
        asset a( itr->for_sale, itr->sell_price.base.symbol );
        FC_ASSERT( a.symbol.is_vesting() == false );
        asset zero_liquid = asset( 0, a.symbol );
        asset zero_vesting = asset( 0, a.symbol.get_paired_symbol() );
        auto insertInfo = theMap.emplace( a.symbol, TCombinedBalance( { a, zero_vesting, zero_liquid, zero_vesting, zero_liquid } ) );
        if( insertInfo.second == false )
          insertInfo.first->second.liquid += a;
      }
    }

    // - Reward funds
FC_TODO( "Add reward_fund_object iteration here once they support SMTs." )

    // - Escrow & savings - no support of SMT is expected.

    // Do the verification of total balances.
    auto itr = get_index< smt_token_index, by_id >().begin();
    auto end = get_index< smt_token_index, by_id >().end();
    for( ; itr != end; ++itr )
    {
      const smt_token_object& smt = *itr;
      asset_symbol_type vesting_symbol = smt.liquid_symbol.get_paired_symbol();
      auto totalIt = theMap.find( smt.liquid_symbol );
      // Check liquid SMT supply.
      asset total_liquid_supply = totalIt == theMap.end() ? asset(0, smt.liquid_symbol) :
        ( totalIt->second.liquid + totalIt->second.pending_liquid );
      total_liquid_supply += asset( smt.total_vesting_fund_smt, smt.liquid_symbol )
                    /*+ gpo.get_total_reward_fund_hive() */
                    + asset( smt.pending_rewarded_vesting_smt, smt.liquid_symbol );
FC_TODO( "Supplement ^ once SMT rewards are implemented" )
      FC_ASSERT( asset(smt.current_supply, smt.liquid_symbol) == total_liquid_supply,
              "", ("smt current_supply",smt.current_supply)("total_liquid_supply",total_liquid_supply) );
      // Check vesting SMT supply.
      asset total_vesting_supply = totalIt == theMap.end() ? asset(0, vesting_symbol) :
        ( totalIt->second.vesting + totalIt->second.pending_vesting_shares );
      asset smt_vesting_supply = asset(smt.total_vesting_shares + smt.pending_rewarded_vesting_shares, vesting_symbol);
      FC_ASSERT( smt_vesting_supply == total_vesting_supply,
              "", ("smt vesting supply",smt_vesting_supply)("total_vesting_supply",total_vesting_supply) );
      // Check pending_vesting_value
      asset pending_vesting_value = totalIt == theMap.end() ? asset(0, smt.liquid_symbol) : totalIt->second.pending_vesting_value;
      FC_ASSERT( asset(smt.pending_rewarded_vesting_smt, smt.liquid_symbol) == pending_vesting_value, "",
        ("smt pending_rewarded_vesting_smt", smt.pending_rewarded_vesting_smt)("pending_vesting_value", pending_vesting_value));
    }
  }
  FC_CAPTURE_LOG_AND_RETHROW( (head_block_num()) );
}
#endif

void database::perform_vesting_share_split( uint32_t magnitude )
{
  try
  {
    modify( get_dynamic_global_properties(), [&]( dynamic_global_property_object& d )
    {
      d.total_vesting_shares.amount *= magnitude;
      d.total_reward_shares2 = 0;
    } );

    // Need to update all VESTS in accounts and the total VESTS in the dgpo
    for( const auto& account : get_index< account_index, by_id >() )
    {
      asset old_vesting_shares = account.get_vesting();
      asset new_vesting_shares = old_vesting_shares;
      modify( account, [&]( account_object& a )
      {
        a.vesting_shares.amount *= magnitude;
        new_vesting_shares = a.get_vesting();
        a.withdrawn.amount *= magnitude;
        a.to_withdraw.amount *= magnitude;
        a.vesting_withdraw_rate  = asset( a.to_withdraw.amount / HIVE_VESTING_WITHDRAW_INTERVALS_PRE_HF_16, VESTS_SYMBOL );
        if( a.vesting_withdraw_rate.amount == 0 )
          a.vesting_withdraw_rate.amount = 1;

        for( uint32_t i = 0; i < HIVE_MAX_PROXY_RECURSION_DEPTH; ++i )
          a.proxied_vsf_votes[i] *= magnitude;
      } );
      if (old_vesting_shares != new_vesting_shares)
        push_virtual_operation( vesting_shares_split_operation(account.get_name(), old_vesting_shares, new_vesting_shares) );
    }

    const auto& comments = get_index< comment_cashout_index >().indices();
    for( const auto& comment : comments )
    {
      modify( comment, [&]( comment_cashout_object& c )
      {
        //c.net_rshares *= magnitude;
        //c.vote_rshares *= magnitude;
        c.accumulate_vote_rshares( c.get_net_rshares() * ( magnitude - 1 ), c.get_vote_rshares() * ( magnitude - 1 ) );
      } );
      modify( *find_comment_cashout_ex( comment.get_comment_id() ), [&]( comment_cashout_ex_object& c_ex )
      {
        //c_ex.abs_rshares *= magnitude;
        c_ex.accumulate_abs_rshares( c_ex.get_abs_rshares() * ( magnitude - 1 ) );
      } );
    }

    for( const auto& c : comments )
    {
      if( c.get_net_rshares() > 0 )
        adjust_rshares2( 0, util::evaluate_reward_curve( c.get_net_rshares() ) );
    }

  }
  FC_CAPTURE_AND_RETHROW()
}

void database::retally_witness_votes()
{
  const auto& witness_idx = get_index< witness_index >().indices();

  // Clear all witness votes
  for( auto itr = witness_idx.begin(); itr != witness_idx.end(); ++itr )
  {
    modify( *itr, [&]( witness_object& w )
    {
      w.votes = 0;
      w.virtual_position = 0;
    } );
  }

  const auto& account_idx = get_index< account_index >().indices();

  // Apply all existing votes by account
  for( auto itr = account_idx.begin(); itr != account_idx.end(); ++itr )
  {
    if( itr->has_proxy() ) continue;

    const auto& a = *itr;

    const auto& vidx = get_index<witness_vote_index>().indices().get<by_account_witness>();
    auto wit_itr = vidx.lower_bound( boost::make_tuple( a.get_name(), account_name_type() ) );
    while( wit_itr != vidx.end() && wit_itr->account == a.get_name() )
    {
      adjust_witness_vote( get< witness_object, by_name >(wit_itr->witness), a.get_governance_vote_power() );
      ++wit_itr;
    }
  }
}

void database::retally_witness_vote_counts( bool force )
{
  const auto& account_idx = get_index< account_index >().indices();

  // Check all existing votes by account
  for( auto itr = account_idx.begin(); itr != account_idx.end(); ++itr )
  {
    const auto& a = *itr;
    uint16_t witnesses_voted_for = 0;
    if( force || a.has_proxy() )
    {
      const auto& vidx = get_index< witness_vote_index >().indices().get< by_account_witness >();
      auto wit_itr = vidx.lower_bound( boost::make_tuple( a.get_name(), account_name_type() ) );
      while( wit_itr != vidx.end() && wit_itr->account == a.get_name() )
      {
        ++witnesses_voted_for;
        ++wit_itr;
      }
    }
    if( a.witnesses_voted_for != witnesses_voted_for )
    {
      modify( a, [&]( account_object& account )
      {
        account.witnesses_voted_for = witnesses_voted_for;
      } );
    }
  }
}

optional< chainbase::database::session >& database::pending_transaction_session()
{
  return _pending_tx_session;
}

void database::remove_expired_governance_votes()
{
  if (!has_hardfork(HIVE_HARDFORK_1_25))
    return;

  const auto& accounts = get_index<account_index, by_governance_vote_expiration_ts>();
  auto acc_it = accounts.begin();
  time_point_sec first_expiring = acc_it->get_governance_vote_expiration_ts();
  time_point_sec now = head_block_time();

  if( first_expiring > now )
    return;

  const auto& proposal_votes = get_index< proposal_vote_index, by_voter_proposal >();
  remove_guard obj_perf( get_remove_threshold() );

  while( acc_it != accounts.end() && acc_it->get_governance_vote_expiration_ts() <= now )
  {
    const auto& account = *acc_it;
    ++acc_it;

    if( dhf_helper::remove_proposal_votes( account, proposal_votes, *this, obj_perf ) )
    {
      nullify_proxied_witness_votes( account );
      clear_witness_votes( account );

      if( account.has_proxy() )
        push_virtual_operation( proxy_cleared_operation( account.get_name(), get_account( account.get_proxy() ).get_name()) );

      modify( account, [&]( account_object& a )
      {
        a.clear_proxy();
        a.set_governance_vote_expired();
      } );
      push_virtual_operation( expired_account_notification_operation( account.get_name() ) );
    }
    else
    {
      ilog("Threshold exceeded while processing account ${account} with expired governance vote.",
        ("account", account.get_name())); // to be continued in next block
      break;
    }
  }
}

//safe to call without chainbase lock
std::vector<block_id_type> database::get_blockchain_synopsis(const block_id_type& reference_point, uint32_t number_of_blocks_after_reference_point)
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

std::deque<block_id_type>::const_iterator database::find_first_item_not_in_blockchain(const std::deque<block_id_type>& item_hashes_received)
{
  return _fork_db.with_read_lock([&](){
    return std::partition_point(item_hashes_received.begin(), item_hashes_received.end(), [&](const block_id_type& block_id) {
      return is_known_block_unlocked(block_id);
    });
  });
}

// requires forkdb read lock, does not require chainbase lock
bool database::is_included_block_unlocked(const block_id_type& block_id)
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
std::vector<block_id_type> database::get_block_ids(const std::vector<block_id_type>& blockchain_synopsis, uint32_t& remaining_item_count, uint32_t limit)
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

} } //hive::chain

