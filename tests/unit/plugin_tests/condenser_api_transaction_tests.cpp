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

BOOST_AUTO_TEST_CASE( get_transaction_signatures )
{ try {

  BOOST_TEST_MESSAGE( "testing get_transaction with different number of signatures" );

  ACTORS( (alice1trx)(bob1trx) );
  fund( "alice1trx", 100000 );
  fund( "temp", 3333 );

  authority multi_authority( 2, bob1trx_public_key, 1, alice1trx_public_key, 1 );
  account_update( "bob1trx", bob1trx_private_key.get_public_key(), "", multi_authority, multi_authority, multi_authority, bob1trx_private_key );

  generate_block();

  uint32_t trx_block = db->head_block_num() + 1;

  // Single transaction with empty signatures.
  transfer_operation op_temp;
  op_temp.from = "temp";
  op_temp.to = "bob1trx";
  op_temp.amount = ASSET( "3.333 TESTS" );

  signed_transaction tx_empty;
  tx_empty.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
  tx_empty.operations.push_back( op_temp );

  push_transaction( tx_empty, fc::ecc::private_key() );

  // Single transaction with single signature.
  transfer( "alice1trx", "bob1trx", ASSET( "2.222 TESTS" ), alice1trx_private_key );

  // Single transaction with multiple signatures.
  transfer_operation op;
  op.from = "bob1trx";
  op.to = "alice1trx";
  op.amount = ASSET( "4.444 TESTS" );

  signed_transaction tx;
  tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
  tx.operations.push_back( op );

  std::vector<fc::ecc::private_key> multi_sig = { bob1trx_private_key, alice1trx_private_key };

  push_transaction( tx, multi_sig );

  expected_t expected_transaction = { { // empty signatures
    R"~({"ref_block_num":0,"ref_block_prefix":0,"expiration":"2016-01-01T01:00:06","operations":[{"type":"transfer_operation","value":{"from":"temp","to":"bob1trx","amount":{"amount":"3333","precision":3,"nai":"@@000000021"},"memo":""}}],"extensions":[],"signatures":[],"transaction_id":"1dd0a5c6d062039d133a2944d1fed477a9ab9a0a","block_num":3,"transaction_num":0})~",
    R"~({"ref_block_num":0,"ref_block_prefix":0,"expiration":"2016-01-01T01:00:06","operations":[["transfer",{"from":"temp","to":"bob1trx","amount":"3.333 TESTS","memo":""}]],"extensions":[],"signatures":[],"transaction_id":"1dd0a5c6d062039d133a2944d1fed477a9ab9a0a","block_num":3,"transaction_num":0})~"
    }, { // single signature
    R"~({"ref_block_num":0,"ref_block_prefix":0,"expiration":"2016-01-01T01:00:06","operations":[{"type":"transfer_operation","value":{"from":"alice1trx","to":"bob1trx","amount":{"amount":"2222","precision":3,"nai":"@@000000021"},"memo":""}}],"extensions":[],"signatures":["1f04f7ee9bf4cebbb7d5f364ba94803b9d19bdab6d263ebd504480e37e10647cc6471b6d148eed6efe842c5edb9048657ecddd75c7a8a1bba7c3bd39b1abb74765"],"transaction_id":"7c966ca7900a58ed6e545cae476b59ad01876e42","block_num":3,"transaction_num":1})~",
    R"~({"ref_block_num":0,"ref_block_prefix":0,"expiration":"2016-01-01T01:00:06","operations":[["transfer",{"from":"alice1trx","to":"bob1trx","amount":"2.222 TESTS","memo":""}]],"extensions":[],"signatures":["1f04f7ee9bf4cebbb7d5f364ba94803b9d19bdab6d263ebd504480e37e10647cc6471b6d148eed6efe842c5edb9048657ecddd75c7a8a1bba7c3bd39b1abb74765"],"transaction_id":"7c966ca7900a58ed6e545cae476b59ad01876e42","block_num":3,"transaction_num":1})~"
    }, { // multiple signatures
    R"~({"ref_block_num":0,"ref_block_prefix":0,"expiration":"2016-01-01T01:00:06","operations":[{"type":"transfer_operation","value":{"from":"bob1trx","to":"alice1trx","amount":{"amount":"4444","precision":3,"nai":"@@000000021"},"memo":""}}],"extensions":[],"signatures":["1f40539ff07b60fd098fd928668a415772a95090d10c911556993481933e394b0d6d658b190966ae9a9d1f7d3b20e6ba478363b3dcf69c2544709d8850c1826a41","1f58547df5c4872255b0a58d84aae9af7a33c31e71ac88387581b1892e72e88dfa5d5d36f3c017f8acc179cd6f9c20893339932baf3c34e9f2ec5f06076bf0d6cb"],"transaction_id":"caaadbcce7583403c4cac4f3f6ae637ac0173ef0","block_num":3,"transaction_num":2})~",
    R"~({"ref_block_num":0,"ref_block_prefix":0,"expiration":"2016-01-01T01:00:06","operations":[["transfer",{"from":"bob1trx","to":"alice1trx","amount":"4.444 TESTS","memo":""}]],"extensions":[],"signatures":["1f40539ff07b60fd098fd928668a415772a95090d10c911556993481933e394b0d6d658b190966ae9a9d1f7d3b20e6ba478363b3dcf69c2544709d8850c1826a41","1f58547df5c4872255b0a58d84aae9af7a33c31e71ac88387581b1892e72e88dfa5d5d36f3c017f8acc179cd6f9c20893339932baf3c34e9f2ec5f06076bf0d6cb"],"transaction_id":"caaadbcce7583403c4cac4f3f6ae637ac0173ef0","block_num":3,"transaction_num":2})~"
    } };

  generate_until_irreversible_block( trx_block );

  test_get_transaction( *this, trx_block, expected_transaction );


} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_SUITE_END() // condenser_get_transaction_tests

#endif
