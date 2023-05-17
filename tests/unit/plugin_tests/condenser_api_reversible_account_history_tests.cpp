#if defined IS_TEST_NET

#include "condenser_api_reversible_fixture.hpp"

#include <hive/plugins/account_history_api/account_history_api_plugin.hpp>
#include <hive/plugins/account_history_api/account_history_api.hpp>
#include <hive/plugins/database_api/database_api_plugin.hpp>

#include <fc/io/json.hpp>

#include <boost/test/unit_test.hpp>
#include <boost/range/combine.hpp>

typedef std::vector< std::string > expected_t;

BOOST_FIXTURE_TEST_SUITE( condenser_reversible_get_account_history_tests, condenser_api_reversible_fixture );

void test_get_account_history_reversible( const condenser_api_reversible_fixture& caf, const std::vector< std:: string >& account_names, const std::vector< expected_t >& expected_operations,
  uint64_t start = 1000, uint32_t limit = 1000, uint64_t filter_low = 0xFFFFFFFF'FFFFFFFFull, uint64_t filter_high = 0xFFFFFFFF'FFFFFFFFull )
{
  // For each requested account ...
  BOOST_REQUIRE_EQUAL( expected_operations.size(), account_names.size() );
  for(const auto [account_name, expected_for_account] : boost::combine(account_names, expected_operations))
  {
    auto ah1 = caf.account_history_api->get_account_history( {account_name, start, limit, true /*include_reversible*/, filter_low, filter_high } );
    BOOST_REQUIRE_EQUAL( expected_for_account.size(), ah1.history.size() );
    ilog( "${n} operation(s) in account ${account} history", ("n", ah1.history.size())("account", account_name) );

    // For each event (operation) in account history ...
    for (const auto& it : ah1.history)
    {
      ilog("ah op: ${op}", ("op", it));
    }
    for(const auto [actual, expected] : boost::combine(ah1.history, expected_for_account))
    {
      // Compare operations in their serialized form with expected patterns:
      BOOST_REQUIRE_EQUAL( expected, fc::json::to_string(actual) );
    }
  }
}

// #define GET_LOW_OPERATION(OPERATION) static_cast<uint64_t>( account_history::get_account_history_op_filter_low::OPERATION )
// #define GET_HIGH_OPERATION(OPERATION) static_cast<uint64_t>( account_history::get_account_history_op_filter_high::OPERATION )

BOOST_AUTO_TEST_CASE( get_account_history_comment_and_reward_all_reversible )
{ try {

  BOOST_TEST_MESSAGE( "testing get_account_history with comment_and_reward_scenario while all blocks are reversible" );

  // Generate a number of blocks sufficient that following claim_reward_operation succeeds.
  auto check_point_1_tester = [ this ]( uint32_t generate_no_further_than ) {
    for (int i = 0; i < 10; ++i) {
      generate_block();
    }
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );
  };

  // Check cumulative history now.
  // Note that comment_payout_beneficiaries occur in both account's comment_options_operation patterns.
  auto check_point_2_tester = [ this ]( uint32_t generate_no_further_than )
  {
    // Generate a single block for claim_reward_balance_operation. All blocks are still reversible.
    generate_block();
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );

    expected_t expected_dan0ah_history = {
      R"~([0,{"trx_id":"08718ba49761f8fa109e16f0cec8bf6951fe5b16","block":45,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:12","op":{"type":"account_create_operation","value":{"fee":{"amount":"0","precision":3,"nai":"@@000000021"},"creator":"initminer","new_account_name":"dan0ah","owner":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST7YJmUoKbPQkrMrZbrgPxDMYJA3uD3utaN3WYRwaFGKYbQ9ftKV",1]]},"active":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST7YJmUoKbPQkrMrZbrgPxDMYJA3uD3utaN3WYRwaFGKYbQ9ftKV",1]]},"posting":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST6HMjs2nWJ6gLw7eyoUySVHGN2uAoSc3CmCjer489SPC3Kwt1UW",1]]},"memo_key":"TST7YJmUoKbPQkrMrZbrgPxDMYJA3uD3utaN3WYRwaFGKYbQ9ftKV","json_metadata":""}},"operation_id":0}])~",
      R"~([1,{"trx_id":"08718ba49761f8fa109e16f0cec8bf6951fe5b16","block":45,"trx_in_block":0,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:02:12","op":{"type":"account_created_operation","value":{"new_account_name":"dan0ah","creator":"initminer","initial_vesting_shares":{"amount":"0","precision":6,"nai":"@@000000037"},"initial_delegation":{"amount":"0","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([2,{"trx_id":"2744c849b0cdfd02e237a4c2340f58229dc02873","block":45,"trx_in_block":1,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:12","op":{"type":"transfer_to_vesting_operation","value":{"from":"initminer","to":"dan0ah","amount":{"amount":"100","precision":3,"nai":"@@000000021"}}},"operation_id":0}])~",
      R"~([3,{"trx_id":"2744c849b0cdfd02e237a4c2340f58229dc02873","block":45,"trx_in_block":1,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:02:12","op":{"type":"transfer_to_vesting_completed_operation","value":{"from_account":"initminer","to_account":"dan0ah","hive_vested":{"amount":"100","precision":3,"nai":"@@000000021"},"vesting_shares_received":{"amount":"98716581286","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([4,{"trx_id":"4e724b453d278d2ac777ffff07d45fbb9b691d4b","block":45,"trx_in_block":5,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:12","op":{"type":"comment_options_operation","value":{"author":"edgar0ah","permlink":"permlink1","max_accepted_payout":{"amount":"10000000","precision":3,"nai":"@@000000013"},"percent_hbd":10000,"allow_votes":true,"allow_curation_rewards":true,"extensions":[{"type":"comment_payout_beneficiaries","value":{"beneficiaries":[{"account":"dan0ah","weight":5000}]}}]}},"operation_id":0}])~",
      R"~([5,{"trx_id":"f6c6bbf72b4f516e9e655ba031f1c679b30f16f4","block":45,"trx_in_block":6,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:12","op":{"type":"vote_operation","value":{"voter":"dan0ah","author":"edgar0ah","permlink":"permlink1","weight":-10000}},"operation_id":0}])~",
      R"~([6,{"trx_id":"f6c6bbf72b4f516e9e655ba031f1c679b30f16f4","block":45,"trx_in_block":6,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:02:12","op":{"type":"effective_comment_vote_operation","value":{"voter":"dan0ah","author":"edgar0ah","permlink":"permlink1","weight":0,"rshares":-1924331626,"total_vote_weight":0,"pending_payout":{"amount":"0","precision":3,"nai":"@@000000013"}}},"operation_id":0}])~",
      R"~([7,{"trx_id":"a349b0bad1f8b170174971206b4bce1808f5359e","block":45,"trx_in_block":7,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:12","op":{"type":"vote_operation","value":{"voter":"dan0ah","author":"edgar0ah","permlink":"permlink1","weight":10000}},"operation_id":0}])~",
      R"~([8,{"trx_id":"a349b0bad1f8b170174971206b4bce1808f5359e","block":45,"trx_in_block":7,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:02:12","op":{"type":"effective_comment_vote_operation","value":{"voter":"dan0ah","author":"edgar0ah","permlink":"permlink1","weight":1924331626,"rshares":1924331626,"total_vote_weight":1924331626,"pending_payout":{"amount":"0","precision":3,"nai":"@@000000013"}}},"operation_id":0}])~",
      R"~([9,{"trx_id":"0000000000000000000000000000000000000000","block":48,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:02:24","op":{"type":"curation_reward_operation","value":{"curator":"dan0ah","reward":{"amount":"2379069375285","precision":6,"nai":"@@000000037"},"comment_author":"edgar0ah","comment_permlink":"permlink1","payout_must_be_claimed":true}},"operation_id":0}])~",
      R"~([10,{"trx_id":"0000000000000000000000000000000000000000","block":48,"trx_in_block":4294967295,"op_in_trx":3,"virtual_op":true,"timestamp":"2016-01-01T00:02:24","op":{"type":"comment_benefactor_reward_operation","value":{"benefactor":"dan0ah","author":"edgar0ah","permlink":"permlink1","hbd_payout":{"amount":"602","precision":3,"nai":"@@000000013"},"hive_payout":{"amount":"0","precision":3,"nai":"@@000000021"},"vesting_payout":{"amount":"595260926679","precision":6,"nai":"@@000000037"},"payout_must_be_claimed":true}},"operation_id":0}])~",
    };

    expected_t expected_edgar0ah_history = {
      R"~([0,{"trx_id":"7640d52e665a4f0f8f682f4caeff036670e0ca05","block":45,"trx_in_block":2,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:12","op":{"type":"account_create_operation","value":{"fee":{"amount":"0","precision":3,"nai":"@@000000021"},"creator":"initminer","new_account_name":"edgar0ah","owner":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST8R8maxJxeBMR3JYmap1n3Pypm886oEUjLYdsetzcnPDFpiq3pZ",1]]},"active":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST8R8maxJxeBMR3JYmap1n3Pypm886oEUjLYdsetzcnPDFpiq3pZ",1]]},"posting":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST8ZCsvwKqttXivgPyJ1MYS4q1r3fBZJh3g1SaBxVbfsqNcmnvD3",1]]},"memo_key":"TST8R8maxJxeBMR3JYmap1n3Pypm886oEUjLYdsetzcnPDFpiq3pZ","json_metadata":""}},"operation_id":0}])~",
      R"~([1,{"trx_id":"7640d52e665a4f0f8f682f4caeff036670e0ca05","block":45,"trx_in_block":2,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:02:12","op":{"type":"account_created_operation","value":{"new_account_name":"edgar0ah","creator":"initminer","initial_vesting_shares":{"amount":"0","precision":6,"nai":"@@000000037"},"initial_delegation":{"amount":"0","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([2,{"trx_id":"99de1c211dff2d8da3ee730f66784d3c94307d53","block":45,"trx_in_block":3,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:12","op":{"type":"transfer_to_vesting_operation","value":{"from":"initminer","to":"edgar0ah","amount":{"amount":"100","precision":3,"nai":"@@000000021"}}},"operation_id":0}])~",
      R"~([3,{"trx_id":"99de1c211dff2d8da3ee730f66784d3c94307d53","block":45,"trx_in_block":3,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:02:12","op":{"type":"transfer_to_vesting_completed_operation","value":{"from_account":"initminer","to_account":"edgar0ah","hive_vested":{"amount":"100","precision":3,"nai":"@@000000021"},"vesting_shares_received":{"amount":"98716581286","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([4,{"trx_id":"49a3e629bc5c9aaa954f0cf6510a2bdde4f697a0","block":45,"trx_in_block":4,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:12","op":{"type":"comment_operation","value":{"parent_author":"","parent_permlink":"parentpermlink1","author":"edgar0ah","permlink":"permlink1","title":"Title 1","body":"Body 1","json_metadata":""}},"operation_id":0}])~",
      R"~([5,{"trx_id":"4e724b453d278d2ac777ffff07d45fbb9b691d4b","block":45,"trx_in_block":5,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:12","op":{"type":"comment_options_operation","value":{"author":"edgar0ah","permlink":"permlink1","max_accepted_payout":{"amount":"10000000","precision":3,"nai":"@@000000013"},"percent_hbd":10000,"allow_votes":true,"allow_curation_rewards":true,"extensions":[{"type":"comment_payout_beneficiaries","value":{"beneficiaries":[{"account":"dan0ah","weight":5000}]}}]}},"operation_id":0}])~",
      R"~([6,{"trx_id":"f6c6bbf72b4f516e9e655ba031f1c679b30f16f4","block":45,"trx_in_block":6,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:12","op":{"type":"vote_operation","value":{"voter":"dan0ah","author":"edgar0ah","permlink":"permlink1","weight":-10000}},"operation_id":0}])~",
      R"~([7,{"trx_id":"f6c6bbf72b4f516e9e655ba031f1c679b30f16f4","block":45,"trx_in_block":6,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:02:12","op":{"type":"effective_comment_vote_operation","value":{"voter":"dan0ah","author":"edgar0ah","permlink":"permlink1","weight":0,"rshares":-1924331626,"total_vote_weight":0,"pending_payout":{"amount":"0","precision":3,"nai":"@@000000013"}}},"operation_id":0}])~",
      R"~([8,{"trx_id":"a349b0bad1f8b170174971206b4bce1808f5359e","block":45,"trx_in_block":7,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:12","op":{"type":"vote_operation","value":{"voter":"dan0ah","author":"edgar0ah","permlink":"permlink1","weight":10000}},"operation_id":0}])~",
      R"~([9,{"trx_id":"a349b0bad1f8b170174971206b4bce1808f5359e","block":45,"trx_in_block":7,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:02:12","op":{"type":"effective_comment_vote_operation","value":{"voter":"dan0ah","author":"edgar0ah","permlink":"permlink1","weight":1924331626,"rshares":1924331626,"total_vote_weight":1924331626,"pending_payout":{"amount":"0","precision":3,"nai":"@@000000013"}}},"operation_id":0}])~",
      R"~([10,{"trx_id":"0000000000000000000000000000000000000000","block":48,"trx_in_block":4294967295,"op_in_trx":3,"virtual_op":true,"timestamp":"2016-01-01T00:02:24","op":{"type":"comment_benefactor_reward_operation","value":{"benefactor":"dan0ah","author":"edgar0ah","permlink":"permlink1","hbd_payout":{"amount":"602","precision":3,"nai":"@@000000013"},"hive_payout":{"amount":"0","precision":3,"nai":"@@000000021"},"vesting_payout":{"amount":"595260926679","precision":6,"nai":"@@000000037"},"payout_must_be_claimed":true}},"operation_id":0}])~",
      R"~([11,{"trx_id":"0000000000000000000000000000000000000000","block":48,"trx_in_block":4294967295,"op_in_trx":4,"virtual_op":true,"timestamp":"2016-01-01T00:02:24","op":{"type":"author_reward_operation","value":{"author":"edgar0ah","permlink":"permlink1","hbd_payout":{"amount":"602","precision":3,"nai":"@@000000013"},"hive_payout":{"amount":"0","precision":3,"nai":"@@000000021"},"vesting_payout":{"amount":"595260926679","precision":6,"nai":"@@000000037"},"curators_vesting_payout":{"amount":"2379069375285","precision":6,"nai":"@@000000037"},"payout_must_be_claimed":true}},"operation_id":0}])~",
      R"~([12,{"trx_id":"0000000000000000000000000000000000000000","block":48,"trx_in_block":4294967295,"op_in_trx":5,"virtual_op":true,"timestamp":"2016-01-01T00:02:24","op":{"type":"comment_reward_operation","value":{"author":"edgar0ah","permlink":"permlink1","payout":{"amount":"4820","precision":3,"nai":"@@000000013"},"author_rewards":1205,"total_payout_value":{"amount":"1205","precision":3,"nai":"@@000000013"},"curator_payout_value":{"amount":"2410","precision":3,"nai":"@@000000013"},"beneficiary_payout_value":{"amount":"1205","precision":3,"nai":"@@000000013"}}},"operation_id":0}])~",
      R"~([13,{"trx_id":"0000000000000000000000000000000000000000","block":48,"trx_in_block":4294967295,"op_in_trx":6,"virtual_op":true,"timestamp":"2016-01-01T00:02:24","op":{"type":"comment_payout_update_operation","value":{"author":"edgar0ah","permlink":"permlink1"}},"operation_id":0}])~",
      R"~([14,{"trx_id":"72eabb13250cd9f473df1dd5df96bd715cf062c0","block":55,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:42","op":{"type":"claim_reward_balance_operation","value":{"account":"edgar0ah","reward_hive":{"amount":"0","precision":3,"nai":"@@000000021"},"reward_hbd":{"amount":"1","precision":3,"nai":"@@000000013"},"reward_vests":{"amount":"1","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
    };

    BOOST_REQUIRE_EQUAL(get_last_irreversible_block_num(), 8);
    test_get_account_history_reversible( *this, { "dan0ah", "edgar0ah" }, { expected_dan0ah_history, expected_edgar0ah_history } );
  };

  comment_and_reward_scenario( check_point_1_tester, check_point_2_tester );

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( get_account_history_comment_and_reward_claim_reward_balance_operation_reversible)
{ try {

  BOOST_TEST_MESSAGE( "testing get_account_history with comment_and_reward_scenario while claim_reward_balance_operation is reversible and previous blocks are irreversible" );

  // Generate a number of blocks sufficient that following claim_reward_operation succeeds.
  auto check_point_1_tester = [ this ]( uint32_t generate_no_further_than ) {
    for (int i = 0; i < 10; ++i) {
      generate_block();
    }
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );
  };

  // Check cumulative history now.
  // Note that comment_payout_beneficiaries occur in both account's comment_options_operation patterns.
  auto check_point_2_tester = [ this ]( uint32_t generate_no_further_than )
  {
    // Generate blocks so that all comment related operations are in irreversible blocks, but claim_reward_balance_operation is still in reversible block.
    generate_until_irreversible_block( 48 );
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );

    expected_t expected_dan0ah_history = {
      R"~([0,{"trx_id":"08718ba49761f8fa109e16f0cec8bf6951fe5b16","block":45,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:12","op":{"type":"account_create_operation","value":{"fee":{"amount":"0","precision":3,"nai":"@@000000021"},"creator":"initminer","new_account_name":"dan0ah","owner":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST7YJmUoKbPQkrMrZbrgPxDMYJA3uD3utaN3WYRwaFGKYbQ9ftKV",1]]},"active":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST7YJmUoKbPQkrMrZbrgPxDMYJA3uD3utaN3WYRwaFGKYbQ9ftKV",1]]},"posting":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST6HMjs2nWJ6gLw7eyoUySVHGN2uAoSc3CmCjer489SPC3Kwt1UW",1]]},"memo_key":"TST7YJmUoKbPQkrMrZbrgPxDMYJA3uD3utaN3WYRwaFGKYbQ9ftKV","json_metadata":""}},"operation_id":0}])~",
      R"~([1,{"trx_id":"08718ba49761f8fa109e16f0cec8bf6951fe5b16","block":45,"trx_in_block":0,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:02:12","op":{"type":"account_created_operation","value":{"new_account_name":"dan0ah","creator":"initminer","initial_vesting_shares":{"amount":"0","precision":6,"nai":"@@000000037"},"initial_delegation":{"amount":"0","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([2,{"trx_id":"2744c849b0cdfd02e237a4c2340f58229dc02873","block":45,"trx_in_block":1,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:12","op":{"type":"transfer_to_vesting_operation","value":{"from":"initminer","to":"dan0ah","amount":{"amount":"100","precision":3,"nai":"@@000000021"}}},"operation_id":0}])~",
      R"~([3,{"trx_id":"2744c849b0cdfd02e237a4c2340f58229dc02873","block":45,"trx_in_block":1,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:02:12","op":{"type":"transfer_to_vesting_completed_operation","value":{"from_account":"initminer","to_account":"dan0ah","hive_vested":{"amount":"100","precision":3,"nai":"@@000000021"},"vesting_shares_received":{"amount":"98716581286","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([4,{"trx_id":"4e724b453d278d2ac777ffff07d45fbb9b691d4b","block":45,"trx_in_block":5,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:12","op":{"type":"comment_options_operation","value":{"author":"edgar0ah","permlink":"permlink1","max_accepted_payout":{"amount":"10000000","precision":3,"nai":"@@000000013"},"percent_hbd":10000,"allow_votes":true,"allow_curation_rewards":true,"extensions":[{"type":"comment_payout_beneficiaries","value":{"beneficiaries":[{"account":"dan0ah","weight":5000}]}}]}},"operation_id":0}])~",
      R"~([5,{"trx_id":"f6c6bbf72b4f516e9e655ba031f1c679b30f16f4","block":45,"trx_in_block":6,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:12","op":{"type":"vote_operation","value":{"voter":"dan0ah","author":"edgar0ah","permlink":"permlink1","weight":-10000}},"operation_id":0}])~",
      R"~([6,{"trx_id":"f6c6bbf72b4f516e9e655ba031f1c679b30f16f4","block":45,"trx_in_block":6,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:02:12","op":{"type":"effective_comment_vote_operation","value":{"voter":"dan0ah","author":"edgar0ah","permlink":"permlink1","weight":0,"rshares":-1924331626,"total_vote_weight":0,"pending_payout":{"amount":"0","precision":3,"nai":"@@000000013"}}},"operation_id":0}])~",
      R"~([7,{"trx_id":"a349b0bad1f8b170174971206b4bce1808f5359e","block":45,"trx_in_block":7,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:12","op":{"type":"vote_operation","value":{"voter":"dan0ah","author":"edgar0ah","permlink":"permlink1","weight":10000}},"operation_id":0}])~",
      R"~([8,{"trx_id":"a349b0bad1f8b170174971206b4bce1808f5359e","block":45,"trx_in_block":7,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:02:12","op":{"type":"effective_comment_vote_operation","value":{"voter":"dan0ah","author":"edgar0ah","permlink":"permlink1","weight":1924331626,"rshares":1924331626,"total_vote_weight":1924331626,"pending_payout":{"amount":"0","precision":3,"nai":"@@000000013"}}},"operation_id":0}])~",
      R"~([9,{"trx_id":"0000000000000000000000000000000000000000","block":48,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:02:24","op":{"type":"curation_reward_operation","value":{"curator":"dan0ah","reward":{"amount":"2379069375285","precision":6,"nai":"@@000000037"},"comment_author":"edgar0ah","comment_permlink":"permlink1","payout_must_be_claimed":true}},"operation_id":0}])~",
      R"~([10,{"trx_id":"0000000000000000000000000000000000000000","block":48,"trx_in_block":4294967295,"op_in_trx":3,"virtual_op":true,"timestamp":"2016-01-01T00:02:24","op":{"type":"comment_benefactor_reward_operation","value":{"benefactor":"dan0ah","author":"edgar0ah","permlink":"permlink1","hbd_payout":{"amount":"602","precision":3,"nai":"@@000000013"},"hive_payout":{"amount":"0","precision":3,"nai":"@@000000021"},"vesting_payout":{"amount":"595260926679","precision":6,"nai":"@@000000037"},"payout_must_be_claimed":true}},"operation_id":0}])~",
    };

    expected_t expected_edgar0ah_history = {
      R"~([0,{"trx_id":"7640d52e665a4f0f8f682f4caeff036670e0ca05","block":45,"trx_in_block":2,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:12","op":{"type":"account_create_operation","value":{"fee":{"amount":"0","precision":3,"nai":"@@000000021"},"creator":"initminer","new_account_name":"edgar0ah","owner":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST8R8maxJxeBMR3JYmap1n3Pypm886oEUjLYdsetzcnPDFpiq3pZ",1]]},"active":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST8R8maxJxeBMR3JYmap1n3Pypm886oEUjLYdsetzcnPDFpiq3pZ",1]]},"posting":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST8ZCsvwKqttXivgPyJ1MYS4q1r3fBZJh3g1SaBxVbfsqNcmnvD3",1]]},"memo_key":"TST8R8maxJxeBMR3JYmap1n3Pypm886oEUjLYdsetzcnPDFpiq3pZ","json_metadata":""}},"operation_id":0}])~",
      R"~([1,{"trx_id":"7640d52e665a4f0f8f682f4caeff036670e0ca05","block":45,"trx_in_block":2,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:02:12","op":{"type":"account_created_operation","value":{"new_account_name":"edgar0ah","creator":"initminer","initial_vesting_shares":{"amount":"0","precision":6,"nai":"@@000000037"},"initial_delegation":{"amount":"0","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([2,{"trx_id":"99de1c211dff2d8da3ee730f66784d3c94307d53","block":45,"trx_in_block":3,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:12","op":{"type":"transfer_to_vesting_operation","value":{"from":"initminer","to":"edgar0ah","amount":{"amount":"100","precision":3,"nai":"@@000000021"}}},"operation_id":0}])~",
      R"~([3,{"trx_id":"99de1c211dff2d8da3ee730f66784d3c94307d53","block":45,"trx_in_block":3,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:02:12","op":{"type":"transfer_to_vesting_completed_operation","value":{"from_account":"initminer","to_account":"edgar0ah","hive_vested":{"amount":"100","precision":3,"nai":"@@000000021"},"vesting_shares_received":{"amount":"98716581286","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([4,{"trx_id":"49a3e629bc5c9aaa954f0cf6510a2bdde4f697a0","block":45,"trx_in_block":4,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:12","op":{"type":"comment_operation","value":{"parent_author":"","parent_permlink":"parentpermlink1","author":"edgar0ah","permlink":"permlink1","title":"Title 1","body":"Body 1","json_metadata":""}},"operation_id":0}])~",
      R"~([5,{"trx_id":"4e724b453d278d2ac777ffff07d45fbb9b691d4b","block":45,"trx_in_block":5,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:12","op":{"type":"comment_options_operation","value":{"author":"edgar0ah","permlink":"permlink1","max_accepted_payout":{"amount":"10000000","precision":3,"nai":"@@000000013"},"percent_hbd":10000,"allow_votes":true,"allow_curation_rewards":true,"extensions":[{"type":"comment_payout_beneficiaries","value":{"beneficiaries":[{"account":"dan0ah","weight":5000}]}}]}},"operation_id":0}])~",
      R"~([6,{"trx_id":"f6c6bbf72b4f516e9e655ba031f1c679b30f16f4","block":45,"trx_in_block":6,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:12","op":{"type":"vote_operation","value":{"voter":"dan0ah","author":"edgar0ah","permlink":"permlink1","weight":-10000}},"operation_id":0}])~",
      R"~([7,{"trx_id":"f6c6bbf72b4f516e9e655ba031f1c679b30f16f4","block":45,"trx_in_block":6,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:02:12","op":{"type":"effective_comment_vote_operation","value":{"voter":"dan0ah","author":"edgar0ah","permlink":"permlink1","weight":0,"rshares":-1924331626,"total_vote_weight":0,"pending_payout":{"amount":"0","precision":3,"nai":"@@000000013"}}},"operation_id":0}])~",
      R"~([8,{"trx_id":"a349b0bad1f8b170174971206b4bce1808f5359e","block":45,"trx_in_block":7,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:12","op":{"type":"vote_operation","value":{"voter":"dan0ah","author":"edgar0ah","permlink":"permlink1","weight":10000}},"operation_id":0}])~",
      R"~([9,{"trx_id":"a349b0bad1f8b170174971206b4bce1808f5359e","block":45,"trx_in_block":7,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:02:12","op":{"type":"effective_comment_vote_operation","value":{"voter":"dan0ah","author":"edgar0ah","permlink":"permlink1","weight":1924331626,"rshares":1924331626,"total_vote_weight":1924331626,"pending_payout":{"amount":"0","precision":3,"nai":"@@000000013"}}},"operation_id":0}])~",
      R"~([10,{"trx_id":"0000000000000000000000000000000000000000","block":48,"trx_in_block":4294967295,"op_in_trx":3,"virtual_op":true,"timestamp":"2016-01-01T00:02:24","op":{"type":"comment_benefactor_reward_operation","value":{"benefactor":"dan0ah","author":"edgar0ah","permlink":"permlink1","hbd_payout":{"amount":"602","precision":3,"nai":"@@000000013"},"hive_payout":{"amount":"0","precision":3,"nai":"@@000000021"},"vesting_payout":{"amount":"595260926679","precision":6,"nai":"@@000000037"},"payout_must_be_claimed":true}},"operation_id":0}])~",
      R"~([11,{"trx_id":"0000000000000000000000000000000000000000","block":48,"trx_in_block":4294967295,"op_in_trx":4,"virtual_op":true,"timestamp":"2016-01-01T00:02:24","op":{"type":"author_reward_operation","value":{"author":"edgar0ah","permlink":"permlink1","hbd_payout":{"amount":"602","precision":3,"nai":"@@000000013"},"hive_payout":{"amount":"0","precision":3,"nai":"@@000000021"},"vesting_payout":{"amount":"595260926679","precision":6,"nai":"@@000000037"},"curators_vesting_payout":{"amount":"2379069375285","precision":6,"nai":"@@000000037"},"payout_must_be_claimed":true}},"operation_id":0}])~",
      R"~([12,{"trx_id":"0000000000000000000000000000000000000000","block":48,"trx_in_block":4294967295,"op_in_trx":5,"virtual_op":true,"timestamp":"2016-01-01T00:02:24","op":{"type":"comment_reward_operation","value":{"author":"edgar0ah","permlink":"permlink1","payout":{"amount":"4820","precision":3,"nai":"@@000000013"},"author_rewards":1205,"total_payout_value":{"amount":"1205","precision":3,"nai":"@@000000013"},"curator_payout_value":{"amount":"2410","precision":3,"nai":"@@000000013"},"beneficiary_payout_value":{"amount":"1205","precision":3,"nai":"@@000000013"}}},"operation_id":0}])~",
      R"~([13,{"trx_id":"0000000000000000000000000000000000000000","block":48,"trx_in_block":4294967295,"op_in_trx":6,"virtual_op":true,"timestamp":"2016-01-01T00:02:24","op":{"type":"comment_payout_update_operation","value":{"author":"edgar0ah","permlink":"permlink1"}},"operation_id":0}])~",
      R"~([14,{"trx_id":"72eabb13250cd9f473df1dd5df96bd715cf062c0","block":55,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:42","op":{"type":"claim_reward_balance_operation","value":{"account":"edgar0ah","reward_hive":{"amount":"0","precision":3,"nai":"@@000000021"},"reward_hbd":{"amount":"1","precision":3,"nai":"@@000000013"},"reward_vests":{"amount":"1","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
    };

    BOOST_REQUIRE_EQUAL(get_last_irreversible_block_num(), 48);
    test_get_account_history_reversible( *this, { "dan0ah", "edgar0ah" }, { expected_dan0ah_history, expected_edgar0ah_history } );
  };

  comment_and_reward_scenario( check_point_1_tester, check_point_2_tester );

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( get_account_history_comment_and_reward_all_irreversible )
{ try {

  BOOST_TEST_MESSAGE( "testing get_account_history with comment_and_reward_scenario while all blocks are irreversible" );

  // Generate a number of blocks sufficient that following claim_reward_operation succeeds.
  auto check_point_1_tester = [ this ]( uint32_t generate_no_further_than ) {
    generate_until_irreversible_block( 48 );
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );
  };

  // Check cumulative history now.
  // Note that comment_payout_beneficiaries occur in both account's comment_options_operation patterns.
  auto check_point_2_tester = [ this ]( uint32_t generate_no_further_than )
  {
    // Generate blocks so that all comment operations and claim_reward_balance_operation are in irreversible blocks.
    generate_until_irreversible_block( 64 );
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );

    expected_t expected_dan0ah_history = {
      R"~([0,{"trx_id":"08718ba49761f8fa109e16f0cec8bf6951fe5b16","block":45,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:12","op":{"type":"account_create_operation","value":{"fee":{"amount":"0","precision":3,"nai":"@@000000021"},"creator":"initminer","new_account_name":"dan0ah","owner":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST7YJmUoKbPQkrMrZbrgPxDMYJA3uD3utaN3WYRwaFGKYbQ9ftKV",1]]},"active":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST7YJmUoKbPQkrMrZbrgPxDMYJA3uD3utaN3WYRwaFGKYbQ9ftKV",1]]},"posting":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST6HMjs2nWJ6gLw7eyoUySVHGN2uAoSc3CmCjer489SPC3Kwt1UW",1]]},"memo_key":"TST7YJmUoKbPQkrMrZbrgPxDMYJA3uD3utaN3WYRwaFGKYbQ9ftKV","json_metadata":""}},"operation_id":0}])~",
      R"~([1,{"trx_id":"08718ba49761f8fa109e16f0cec8bf6951fe5b16","block":45,"trx_in_block":0,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:02:12","op":{"type":"account_created_operation","value":{"new_account_name":"dan0ah","creator":"initminer","initial_vesting_shares":{"amount":"0","precision":6,"nai":"@@000000037"},"initial_delegation":{"amount":"0","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([2,{"trx_id":"2744c849b0cdfd02e237a4c2340f58229dc02873","block":45,"trx_in_block":1,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:12","op":{"type":"transfer_to_vesting_operation","value":{"from":"initminer","to":"dan0ah","amount":{"amount":"100","precision":3,"nai":"@@000000021"}}},"operation_id":0}])~",
      R"~([3,{"trx_id":"2744c849b0cdfd02e237a4c2340f58229dc02873","block":45,"trx_in_block":1,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:02:12","op":{"type":"transfer_to_vesting_completed_operation","value":{"from_account":"initminer","to_account":"dan0ah","hive_vested":{"amount":"100","precision":3,"nai":"@@000000021"},"vesting_shares_received":{"amount":"98716581286","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([4,{"trx_id":"4e724b453d278d2ac777ffff07d45fbb9b691d4b","block":45,"trx_in_block":5,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:12","op":{"type":"comment_options_operation","value":{"author":"edgar0ah","permlink":"permlink1","max_accepted_payout":{"amount":"10000000","precision":3,"nai":"@@000000013"},"percent_hbd":10000,"allow_votes":true,"allow_curation_rewards":true,"extensions":[{"type":"comment_payout_beneficiaries","value":{"beneficiaries":[{"account":"dan0ah","weight":5000}]}}]}},"operation_id":0}])~",
      R"~([5,{"trx_id":"f6c6bbf72b4f516e9e655ba031f1c679b30f16f4","block":45,"trx_in_block":6,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:12","op":{"type":"vote_operation","value":{"voter":"dan0ah","author":"edgar0ah","permlink":"permlink1","weight":-10000}},"operation_id":0}])~",
      R"~([6,{"trx_id":"f6c6bbf72b4f516e9e655ba031f1c679b30f16f4","block":45,"trx_in_block":6,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:02:12","op":{"type":"effective_comment_vote_operation","value":{"voter":"dan0ah","author":"edgar0ah","permlink":"permlink1","weight":0,"rshares":-1924331626,"total_vote_weight":0,"pending_payout":{"amount":"0","precision":3,"nai":"@@000000013"}}},"operation_id":0}])~",
      R"~([7,{"trx_id":"a349b0bad1f8b170174971206b4bce1808f5359e","block":45,"trx_in_block":7,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:12","op":{"type":"vote_operation","value":{"voter":"dan0ah","author":"edgar0ah","permlink":"permlink1","weight":10000}},"operation_id":0}])~",
      R"~([8,{"trx_id":"a349b0bad1f8b170174971206b4bce1808f5359e","block":45,"trx_in_block":7,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:02:12","op":{"type":"effective_comment_vote_operation","value":{"voter":"dan0ah","author":"edgar0ah","permlink":"permlink1","weight":1924331626,"rshares":1924331626,"total_vote_weight":1924331626,"pending_payout":{"amount":"0","precision":3,"nai":"@@000000013"}}},"operation_id":0}])~",
      R"~([9,{"trx_id":"0000000000000000000000000000000000000000","block":48,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:02:24","op":{"type":"curation_reward_operation","value":{"curator":"dan0ah","reward":{"amount":"2379069375285","precision":6,"nai":"@@000000037"},"comment_author":"edgar0ah","comment_permlink":"permlink1","payout_must_be_claimed":true}},"operation_id":0}])~",
      R"~([10,{"trx_id":"0000000000000000000000000000000000000000","block":48,"trx_in_block":4294967295,"op_in_trx":3,"virtual_op":true,"timestamp":"2016-01-01T00:02:24","op":{"type":"comment_benefactor_reward_operation","value":{"benefactor":"dan0ah","author":"edgar0ah","permlink":"permlink1","hbd_payout":{"amount":"602","precision":3,"nai":"@@000000013"},"hive_payout":{"amount":"0","precision":3,"nai":"@@000000021"},"vesting_payout":{"amount":"595260926679","precision":6,"nai":"@@000000037"},"payout_must_be_claimed":true}},"operation_id":0}])~",
    };

    expected_t expected_edgar0ah_history = {
      R"~([0,{"trx_id":"7640d52e665a4f0f8f682f4caeff036670e0ca05","block":45,"trx_in_block":2,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:12","op":{"type":"account_create_operation","value":{"fee":{"amount":"0","precision":3,"nai":"@@000000021"},"creator":"initminer","new_account_name":"edgar0ah","owner":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST8R8maxJxeBMR3JYmap1n3Pypm886oEUjLYdsetzcnPDFpiq3pZ",1]]},"active":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST8R8maxJxeBMR3JYmap1n3Pypm886oEUjLYdsetzcnPDFpiq3pZ",1]]},"posting":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST8ZCsvwKqttXivgPyJ1MYS4q1r3fBZJh3g1SaBxVbfsqNcmnvD3",1]]},"memo_key":"TST8R8maxJxeBMR3JYmap1n3Pypm886oEUjLYdsetzcnPDFpiq3pZ","json_metadata":""}},"operation_id":0}])~",
      R"~([1,{"trx_id":"7640d52e665a4f0f8f682f4caeff036670e0ca05","block":45,"trx_in_block":2,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:02:12","op":{"type":"account_created_operation","value":{"new_account_name":"edgar0ah","creator":"initminer","initial_vesting_shares":{"amount":"0","precision":6,"nai":"@@000000037"},"initial_delegation":{"amount":"0","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([2,{"trx_id":"99de1c211dff2d8da3ee730f66784d3c94307d53","block":45,"trx_in_block":3,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:12","op":{"type":"transfer_to_vesting_operation","value":{"from":"initminer","to":"edgar0ah","amount":{"amount":"100","precision":3,"nai":"@@000000021"}}},"operation_id":0}])~",
      R"~([3,{"trx_id":"99de1c211dff2d8da3ee730f66784d3c94307d53","block":45,"trx_in_block":3,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:02:12","op":{"type":"transfer_to_vesting_completed_operation","value":{"from_account":"initminer","to_account":"edgar0ah","hive_vested":{"amount":"100","precision":3,"nai":"@@000000021"},"vesting_shares_received":{"amount":"98716581286","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([4,{"trx_id":"49a3e629bc5c9aaa954f0cf6510a2bdde4f697a0","block":45,"trx_in_block":4,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:12","op":{"type":"comment_operation","value":{"parent_author":"","parent_permlink":"parentpermlink1","author":"edgar0ah","permlink":"permlink1","title":"Title 1","body":"Body 1","json_metadata":""}},"operation_id":0}])~",
      R"~([5,{"trx_id":"4e724b453d278d2ac777ffff07d45fbb9b691d4b","block":45,"trx_in_block":5,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:12","op":{"type":"comment_options_operation","value":{"author":"edgar0ah","permlink":"permlink1","max_accepted_payout":{"amount":"10000000","precision":3,"nai":"@@000000013"},"percent_hbd":10000,"allow_votes":true,"allow_curation_rewards":true,"extensions":[{"type":"comment_payout_beneficiaries","value":{"beneficiaries":[{"account":"dan0ah","weight":5000}]}}]}},"operation_id":0}])~",
      R"~([6,{"trx_id":"f6c6bbf72b4f516e9e655ba031f1c679b30f16f4","block":45,"trx_in_block":6,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:12","op":{"type":"vote_operation","value":{"voter":"dan0ah","author":"edgar0ah","permlink":"permlink1","weight":-10000}},"operation_id":0}])~",
      R"~([7,{"trx_id":"f6c6bbf72b4f516e9e655ba031f1c679b30f16f4","block":45,"trx_in_block":6,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:02:12","op":{"type":"effective_comment_vote_operation","value":{"voter":"dan0ah","author":"edgar0ah","permlink":"permlink1","weight":0,"rshares":-1924331626,"total_vote_weight":0,"pending_payout":{"amount":"0","precision":3,"nai":"@@000000013"}}},"operation_id":0}])~",
      R"~([8,{"trx_id":"a349b0bad1f8b170174971206b4bce1808f5359e","block":45,"trx_in_block":7,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:12","op":{"type":"vote_operation","value":{"voter":"dan0ah","author":"edgar0ah","permlink":"permlink1","weight":10000}},"operation_id":0}])~",
      R"~([9,{"trx_id":"a349b0bad1f8b170174971206b4bce1808f5359e","block":45,"trx_in_block":7,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:02:12","op":{"type":"effective_comment_vote_operation","value":{"voter":"dan0ah","author":"edgar0ah","permlink":"permlink1","weight":1924331626,"rshares":1924331626,"total_vote_weight":1924331626,"pending_payout":{"amount":"0","precision":3,"nai":"@@000000013"}}},"operation_id":0}])~",
      R"~([10,{"trx_id":"0000000000000000000000000000000000000000","block":48,"trx_in_block":4294967295,"op_in_trx":3,"virtual_op":true,"timestamp":"2016-01-01T00:02:24","op":{"type":"comment_benefactor_reward_operation","value":{"benefactor":"dan0ah","author":"edgar0ah","permlink":"permlink1","hbd_payout":{"amount":"602","precision":3,"nai":"@@000000013"},"hive_payout":{"amount":"0","precision":3,"nai":"@@000000021"},"vesting_payout":{"amount":"595260926679","precision":6,"nai":"@@000000037"},"payout_must_be_claimed":true}},"operation_id":0}])~",
      R"~([11,{"trx_id":"0000000000000000000000000000000000000000","block":48,"trx_in_block":4294967295,"op_in_trx":4,"virtual_op":true,"timestamp":"2016-01-01T00:02:24","op":{"type":"author_reward_operation","value":{"author":"edgar0ah","permlink":"permlink1","hbd_payout":{"amount":"602","precision":3,"nai":"@@000000013"},"hive_payout":{"amount":"0","precision":3,"nai":"@@000000021"},"vesting_payout":{"amount":"595260926679","precision":6,"nai":"@@000000037"},"curators_vesting_payout":{"amount":"2379069375285","precision":6,"nai":"@@000000037"},"payout_must_be_claimed":true}},"operation_id":0}])~",
      R"~([12,{"trx_id":"0000000000000000000000000000000000000000","block":48,"trx_in_block":4294967295,"op_in_trx":5,"virtual_op":true,"timestamp":"2016-01-01T00:02:24","op":{"type":"comment_reward_operation","value":{"author":"edgar0ah","permlink":"permlink1","payout":{"amount":"4820","precision":3,"nai":"@@000000013"},"author_rewards":1205,"total_payout_value":{"amount":"1205","precision":3,"nai":"@@000000013"},"curator_payout_value":{"amount":"2410","precision":3,"nai":"@@000000013"},"beneficiary_payout_value":{"amount":"1205","precision":3,"nai":"@@000000013"}}},"operation_id":0}])~",
      R"~([13,{"trx_id":"0000000000000000000000000000000000000000","block":48,"trx_in_block":4294967295,"op_in_trx":6,"virtual_op":true,"timestamp":"2016-01-01T00:02:24","op":{"type":"comment_payout_update_operation","value":{"author":"edgar0ah","permlink":"permlink1"}},"operation_id":0}])~",
      R"~([14,{"trx_id":"7347df12e3a394ba2f2f231239cd665566b3e112","block":64,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:03:09","op":{"type":"claim_reward_balance_operation","value":{"account":"edgar0ah","reward_hive":{"amount":"0","precision":3,"nai":"@@000000021"},"reward_hbd":{"amount":"1","precision":3,"nai":"@@000000013"},"reward_vests":{"amount":"1","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
    };

    BOOST_REQUIRE_EQUAL(get_last_irreversible_block_num(), 64);
    test_get_account_history_reversible( *this, { "dan0ah", "edgar0ah" }, { expected_dan0ah_history, expected_edgar0ah_history } );
  };

  comment_and_reward_scenario( check_point_1_tester, check_point_2_tester );

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_SUITE_END() // condenser_reversible_get_account_history_tests

#endif
