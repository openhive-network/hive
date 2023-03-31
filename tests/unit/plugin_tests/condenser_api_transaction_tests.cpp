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

BOOST_FIXTURE_TEST_SUITE( condenser_get_transaction_tests, condenser_api_fixture );

void test_get_transaction( const condenser_api_fixture& caf, uint32_t block_num, const expected_t& expected_transactions )
{
  auto full_block_ptr = caf.db->fetch_block_by_number( block_num );
  const signed_block& block = full_block_ptr->get_block();
  ilog( "Block #${b} contains ${n1} transaction(s), ${n2} expected.",
    ("n1", block.transactions.size())("b", block_num)("n2", expected_transactions.size()) );
  BOOST_REQUIRE( expected_transactions.size() == block.transactions.size() );

  for( size_t transaction_index = 0; transaction_index < expected_transactions.size(); ++transaction_index )
  {
    const auto& trx = block.transactions[ transaction_index ];
    const auto& trx_id = trx.id();
    BOOST_REQUIRE( trx_id != hive::protocol::transaction_id_type() );
    const auto& expected_transaction = expected_transactions[ transaction_index ];

    const auto tx_hash = trx_id.str();
    // Call condenser get_transaction and verify results against expected pattern.
    const auto cn_trx = caf.condenser_api->get_transaction( condenser_api::get_transaction_args(1, fc::variant(tx_hash)) );
    // Call account history get_transaction and verify results against expected pattern.
    const account_history::annotated_signed_transaction ah_trx = 
      caf.account_history_api->get_transaction( {tx_hash, false /*include_reversible*/} );

    ilog("ah trx: ${ah_trx}", (ah_trx));
    ilog("cn trx: ${cn_trx}", (cn_trx));

    // Compare transactions in their serialized form with expected patterns:
    BOOST_REQUIRE_EQUAL( expected_transaction.first, fc::json::to_string(ah_trx) );
    BOOST_REQUIRE_EQUAL( expected_transaction.second, fc::json::to_string(cn_trx) );

    // Do additional checks of condenser variant
    // Too few arguments
    BOOST_REQUIRE_THROW( caf.condenser_api->get_transaction( condenser_api::get_transaction_args() ), fc::assert_exception );
    // Too many arguments
    BOOST_REQUIRE_THROW( caf.condenser_api->get_transaction( condenser_api::get_transaction_args(2, fc::variant(tx_hash)) ), fc::assert_exception );
  }
}

BOOST_AUTO_TEST_CASE( get_transaction_1 )
{ try {

  BOOST_TEST_MESSAGE( "testing get_transaction with single operation in transaction" );

  ACTORS( (alice1trx)(bob1trx) );
  fund( "alice1trx", 100000 );
  generate_block();

  uint32_t trx_block = db->head_block_num() + 1;

  // Single transaction with single operation inside.
  transfer( "alice1trx", "bob1trx", ASSET( "3.333 TESTS" ) );

  generate_until_irreversible_block( trx_block );

  expected_t expected_transaction1 = { { // transaction with 1 operation
    R"~({"ref_block_num":0,"ref_block_prefix":0,"expiration":"2016-01-01T01:00:06","operations":[{"type":"transfer_operation","value":{"from":"alice1trx","to":"bob1trx","amount":{"amount":"3333","precision":3,"nai":"@@000000021"},"memo":""}}],"extensions":[],"signatures":[],"transaction_id":"c88bb2494897d34579370dcc4fab0d3fd2f1625c","block_num":3,"transaction_num":0})~",
    R"~({"ref_block_num":0,"ref_block_prefix":0,"expiration":"2016-01-01T01:00:06","operations":[["transfer",{"from":"alice1trx","to":"bob1trx","amount":"3.333 TESTS","memo":""}]],"extensions":[],"signatures":[],"transaction_id":"c88bb2494897d34579370dcc4fab0d3fd2f1625c","block_num":3,"transaction_num":0})~"
    } };
  test_get_transaction( *this, trx_block, { expected_transaction1 } );

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_SUITE_END() // condenser_get_transaction_tests

#endif
