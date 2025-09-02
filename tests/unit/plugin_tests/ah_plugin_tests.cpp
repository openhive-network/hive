#ifdef IS_TEST_NET
#include <boost/test/unit_test.hpp>

#include <hive/utilities/signal.hpp>

#include <fc/io/json.hpp>
#include <fc/variant_object.hpp>

#include "../db_fixture/clean_database_fixture.hpp"

using namespace hive::chain;
using namespace hive::protocol;
using namespace hive::plugins;
using namespace hive::plugins::account_history_rocksdb;

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
  boost::signals2::connection _post_apply_block;
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
      fixture.account_create_default_fee( "alice", fixture.init_account_pub_key, fixture.init_account_pub_key );
      fixture.account_create_default_fee( "bob", fixture.init_account_pub_key, fixture.init_account_pub_key );
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
        R"~({"type":"account_create_operation","value":{"fee":{"amount":"0","precision":3,"nai":"@@000000021"},"creator":"initminer","new_account_name":"alice","owner":{"weight_threshold":1,"account_auths":[],"key_auths":[["STM6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4",1]]},"active":{"weight_threshold":1,"account_auths":[],"key_auths":[["STM6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4",1]]},"posting":{"weight_threshold":1,"account_auths":[],"key_auths":[["STM6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4",1]]},"memo_key":"STM6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4","json_metadata":""}})~",
        R"~({"type":"account_created_operation","value":{"new_account_name":"alice","creator":"initminer","initial_vesting_shares":{"amount":"0","precision":6,"nai":"@@000000037"},"initial_delegation":{"amount":"0","precision":6,"nai":"@@000000037"}}})~",
        R"~({"type":"comment_operation","value":{"parent_author":"","parent_permlink":"category","author":"alice","permlink":"test","title":"test","body":"test body","json_metadata":""}})~",
        R"~({"type":"vote_operation","value":{"voter":"bob","author":"alice","permlink":"test","weight":10000}})~",
        R"~({"type":"effective_comment_vote_operation","value":{"voter":"bob","author":"alice","permlink":"test","weight":0,"rshares":0,"total_vote_weight":0,"pending_payout":{"amount":"0","precision":3,"nai":"@@000000013"}}})~",
        R"~({"type":"vote_operation","value":{"voter":"alice","author":"bob","permlink":"test","weight":5000}})~",
        R"~({"type":"effective_comment_vote_operation","value":{"voter":"alice","author":"bob","permlink":"test","weight":0,"rshares":0,"total_vote_weight":0,"pending_payout":{"amount":"0","precision":3,"nai":"@@000000013"}}})~",
        R"~({"type":"comment_operation","value":{"parent_author":"alice","parent_permlink":"test","author":"bob","permlink":"reply","title":"reply","body":"I'm replying","json_metadata":""}})~"
      };
      int count = 0;
      fixture.ah_plugin->find_account_history_data( "alice", -1, 100, false, [&]( unsigned int idx, const account_history_rocksdb::rocksdb_operation_object& op ) {
        hive::protocol::operation raw_op = fc::raw::unpack_from_buffer< hive::protocol::operation >( op.serialized_op );
        auto op_str = fc::json::to_string( fc::variant( raw_op ) );
        BOOST_REQUIRE( idx < pattern_alice.size() );
        BOOST_CHECK_EQUAL( op_str, pattern_alice.at( idx ) );
        ++count;
        return true;
      } );
      BOOST_CHECK_EQUAL( count, pattern_alice.size() );
      count = 0;
      std::vector<fc::string> pattern_bob = {
        R"~({"type":"account_create_operation","value":{"fee":{"amount":"0","precision":3,"nai":"@@000000021"},"creator":"initminer","new_account_name":"bob","owner":{"weight_threshold":1,"account_auths":[],"key_auths":[["STM6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4",1]]},"active":{"weight_threshold":1,"account_auths":[],"key_auths":[["STM6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4",1]]},"posting":{"weight_threshold":1,"account_auths":[],"key_auths":[["STM6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4",1]]},"memo_key":"STM6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4","json_metadata":""}})~",
        R"~({"type":"account_created_operation","value":{"new_account_name":"bob","creator":"initminer","initial_vesting_shares":{"amount":"0","precision":6,"nai":"@@000000037"},"initial_delegation":{"amount":"0","precision":6,"nai":"@@000000037"}}})~",
        R"~({"type":"vote_operation","value":{"voter":"bob","author":"alice","permlink":"test","weight":10000}})~",
        R"~({"type":"effective_comment_vote_operation","value":{"voter":"bob","author":"alice","permlink":"test","weight":0,"rshares":0,"total_vote_weight":0,"pending_payout":{"amount":"0","precision":3,"nai":"@@000000013"}}})~",
        R"~({"type":"comment_operation","value":{"parent_author":"","parent_permlink":"category","author":"bob","permlink":"test","title":"test","body":"test body","json_metadata":""}})~",
        R"~({"type":"vote_operation","value":{"voter":"alice","author":"bob","permlink":"test","weight":5000}})~",
        R"~({"type":"effective_comment_vote_operation","value":{"voter":"alice","author":"bob","permlink":"test","weight":0,"rshares":0,"total_vote_weight":0,"pending_payout":{"amount":"0","precision":3,"nai":"@@000000013"}}})~",
        R"~({"type":"comment_operation","value":{"parent_author":"alice","parent_permlink":"test","author":"bob","permlink":"reply","title":"reply","body":"I'm replying","json_metadata":""}})~"
      };
      fixture.ah_plugin->find_account_history_data( "bob", -1, 100, false, [&]( unsigned int idx, const account_history_rocksdb::rocksdb_operation_object& op ) {
        hive::protocol::operation raw_op = fc::raw::unpack_from_buffer< hive::protocol::operation >( op.serialized_op );
        auto op_str = fc::json::to_string( fc::variant( raw_op ) );
        BOOST_REQUIRE( idx < pattern_bob.size() );
        BOOST_CHECK_EQUAL( op_str, pattern_bob.at( idx ) );
        ++count;
        return true;
      } );
      BOOST_CHECK_EQUAL( count, pattern_bob.size() );
    }
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()

#endif
