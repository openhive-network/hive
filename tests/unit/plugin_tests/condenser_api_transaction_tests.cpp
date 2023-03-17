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

BOOST_FIXTURE_TEST_SUITE( condenser_get_transaction_tests, condenser_api_fixture );

// TODO: Improve by comparing get_transaction results against patterns provided as additional argument.
void test_get_transaction( const condenser_api_fixture& caf, uint32_t block_num )
{
  auto full_block_ptr = caf.db->fetch_block_by_number( block_num );
  const signed_block& block = full_block_ptr->get_block();

  for( const auto& trx : block.transactions )
  {
    const auto& trx_id = trx.id();
    BOOST_REQUIRE( trx_id != hive::protocol::transaction_id_type() );

      // Call condenser get_transaction and verify results with result of account history variant.
      const auto tx_hash = trx_id.str();
      const auto result = caf.condenser_api->get_transaction( condenser_api::get_transaction_args(1, fc::variant(tx_hash)) );
      const condenser_api::annotated_signed_transaction op_tx = result.value;
      ilog("operation transaction is ${tx}", ("tx", op_tx));
      const account_history::annotated_signed_transaction ah_op_tx = 
        caf.account_history_api->get_transaction( {tx_hash, false /*include_reversible*/} );
      ilog("operation transaction is ${tx}", ("tx", ah_op_tx));
      BOOST_REQUIRE_EQUAL( op_tx.transaction_id, ah_op_tx.transaction_id );
      BOOST_REQUIRE_EQUAL( op_tx.block_num, ah_op_tx.block_num );
      BOOST_REQUIRE_EQUAL( op_tx.transaction_num, ah_op_tx.transaction_num );

      // Do additional checks of condenser variant
      // Too few arguments
      BOOST_REQUIRE_THROW( caf.condenser_api->get_transaction( condenser_api::get_transaction_args() ), fc::assert_exception );
      // Too many arguments
      BOOST_REQUIRE_THROW( caf.condenser_api->get_transaction( condenser_api::get_transaction_args(2, fc::variant(tx_hash)) ), fc::assert_exception );
    }
}

BOOST_AUTO_TEST_CASE( get_transaction_hf1 )
{ try {

  BOOST_TEST_MESSAGE( "testing get_transaction on operations of HF1" );

  auto check_point_tester = [ this ]( uint32_t generate_no_further_than )
  {
    generate_until_irreversible_block( 2 );
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );

    test_get_transaction( *this, 2 ); // <- TODO: Enhance with patterns

    // In block 21 maximum block size is being changed:
    generate_until_irreversible_block( 21 );
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );

    test_get_transaction( *this, 21 ); // <- TODO: Enhance with patterns
  };

  hf1_scenario( check_point_tester );

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( get_transaction_hf12 )
{ try {

  BOOST_TEST_MESSAGE( "testing get_transaction with hf12_scenario" );

  auto check_point_tester = [ this ]( uint32_t generate_no_further_than )
  {
    generate_until_irreversible_block( 3 );
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );

    test_get_transaction( *this, 3 ); // <- TODO: Enhance with patterns
  };

  hf12_scenario( check_point_tester );

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( get_transaction_hf13 )
{ try {

  BOOST_TEST_MESSAGE( "testing get_transaction with hf13_scenario" );

  auto check_point_1_tester = [ this ]( uint32_t generate_no_further_than )
  {
    generate_until_irreversible_block( 3 );
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );

    test_get_transaction( *this, 3 ); // <- TODO: Enhance with patterns
  };

  auto check_point_2_tester = [ this ]( uint32_t generate_no_further_than )
  {
    generate_until_irreversible_block( 25 );
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );

    test_get_transaction( *this, 25 ); // <- TODO: Enhance with patterns
  };

  hf13_scenario( check_point_1_tester, check_point_2_tester );

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( get_transaction_comment_and_reward )
{ try {

  BOOST_TEST_MESSAGE( "testing get_transaction with comment_and_reward_scenario" );

  // Check virtual operations resulting from scenario's 1st set of actions some blocks later:
  auto check_point_1_tester = [ this ]( uint32_t generate_no_further_than )
  {
    generate_until_irreversible_block( 6 );
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );

    test_get_transaction( *this, 6 ); // <- TODO: Enhance with patterns
  };

  // Check operations resulting from 2nd set of actions:
  auto check_point_2_tester = [ this ]( uint32_t generate_no_further_than )
  { 
    generate_until_irreversible_block( 28 );
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );
    
    test_get_transaction( *this, 25 ); // <- TODO: Enhance with patterns
  };

  comment_and_reward_scenario( check_point_1_tester, check_point_2_tester );

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( get_transaction_convert_and_limit_order )
{ try {

  BOOST_TEST_MESSAGE( "testing get_transaction with convert_and_limit_order_scenario" );

  auto check_point_tester = [ this ]( uint32_t generate_no_further_than )
  {
    generate_until_irreversible_block( 5 );
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );

    test_get_transaction( *this, 5 ); // <- TODO: Enhance with patterns

    generate_until_irreversible_block( 1684 );
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );

    test_get_transaction( *this, 1684 ); // <- TODO: Enhance with patterns
  };

  convert_and_limit_order_scenario( check_point_tester );

} FC_LOG_AND_RETHROW() }

// TODO Create get_transaction_hf8 test here.
// TODO Create get_transaction_vesting test here.
// TODO Create get_transaction_witness test here.
// TODO Create get_transaction_escrow_and_savings test here.
// TODO Create get_transaction_proposal test here.
// TODO Create get_transaction_account test here.
// TODO Create get_transaction_custom test here.
// TODO Create get_transaction_recurrent_transfer test here.
// TODO Create get_transaction_decline_voting_rights here.

BOOST_AUTO_TEST_SUITE_END() // condenser_get_transaction_tests

#endif
