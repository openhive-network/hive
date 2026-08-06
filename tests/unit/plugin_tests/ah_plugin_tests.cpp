#ifdef IS_TEST_NET
#include <boost/test/unit_test.hpp>

#include <hive/utilities/signal.hpp>

#include <fc/io/json.hpp>
#include <fc/variant_object.hpp>

#include "../db_fixture/clean_database_fixture.hpp"
#include "../db_fixture/witness_fixture.hpp"

#include <boost/filesystem.hpp>

using namespace hive::chain;
using namespace hive::protocol;
using namespace hive::plugins;
using namespace hive::plugins::account_history_rocksdb;

namespace {

/// Immutable table files the account history store currently has on disk.
size_t count_sst_files( const bfs::path& dir )
{
  size_t count = 0;
  for( bfs::directory_iterator it( dir ), end; it != end; ++it )
    if( it->path().extension() == ".sst" )
      ++count;
  return count;
}

} // namespace

BOOST_FIXTURE_TEST_SUITE( ah_plugin_tests, clean_database_fixture )

BOOST_AUTO_TEST_CASE( get_ops_in_block_zero_bug )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing bug in get_ops_in_block with block 0" );

    //instead of using api (which we don't have here and it's a pain to install), we are using AH plugin
    //directly - routine below is used by account_history_api::get_ops_in_block
    ah_plugin->find_operations_by_block( 0, true, []( const rocksdb_operation_object& op )
    {
      BOOST_REQUIRE( false && "List of operations should be empty" );
    } );
    //the bug was with block 0 that was waiting on lock until irreversible state changed, which could
    //take some time (and since unit tests work on single thread this particular test was waiting
    //indefinitely - same happened with stopped node, only live node would give response, but after long
    //time)
    
    BOOST_TEST_MESSAGE( "If you are here it means the test works" );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( enum_virtual_ops_zero_bug )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing bug in enum_virtual_ops with block 0" );

    //the cause and fix is the same as in get_ops_in_block_zero_bug
    ah_plugin->enum_operations_from_block_range( 0, 9, true, 0, 5,
      []( const rocksdb_operation_object&, uint64_t, bool ) { return true; } );

    BOOST_TEST_MESSAGE( "If you are here it means the test works" );
  }
  FC_LOG_AND_RETHROW()
}

struct trigger_bug : appbase::plugin< trigger_bug >
{
  database& _db;
  database::signal_connection_ptr _post_apply_block;
  bool trigger = false;

  trigger_bug( appbase::application& app, database& db ) : appbase::plugin<trigger_bug>(), _db( db )
  {
    _post_apply_block = _db.add_post_apply_block_handler( [this]( const block_notification& )
    {
      if( trigger )
      {
        trigger = false;
        FC_ASSERT( false ); //trigger problem in plugin
      }
    }, *this, -100 );
  }
  virtual ~trigger_bug()
  {
    hive::utilities::disconnect_signal( _post_apply_block );
  }

  static const std::string& name() { static std::string name = "bug"; return name; }
private: //just because it is (almost unused) part of signal registration
  virtual void set_program_options( appbase::options_description& cli, appbase::options_description& cfg ) override {}
  virtual void plugin_for_each_dependency( plugin_processor&& processor ) override {}
  virtual void plugin_initialize( const appbase::variables_map& options ) override {}
  virtual void plugin_startup() override {}
  virtual void plugin_shutdown() override {}
};

BOOST_AUTO_TEST_CASE( mutex_reentry_bug )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing bug where mutex used to stop API during block processing is not unlocked" );

    trigger_bug bug( theApp, *db );

    bug.trigger = true;
    generate_block();
    generate_block(); //second block caused reentry on _currently_processed_block_mtx mutex and
      //deadlock; we could achieve the same effect, although only in unit test, by using AH API
      //asking for reversible data; BTW. "buggy plugin" only works as bug trigger because plugin
      //signals are not isolated yet (see issue#255)

    BOOST_TEST_MESSAGE( "If you are here it means the test works" );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_FIXTURE_TEST_CASE( inconsistent_ah_rocksdb_storage, empty_fixture )
{
  try
  {
    BOOST_TEST_MESSAGE( "Test what happens when AH RocksDB data is inconsistent with rest of state." );
    // The test is a copy of comments_in_external_storage/inconsistent_comment_archive, but with
    // AH RocksDB being affected instead of comment archive

    configuration_data.set_cashout_related_values( 0, 9, 9 * 2, 9 * 7, 3 );
    autoscope( []() { configuration_data.reset_cashout_values(); } );

    fc::path ah_rocksdb_dir;
    {
      clean_database_fixture fixture;
      ah_rocksdb_dir = fixture.ah_plugin->storage_dir();
      fixture.account_create( "alice", fixture.init_account_pub_key, fixture.init_account_pub_key, NO_VESTING );
      fixture.account_create( "bob", fixture.init_account_pub_key, fixture.init_account_pub_key, HIVE_asset( 3'000 ) ); // needs extra vests to cover RC for multiple comments
      fixture.post_comment( "alice", "test", "test", "test body", "category", fixture.init_account_priv_key );
      // first 30 blocks use different LIB mechanics
      fixture.generate_blocks( 3 * HIVE_MAX_WITNESSES );
    }
    fc::temp_directory ah_copy_dir( hive::utilities::temp_directory_path() );
    BOOST_REQUIRE( ah_copy_dir.path().is_absolute() ); // if it wasn't absolute, it would later "try to place itself" inside data_dir
    fc::copy( ah_rocksdb_dir, ah_copy_dir.path() );
    {
      hived_fixture fixture( false );
      fixture.postponed_init( { hived_fixture::config_line_t( { "plugin", { HIVE_ACCOUNT_HISTORY_ROCKSDB_PLUGIN_NAME } } ) } );
      fixture.vote( "alice", "test", "bob", HIVE_100_PERCENT, fixture.init_account_priv_key );
      fixture.generate_block();
      fixture.post_comment( "bob", "test", "test", "test body", "category", fixture.init_account_priv_key );
      fixture.generate_blocks( 2 * HIVE_MAX_WITNESSES );
    }
    // try to open with AH copied before - LIB will be inconsistent (and content as well)
    {
      hived_fixture fixture( false );
      HIVE_REQUIRE_ASSERT( fixture.postponed_init( {
        hived_fixture::config_line_t( { "plugin", { HIVE_ACCOUNT_HISTORY_ROCKSDB_PLUGIN_NAME } } ),
        hived_fixture::config_line_t( { "account-history-rocksdb-path", { ah_copy_dir.path().generic_string() }} )
      } ), "lib == _cached_irreversible_block" );
    }
    // try to open with AH in completely new directory - LIB will be fresh 0 so also inconsistent
    {
      fc::temp_directory ah_empty_dir( hive::utilities::temp_directory_path() );
      hived_fixture fixture( false );
      HIVE_REQUIRE_ASSERT( fixture.postponed_init( {
        hived_fixture::config_line_t( { "plugin", { HIVE_ACCOUNT_HISTORY_ROCKSDB_PLUGIN_NAME } } ),
        hived_fixture::config_line_t( { "account-history-rocksdb-path", { ah_empty_dir.path().generic_string() }} )
      } ), "0 == _cached_irreversible_block" );
    }
    // restart node with AH in normal location and continue (previous tries should not break anything in state)
    {
      hived_fixture fixture( false );
      fixture.postponed_init( { hived_fixture::config_line_t( { "plugin", { HIVE_ACCOUNT_HISTORY_ROCKSDB_PLUGIN_NAME } } ) } );
      fixture.vote( "bob", "test", "alice", 50 * HIVE_1_PERCENT, fixture.init_account_priv_key );
      fixture.post_comment_to_comment( "bob", "reply", "reply", "I'm replying", "alice", "test", fixture.init_account_priv_key );
      fixture.generate_block();
      auto head_block_num = fixture.db->head_block_num();
      fixture.generate_until_irreversible_block( head_block_num );

      // check existence of all previous operations
      std::vector<fc::string> pattern_alice = {
        R"~({"type":"account_create_operation","value":{"fee":{"amount":"3000","precision":3,"nai":"@@000000021"},"creator":"initminer","new_account_name":"alice","owner":{"weight_threshold":1,"account_auths":[],"key_auths":[["STM6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4",1]]},"active":{"weight_threshold":1,"account_auths":[],"key_auths":[["STM6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4",1]]},"posting":{"weight_threshold":1,"account_auths":[],"key_auths":[["STM6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4",1]]},"memo_key":"STM6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4","json_metadata":""}})~",
        R"~({"type":"account_created_operation","value":{"new_account_name":"alice","creator":"initminer","initial_vesting_shares":{"amount":"0","precision":6,"nai":"@@000000037"},"initial_delegation":{"amount":"0","precision":6,"nai":"@@000000037"}}})~",
        R"~({"type":"comment_operation","value":{"parent_author":"","parent_permlink":"category","author":"alice","permlink":"test","title":"test","body":"test body","json_metadata":""}})~",
        R"~({"type":"vote_operation","value":{"voter":"bob","author":"alice","permlink":"test","weight":10000}})~",
        R"~({"type":"effective_comment_vote_operation","value":{"voter":"bob","author":"alice","permlink":"test","weight":58000000,"rshares":58000000,"total_vote_weight":58000000,"pending_payout":{"amount":"0","precision":3,"nai":"@@000000013"}}})~",
        R"~({"type":"vote_operation","value":{"voter":"alice","author":"bob","permlink":"test","weight":5000}})~",
        R"~({"type":"effective_comment_vote_operation","value":{"voter":"alice","author":"bob","permlink":"test","weight":0,"rshares":0,"total_vote_weight":0,"pending_payout":{"amount":"0","precision":3,"nai":"@@000000013"}}})~",
        R"~({"type":"comment_operation","value":{"parent_author":"alice","parent_permlink":"test","author":"bob","permlink":"reply","title":"reply","body":"I'm replying","json_metadata":""}})~"
      };
      int count = 0;
      fixture.ah_plugin->find_account_history_data( "alice", -1, 100, false, [&]( unsigned int idx, const account_history_rocksdb::rocksdb_operation_object& op ) {
        hive::protocol::operation raw_op = fc::raw::unpack_from_buffer< hive::protocol::operation >( op.serialized_op );
        auto op_str = fc::json::to_string( fc::variant( raw_op ) );
        BOOST_REQUIRE_LT( idx, pattern_alice.size() );
        BOOST_CHECK_EQUAL( op_str, pattern_alice.at( idx ) );
        ++count;
        return true;
      } );
      BOOST_CHECK_EQUAL( count, pattern_alice.size() );
      count = 0;
      std::vector<fc::string> pattern_bob = {
        R"~({"type":"account_create_operation","value":{"fee":{"amount":"3000","precision":3,"nai":"@@000000021"},"creator":"initminer","new_account_name":"bob","owner":{"weight_threshold":1,"account_auths":[],"key_auths":[["STM6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4",1]]},"active":{"weight_threshold":1,"account_auths":[],"key_auths":[["STM6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4",1]]},"posting":{"weight_threshold":1,"account_auths":[],"key_auths":[["STM6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4",1]]},"memo_key":"STM6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4","json_metadata":""}})~",
        R"~({"type":"account_created_operation","value":{"new_account_name":"bob","creator":"initminer","initial_vesting_shares":{"amount":"0","precision":6,"nai":"@@000000037"},"initial_delegation":{"amount":"0","precision":6,"nai":"@@000000037"}}})~",
        R"~({"type":"transfer_to_vesting_operation","value":{"from":"initminer","to":"bob","amount":{"amount":"3000","precision":3,"nai":"@@000000021"}}})~",
        R"~({"type":"transfer_to_vesting_completed_operation","value":{"from_account":"initminer","to_account":"bob","hive_vested":{"amount":"3000","precision":3,"nai":"@@000000021"},"vesting_shares_received":{"amount":"5399999980","precision":6,"nai":"@@000000037"}}})~",
        R"~({"type":"vote_operation","value":{"voter":"bob","author":"alice","permlink":"test","weight":10000}})~",
        R"~({"type":"effective_comment_vote_operation","value":{"voter":"bob","author":"alice","permlink":"test","weight":58000000,"rshares":58000000,"total_vote_weight":58000000,"pending_payout":{"amount":"0","precision":3,"nai":"@@000000013"}}})~",
        R"~({"type":"comment_operation","value":{"parent_author":"","parent_permlink":"category","author":"bob","permlink":"test","title":"test","body":"test body","json_metadata":""}})~",
        R"~({"type":"vote_operation","value":{"voter":"alice","author":"bob","permlink":"test","weight":5000}})~",
        R"~({"type":"effective_comment_vote_operation","value":{"voter":"alice","author":"bob","permlink":"test","weight":0,"rshares":0,"total_vote_weight":0,"pending_payout":{"amount":"0","precision":3,"nai":"@@000000013"}}})~",
        R"~({"type":"comment_operation","value":{"parent_author":"alice","parent_permlink":"test","author":"bob","permlink":"reply","title":"reply","body":"I'm replying","json_metadata":""}})~"
      };
      fixture.ah_plugin->find_account_history_data( "bob", -1, 100, false, [&]( unsigned int idx, const account_history_rocksdb::rocksdb_operation_object& op ) {
        hive::protocol::operation raw_op = fc::raw::unpack_from_buffer< hive::protocol::operation >( op.serialized_op );
        auto op_str = fc::json::to_string( fc::variant( raw_op ) );
        BOOST_REQUIRE_LT( idx, pattern_bob.size() );
        BOOST_CHECK_EQUAL( op_str, pattern_bob.at( idx ) );
        ++count;
        return true;
      } );
      BOOST_CHECK_EQUAL( count, pattern_bob.size() );
    }
  }
  FC_LOG_AND_RETHROW()
}

/**
  * Applying blocks must not make the account history store create files.
  *
  * Producing blocks with colony driving queen, with the flush interval set to 1 so state - and
  * with it the external storages - is flushed after every block, the way a live mainnet node
  * runs. The store's data goes into its write batch and its write-ahead log on every block, but
  * the number of immutable table files it holds must follow how much data was written, not how
  * many blocks went by. Before the fix each of those per-block flushes forced every column family
  * out into its own file, so the file count - and the descriptor held per file - grew with the
  * block count until the node ran out of descriptors (issue #869).
  */
BOOST_FIXTURE_TEST_CASE( ah_rocksdb_files_do_not_track_block_count, witness_fixture )
{
  try
  {
    bool test_passed = false;

    BOOST_SCOPE_EXIT( this_ )
    {
      this_->theApp.generate_interrupt_request();
      this_->theApp.wait4interrupt_request();
      this_->theApp.quit( true );
    } BOOST_SCOPE_EXIT_END

    const uint32_t COLONY_START = 25;
    const uint32_t MEASURED_BLOCKS = 60;
    const int WORKERS = 60;

    initialize( 3, {}, {}, // queen ignores configured witnesses and only cares about the signing key
      {
        config_line_t( { "plugin", { HIVE_ACCOUNT_HISTORY_ROCKSDB_PLUGIN_NAME } } ),
        // Emulate live mode: there notify_end_of_syncing() switches the interval to
        // flush-state-interval-live, which defaults to flushing after every block. That switch is
        // compiled out of testnet builds, so the test has to ask for the cadence explicitly.
        config_line_t( { "flush-state-interval", { "1" } } ),
        config_line_t( { "plugin", { HIVE_COLONY_PLUGIN_NAME } } ),
        config_line_t( { "colony-sign-with", { init_account_priv_key.key_to_wif() } } ),
        config_line_t( { "colony-start-at-block", { std::to_string( COLONY_START ) } } ),
        config_line_t( { "colony-threads", { "2" } } ),
        // unlimited: rate limited colony and queen deadlock each other, since colony waits for a
        // block before producing more and queen waits for its transaction count, which a single
        // rejected transaction is then enough to keep it short of forever
        config_line_t( { "colony-transactions-per-block", { "0" } } ),
        config_line_t( { "colony-no-broadcast", { "1" } } ),
        // transfers only - no comments needed as targets, and workers sign with their active authority
        config_line_t( { "colony-transfer", { R"~({"min":0,"max":350,"weight":10000,"exponent":4})~" } } ),
        config_line_t( { "plugin", { HIVE_QUEEN_PLUGIN_NAME } } ),
        config_line_t( { "queen-tx-count", { "50" } } )
      }, "1G" );

    BOOST_REQUIRE( ah_plugin );

    generate_block(); // add block to trigger hardforks

    fc::thread api_thread;
    auto _future = api_thread.async( [&]()
    {
      BOOST_SCOPE_EXIT( this_ ) { this_->theApp.generate_interrupt_request(); } BOOST_SCOPE_EXIT_END
      try
      {
        BOOST_REQUIRE( db->has_hardfork( HIVE_NUM_HARDFORKS ) );

        // colony and queen rarely leave room for anything outside the write lock, so the head
        // block number is read without one - the exact value does not matter here
        auto wait_for_block = [&]( uint32_t target, uint32_t timeout_seconds )
        {
          const auto deadline = fc::time_point::now() + fc::seconds( timeout_seconds );
          uint32_t block_num = db->head_block_num();
          while( block_num < target && !theApp.is_interrupt_request() && fc::time_point::now() < deadline )
          {
            fc::usleep( fc::milliseconds( 50 ) );
            block_num = db->head_block_num();
          }
          BOOST_REQUIRE_MESSAGE( block_num >= target || theApp.is_interrupt_request(),
            "Timed out waiting for block " + std::to_string( target ) );
          return block_num;
        };

        ilog( "Creating accounts for colony to work with" );
        for( int i = 0; i < WORKERS && !theApp.is_interrupt_request(); ++i )
        {
          const auto worker = "worker" + std::to_string( i );
          schedule_account_create( worker );
          schedule_vest( worker, ASSET( "10000.000 TESTS" ) ); // enough RC to keep colony going
          schedule_fund( worker, ASSET( "100.000 TBD" ) );
        }

        uint32_t block_num = get_block_num();
        BOOST_REQUIRE_LT( block_num, COLONY_START - 2 ); // preparation has to fit before colony starts
        schedule_blocks( COLONY_START + 2 - block_num ); // push past the colony activation point

        // let colony and queen settle into producing before taking the first sample
        block_num = wait_for_block( COLONY_START + 5, 120 );

        const bfs::path ah_dir = ah_plugin->storage_dir();
        const size_t files_before = count_sst_files( ah_dir );
        const uint32_t first_block = block_num;

        const uint32_t last_block = wait_for_block( first_block + MEASURED_BLOCKS, 300 );
        const size_t files_after = count_sst_files( ah_dir );
        const uint32_t blocks = last_block - first_block;

        BOOST_REQUIRE_GE( blocks, MEASURED_BLOCKS );

        // compaction can only merge files away, so the difference is signed on purpose
        const long new_files = static_cast< long >( files_after ) - static_cast< long >( files_before );
        ilog( "Account history store went from ${before} to ${after} table files over ${blocks} blocks.",
          ( "before", files_before )( "after", files_after )( blocks ) );

        // Before the fix this was at least one new file per column family per block. The bound is
        // deliberately phrased against the block count rather than an absolute number: what must
        // not happen is file creation tracking block production.
        BOOST_REQUIRE_LE( new_files, static_cast< long >( blocks / 10 ) );

        // and the data is in the store, not merely unwritten - read it back from RocksDB alone,
        // with no reversible data involved
        unsigned int block_ops = 0;
        ah_plugin->find_operations_by_block( first_block, false,
          [&]( const rocksdb_operation_object& ) { ++block_ops; } );
        unsigned int account_ops = 0;
        ah_plugin->find_account_history_data( "worker0", -1, 100, false,
          [&]( unsigned int, const rocksdb_operation_object& ) { ++account_ops; return true; } );
        ilog( "Account history read back from the store: ${b} operations in block ${n}, "
          "${a} for worker0.", ( "b", block_ops )( "n", first_block )( "a", account_ops ) );
        BOOST_REQUIRE_GT( block_ops, 0u );
        BOOST_REQUIRE_GT( account_ops, 0u );

        test_passed = !theApp.is_interrupt_request();
      }
      CATCH( "API" )
    } );

    _future.wait();
    theApp.wait4interrupt_request();
    theApp.quit( true );
    db = nullptr; // prevent fixture destructor from accessing database after it was closed
    BOOST_REQUIRE( test_passed );
    ilog( "Test done" );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()

#endif
