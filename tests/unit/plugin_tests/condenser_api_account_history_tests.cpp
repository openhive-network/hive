#if defined IS_TEST_NET

#include "condenser_api_fixture.hpp"

#include <hive/plugins/account_history_api/account_history_api_plugin.hpp>
#include <hive/plugins/account_history_api/account_history_api.hpp>
#include <hive/plugins/database_api/database_api_plugin.hpp>
#include <hive/plugins/condenser_api/condenser_api_plugin.hpp>
#include <hive/plugins/condenser_api/condenser_api.hpp>

#include <fc/io/json.hpp>

#include <boost/test/unit_test.hpp>

// account history API -> where it's used in condenser API implementation
//  get_ops_in_block -> get_ops_in_block
//  get_transaction -> ditto get_transaction
//  get_account_history -> ditto get_account_history
//  enum_virtual_ops -> not used

/// Account history pattern goes first in the pair, condenser version pattern follows.
typedef std::vector< std::pair< std::string, std::string > > expected_t;

BOOST_FIXTURE_TEST_SUITE( condenser_get_account_history_tests, condenser_api_fixture );

void test_get_account_history( const condenser_api_fixture& caf, const std::vector< std:: string >& account_names, const std::vector< expected_t >& expected_operations,
  uint64_t start = 1000, uint32_t limit = 1000, uint64_t filter_low = 0xFFFFFFFF'FFFFFFFFull, uint64_t filter_high = 0xFFFFFFFF'FFFFFFFFull )
{
  // For each requested account ...
  BOOST_REQUIRE_EQUAL( expected_operations.size(), account_names.size() );
  for( size_t account_index = 0; account_index < account_names.size(); ++account_index)
  {
    const auto& account_name = account_names[ account_index ];
    const auto& expected_for_account = expected_operations[ account_index ];

    auto ah1 = caf.account_history_api->get_account_history( {account_name, start, limit, false /*include_reversible*/, filter_low, filter_high } );
    auto ah2 = caf.condenser_api->get_account_history( condenser_api::get_account_history_args( {account_name, start, limit, filter_low, filter_high} ) );
    BOOST_REQUIRE_EQUAL( ah1.history.size(), ah2.size() );
    BOOST_REQUIRE_EQUAL( expected_for_account.size(), ah2.size() );
    ilog( "${n} operation(s) in account ${account} history", ("n", ah2.size())("account", account_name) );

    // For each event (operation) in account history ...
    auto it_ah = ah1.history.begin();
    auto it_cn = ah2.begin();
    for( size_t op_index = 0; it_cn != ah2.end(); ++it_ah, ++it_cn, ++op_index )
    {
      ilog("ah op: ${op}", ("op", *it_ah));
      ilog("cn op: ${op}", ("op", *it_cn));

      // Compare operations in their serialized form with expected patterns:
      const auto expected = expected_for_account[ op_index ];
      BOOST_REQUIRE_EQUAL( expected.first, fc::json::to_string(*it_ah) );
      BOOST_REQUIRE_EQUAL( expected.second, fc::json::to_string(*it_cn) );
    }

    // Do additional checks of condenser variant
    // Too few arguments
    BOOST_REQUIRE_THROW( caf.condenser_api->get_account_history( condenser_api::get_account_history_args( {account_name, 100 /*start*/ } ) ), fc::assert_exception );
    // Too many arguments
    BOOST_REQUIRE_THROW( caf.condenser_api->get_account_history( condenser_api::get_account_history_args( 
      {account_name, 100 /*start*/, 100 /*limit*/, 50 /*filter_low*/, 200 /*filter_high*/, 100 /*redundant*/ } ) ), fc::assert_exception );
  }
}

#define GET_LOW_OPERATION(OPERATION) static_cast<uint64_t>( account_history::get_account_history_op_filter_low::OPERATION )
#define GET_HIGH_OPERATION(OPERATION) static_cast<uint64_t>( account_history::get_account_history_op_filter_high::OPERATION )

// Uses hf1_scenario to test additional parameters of get_account_history.
BOOST_AUTO_TEST_CASE( get_account_history_hf1 )
{ try {

  BOOST_TEST_MESSAGE( "testing get_account_history with hf1_scenario (and different args)" );

  auto check_point_tester = [ this ]( uint32_t generate_no_further_than )
  {
    generate_until_irreversible_block( 21 );
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );

    // Check all low, except hardfork_operation.
    uint64_t filter_low = -1ull & ~GET_LOW_OPERATION( hardfork_operation );

    // Filter out producer_reward_operation
    expected_t expected_initminer_history = { {
      R"~([0,{"trx_id":"0000000000000000000000000000000000000000","block":1,"trx_in_block":4294967295,"op_in_trx":6,"virtual_op":true,"timestamp":"2016-01-01T00:00:00","op":{"type":"account_created_operation","value":{"new_account_name":"initminer","creator":"initminer","initial_vesting_shares":{"amount":"0","precision":6,"nai":"@@000000037"},"initial_delegation":{"amount":"0","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([0,{"trx_id":"0000000000000000000000000000000000000000","block":1,"trx_in_block":4294967295,"op_in_trx":6,"virtual_op":true,"timestamp":"2016-01-01T00:00:00","op":["account_created",{"new_account_name":"initminer","creator":"initminer","initial_vesting_shares":"0.000000 VESTS","initial_delegation":"0.000000 VESTS"}]}])~"
      }, {
      R"~([4,{"trx_id":"0000000000000000000000000000000000000000","block":2,"trx_in_block":4294967295,"op_in_trx":3,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":{"type":"vesting_shares_split_operation","value":{"owner":"initminer","vesting_shares_before_split":{"amount":"1000000","precision":6,"nai":"@@000000037"},"vesting_shares_after_split":{"amount":"1000000000000","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([4,{"trx_id":"0000000000000000000000000000000000000000","block":2,"trx_in_block":4294967295,"op_in_trx":3,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":["vesting_shares_split",{"owner":"initminer","vesting_shares_before_split":"1.000000 VESTS","vesting_shares_after_split":"1000000.000000 VESTS"}]}])~"
      }, {
      R"~([23,{"trx_id":"0000000000000000000000000000000000000000","block":21,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:01:03","op":{"type":"system_warning_operation","value":{"message":"Changing maximum block size from 2097152 to 131072"}},"operation_id":0}])~",
      R"~([23,{"trx_id":"0000000000000000000000000000000000000000","block":21,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:01:03","op":["system_warning",{"message":"Changing maximum block size from 2097152 to 131072"}]}])~"
      } };
    uint64_t filter_high = -1ull & ~GET_HIGH_OPERATION( producer_reward_operation );
    test_get_account_history( *this, { "initminer" }, { expected_initminer_history }, 1000, 1000,
                              filter_low /*all low*/, filter_high /*all high except producer_reward_operation*/ );

    // Filter out producer_reward_operation & system_warning_operation
    expected_initminer_history = { {
      R"~([0,{"trx_id":"0000000000000000000000000000000000000000","block":1,"trx_in_block":4294967295,"op_in_trx":6,"virtual_op":true,"timestamp":"2016-01-01T00:00:00","op":{"type":"account_created_operation","value":{"new_account_name":"initminer","creator":"initminer","initial_vesting_shares":{"amount":"0","precision":6,"nai":"@@000000037"},"initial_delegation":{"amount":"0","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([0,{"trx_id":"0000000000000000000000000000000000000000","block":1,"trx_in_block":4294967295,"op_in_trx":6,"virtual_op":true,"timestamp":"2016-01-01T00:00:00","op":["account_created",{"new_account_name":"initminer","creator":"initminer","initial_vesting_shares":"0.000000 VESTS","initial_delegation":"0.000000 VESTS"}]}])~"
      }, {
      R"~([4,{"trx_id":"0000000000000000000000000000000000000000","block":2,"trx_in_block":4294967295,"op_in_trx":3,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":{"type":"vesting_shares_split_operation","value":{"owner":"initminer","vesting_shares_before_split":{"amount":"1000000","precision":6,"nai":"@@000000037"},"vesting_shares_after_split":{"amount":"1000000000000","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([4,{"trx_id":"0000000000000000000000000000000000000000","block":2,"trx_in_block":4294967295,"op_in_trx":3,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":["vesting_shares_split",{"owner":"initminer","vesting_shares_before_split":"1.000000 VESTS","vesting_shares_after_split":"1000000.000000 VESTS"}]}])~"
      } };
    filter_high &= ~GET_HIGH_OPERATION( system_warning_operation );
    test_get_account_history( *this, { "initminer" }, { expected_initminer_history }, 1000, 1000,
                              filter_low /*all low*/, filter_high /*all high except producer_reward_operation & system_warning_operation*/ );

    // Filter out producer_reward_operation, system_warning_operation & account_created_operation
    expected_initminer_history = { {
      R"~([4,{"trx_id":"0000000000000000000000000000000000000000","block":2,"trx_in_block":4294967295,"op_in_trx":3,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":{"type":"vesting_shares_split_operation","value":{"owner":"initminer","vesting_shares_before_split":{"amount":"1000000","precision":6,"nai":"@@000000037"},"vesting_shares_after_split":{"amount":"1000000000000","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([4,{"trx_id":"0000000000000000000000000000000000000000","block":2,"trx_in_block":4294967295,"op_in_trx":3,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":["vesting_shares_split",{"owner":"initminer","vesting_shares_before_split":"1.000000 VESTS","vesting_shares_after_split":"1000000.000000 VESTS"}]}])~"
      } };
    filter_high &= ~GET_HIGH_OPERATION( account_created_operation );
    test_get_account_history( *this, { "initminer" }, { expected_initminer_history }, 1000, 1000,
                              filter_low /*all low*/, filter_high /*all high except producer_reward_operation, system_warning_operation & account_created_operation*/ );

    // Pick vesting_shares_split_operation and surounding producer_reward_operation using start/limit.
    expected_initminer_history = { {
      R"~([22,{"trx_id":"0000000000000000000000000000000000000000","block":20,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:01:00","op":{"type":"producer_reward_operation","value":{"producer":"initminer","vesting_shares":{"amount":"1000","precision":3,"nai":"@@000000021"}}},"operation_id":0}])~",
      R"~([22,{"trx_id":"0000000000000000000000000000000000000000","block":20,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:01:00","op":["producer_reward",{"producer":"initminer","vesting_shares":"1.000 TESTS"}]}])~"
      }, {
      R"~([23,{"trx_id":"0000000000000000000000000000000000000000","block":21,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:01:03","op":{"type":"system_warning_operation","value":{"message":"Changing maximum block size from 2097152 to 131072"}},"operation_id":0}])~",
      R"~([23,{"trx_id":"0000000000000000000000000000000000000000","block":21,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:01:03","op":["system_warning",{"message":"Changing maximum block size from 2097152 to 131072"}]}])~"
      }, {
      R"~([24,{"trx_id":"0000000000000000000000000000000000000000","block":21,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:01:03","op":{"type":"producer_reward_operation","value":{"producer":"initminer","vesting_shares":{"amount":"1000","precision":3,"nai":"@@000000021"}}},"operation_id":0}])~",
      R"~([24,{"trx_id":"0000000000000000000000000000000000000000","block":21,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:01:03","op":["producer_reward",{"producer":"initminer","vesting_shares":"1.000 TESTS"}]}])~"
      } };
    test_get_account_history( *this, { "initminer" }, { expected_initminer_history }, 24, 3 );
  };

  hf1_scenario( check_point_tester );

} FC_LOG_AND_RETHROW() }

/**
 * Uses hf23_scenario scenario to test:
 * - the history of null account (creation, transfer to & asset burning / clear_null_account_balance_operation)
 * - transfer of HBD/TBD
 * - transfer with non-empty memo
 * - clearing & restoring suspicious account (hardfork_hive(_restore)_operation)
 * - transfer to vesting to another account.
 */
BOOST_AUTO_TEST_CASE( get_account_history_hf23 )
{ try {

  BOOST_TEST_MESSAGE( "testing get_account_history with hf23_scenario" );

  auto check_point_tester = [ this ]( uint32_t generate_no_further_than )
  {
    generate_until_irreversible_block( 6 );
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );

    expected_t expected_steemflower_history = { {
      R"~([0,{"trx_id":"da8ac5d9bb25c37c7d3846799344976a24d72d3d","block":3,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":{"type":"account_create_operation","value":{"fee":{"amount":"0","precision":3,"nai":"@@000000021"},"creator":"initminer","new_account_name":"steemflower","owner":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST6V7YxA9xvhM6QK5HzAjnMGxPEzpLNrbCvxv9pkvqJhJgst5xNL",1]]},"active":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST6V7YxA9xvhM6QK5HzAjnMGxPEzpLNrbCvxv9pkvqJhJgst5xNL",1]]},"posting":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST55Mrt8AaKKaH3WsggFF6iRqyXH1ijNCHcNqAaD6E1j5PcRG4FG",1]]},"memo_key":"TST6V7YxA9xvhM6QK5HzAjnMGxPEzpLNrbCvxv9pkvqJhJgst5xNL","json_metadata":""}},"operation_id":0}])~",
      R"~([0,{"trx_id":"da8ac5d9bb25c37c7d3846799344976a24d72d3d","block":3,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":["account_create",{"fee":"0.000 TESTS","creator":"initminer","new_account_name":"steemflower","owner":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST6V7YxA9xvhM6QK5HzAjnMGxPEzpLNrbCvxv9pkvqJhJgst5xNL",1]]},"active":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST6V7YxA9xvhM6QK5HzAjnMGxPEzpLNrbCvxv9pkvqJhJgst5xNL",1]]},"posting":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST55Mrt8AaKKaH3WsggFF6iRqyXH1ijNCHcNqAaD6E1j5PcRG4FG",1]]},"memo_key":"TST6V7YxA9xvhM6QK5HzAjnMGxPEzpLNrbCvxv9pkvqJhJgst5xNL","json_metadata":""}]}])~"
      }, {
      R"~([1,{"trx_id":"da8ac5d9bb25c37c7d3846799344976a24d72d3d","block":3,"trx_in_block":0,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":{"type":"account_created_operation","value":{"new_account_name":"steemflower","creator":"initminer","initial_vesting_shares":{"amount":"0","precision":6,"nai":"@@000000037"},"initial_delegation":{"amount":"0","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([1,{"trx_id":"da8ac5d9bb25c37c7d3846799344976a24d72d3d","block":3,"trx_in_block":0,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":["account_created",{"new_account_name":"steemflower","creator":"initminer","initial_vesting_shares":"0.000000 VESTS","initial_delegation":"0.000000 VESTS"}]}])~"
      }, {
      R"~([2,{"trx_id":"bba790da82d142585bb32d9c1f8d7b037f9e8c8b","block":3,"trx_in_block":1,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":{"type":"transfer_to_vesting_operation","value":{"from":"initminer","to":"steemflower","amount":{"amount":"100","precision":3,"nai":"@@000000021"}}},"operation_id":0}])~",
      R"~([2,{"trx_id":"bba790da82d142585bb32d9c1f8d7b037f9e8c8b","block":3,"trx_in_block":1,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":["transfer_to_vesting",{"from":"initminer","to":"steemflower","amount":"0.100 TESTS"}]}])~"
      }, {
      R"~([3,{"trx_id":"bba790da82d142585bb32d9c1f8d7b037f9e8c8b","block":3,"trx_in_block":1,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":{"type":"transfer_to_vesting_completed_operation","value":{"from_account":"initminer","to_account":"steemflower","hive_vested":{"amount":"100","precision":3,"nai":"@@000000021"},"vesting_shares_received":{"amount":"98716683119","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([3,{"trx_id":"bba790da82d142585bb32d9c1f8d7b037f9e8c8b","block":3,"trx_in_block":1,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":["transfer_to_vesting_completed",{"from_account":"initminer","to_account":"steemflower","hive_vested":"0.100 TESTS","vesting_shares_received":"98716.683119 VESTS"}]}])~"
      }, {
      R"~([4,{"trx_id":"fc9573c39c9d474b812e4f44beea6cc4d02b981e","block":5,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":{"type":"transfer_operation","value":{"from":"steemflower","to":"null","amount":{"amount":"12","precision":3,"nai":"@@000000013"},"memo":"For flowers :)"}},"operation_id":0}])~",
      R"~([4,{"trx_id":"fc9573c39c9d474b812e4f44beea6cc4d02b981e","block":5,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":["transfer",{"from":"steemflower","to":"null","amount":"0.012 TBD","memo":"For flowers :)"}]}])~"
      }, {
      R"~([5,{"trx_id":"0000000000000000000000000000000000000000","block":5,"trx_in_block":4294967295,"op_in_trx":5,"virtual_op":true,"timestamp":"2016-01-01T00:00:15","op":{"type":"hardfork_hive_operation","value":{"account":"steemflower","treasury":"steem.dao","other_affected_accounts":[],"hbd_transferred":{"amount":"0","precision":3,"nai":"@@000000013"},"hive_transferred":{"amount":"0","precision":3,"nai":"@@000000021"},"vests_converted":{"amount":"0","precision":6,"nai":"@@000000037"},"total_hive_from_vests":{"amount":"0","precision":3,"nai":"@@000000021"}}},"operation_id":0}])~",
      R"~([5,{"trx_id":"0000000000000000000000000000000000000000","block":5,"trx_in_block":4294967295,"op_in_trx":5,"virtual_op":true,"timestamp":"2016-01-01T00:00:15","op":["hardfork_hive",{"account":"steemflower","treasury":"steem.dao","other_affected_accounts":[],"hbd_transferred":"0.000 TBD","hive_transferred":"0.000 TESTS","vests_converted":"0.000000 VESTS","total_hive_from_vests":"0.000 TESTS"}]}])~"
      }, {
      R"~([6,{"trx_id":"0000000000000000000000000000000000000000","block":6,"trx_in_block":4294967295,"op_in_trx":3,"virtual_op":true,"timestamp":"2016-01-01T00:00:18","op":{"type":"hardfork_hive_restore_operation","value":{"account":"steemflower","treasury":"steem.dao","hbd_transferred":{"amount":"123456789000","precision":3,"nai":"@@000000013"},"hive_transferred":{"amount":"0","precision":3,"nai":"@@000000021"}}},"operation_id":0}])~",
      R"~([6,{"trx_id":"0000000000000000000000000000000000000000","block":6,"trx_in_block":4294967295,"op_in_trx":3,"virtual_op":true,"timestamp":"2016-01-01T00:00:18","op":["hardfork_hive_restore",{"account":"steemflower","treasury":"steem.dao","hbd_transferred":"123456789.000 TBD","hive_transferred":"0.000 TESTS"}]}])~"
      } };

    expected_t expected_null_history = { {
      R"~([0,{"trx_id":"0000000000000000000000000000000000000000","block":1,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:00:00","op":{"type":"account_created_operation","value":{"new_account_name":"null","creator":"null","initial_vesting_shares":{"amount":"0","precision":6,"nai":"@@000000037"},"initial_delegation":{"amount":"0","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([0,{"trx_id":"0000000000000000000000000000000000000000","block":1,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:00:00","op":["account_created",{"new_account_name":"null","creator":"null","initial_vesting_shares":"0.000000 VESTS","initial_delegation":"0.000000 VESTS"}]}])~"
      }, {
      R"~([1,{"trx_id":"fc9573c39c9d474b812e4f44beea6cc4d02b981e","block":5,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":{"type":"transfer_operation","value":{"from":"steemflower","to":"null","amount":{"amount":"12","precision":3,"nai":"@@000000013"},"memo":"For flowers :)"}},"operation_id":0}])~",
      R"~([1,{"trx_id":"fc9573c39c9d474b812e4f44beea6cc4d02b981e","block":5,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":["transfer",{"from":"steemflower","to":"null","amount":"0.012 TBD","memo":"For flowers :)"}]}])~"
      }, {
      R"~([2,{"trx_id":"0000000000000000000000000000000000000000","block":5,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:15","op":{"type":"clear_null_account_balance_operation","value":{"total_cleared":[{"amount":"12","precision":3,"nai":"@@000000013"}]}},"operation_id":0}])~",
      R"~([2,{"trx_id":"0000000000000000000000000000000000000000","block":5,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:15","op":["clear_null_account_balance",{"total_cleared":["0.012 TBD"]}]}])~"
      } };

    test_get_account_history( *this, { "steemflower", HIVE_NULL_ACCOUNT }, { expected_steemflower_history, expected_null_history } );

    expected_t expected_initminer_history = { {
      R"~([28,{"trx_id":"bba790da82d142585bb32d9c1f8d7b037f9e8c8b","block":3,"trx_in_block":1,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":{"type":"transfer_to_vesting_operation","value":{"from":"initminer","to":"steemflower","amount":{"amount":"100","precision":3,"nai":"@@000000021"}}},"operation_id":0}])~",
      R"~([28,{"trx_id":"bba790da82d142585bb32d9c1f8d7b037f9e8c8b","block":3,"trx_in_block":1,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":["transfer_to_vesting",{"from":"initminer","to":"steemflower","amount":"0.100 TESTS"}]}])~"
      }, {
      R"~([29,{"trx_id":"bba790da82d142585bb32d9c1f8d7b037f9e8c8b","block":3,"trx_in_block":1,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":{"type":"transfer_to_vesting_completed_operation","value":{"from_account":"initminer","to_account":"steemflower","hive_vested":{"amount":"100","precision":3,"nai":"@@000000021"},"vesting_shares_received":{"amount":"98716683119","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([29,{"trx_id":"bba790da82d142585bb32d9c1f8d7b037f9e8c8b","block":3,"trx_in_block":1,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":["transfer_to_vesting_completed",{"from_account":"initminer","to_account":"steemflower","hive_vested":"0.100 TESTS","vesting_shares_received":"98716.683119 VESTS"}]}])~"
      } };

    // Let's see transfer to vesting's operation "from" account being impacted too.
    test_get_account_history( *this, { "initminer" }, { expected_initminer_history }, 1000, 1000, 
      GET_LOW_OPERATION( transfer_to_vesting_operation ), GET_HIGH_OPERATION( transfer_to_vesting_completed_operation ) );
  };

  hf23_scenario( check_point_tester );

} FC_LOG_AND_RETHROW() }

// TODO create get_account_history_hf8_test here
// TODO create get_account_history_hf12 test here
// TODO Create get_account_history_hf13 here
// TODO create get_account_history_witness test here
// TODO create get_account_history_escrow_and_savings test here
// TODO create get_account_history_proposal test here
// TODO create get_account_history_account test here
// TODO create get_account_history_custom test here
// TODO create get_account_history_recurrent_transfer test here
// TODO create get_account_history_decline_voting_rights test here

BOOST_AUTO_TEST_CASE( get_account_history_comment_and_reward )
{ try {

  BOOST_TEST_MESSAGE( "testing get_account_history with comment_and_reward_scenario" );

  // Generate a number of blocks sufficient that following claim_reward_operation succeeds.
  auto check_point_1_tester = [ this ]( uint32_t generate_no_further_than ) {
    generate_until_irreversible_block( 6 );
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );
  };

  // Check cumulative history now.
  // Note that comment_payout_beneficiaries occur in both account's comment_options_operation patterns.
  auto check_point_2_tester = [ this ]( uint32_t generate_no_further_than )
  { 
    generate_until_irreversible_block( 28 );
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );

    expected_t expected_dan0ah_history = { {
      R"~([0,{"trx_id":"2493b59d55f9066142c0afef860ebf679695b12e","block":3,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":{"type":"account_create_operation","value":{"fee":{"amount":"0","precision":3,"nai":"@@000000021"},"creator":"initminer","new_account_name":"dan0ah","owner":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST7YJmUoKbPQkrMrZbrgPxDMYJA3uD3utaN3WYRwaFGKYbQ9ftKV",1]]},"active":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST7YJmUoKbPQkrMrZbrgPxDMYJA3uD3utaN3WYRwaFGKYbQ9ftKV",1]]},"posting":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST6HMjs2nWJ6gLw7eyoUySVHGN2uAoSc3CmCjer489SPC3Kwt1UW",1]]},"memo_key":"TST7YJmUoKbPQkrMrZbrgPxDMYJA3uD3utaN3WYRwaFGKYbQ9ftKV","json_metadata":""}},"operation_id":0}])~",
      R"~([0,{"trx_id":"2493b59d55f9066142c0afef860ebf679695b12e","block":3,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":["account_create",{"fee":"0.000 TESTS","creator":"initminer","new_account_name":"dan0ah","owner":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST7YJmUoKbPQkrMrZbrgPxDMYJA3uD3utaN3WYRwaFGKYbQ9ftKV",1]]},"active":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST7YJmUoKbPQkrMrZbrgPxDMYJA3uD3utaN3WYRwaFGKYbQ9ftKV",1]]},"posting":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST6HMjs2nWJ6gLw7eyoUySVHGN2uAoSc3CmCjer489SPC3Kwt1UW",1]]},"memo_key":"TST7YJmUoKbPQkrMrZbrgPxDMYJA3uD3utaN3WYRwaFGKYbQ9ftKV","json_metadata":""}]}])~"
      }, {
      R"~([1,{"trx_id":"2493b59d55f9066142c0afef860ebf679695b12e","block":3,"trx_in_block":0,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":{"type":"account_created_operation","value":{"new_account_name":"dan0ah","creator":"initminer","initial_vesting_shares":{"amount":"0","precision":6,"nai":"@@000000037"},"initial_delegation":{"amount":"0","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([1,{"trx_id":"2493b59d55f9066142c0afef860ebf679695b12e","block":3,"trx_in_block":0,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":["account_created",{"new_account_name":"dan0ah","creator":"initminer","initial_vesting_shares":"0.000000 VESTS","initial_delegation":"0.000000 VESTS"}]}])~"
      }, {
      R"~([2,{"trx_id":"53c52c695d5d6ec7112bfcd09011de07ea0732b3","block":3,"trx_in_block":1,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":{"type":"transfer_to_vesting_operation","value":{"from":"initminer","to":"dan0ah","amount":{"amount":"100","precision":3,"nai":"@@000000021"}}},"operation_id":0}])~",
      R"~([2,{"trx_id":"53c52c695d5d6ec7112bfcd09011de07ea0732b3","block":3,"trx_in_block":1,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":["transfer_to_vesting",{"from":"initminer","to":"dan0ah","amount":"0.100 TESTS"}]}])~"
      }, {
      R"~([3,{"trx_id":"53c52c695d5d6ec7112bfcd09011de07ea0732b3","block":3,"trx_in_block":1,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":{"type":"transfer_to_vesting_completed_operation","value":{"from_account":"initminer","to_account":"dan0ah","hive_vested":{"amount":"100","precision":3,"nai":"@@000000021"},"vesting_shares_received":{"amount":"98716683119","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([3,{"trx_id":"53c52c695d5d6ec7112bfcd09011de07ea0732b3","block":3,"trx_in_block":1,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":["transfer_to_vesting_completed",{"from_account":"initminer","to_account":"dan0ah","hive_vested":"0.100 TESTS","vesting_shares_received":"98716.683119 VESTS"}]}])~"
      }, {
      R"~([4,{"trx_id":"2cf221a357e8ee7776279bb520d10db4f3cf570c","block":3,"trx_in_block":5,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":{"type":"comment_options_operation","value":{"author":"edgar0ah","permlink":"permlink1","max_accepted_payout":{"amount":"10000000","precision":3,"nai":"@@000000013"},"percent_hbd":10000,"allow_votes":true,"allow_curation_rewards":true,"extensions":[{"type":"comment_payout_beneficiaries","value":{"beneficiaries":[{"account":"dan0ah","weight":5000}]}}]}},"operation_id":0}])~",
      R"~([4,{"trx_id":"2cf221a357e8ee7776279bb520d10db4f3cf570c","block":3,"trx_in_block":5,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":["comment_options",{"author":"edgar0ah","permlink":"permlink1","max_accepted_payout":"10000.000 TBD","percent_hbd":10000,"allow_votes":true,"allow_curation_rewards":true,"extensions":[["comment_payout_beneficiaries",{"beneficiaries":[{"account":"dan0ah","weight":5000}]}]]}]}])~"
      }, {
      R"~([5,{"trx_id":"e20072068c9aa6a31df33e72b47160454f4bbafd","block":3,"trx_in_block":6,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":{"type":"vote_operation","value":{"voter":"dan0ah","author":"edgar0ah","permlink":"permlink1","weight":10000}},"operation_id":0}])~",
      R"~([5,{"trx_id":"e20072068c9aa6a31df33e72b47160454f4bbafd","block":3,"trx_in_block":6,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":["vote",{"voter":"dan0ah","author":"edgar0ah","permlink":"permlink1","weight":10000}]}])~"
      }, {
      R"~([6,{"trx_id":"e20072068c9aa6a31df33e72b47160454f4bbafd","block":3,"trx_in_block":6,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":{"type":"effective_comment_vote_operation","value":{"voter":"dan0ah","author":"edgar0ah","permlink":"permlink1","weight":1924333663,"rshares":1924333663,"total_vote_weight":1924333663,"pending_payout":{"amount":"0","precision":3,"nai":"@@000000013"}}},"operation_id":0}])~",
      R"~([6,{"trx_id":"e20072068c9aa6a31df33e72b47160454f4bbafd","block":3,"trx_in_block":6,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":["effective_comment_vote",{"voter":"dan0ah","author":"edgar0ah","permlink":"permlink1","weight":1924333663,"rshares":1924333663,"total_vote_weight":1924333663,"pending_payout":"0.000 TBD"}]}])~"
      }, {
      R"~([7,{"trx_id":"0000000000000000000000000000000000000000","block":6,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:00:18","op":{"type":"curation_reward_operation","value":{"curator":"dan0ah","reward":{"amount":"1089380190306","precision":6,"nai":"@@000000037"},"comment_author":"edgar0ah","comment_permlink":"permlink1","payout_must_be_claimed":true}},"operation_id":0}])~",
      R"~([7,{"trx_id":"0000000000000000000000000000000000000000","block":6,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:00:18","op":["curation_reward",{"curator":"dan0ah","reward":"1089380.190306 VESTS","comment_author":"edgar0ah","comment_permlink":"permlink1","payout_must_be_claimed":true}]}])~"
      }, {
      R"~([8,{"trx_id":"0000000000000000000000000000000000000000","block":6,"trx_in_block":4294967295,"op_in_trx":3,"virtual_op":true,"timestamp":"2016-01-01T00:00:18","op":{"type":"comment_benefactor_reward_operation","value":{"benefactor":"dan0ah","author":"edgar0ah","permlink":"permlink1","hbd_payout":{"amount":"287","precision":3,"nai":"@@000000013"},"hive_payout":{"amount":"0","precision":3,"nai":"@@000000021"},"vesting_payout":{"amount":"272818691137","precision":6,"nai":"@@000000037"},"payout_must_be_claimed":true}},"operation_id":0}])~",
      R"~([8,{"trx_id":"0000000000000000000000000000000000000000","block":6,"trx_in_block":4294967295,"op_in_trx":3,"virtual_op":true,"timestamp":"2016-01-01T00:00:18","op":["comment_benefactor_reward",{"benefactor":"dan0ah","author":"edgar0ah","permlink":"permlink1","hbd_payout":"0.287 TBD","hive_payout":"0.000 TESTS","vesting_payout":"272818.691137 VESTS","payout_must_be_claimed":true}]}])~"
      } };
    expected_t expected_edgar0ah_history = { {
      R"~([0,{"trx_id":"8fb83c38a69fc695f507a0ac5692ca199b9175ac","block":3,"trx_in_block":2,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":{"type":"account_create_operation","value":{"fee":{"amount":"0","precision":3,"nai":"@@000000021"},"creator":"initminer","new_account_name":"edgar0ah","owner":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST8R8maxJxeBMR3JYmap1n3Pypm886oEUjLYdsetzcnPDFpiq3pZ",1]]},"active":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST8R8maxJxeBMR3JYmap1n3Pypm886oEUjLYdsetzcnPDFpiq3pZ",1]]},"posting":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST8ZCsvwKqttXivgPyJ1MYS4q1r3fBZJh3g1SaBxVbfsqNcmnvD3",1]]},"memo_key":"TST8R8maxJxeBMR3JYmap1n3Pypm886oEUjLYdsetzcnPDFpiq3pZ","json_metadata":""}},"operation_id":0}])~",
      R"~([0,{"trx_id":"8fb83c38a69fc695f507a0ac5692ca199b9175ac","block":3,"trx_in_block":2,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":["account_create",{"fee":"0.000 TESTS","creator":"initminer","new_account_name":"edgar0ah","owner":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST8R8maxJxeBMR3JYmap1n3Pypm886oEUjLYdsetzcnPDFpiq3pZ",1]]},"active":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST8R8maxJxeBMR3JYmap1n3Pypm886oEUjLYdsetzcnPDFpiq3pZ",1]]},"posting":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST8ZCsvwKqttXivgPyJ1MYS4q1r3fBZJh3g1SaBxVbfsqNcmnvD3",1]]},"memo_key":"TST8R8maxJxeBMR3JYmap1n3Pypm886oEUjLYdsetzcnPDFpiq3pZ","json_metadata":""}]}])~"
      }, {
      R"~([1,{"trx_id":"8fb83c38a69fc695f507a0ac5692ca199b9175ac","block":3,"trx_in_block":2,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":{"type":"account_created_operation","value":{"new_account_name":"edgar0ah","creator":"initminer","initial_vesting_shares":{"amount":"0","precision":6,"nai":"@@000000037"},"initial_delegation":{"amount":"0","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([1,{"trx_id":"8fb83c38a69fc695f507a0ac5692ca199b9175ac","block":3,"trx_in_block":2,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":["account_created",{"new_account_name":"edgar0ah","creator":"initminer","initial_vesting_shares":"0.000000 VESTS","initial_delegation":"0.000000 VESTS"}]}])~"
      }, {
      R"~([2,{"trx_id":"35a656b9d2cdd5ebc4cdac317f3d021d517fdcf8","block":3,"trx_in_block":3,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":{"type":"transfer_to_vesting_operation","value":{"from":"initminer","to":"edgar0ah","amount":{"amount":"100","precision":3,"nai":"@@000000021"}}},"operation_id":0}])~",
      R"~([2,{"trx_id":"35a656b9d2cdd5ebc4cdac317f3d021d517fdcf8","block":3,"trx_in_block":3,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":["transfer_to_vesting",{"from":"initminer","to":"edgar0ah","amount":"0.100 TESTS"}]}])~"
      }, {
      R"~([3,{"trx_id":"35a656b9d2cdd5ebc4cdac317f3d021d517fdcf8","block":3,"trx_in_block":3,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":{"type":"transfer_to_vesting_completed_operation","value":{"from_account":"initminer","to_account":"edgar0ah","hive_vested":{"amount":"100","precision":3,"nai":"@@000000021"},"vesting_shares_received":{"amount":"98716683119","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([3,{"trx_id":"35a656b9d2cdd5ebc4cdac317f3d021d517fdcf8","block":3,"trx_in_block":3,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":["transfer_to_vesting_completed",{"from_account":"initminer","to_account":"edgar0ah","hive_vested":"0.100 TESTS","vesting_shares_received":"98716.683119 VESTS"}]}])~"
      }, {
      R"~([4,{"trx_id":"63807c110bc33695772793f61972ac9d29d7689a","block":3,"trx_in_block":4,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":{"type":"comment_operation","value":{"parent_author":"","parent_permlink":"parentpermlink1","author":"edgar0ah","permlink":"permlink1","title":"Title 1","body":"Body 1","json_metadata":""}},"operation_id":0}])~",
      R"~([4,{"trx_id":"63807c110bc33695772793f61972ac9d29d7689a","block":3,"trx_in_block":4,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":["comment",{"parent_author":"","parent_permlink":"parentpermlink1","author":"edgar0ah","permlink":"permlink1","title":"Title 1","body":"Body 1","json_metadata":""}]}])~"
      }, {
      R"~([5,{"trx_id":"2cf221a357e8ee7776279bb520d10db4f3cf570c","block":3,"trx_in_block":5,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":{"type":"comment_options_operation","value":{"author":"edgar0ah","permlink":"permlink1","max_accepted_payout":{"amount":"10000000","precision":3,"nai":"@@000000013"},"percent_hbd":10000,"allow_votes":true,"allow_curation_rewards":true,"extensions":[{"type":"comment_payout_beneficiaries","value":{"beneficiaries":[{"account":"dan0ah","weight":5000}]}}]}},"operation_id":0}])~",
      R"~([5,{"trx_id":"2cf221a357e8ee7776279bb520d10db4f3cf570c","block":3,"trx_in_block":5,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":["comment_options",{"author":"edgar0ah","permlink":"permlink1","max_accepted_payout":"10000.000 TBD","percent_hbd":10000,"allow_votes":true,"allow_curation_rewards":true,"extensions":[["comment_payout_beneficiaries",{"beneficiaries":[{"account":"dan0ah","weight":5000}]}]]}]}])~"
      }, {
      R"~([6,{"trx_id":"e20072068c9aa6a31df33e72b47160454f4bbafd","block":3,"trx_in_block":6,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":{"type":"vote_operation","value":{"voter":"dan0ah","author":"edgar0ah","permlink":"permlink1","weight":10000}},"operation_id":0}])~",
      R"~([6,{"trx_id":"e20072068c9aa6a31df33e72b47160454f4bbafd","block":3,"trx_in_block":6,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":["vote",{"voter":"dan0ah","author":"edgar0ah","permlink":"permlink1","weight":10000}]}])~"
      }, {
      R"~([7,{"trx_id":"e20072068c9aa6a31df33e72b47160454f4bbafd","block":3,"trx_in_block":6,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":{"type":"effective_comment_vote_operation","value":{"voter":"dan0ah","author":"edgar0ah","permlink":"permlink1","weight":1924333663,"rshares":1924333663,"total_vote_weight":1924333663,"pending_payout":{"amount":"0","precision":3,"nai":"@@000000013"}}},"operation_id":0}])~",
      R"~([7,{"trx_id":"e20072068c9aa6a31df33e72b47160454f4bbafd","block":3,"trx_in_block":6,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":["effective_comment_vote",{"voter":"dan0ah","author":"edgar0ah","permlink":"permlink1","weight":1924333663,"rshares":1924333663,"total_vote_weight":1924333663,"pending_payout":"0.000 TBD"}]}])~"
      }, {
      R"~([8,{"trx_id":"0000000000000000000000000000000000000000","block":6,"trx_in_block":4294967295,"op_in_trx":3,"virtual_op":true,"timestamp":"2016-01-01T00:00:18","op":{"type":"comment_benefactor_reward_operation","value":{"benefactor":"dan0ah","author":"edgar0ah","permlink":"permlink1","hbd_payout":{"amount":"287","precision":3,"nai":"@@000000013"},"hive_payout":{"amount":"0","precision":3,"nai":"@@000000021"},"vesting_payout":{"amount":"272818691137","precision":6,"nai":"@@000000037"},"payout_must_be_claimed":true}},"operation_id":0}])~",
      R"~([8,{"trx_id":"0000000000000000000000000000000000000000","block":6,"trx_in_block":4294967295,"op_in_trx":3,"virtual_op":true,"timestamp":"2016-01-01T00:00:18","op":["comment_benefactor_reward",{"benefactor":"dan0ah","author":"edgar0ah","permlink":"permlink1","hbd_payout":"0.287 TBD","hive_payout":"0.000 TESTS","vesting_payout":"272818.691137 VESTS","payout_must_be_claimed":true}]}])~"
      }, {
      R"~([9,{"trx_id":"0000000000000000000000000000000000000000","block":6,"trx_in_block":4294967295,"op_in_trx":4,"virtual_op":true,"timestamp":"2016-01-01T00:00:18","op":{"type":"author_reward_operation","value":{"author":"edgar0ah","permlink":"permlink1","hbd_payout":{"amount":"287","precision":3,"nai":"@@000000013"},"hive_payout":{"amount":"0","precision":3,"nai":"@@000000021"},"vesting_payout":{"amount":"272818691137","precision":6,"nai":"@@000000037"},"curators_vesting_payout":{"amount":"1089380190305","precision":6,"nai":"@@000000037"},"payout_must_be_claimed":true}},"operation_id":0}])~",
      R"~([9,{"trx_id":"0000000000000000000000000000000000000000","block":6,"trx_in_block":4294967295,"op_in_trx":4,"virtual_op":true,"timestamp":"2016-01-01T00:00:18","op":["author_reward",{"author":"edgar0ah","permlink":"permlink1","hbd_payout":"0.287 TBD","hive_payout":"0.000 TESTS","vesting_payout":"272818.691137 VESTS","curators_vesting_payout":"1089380.190305 VESTS","payout_must_be_claimed":true}]}])~"
      }, {
      R"~([10,{"trx_id":"0000000000000000000000000000000000000000","block":6,"trx_in_block":4294967295,"op_in_trx":5,"virtual_op":true,"timestamp":"2016-01-01T00:00:18","op":{"type":"comment_reward_operation","value":{"author":"edgar0ah","permlink":"permlink1","payout":{"amount":"2300","precision":3,"nai":"@@000000013"},"author_rewards":575,"total_payout_value":{"amount":"575","precision":3,"nai":"@@000000013"},"curator_payout_value":{"amount":"1150","precision":3,"nai":"@@000000013"},"beneficiary_payout_value":{"amount":"575","precision":3,"nai":"@@000000013"}}},"operation_id":0}])~",
      R"~([10,{"trx_id":"0000000000000000000000000000000000000000","block":6,"trx_in_block":4294967295,"op_in_trx":5,"virtual_op":true,"timestamp":"2016-01-01T00:00:18","op":["comment_reward",{"author":"edgar0ah","permlink":"permlink1","payout":"2.300 TBD","author_rewards":575,"total_payout_value":"0.575 TBD","curator_payout_value":"1.150 TBD","beneficiary_payout_value":"0.575 TBD"}]}])~"
      }, {
      R"~([11,{"trx_id":"0000000000000000000000000000000000000000","block":6,"trx_in_block":4294967295,"op_in_trx":6,"virtual_op":true,"timestamp":"2016-01-01T00:00:18","op":{"type":"comment_payout_update_operation","value":{"author":"edgar0ah","permlink":"permlink1"}},"operation_id":0}])~",
      R"~([11,{"trx_id":"0000000000000000000000000000000000000000","block":6,"trx_in_block":4294967295,"op_in_trx":6,"virtual_op":true,"timestamp":"2016-01-01T00:00:18","op":["comment_payout_update",{"author":"edgar0ah","permlink":"permlink1"}]}])~"
      }, {
      R"~([12,{"trx_id":"daa9aa439e8af76a93cf4b539c3337b1bc303466","block":28,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:01:21","op":{"type":"claim_reward_balance_operation","value":{"account":"edgar0ah","reward_hive":{"amount":"0","precision":3,"nai":"@@000000021"},"reward_hbd":{"amount":"1","precision":3,"nai":"@@000000013"},"reward_vests":{"amount":"1","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([12,{"trx_id":"daa9aa439e8af76a93cf4b539c3337b1bc303466","block":28,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:01:21","op":["claim_reward_balance",{"account":"edgar0ah","reward_hive":"0.000 TESTS","reward_hbd":"0.001 TBD","reward_vests":"0.000001 VESTS"}]}])~"
      } };
    test_get_account_history( *this, { "dan0ah", "edgar0ah" }, { expected_dan0ah_history, expected_edgar0ah_history } );
  };

  comment_and_reward_scenario( check_point_1_tester, check_point_2_tester );

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( get_account_history_convert_and_limit_order )
{ try {

  BOOST_TEST_MESSAGE( "testing get_account_history with convert_and_limit_order_scenario" );

  auto check_point_tester = [ this ]( uint32_t generate_no_further_than )
  {
    generate_until_irreversible_block( 88 );
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );

    expected_t expected_carol3ah_history = { {
      R"~([0,{"trx_id":"99b3d543d8dbd7da66dc4939cea81138e9171073","block":3,"trx_in_block":2,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":{"type":"account_create_operation","value":{"fee":{"amount":"0","precision":3,"nai":"@@000000021"},"creator":"initminer","new_account_name":"carol3ah","owner":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST7jcq47bH93zcuTCdP394BYJLrhGWzyGwqkukB46zyFsghQPeoz",1]]},"active":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST7jcq47bH93zcuTCdP394BYJLrhGWzyGwqkukB46zyFsghQPeoz",1]]},"posting":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST5kavbaHAVwb9mANYyUEubtGybsJ4zySnVrpdmpDM8pDRhKzyN3",1]]},"memo_key":"TST7jcq47bH93zcuTCdP394BYJLrhGWzyGwqkukB46zyFsghQPeoz","json_metadata":""}},"operation_id":0}])~",
      R"~([0,{"trx_id":"99b3d543d8dbd7da66dc4939cea81138e9171073","block":3,"trx_in_block":2,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":["account_create",{"fee":"0.000 TESTS","creator":"initminer","new_account_name":"carol3ah","owner":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST7jcq47bH93zcuTCdP394BYJLrhGWzyGwqkukB46zyFsghQPeoz",1]]},"active":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST7jcq47bH93zcuTCdP394BYJLrhGWzyGwqkukB46zyFsghQPeoz",1]]},"posting":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST5kavbaHAVwb9mANYyUEubtGybsJ4zySnVrpdmpDM8pDRhKzyN3",1]]},"memo_key":"TST7jcq47bH93zcuTCdP394BYJLrhGWzyGwqkukB46zyFsghQPeoz","json_metadata":""}]}])~"
      }, {
      R"~([1,{"trx_id":"99b3d543d8dbd7da66dc4939cea81138e9171073","block":3,"trx_in_block":2,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":{"type":"account_created_operation","value":{"new_account_name":"carol3ah","creator":"initminer","initial_vesting_shares":{"amount":"0","precision":6,"nai":"@@000000037"},"initial_delegation":{"amount":"0","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([1,{"trx_id":"99b3d543d8dbd7da66dc4939cea81138e9171073","block":3,"trx_in_block":2,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":["account_created",{"new_account_name":"carol3ah","creator":"initminer","initial_vesting_shares":"0.000000 VESTS","initial_delegation":"0.000000 VESTS"}]}])~"
      }, {
      R"~([2,{"trx_id":"eae9a5104a476a51007f1e99f787df120cc6def2","block":3,"trx_in_block":3,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":{"type":"transfer_to_vesting_operation","value":{"from":"initminer","to":"carol3ah","amount":{"amount":"100","precision":3,"nai":"@@000000021"}}},"operation_id":0}])~",
      R"~([2,{"trx_id":"eae9a5104a476a51007f1e99f787df120cc6def2","block":3,"trx_in_block":3,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":["transfer_to_vesting",{"from":"initminer","to":"carol3ah","amount":"0.100 TESTS"}]}])~"
      }, {
      R"~([3,{"trx_id":"eae9a5104a476a51007f1e99f787df120cc6def2","block":3,"trx_in_block":3,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":{"type":"transfer_to_vesting_completed_operation","value":{"from_account":"initminer","to_account":"carol3ah","hive_vested":{"amount":"100","precision":3,"nai":"@@000000021"},"vesting_shares_received":{"amount":"98716683119","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([3,{"trx_id":"eae9a5104a476a51007f1e99f787df120cc6def2","block":3,"trx_in_block":3,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":["transfer_to_vesting_completed",{"from_account":"initminer","to_account":"carol3ah","hive_vested":"0.100 TESTS","vesting_shares_received":"98716.683119 VESTS"}]}])~"
      }, {
      R"~([4,{"trx_id":"9f3d234471e6b053be812e180f8f63a4811f462d","block":5,"trx_in_block":1,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":{"type":"collateralized_convert_operation","value":{"owner":"carol3ah","requestid":0,"amount":{"amount":"22102","precision":3,"nai":"@@000000021"}}},"operation_id":0}])~",
      R"~([4,{"trx_id":"9f3d234471e6b053be812e180f8f63a4811f462d","block":5,"trx_in_block":1,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":["collateralized_convert",{"owner":"carol3ah","requestid":0,"amount":"22.102 TESTS"}]}])~"
      }, {
      R"~([5,{"trx_id":"9f3d234471e6b053be812e180f8f63a4811f462d","block":5,"trx_in_block":1,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:12","op":{"type":"collateralized_convert_immediate_conversion_operation","value":{"owner":"carol3ah","requestid":0,"hbd_out":{"amount":"10524","precision":3,"nai":"@@000000013"}}},"operation_id":0}])~",
      R"~([5,{"trx_id":"9f3d234471e6b053be812e180f8f63a4811f462d","block":5,"trx_in_block":1,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:12","op":["collateralized_convert_immediate_conversion",{"owner":"carol3ah","requestid":0,"hbd_out":"10.524 TBD"}]}])~"
      }, {
      R"~([6,{"trx_id":"ad19c50dc64931096ca6bd82574f03330c37c7d9","block":5,"trx_in_block":2,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":{"type":"limit_order_create_operation","value":{"owner":"carol3ah","orderid":1,"amount_to_sell":{"amount":"11400","precision":3,"nai":"@@000000021"},"min_to_receive":{"amount":"11650","precision":3,"nai":"@@000000013"},"fill_or_kill":false,"expiration":"2016-01-29T00:00:12"}},"operation_id":0}])~",
      R"~([6,{"trx_id":"ad19c50dc64931096ca6bd82574f03330c37c7d9","block":5,"trx_in_block":2,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":["limit_order_create",{"owner":"carol3ah","orderid":1,"amount_to_sell":"11.400 TESTS","min_to_receive":"11.650 TBD","fill_or_kill":false,"expiration":"2016-01-29T00:00:12"}]}])~"
      }, {
      R"~([7,{"trx_id":"5381fe94856170a97821cf6c1a3518d279111d05","block":5,"trx_in_block":3,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":{"type":"limit_order_create2_operation","value":{"owner":"carol3ah","orderid":2,"amount_to_sell":{"amount":"22075","precision":3,"nai":"@@000000021"},"exchange_rate":{"base":{"amount":"10","precision":3,"nai":"@@000000021"},"quote":{"amount":"10","precision":3,"nai":"@@000000013"}},"fill_or_kill":false,"expiration":"2016-01-29T00:00:12"}},"operation_id":0}])~",
      R"~([7,{"trx_id":"5381fe94856170a97821cf6c1a3518d279111d05","block":5,"trx_in_block":3,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":["limit_order_create2",{"owner":"carol3ah","orderid":2,"amount_to_sell":"22.075 TESTS","exchange_rate":{"base":"0.010 TESTS","quote":"0.010 TBD"},"fill_or_kill":false,"expiration":"2016-01-29T00:00:12"}]}])~"
      }, {
      R"~([8,{"trx_id":"8c57b6cc735c077c0f0e41974e3f74dafab89319","block":5,"trx_in_block":4,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":{"type":"limit_order_cancel_operation","value":{"owner":"carol3ah","orderid":1}},"operation_id":0}])~",
      R"~([8,{"trx_id":"8c57b6cc735c077c0f0e41974e3f74dafab89319","block":5,"trx_in_block":4,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":["limit_order_cancel",{"owner":"carol3ah","orderid":1}]}])~"
      }, {
      R"~([9,{"trx_id":"8c57b6cc735c077c0f0e41974e3f74dafab89319","block":5,"trx_in_block":4,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:12","op":{"type":"limit_order_cancelled_operation","value":{"seller":"carol3ah","orderid":1,"amount_back":{"amount":"11400","precision":3,"nai":"@@000000021"}}},"operation_id":0}])~",
      R"~([9,{"trx_id":"8c57b6cc735c077c0f0e41974e3f74dafab89319","block":5,"trx_in_block":4,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:12","op":["limit_order_cancelled",{"seller":"carol3ah","orderid":1,"amount_back":"11.400 TESTS"}]}])~"
      }, {
      R"~([10,{"trx_id":"0000000000000000000000000000000000000000","block":88,"trx_in_block":4294967295,"op_in_trx":3,"virtual_op":true,"timestamp":"2016-01-01T00:04:24","op":{"type":"fill_collateralized_convert_request_operation","value":{"owner":"carol3ah","requestid":0,"amount_in":{"amount":"11050","precision":3,"nai":"@@000000021"},"amount_out":{"amount":"10524","precision":3,"nai":"@@000000013"},"excess_collateral":{"amount":"11052","precision":3,"nai":"@@000000021"}}},"operation_id":0}])~",
      R"~([10,{"trx_id":"0000000000000000000000000000000000000000","block":88,"trx_in_block":4294967295,"op_in_trx":3,"virtual_op":true,"timestamp":"2016-01-01T00:04:24","op":["fill_collateralized_convert_request",{"owner":"carol3ah","requestid":0,"amount_in":"11.050 TESTS","amount_out":"10.524 TBD","excess_collateral":"11.052 TESTS"}]}])~"
      } };
    expected_t expected_edgar3ah_history = { {
      R"~([0,{"trx_id":"7a63597b9b1d07c6be6a5cb906c7477be31b140c","block":3,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":{"type":"account_create_operation","value":{"fee":{"amount":"0","precision":3,"nai":"@@000000021"},"creator":"initminer","new_account_name":"edgar3ah","owner":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST5WY8U7VmTGmJuHeEJBtXrZwDfwVhGMwgGPh5Rrrskw3XmhSDgs",1]]},"active":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST5WY8U7VmTGmJuHeEJBtXrZwDfwVhGMwgGPh5Rrrskw3XmhSDgs",1]]},"posting":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST5EUgu1BPGi9mPmFhfjELAeZvwdRMdPuKRuPE9CXGbpX9TgzDA5",1]]},"memo_key":"TST5WY8U7VmTGmJuHeEJBtXrZwDfwVhGMwgGPh5Rrrskw3XmhSDgs","json_metadata":""}},"operation_id":0}])~",
      R"~([0,{"trx_id":"7a63597b9b1d07c6be6a5cb906c7477be31b140c","block":3,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":["account_create",{"fee":"0.000 TESTS","creator":"initminer","new_account_name":"edgar3ah","owner":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST5WY8U7VmTGmJuHeEJBtXrZwDfwVhGMwgGPh5Rrrskw3XmhSDgs",1]]},"active":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST5WY8U7VmTGmJuHeEJBtXrZwDfwVhGMwgGPh5Rrrskw3XmhSDgs",1]]},"posting":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST5EUgu1BPGi9mPmFhfjELAeZvwdRMdPuKRuPE9CXGbpX9TgzDA5",1]]},"memo_key":"TST5WY8U7VmTGmJuHeEJBtXrZwDfwVhGMwgGPh5Rrrskw3XmhSDgs","json_metadata":""}]}])~"
      }, {
      R"~([1,{"trx_id":"7a63597b9b1d07c6be6a5cb906c7477be31b140c","block":3,"trx_in_block":0,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":{"type":"account_created_operation","value":{"new_account_name":"edgar3ah","creator":"initminer","initial_vesting_shares":{"amount":"0","precision":6,"nai":"@@000000037"},"initial_delegation":{"amount":"0","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([1,{"trx_id":"7a63597b9b1d07c6be6a5cb906c7477be31b140c","block":3,"trx_in_block":0,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":["account_created",{"new_account_name":"edgar3ah","creator":"initminer","initial_vesting_shares":"0.000000 VESTS","initial_delegation":"0.000000 VESTS"}]}])~"
      }, {
      R"~([2,{"trx_id":"98e8e0aa2b7c3422360f79285c183935df1d14ea","block":3,"trx_in_block":1,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":{"type":"transfer_to_vesting_operation","value":{"from":"initminer","to":"edgar3ah","amount":{"amount":"100","precision":3,"nai":"@@000000021"}}},"operation_id":0}])~",
      R"~([2,{"trx_id":"98e8e0aa2b7c3422360f79285c183935df1d14ea","block":3,"trx_in_block":1,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":["transfer_to_vesting",{"from":"initminer","to":"edgar3ah","amount":"0.100 TESTS"}]}])~"
      }, {
      R"~([3,{"trx_id":"98e8e0aa2b7c3422360f79285c183935df1d14ea","block":3,"trx_in_block":1,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":{"type":"transfer_to_vesting_completed_operation","value":{"from_account":"initminer","to_account":"edgar3ah","hive_vested":{"amount":"100","precision":3,"nai":"@@000000021"},"vesting_shares_received":{"amount":"98716683119","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([3,{"trx_id":"98e8e0aa2b7c3422360f79285c183935df1d14ea","block":3,"trx_in_block":1,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":["transfer_to_vesting_completed",{"from_account":"initminer","to_account":"edgar3ah","hive_vested":"0.100 TESTS","vesting_shares_received":"98716.683119 VESTS"}]}])~"
      }, {
      R"~([4,{"trx_id":"6a79f548e62e1013e6fcbc7442f215cd7879f431","block":5,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":{"type":"convert_operation","value":{"owner":"edgar3ah","requestid":0,"amount":{"amount":"11201","precision":3,"nai":"@@000000013"}}},"operation_id":0}])~",
      R"~([4,{"trx_id":"6a79f548e62e1013e6fcbc7442f215cd7879f431","block":5,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":["convert",{"owner":"edgar3ah","requestid":0,"amount":"11.201 TBD"}]}])~"
      }, {
      R"~([5,{"trx_id":"0000000000000000000000000000000000000000","block":88,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:04:24","op":{"type":"fill_convert_request_operation","value":{"owner":"edgar3ah","requestid":0,"amount_in":{"amount":"11201","precision":3,"nai":"@@000000013"},"amount_out":{"amount":"11201","precision":3,"nai":"@@000000021"}}},"operation_id":0}])~",
      R"~([5,{"trx_id":"0000000000000000000000000000000000000000","block":88,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:04:24","op":["fill_convert_request",{"owner":"edgar3ah","requestid":0,"amount_in":"11.201 TBD","amount_out":"11.201 TESTS"}]}])~"
      } };
    test_get_account_history( *this, { "carol3ah", "edgar3ah" }, { expected_carol3ah_history, expected_edgar3ah_history } );
  };

  convert_and_limit_order_scenario( check_point_tester );

} FC_LOG_AND_RETHROW() }

/**
 * Uses vesting_scenario to test:
 * - transfer to vesting to self.
 * - vesting withdraw route impacted accounts
 * - vesting withdrawal both to self and routed impacted accounts
 * - vesting delegation impacted accounts
 * - vesting delegation return impacted accounts
 */
BOOST_AUTO_TEST_CASE( get_account_history_vesting )
{ try {

  BOOST_TEST_MESSAGE( "testing get_account_history with vesting_scenario" );

  auto check_point_tester = [ this ]( uint32_t generate_no_further_than )
  {
    generate_until_irreversible_block( 9 );
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );

    expected_t expected_alice4ah_history = { {
      R"~([2,{"trx_id":"a6acc8f4d22b472c0688aac55a7e8cc786003ce9","block":3,"trx_in_block":1,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":{"type":"transfer_to_vesting_operation","value":{"from":"initminer","to":"alice4ah","amount":{"amount":"100","precision":3,"nai":"@@000000021"}}},"operation_id":0}])~",
      R"~([2,{"trx_id":"a6acc8f4d22b472c0688aac55a7e8cc786003ce9","block":3,"trx_in_block":1,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":["transfer_to_vesting",{"from":"initminer","to":"alice4ah","amount":"0.100 TESTS"}]}])~"
      }, {
      R"~([3,{"trx_id":"a6acc8f4d22b472c0688aac55a7e8cc786003ce9","block":3,"trx_in_block":1,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":{"type":"transfer_to_vesting_completed_operation","value":{"from_account":"initminer","to_account":"alice4ah","hive_vested":{"amount":"100","precision":3,"nai":"@@000000021"},"vesting_shares_received":{"amount":"98716683119","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([3,{"trx_id":"a6acc8f4d22b472c0688aac55a7e8cc786003ce9","block":3,"trx_in_block":1,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":["transfer_to_vesting_completed",{"from_account":"initminer","to_account":"alice4ah","hive_vested":"0.100 TESTS","vesting_shares_received":"98716.683119 VESTS"}]}])~"
      }, {
      R"~([4,{"trx_id":"0ace3d6e57043dfde1daf6c00d5d7a6c1e556126","block":5,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":{"type":"transfer_to_vesting_operation","value":{"from":"alice4ah","to":"alice4ah","amount":{"amount":"2000","precision":3,"nai":"@@000000021"}}},"operation_id":0}])~",
      R"~([4,{"trx_id":"0ace3d6e57043dfde1daf6c00d5d7a6c1e556126","block":5,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":["transfer_to_vesting",{"from":"alice4ah","to":"alice4ah","amount":"2.000 TESTS"}]}])~"
      }, {
      R"~([5,{"trx_id":"0ace3d6e57043dfde1daf6c00d5d7a6c1e556126","block":5,"trx_in_block":0,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:12","op":{"type":"transfer_to_vesting_completed_operation","value":{"from_account":"alice4ah","to_account":"alice4ah","hive_vested":{"amount":"2000","precision":3,"nai":"@@000000021"},"vesting_shares_received":{"amount":"1936378093693","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([5,{"trx_id":"0ace3d6e57043dfde1daf6c00d5d7a6c1e556126","block":5,"trx_in_block":0,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:12","op":["transfer_to_vesting_completed",{"from_account":"alice4ah","to_account":"alice4ah","hive_vested":"2.000 TESTS","vesting_shares_received":"1936378.093693 VESTS"}]}])~"
      }, {
      R"~([6,{"trx_id":"f836d85674c299b307b8168bd3c18d9018de4bb6","block":5,"trx_in_block":1,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":{"type":"set_withdraw_vesting_route_operation","value":{"from_account":"alice4ah","to_account":"ben4ah","percent":5000,"auto_vest":true}},"operation_id":0}])~",
      R"~([6,{"trx_id":"f836d85674c299b307b8168bd3c18d9018de4bb6","block":5,"trx_in_block":1,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":["set_withdraw_vesting_route",{"from_account":"alice4ah","to_account":"ben4ah","percent":5000,"auto_vest":true}]}])~"
      }, {
      R"~([7,{"trx_id":"64261f6d3e997f3e9c7846712bde0b5d7da983fb","block":5,"trx_in_block":2,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":{"type":"delegate_vesting_shares_operation","value":{"delegator":"alice4ah","delegatee":"carol4ah","vesting_shares":{"amount":"3","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([7,{"trx_id":"64261f6d3e997f3e9c7846712bde0b5d7da983fb","block":5,"trx_in_block":2,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":["delegate_vesting_shares",{"delegator":"alice4ah","delegatee":"carol4ah","vesting_shares":"0.000003 VESTS"}]}])~"
      }, {
      R"~([8,{"trx_id":"f5db21d976c5d7281e3c09838c6b14a8d78e9913","block":5,"trx_in_block":3,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":{"type":"withdraw_vesting_operation","value":{"account":"alice4ah","vesting_shares":{"amount":"123","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([8,{"trx_id":"f5db21d976c5d7281e3c09838c6b14a8d78e9913","block":5,"trx_in_block":3,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":["withdraw_vesting",{"account":"alice4ah","vesting_shares":"0.000123 VESTS"}]}])~"
      }, {
      R"~([9,{"trx_id":"650e7569d7c96f825886f04b4b53168e27c5707a","block":5,"trx_in_block":4,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":{"type":"delegate_vesting_shares_operation","value":{"delegator":"alice4ah","delegatee":"carol4ah","vesting_shares":{"amount":"2","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([9,{"trx_id":"650e7569d7c96f825886f04b4b53168e27c5707a","block":5,"trx_in_block":4,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":["delegate_vesting_shares",{"delegator":"alice4ah","delegatee":"carol4ah","vesting_shares":"0.000002 VESTS"}]}])~"
      }, {
      R"~([10,{"trx_id":"0000000000000000000000000000000000000000","block":8,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:00:24","op":{"type":"fill_vesting_withdraw_operation","value":{"from_account":"alice4ah","to_account":"ben4ah","withdrawn":{"amount":"4","precision":6,"nai":"@@000000037"},"deposited":{"amount":"4","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([10,{"trx_id":"0000000000000000000000000000000000000000","block":8,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:00:24","op":["fill_vesting_withdraw",{"from_account":"alice4ah","to_account":"ben4ah","withdrawn":"0.000004 VESTS","deposited":"0.000004 VESTS"}]}])~"
      }, {
      R"~([11,{"trx_id":"0000000000000000000000000000000000000000","block":8,"trx_in_block":4294967295,"op_in_trx":3,"virtual_op":true,"timestamp":"2016-01-01T00:00:24","op":{"type":"fill_vesting_withdraw_operation","value":{"from_account":"alice4ah","to_account":"alice4ah","withdrawn":{"amount":"5","precision":6,"nai":"@@000000037"},"deposited":{"amount":"0","precision":3,"nai":"@@000000021"}}},"operation_id":0}])~",
      R"~([11,{"trx_id":"0000000000000000000000000000000000000000","block":8,"trx_in_block":4294967295,"op_in_trx":3,"virtual_op":true,"timestamp":"2016-01-01T00:00:24","op":["fill_vesting_withdraw",{"from_account":"alice4ah","to_account":"alice4ah","withdrawn":"0.000005 VESTS","deposited":"0.000 TESTS"}]}])~"
      }, {
      R"~([12,{"trx_id":"0000000000000000000000000000000000000000","block":9,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:27","op":{"type":"return_vesting_delegation_operation","value":{"account":"alice4ah","vesting_shares":{"amount":"1","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([12,{"trx_id":"0000000000000000000000000000000000000000","block":9,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:27","op":["return_vesting_delegation",{"account":"alice4ah","vesting_shares":"0.000001 VESTS"}]}])~"
      }, {
      R"~([13,{"trx_id":"0000000000000000000000000000000000000000","block":12,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:00:36","op":{"type":"fill_vesting_withdraw_operation","value":{"from_account":"alice4ah","to_account":"ben4ah","withdrawn":{"amount":"4","precision":6,"nai":"@@000000037"},"deposited":{"amount":"4","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([13,{"trx_id":"0000000000000000000000000000000000000000","block":12,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:00:36","op":["fill_vesting_withdraw",{"from_account":"alice4ah","to_account":"ben4ah","withdrawn":"0.000004 VESTS","deposited":"0.000004 VESTS"}]}])~"
      }, {
      R"~([14,{"trx_id":"0000000000000000000000000000000000000000","block":12,"trx_in_block":4294967295,"op_in_trx":3,"virtual_op":true,"timestamp":"2016-01-01T00:00:36","op":{"type":"fill_vesting_withdraw_operation","value":{"from_account":"alice4ah","to_account":"alice4ah","withdrawn":{"amount":"5","precision":6,"nai":"@@000000037"},"deposited":{"amount":"0","precision":3,"nai":"@@000000021"}}},"operation_id":0}])~",
      R"~([14,{"trx_id":"0000000000000000000000000000000000000000","block":12,"trx_in_block":4294967295,"op_in_trx":3,"virtual_op":true,"timestamp":"2016-01-01T00:00:36","op":["fill_vesting_withdraw",{"from_account":"alice4ah","to_account":"alice4ah","withdrawn":"0.000005 VESTS","deposited":"0.000 TESTS"}]}])~"
      }, {
      R"~([15,{"trx_id":"0000000000000000000000000000000000000000","block":16,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:00:48","op":{"type":"fill_vesting_withdraw_operation","value":{"from_account":"alice4ah","to_account":"ben4ah","withdrawn":{"amount":"4","precision":6,"nai":"@@000000037"},"deposited":{"amount":"4","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([15,{"trx_id":"0000000000000000000000000000000000000000","block":16,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:00:48","op":["fill_vesting_withdraw",{"from_account":"alice4ah","to_account":"ben4ah","withdrawn":"0.000004 VESTS","deposited":"0.000004 VESTS"}]}])~"
      }, {
      R"~([16,{"trx_id":"0000000000000000000000000000000000000000","block":16,"trx_in_block":4294967295,"op_in_trx":3,"virtual_op":true,"timestamp":"2016-01-01T00:00:48","op":{"type":"fill_vesting_withdraw_operation","value":{"from_account":"alice4ah","to_account":"alice4ah","withdrawn":{"amount":"5","precision":6,"nai":"@@000000037"},"deposited":{"amount":"0","precision":3,"nai":"@@000000021"}}},"operation_id":0}])~",
      R"~([16,{"trx_id":"0000000000000000000000000000000000000000","block":16,"trx_in_block":4294967295,"op_in_trx":3,"virtual_op":true,"timestamp":"2016-01-01T00:00:48","op":["fill_vesting_withdraw",{"from_account":"alice4ah","to_account":"alice4ah","withdrawn":"0.000005 VESTS","deposited":"0.000 TESTS"}]}])~"
      }, {
      R"~([17,{"trx_id":"0000000000000000000000000000000000000000","block":20,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:01:00","op":{"type":"fill_vesting_withdraw_operation","value":{"from_account":"alice4ah","to_account":"ben4ah","withdrawn":{"amount":"4","precision":6,"nai":"@@000000037"},"deposited":{"amount":"4","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([17,{"trx_id":"0000000000000000000000000000000000000000","block":20,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:01:00","op":["fill_vesting_withdraw",{"from_account":"alice4ah","to_account":"ben4ah","withdrawn":"0.000004 VESTS","deposited":"0.000004 VESTS"}]}])~"
      }, {
      R"~([18,{"trx_id":"0000000000000000000000000000000000000000","block":20,"trx_in_block":4294967295,"op_in_trx":3,"virtual_op":true,"timestamp":"2016-01-01T00:01:00","op":{"type":"fill_vesting_withdraw_operation","value":{"from_account":"alice4ah","to_account":"alice4ah","withdrawn":{"amount":"5","precision":6,"nai":"@@000000037"},"deposited":{"amount":"0","precision":3,"nai":"@@000000021"}}},"operation_id":0}])~",
      R"~([18,{"trx_id":"0000000000000000000000000000000000000000","block":20,"trx_in_block":4294967295,"op_in_trx":3,"virtual_op":true,"timestamp":"2016-01-01T00:01:00","op":["fill_vesting_withdraw",{"from_account":"alice4ah","to_account":"alice4ah","withdrawn":"0.000005 VESTS","deposited":"0.000 TESTS"}]}])~"
      }, {
      R"~([19,{"trx_id":"0000000000000000000000000000000000000000","block":24,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:01:12","op":{"type":"fill_vesting_withdraw_operation","value":{"from_account":"alice4ah","to_account":"ben4ah","withdrawn":{"amount":"4","precision":6,"nai":"@@000000037"},"deposited":{"amount":"4","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([19,{"trx_id":"0000000000000000000000000000000000000000","block":24,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:01:12","op":["fill_vesting_withdraw",{"from_account":"alice4ah","to_account":"ben4ah","withdrawn":"0.000004 VESTS","deposited":"0.000004 VESTS"}]}])~"
      }, {
      R"~([20,{"trx_id":"0000000000000000000000000000000000000000","block":24,"trx_in_block":4294967295,"op_in_trx":3,"virtual_op":true,"timestamp":"2016-01-01T00:01:12","op":{"type":"fill_vesting_withdraw_operation","value":{"from_account":"alice4ah","to_account":"alice4ah","withdrawn":{"amount":"5","precision":6,"nai":"@@000000037"},"deposited":{"amount":"0","precision":3,"nai":"@@000000021"}}},"operation_id":0}])~",
      R"~([20,{"trx_id":"0000000000000000000000000000000000000000","block":24,"trx_in_block":4294967295,"op_in_trx":3,"virtual_op":true,"timestamp":"2016-01-01T00:01:12","op":["fill_vesting_withdraw",{"from_account":"alice4ah","to_account":"alice4ah","withdrawn":"0.000005 VESTS","deposited":"0.000 TESTS"}]}])~"
      }, {
      R"~([21,{"trx_id":"0000000000000000000000000000000000000000","block":28,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:01:24","op":{"type":"fill_vesting_withdraw_operation","value":{"from_account":"alice4ah","to_account":"ben4ah","withdrawn":{"amount":"4","precision":6,"nai":"@@000000037"},"deposited":{"amount":"4","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([21,{"trx_id":"0000000000000000000000000000000000000000","block":28,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:01:24","op":["fill_vesting_withdraw",{"from_account":"alice4ah","to_account":"ben4ah","withdrawn":"0.000004 VESTS","deposited":"0.000004 VESTS"}]}])~"
      }, {
      R"~([22,{"trx_id":"0000000000000000000000000000000000000000","block":28,"trx_in_block":4294967295,"op_in_trx":3,"virtual_op":true,"timestamp":"2016-01-01T00:01:24","op":{"type":"fill_vesting_withdraw_operation","value":{"from_account":"alice4ah","to_account":"alice4ah","withdrawn":{"amount":"5","precision":6,"nai":"@@000000037"},"deposited":{"amount":"0","precision":3,"nai":"@@000000021"}}},"operation_id":0}])~",
      R"~([22,{"trx_id":"0000000000000000000000000000000000000000","block":28,"trx_in_block":4294967295,"op_in_trx":3,"virtual_op":true,"timestamp":"2016-01-01T00:01:24","op":["fill_vesting_withdraw",{"from_account":"alice4ah","to_account":"alice4ah","withdrawn":"0.000005 VESTS","deposited":"0.000 TESTS"}]}])~"
      } };

    expected_t expected_ben4ah_history = { {
      R"~([2,{"trx_id":"360a81f869a3ffaa40fdead3ab857804851662c9","block":3,"trx_in_block":3,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":{"type":"transfer_to_vesting_operation","value":{"from":"initminer","to":"ben4ah","amount":{"amount":"100","precision":3,"nai":"@@000000021"}}},"operation_id":0}])~",
      R"~([2,{"trx_id":"360a81f869a3ffaa40fdead3ab857804851662c9","block":3,"trx_in_block":3,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":["transfer_to_vesting",{"from":"initminer","to":"ben4ah","amount":"0.100 TESTS"}]}])~"
      }, {
      R"~([3,{"trx_id":"360a81f869a3ffaa40fdead3ab857804851662c9","block":3,"trx_in_block":3,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":{"type":"transfer_to_vesting_completed_operation","value":{"from_account":"initminer","to_account":"ben4ah","hive_vested":{"amount":"100","precision":3,"nai":"@@000000021"},"vesting_shares_received":{"amount":"98716683119","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([3,{"trx_id":"360a81f869a3ffaa40fdead3ab857804851662c9","block":3,"trx_in_block":3,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":["transfer_to_vesting_completed",{"from_account":"initminer","to_account":"ben4ah","hive_vested":"0.100 TESTS","vesting_shares_received":"98716.683119 VESTS"}]}])~"
      }, {
      R"~([4,{"trx_id":"f836d85674c299b307b8168bd3c18d9018de4bb6","block":5,"trx_in_block":1,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":{"type":"set_withdraw_vesting_route_operation","value":{"from_account":"alice4ah","to_account":"ben4ah","percent":5000,"auto_vest":true}},"operation_id":0}])~",
      R"~([4,{"trx_id":"f836d85674c299b307b8168bd3c18d9018de4bb6","block":5,"trx_in_block":1,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":["set_withdraw_vesting_route",{"from_account":"alice4ah","to_account":"ben4ah","percent":5000,"auto_vest":true}]}])~"
      }, {
      R"~([5,{"trx_id":"0000000000000000000000000000000000000000","block":8,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:00:24","op":{"type":"fill_vesting_withdraw_operation","value":{"from_account":"alice4ah","to_account":"ben4ah","withdrawn":{"amount":"4","precision":6,"nai":"@@000000037"},"deposited":{"amount":"4","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([5,{"trx_id":"0000000000000000000000000000000000000000","block":8,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:00:24","op":["fill_vesting_withdraw",{"from_account":"alice4ah","to_account":"ben4ah","withdrawn":"0.000004 VESTS","deposited":"0.000004 VESTS"}]}])~"
      }, {
      R"~([6,{"trx_id":"0000000000000000000000000000000000000000","block":12,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:00:36","op":{"type":"fill_vesting_withdraw_operation","value":{"from_account":"alice4ah","to_account":"ben4ah","withdrawn":{"amount":"4","precision":6,"nai":"@@000000037"},"deposited":{"amount":"4","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([6,{"trx_id":"0000000000000000000000000000000000000000","block":12,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:00:36","op":["fill_vesting_withdraw",{"from_account":"alice4ah","to_account":"ben4ah","withdrawn":"0.000004 VESTS","deposited":"0.000004 VESTS"}]}])~"
      }, {
      R"~([7,{"trx_id":"0000000000000000000000000000000000000000","block":16,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:00:48","op":{"type":"fill_vesting_withdraw_operation","value":{"from_account":"alice4ah","to_account":"ben4ah","withdrawn":{"amount":"4","precision":6,"nai":"@@000000037"},"deposited":{"amount":"4","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([7,{"trx_id":"0000000000000000000000000000000000000000","block":16,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:00:48","op":["fill_vesting_withdraw",{"from_account":"alice4ah","to_account":"ben4ah","withdrawn":"0.000004 VESTS","deposited":"0.000004 VESTS"}]}])~"
      }, {
      R"~([8,{"trx_id":"0000000000000000000000000000000000000000","block":20,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:01:00","op":{"type":"fill_vesting_withdraw_operation","value":{"from_account":"alice4ah","to_account":"ben4ah","withdrawn":{"amount":"4","precision":6,"nai":"@@000000037"},"deposited":{"amount":"4","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([8,{"trx_id":"0000000000000000000000000000000000000000","block":20,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:01:00","op":["fill_vesting_withdraw",{"from_account":"alice4ah","to_account":"ben4ah","withdrawn":"0.000004 VESTS","deposited":"0.000004 VESTS"}]}])~"
      }, {
      R"~([9,{"trx_id":"0000000000000000000000000000000000000000","block":24,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:01:12","op":{"type":"fill_vesting_withdraw_operation","value":{"from_account":"alice4ah","to_account":"ben4ah","withdrawn":{"amount":"4","precision":6,"nai":"@@000000037"},"deposited":{"amount":"4","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([9,{"trx_id":"0000000000000000000000000000000000000000","block":24,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:01:12","op":["fill_vesting_withdraw",{"from_account":"alice4ah","to_account":"ben4ah","withdrawn":"0.000004 VESTS","deposited":"0.000004 VESTS"}]}])~"
      }, {
      R"~([10,{"trx_id":"0000000000000000000000000000000000000000","block":28,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:01:24","op":{"type":"fill_vesting_withdraw_operation","value":{"from_account":"alice4ah","to_account":"ben4ah","withdrawn":{"amount":"4","precision":6,"nai":"@@000000037"},"deposited":{"amount":"4","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([10,{"trx_id":"0000000000000000000000000000000000000000","block":28,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:01:24","op":["fill_vesting_withdraw",{"from_account":"alice4ah","to_account":"ben4ah","withdrawn":"0.000004 VESTS","deposited":"0.000004 VESTS"}]}])~"
      } };

    expected_t expected_carol4ah_history = { {
      R"~([2,{"trx_id":"5a24ddb0615adf4dac27aceec853a8a37bbd8a63","block":3,"trx_in_block":5,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":{"type":"transfer_to_vesting_operation","value":{"from":"initminer","to":"carol4ah","amount":{"amount":"100","precision":3,"nai":"@@000000021"}}},"operation_id":0}])~",
      R"~([2,{"trx_id":"5a24ddb0615adf4dac27aceec853a8a37bbd8a63","block":3,"trx_in_block":5,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":["transfer_to_vesting",{"from":"initminer","to":"carol4ah","amount":"0.100 TESTS"}]}])~"
      }, {
      R"~([3,{"trx_id":"5a24ddb0615adf4dac27aceec853a8a37bbd8a63","block":3,"trx_in_block":5,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":{"type":"transfer_to_vesting_completed_operation","value":{"from_account":"initminer","to_account":"carol4ah","hive_vested":{"amount":"100","precision":3,"nai":"@@000000021"},"vesting_shares_received":{"amount":"98716683119","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([3,{"trx_id":"5a24ddb0615adf4dac27aceec853a8a37bbd8a63","block":3,"trx_in_block":5,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":["transfer_to_vesting_completed",{"from_account":"initminer","to_account":"carol4ah","hive_vested":"0.100 TESTS","vesting_shares_received":"98716.683119 VESTS"}]}])~"
      }, {
      R"~([4,{"trx_id":"64261f6d3e997f3e9c7846712bde0b5d7da983fb","block":5,"trx_in_block":2,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":{"type":"delegate_vesting_shares_operation","value":{"delegator":"alice4ah","delegatee":"carol4ah","vesting_shares":{"amount":"3","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([4,{"trx_id":"64261f6d3e997f3e9c7846712bde0b5d7da983fb","block":5,"trx_in_block":2,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":["delegate_vesting_shares",{"delegator":"alice4ah","delegatee":"carol4ah","vesting_shares":"0.000003 VESTS"}]}])~"
      }, {
      R"~([5,{"trx_id":"650e7569d7c96f825886f04b4b53168e27c5707a","block":5,"trx_in_block":4,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":{"type":"delegate_vesting_shares_operation","value":{"delegator":"alice4ah","delegatee":"carol4ah","vesting_shares":{"amount":"2","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([5,{"trx_id":"650e7569d7c96f825886f04b4b53168e27c5707a","block":5,"trx_in_block":4,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":["delegate_vesting_shares",{"delegator":"alice4ah","delegatee":"carol4ah","vesting_shares":"0.000002 VESTS"}]}])~"
      } };

    // Filter out usual account_create(d)_operation checked in other tests.
    uint64_t filter_low = -1ull & ~GET_LOW_OPERATION( account_create_operation );
    uint64_t filter_high = -1ull & ~GET_HIGH_OPERATION( account_created_operation );
    test_get_account_history( *this, { "alice4ah", "ben4ah", "carol4ah" }, { expected_alice4ah_history, expected_ben4ah_history, expected_carol4ah_history }, 1000, 1000,
      filter_low /*minus account_create_operation*/, filter_high /*minus account_created_operation*/ );
  };

  vesting_scenario( check_point_tester );

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_SUITE_END() // condenser_get_account_history_tests

#endif
