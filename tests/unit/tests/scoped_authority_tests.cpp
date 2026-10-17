#ifdef IS_TEST_NET
#include <boost/test/unit_test.hpp>

#include <hive/chain/detail/state/scoped_authority_object_multiindex.hpp>
#include <hive/protocol/hardfork.hpp>
#include <hive/protocol/forward_impacted.hpp>

#include "../db_fixture/clean_database_fixture.hpp"

using namespace hive;
using namespace hive::chain;
using namespace hive::protocol;

BOOST_FIXTURE_TEST_SUITE( scoped_authority_tests, clean_database_fixture )

namespace {
create_scoped_authority_operation make_hbd_grant( const database& db, const std::vector< account_name_type >& recipients = {},
  uint32_t grant_id = 1, int64_t per_transfer = 5000, int64_t per_period = 50000,
  int64_t lifetime = 250000, uint32_t period = 86400, uint32_t duration = 7 * 86400 )
{
  create_scoped_authority_operation op;
  op.grantor = "alice";
  op.actor = "bob";
  op.grant_id = grant_id;
  op.max_per_transfer = HBD_asset( per_transfer ).to_asset();
  op.max_per_period = HBD_asset( per_period ).to_asset();
  op.max_lifetime = HBD_asset( lifetime ).to_asset();
  op.period_seconds = period;
  op.valid_from = db.head_block_time();
  op.expires_at = op.valid_from + fc::seconds( duration );
  op.allowed_recipients = recipients;
  return op;
}

const scoped_authority_object& find_grant( database& db, const account_name_type& grantor, uint32_t grant_id )
{
  return db.get< scoped_authority_object, by_grantor_id >( boost::make_tuple( grantor, grant_id ) );
}

template< typename F > bool throws_consensus_error( F&& action )
{
  try { action(); }
  catch( ... ) { return true; }
  return false;
}

scoped_transfer_operation make_transfer( const account_name_type& recipient, int64_t amount, uint32_t grant_id = 1 )
{
  scoped_transfer_operation op;
  op.grantor = "alice";
  op.actor = "bob";
  op.to = recipient;
  op.grant_id = grant_id;
  op.amount = HBD_asset( amount ).to_asset();
  op.memo = "scoped";
  return op;
}

compressed_batch_transfer_operation make_batch( std::vector< compressed_batch_transfer_input > inputs,
  std::vector< compressed_batch_transfer_output > outputs )
{
  compressed_batch_transfer_operation op;
  op.symbol = HBD_SYMBOL;
  op.inputs = std::move( inputs );
  op.outputs = std::move( outputs );
  op.memo = "batch";
  return op;
}
}

BOOST_AUTO_TEST_CASE( scoped_create_and_transfer_updates_persistent_counters )
{
  ACTORS( DEFAULT_VESTING, (alice)(bob)(merchant) );
  issue_funds( "alice", HBD_asset( 100000 ) );

  const auto grant_op = make_hbd_grant( *db, { "merchant" } );
  push_transaction( grant_op, alice_private_key );
  BOOST_REQUIRE_EQUAL( db->count< scoped_authority_object >(), 1u );

  const auto transfer_op = make_transfer( "merchant", 5000 );
  push_transaction( transfer_op, bob_private_key );

  const auto& grant = find_grant( *db, "alice", 1 );
  BOOST_REQUIRE_EQUAL( grant.lifetime_spent, HBD_asset( 5000 ).to_asset() );
  BOOST_REQUIRE_EQUAL( grant.period_spent, HBD_asset( 5000 ).to_asset() );
  BOOST_REQUIRE_EQUAL( db->get_account( "merchant" ).get_hbd_balance(), HBD_asset( 5000 ) );
  BOOST_REQUIRE_EQUAL( db->get_account( "alice" ).get_hbd_balance(), HBD_asset( 95000 ) );
}

BOOST_AUTO_TEST_CASE( scoped_per_transfer_period_and_lifetime_limits_are_enforced )
{
  ACTORS( DEFAULT_VESTING, (alice)(bob)(merchant) );
  issue_funds( "alice", HBD_asset( 100000 ) );
  push_transaction( make_hbd_grant( *db, {}, 1, 5000, 8000, 10000 ), alice_private_key );

  push_transaction( make_transfer( "merchant", 5000 ), bob_private_key ); // exact per-transfer limit
  push_transaction( make_transfer( "merchant", 3000 ), bob_private_key ); // exact period limit
  HIVE_REQUIRE_THROW( push_transaction( make_transfer( "merchant", 1 ), bob_private_key ), fc::exception );
  HIVE_REQUIRE_THROW( push_transaction( make_transfer( "merchant", 5001 ), bob_private_key ), fc::exception );

  const auto& grant = find_grant( *db, "alice", 1 );
  BOOST_REQUIRE_EQUAL( grant.period_spent, HBD_asset( 8000 ).to_asset() );
  BOOST_REQUIRE_EQUAL( grant.lifetime_spent, HBD_asset( 8000 ).to_asset() );
  BOOST_REQUIRE_EQUAL( db->get_account( "merchant" ).get_hbd_balance(), HBD_asset( 8000 ) );
}

BOOST_AUTO_TEST_CASE( scoped_period_counter_resets_but_lifetime_counter_does_not )
{
  ACTORS( DEFAULT_VESTING, (alice)(bob)(merchant) );
  issue_funds( "alice", HBD_asset( 100000 ) );
  push_transaction( make_hbd_grant( *db, {}, 1, 5000, 5000, 10000, 6, 60 ), alice_private_key );
  push_transaction( make_transfer( "merchant", 5000 ), bob_private_key );

  generate_blocks( 2 );
  push_transaction( make_transfer( "merchant", 5000 ), bob_private_key );

  const auto& grant = find_grant( *db, "alice", 1 );
  BOOST_REQUIRE_EQUAL( grant.period_spent, HBD_asset( 5000 ).to_asset() );
  BOOST_REQUIRE_EQUAL( grant.lifetime_spent, HBD_asset( 10000 ).to_asset() );
  HIVE_REQUIRE_THROW( push_transaction( make_transfer( "merchant", 1 ), bob_private_key ), fc::exception );
}

BOOST_AUTO_TEST_CASE( scoped_allowlist_revocation_and_expiration_are_consensus_enforced )
{
  ACTORS( DEFAULT_VESTING, (alice)(bob)(merchant)(other) );
  issue_funds( "alice", HBD_asset( 100000 ) );
  push_transaction( make_hbd_grant( *db, { "merchant" }, 1, 5000, 50000, 50000, 60, 3600 ), alice_private_key );

  if( !throws_consensus_error( [&] { push_transaction( make_transfer( "other", 1 ), bob_private_key ); } ) )
    BOOST_FAIL( "Transfer to a disallowed recipient was accepted" );
  push_transaction( make_transfer( "merchant", 1 ), bob_private_key );

  revoke_scoped_authority_operation revoke;
  revoke.grantor = "alice";
  revoke.grant_id = 1;
  push_transaction( revoke, alice_private_key );
  const bool revoked_pending = static_cast< bool >( find_grant( *db, "alice", 1 ).revoked );
  if( !revoked_pending ) BOOST_FAIL( "Revoke operation did not mark the grant in pending state" );
  generate_blocks( 1 );
  const bool revoked_confirmed = static_cast< bool >( find_grant( *db, "alice", 1 ).revoked );
  if( !revoked_confirmed ) BOOST_FAIL( "Revoke operation did not persist after block application" );
  const uint32_t revoke_block_num = db->head_block_num();
  for( uint32_t i = 0; i < 9; ++i )
    generate_block_from_pending();
  const bool revoked_after_ten_blocks = static_cast< bool >( find_grant( *db, "alice", 1 ).revoked );
  if( !revoked_after_ten_blocks || db->head_block_num() != revoke_block_num + 9 )
    BOOST_FAIL( "Revocation did not persist through ten confirmed blocks" );
  for( uint32_t i = 0; i < 90; ++i )
    generate_block_from_pending();
  const bool revoked_after_hundred_blocks = static_cast< bool >( find_grant( *db, "alice", 1 ).revoked );
  if( !revoked_after_hundred_blocks || db->head_block_num() != revoke_block_num + 99 )
    BOOST_FAIL( "Revocation did not persist through one hundred confirmed blocks" );
  const long long revoked_lifetime_spent = static_cast< long long >( find_grant( *db, "alice", 1 ).lifetime_spent.amount.value );
  const long long revoked_period_spent = static_cast< long long >( find_grant( *db, "alice", 1 ).period_spent.amount.value );
  const HBD_asset alice_balance_before_rejected_transfer = db->get_account( "alice" ).get_hbd_balance();
  const HBD_asset merchant_balance_before_rejected_transfer = db->get_account( "merchant" ).get_hbd_balance();
  if( !throws_consensus_error( [&] { push_transaction( make_transfer( "merchant", 2 ), bob_private_key ); } ) )
    BOOST_FAIL( "Transfer using revoked grant was accepted" );
  const long long revoked_lifetime_after = static_cast< long long >( find_grant( *db, "alice", 1 ).lifetime_spent.amount.value );
  const long long revoked_period_after = static_cast< long long >( find_grant( *db, "alice", 1 ).period_spent.amount.value );
  if( revoked_lifetime_after != revoked_lifetime_spent )
    BOOST_FAIL( "Rejected revoked transfer changed lifetime spending from " + std::to_string( revoked_lifetime_spent ) +
      " to " + std::to_string( revoked_lifetime_after ) );
  if( revoked_period_after != revoked_period_spent )
    BOOST_FAIL( "Rejected revoked transfer changed period spending" );
  BOOST_REQUIRE_EQUAL( db->get_account( "alice" ).get_hbd_balance(), alice_balance_before_rejected_transfer );
  BOOST_REQUIRE_EQUAL( db->get_account( "merchant" ).get_hbd_balance(), merchant_balance_before_rejected_transfer );

}

BOOST_AUTO_TEST_CASE( scoped_expiration_survives_confirmed_blocks )
{
  ACTORS( DEFAULT_VESTING, (alice)(bob)(merchant) );
  issue_funds( "alice", HBD_asset( 100000 ) );
  push_transaction( make_hbd_grant( *db, {}, 2, 5000, 50000, 50000, 60, 60 ), alice_private_key );
  generate_blocks( 1 ); // Confirm grant creation through normal pending transaction reapplication.
  BOOST_REQUIRE( find_grant( *db, "alice", 2 ).grant_id == 2 );

  // No pending transactions remain. Advance exactly to expires_at using normal
  // empty blocks; no grant or transfer transaction skips block validation.
  const auto expires_at = find_grant( *db, "alice", 2 ).expires_at;
  const auto seconds_to_expiry = ( expires_at - db->head_block_time() ).to_seconds();
  BOOST_REQUIRE_EQUAL( seconds_to_expiry % HIVE_BLOCK_INTERVAL, 0 );
  const auto blocks_to_expiry = seconds_to_expiry / HIVE_BLOCK_INTERVAL;
  for( uint32_t i = 0; i < blocks_to_expiry; ++i )
    generate_block_from_pending();
  BOOST_REQUIRE_EQUAL( db->head_block_time(), expires_at );

  const asset expired_lifetime_spent = find_grant( *db, "alice", 2 ).lifetime_spent;
  if( !throws_consensus_error( [&] { push_transaction( make_transfer( "merchant", 1, 2 ), bob_private_key ); } ) )
    BOOST_FAIL( "Transfer using expired grant was accepted" );
  BOOST_REQUIRE_EQUAL( find_grant( *db, "alice", 2 ).lifetime_spent, expired_lifetime_spent );
}

BOOST_AUTO_TEST_CASE( scoped_revocation_rolls_back_with_its_block )
{
  ACTORS( DEFAULT_VESTING, (alice)(bob)(merchant) );
  issue_funds( "alice", HBD_asset( 100000 ) );
  push_transaction( make_hbd_grant( *db, {}, 3, 5000, 50000, 50000, 60, 3600 ), alice_private_key );
  generate_blocks( 1 );
  push_transaction( make_transfer( "merchant", 1000, 3 ), bob_private_key );
  generate_blocks( 1 );

  const auto spent_before_revoke = find_grant( *db, "alice", 3 ).lifetime_spent;
  BOOST_REQUIRE_EQUAL( spent_before_revoke, HBD_asset( 1000 ).to_asset() );
  revoke_scoped_authority_operation revoke;
  revoke.grantor = "alice";
  revoke.grant_id = 3;
  push_transaction( revoke, alice_private_key );
  generate_blocks( 1 );
  BOOST_REQUIRE( find_grant( *db, "alice", 3 ).revoked );

  db->pop_block();
  BOOST_REQUIRE( !find_grant( *db, "alice", 3 ).revoked );
  BOOST_REQUIRE_EQUAL( find_grant( *db, "alice", 3 ).lifetime_spent, spent_before_revoke );
  BOOST_REQUIRE_EQUAL( db->get_account( "merchant" ).get_hbd_balance(), HBD_asset( 1000 ) );
}

BOOST_AUTO_TEST_CASE( scoped_transfer_requires_actor_authority_and_transaction_rollback_is_atomic )
{
  ACTORS( DEFAULT_VESTING, (alice)(bob)(merchant) );
  issue_funds( "alice", HBD_asset( 100000 ) );
  push_transaction( make_hbd_grant( *db, {}, 1, 5000, 8000, 10000 ), alice_private_key );

  HIVE_REQUIRE_THROW( push_transaction( make_transfer( "merchant", 1000 ), alice_private_key ), fc::exception );

  signed_transaction tx;
  tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
  tx.operations.push_back( make_transfer( "merchant", 5000 ) );
  tx.operations.push_back( make_transfer( "merchant", 4000 ) ); // fails after first operation; whole tx must roll back
  HIVE_REQUIRE_THROW( push_transaction( tx, bob_private_key ), fc::exception );

  const auto& grant = find_grant( *db, "alice", 1 );
  BOOST_REQUIRE_EQUAL( grant.lifetime_spent, HBD_asset( 0 ).to_asset() );
  BOOST_REQUIRE_EQUAL( grant.period_spent, HBD_asset( 0 ).to_asset() );
  BOOST_REQUIRE_EQUAL( db->get_account( "merchant" ).get_hbd_balance(), HBD_asset( 0 ) );
  BOOST_REQUIRE_EQUAL( db->get_account( "alice" ).get_hbd_balance(), HBD_asset( 100000 ) );
}

BOOST_AUTO_TEST_CASE( scoped_grants_are_isolated_across_transactions_in_one_block )
{
  ACTORS( DEFAULT_VESTING, (alice)(bob)(merchant) );
  vest( "alice", HIVE_asset( 100000 ) );
  issue_funds( "alice", HBD_asset( 100000 ) );
  push_transaction( make_hbd_grant( *db, {}, 1, 5000, 5000, 10000 ), alice_private_key );
  push_transaction( make_hbd_grant( *db, {}, 2, 5000, 5000, 10000 ), alice_private_key );
  generate_blocks( 1 );

  push_transaction( make_transfer( "merchant", 2000, 1 ), bob_private_key );
  push_transaction( make_transfer( "merchant", 3000, 1 ), bob_private_key );
  push_transaction( make_transfer( "merchant", 1000, 2 ), bob_private_key );
  generate_blocks( 1 );

  BOOST_REQUIRE_EQUAL( find_grant( *db, "alice", 1 ).lifetime_spent, HBD_asset( 5000 ).to_asset() );
  BOOST_REQUIRE_EQUAL( find_grant( *db, "alice", 1 ).period_spent, HBD_asset( 5000 ).to_asset() );
  BOOST_REQUIRE_EQUAL( find_grant( *db, "alice", 2 ).lifetime_spent, HBD_asset( 1000 ).to_asset() );
  BOOST_REQUIRE_EQUAL( find_grant( *db, "alice", 2 ).period_spent, HBD_asset( 1000 ).to_asset() );
  BOOST_REQUIRE_EQUAL( db->get_account( "merchant" ).get_hbd_balance(), HBD_asset( 6000 ) );
}

BOOST_AUTO_TEST_CASE( compressed_batch_transfer_requires_each_input_authority_and_conserves_funds )
{
  ACTORS( DEFAULT_VESTING, (alice)(bob)(carol)(merchant)(other) );
  issue_funds( "alice", HBD_asset( 10000 ) );
  issue_funds( "bob", HBD_asset( 10000 ) );
  auto batch = make_batch( { { "alice", 4000 }, { "bob", 6000 } },
    { { "merchant", 5000 }, { "other", 5000 } } );

  HIVE_REQUIRE_THROW( push_transaction( batch, alice_private_key ), fc::exception );
  BOOST_REQUIRE_EQUAL( db->get_account( "alice" ).get_hbd_balance(), HBD_asset( 10000 ) );
  BOOST_REQUIRE_EQUAL( db->get_account( "bob" ).get_hbd_balance(), HBD_asset( 10000 ) );

  auto insufficient = make_batch( { { "alice", 4000 }, { "bob", 11000 } },
    { { "merchant", 7500 }, { "other", 7500 } } );
  signed_transaction insufficient_tx;
  insufficient_tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
  insufficient_tx.operations.emplace_back( insufficient );
  HIVE_REQUIRE_THROW( push_transaction( insufficient_tx,
    std::vector<fc::ecc::private_key>{ alice_private_key, bob_private_key } ), fc::exception );
  BOOST_REQUIRE_EQUAL( db->get_account( "alice" ).get_hbd_balance(), HBD_asset( 10000 ) );
  BOOST_REQUIRE_EQUAL( db->get_account( "bob" ).get_hbd_balance(), HBD_asset( 10000 ) );

  signed_transaction batch_tx;
  batch_tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
  batch_tx.operations.emplace_back( batch );
  push_transaction( batch_tx, std::vector<fc::ecc::private_key>{ alice_private_key, bob_private_key } );
  BOOST_REQUIRE_EQUAL( db->get_account( "alice" ).get_hbd_balance(), HBD_asset( 6000 ) );
  BOOST_REQUIRE_EQUAL( db->get_account( "bob" ).get_hbd_balance(), HBD_asset( 4000 ) );
  BOOST_REQUIRE_EQUAL( db->get_account( "merchant" ).get_hbd_balance(), HBD_asset( 5000 ) );
  BOOST_REQUIRE_EQUAL( db->get_account( "other" ).get_hbd_balance(), HBD_asset( 5000 ) );
}

BOOST_AUTO_TEST_CASE( compressed_batch_transfer_rejects_imbalance_and_rolls_back_block )
{
  ACTORS( DEFAULT_VESTING, (alice)(bob)(merchant)(other) );
  issue_funds( "alice", HBD_asset( 10000 ) );
  issue_funds( "bob", HBD_asset( 10000 ) );
  generate_blocks( 1 ); // Confirm setup state before testing a block pop.
  auto unbalanced = make_batch( { { "alice", 4000 }, { "bob", 6000 } },
    { { "merchant", 9999 } } );
  if( !throws_consensus_error( [&] { unbalanced.validate(); } ) )
    BOOST_FAIL( "Unbalanced compressed batch operation passed validation" );

  auto batch = make_batch( { { "alice", 4000 }, { "bob", 6000 } },
    { { "merchant", 5000 }, { "other", 5000 } } );
  signed_transaction batch_tx;
  batch_tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
  batch_tx.operations.emplace_back( batch );
  push_transaction( batch_tx, std::vector<fc::ecc::private_key>{ alice_private_key, bob_private_key } );
  const auto produced_blocks = db_plugin->debug_generate_blocks( init_account_priv_key, 1, default_skip, 0, true );
  BOOST_REQUIRE( produced_blocks == 1 );
  const auto balance_before_pop = db->get_account( "merchant" ).get_hbd_balance();
  BOOST_REQUIRE( balance_before_pop.amount.value == HBD_asset( 5000 ).amount.value );
  db->pop_block();
  db->clear_pending(); // The popped transaction is re-queued by the fork database.
  BOOST_REQUIRE( db->get_account( "alice" ).get_hbd_balance().amount.value == HBD_asset( 10000 ).amount.value );
  BOOST_REQUIRE( db->get_account( "bob" ).get_hbd_balance().amount.value == HBD_asset( 10000 ).amount.value );
  BOOST_REQUIRE( db->get_account( "merchant" ).get_hbd_balance().amount.value == 0 );
}

BOOST_AUTO_TEST_CASE( compressed_batch_executes_three_inputs_into_two_outputs )
{
  ACTORS( DEFAULT_VESTING, (alice)(bob)(carol)(merchant)(other) );
  issue_funds( "alice", HBD_asset( 10000 ) );
  issue_funds( "bob", HBD_asset( 10000 ) );
  issue_funds( "carol", HBD_asset( 10000 ) );
  auto batch = make_batch( { { "alice", 10000 }, { "bob", 10000 }, { "carol", 10000 } },
    { { "merchant", 15000 }, { "other", 15000 } } );
  signed_transaction tx;
  tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
  tx.operations.emplace_back( batch );
  push_transaction( tx, std::vector<fc::ecc::private_key>{ alice_private_key, bob_private_key, carol_private_key } );
  BOOST_REQUIRE( db->get_account( "alice" ).get_hbd_balance().amount.value == 0 );
  BOOST_REQUIRE( db->get_account( "bob" ).get_hbd_balance().amount.value == 0 );
  BOOST_REQUIRE( db->get_account( "carol" ).get_hbd_balance().amount.value == 0 );
  BOOST_REQUIRE( db->get_account( "merchant" ).get_hbd_balance().amount.value == 15000 );
  BOOST_REQUIRE( db->get_account( "other" ).get_hbd_balance().amount.value == 15000 );
}

BOOST_AUTO_TEST_CASE( compressed_batch_transfer_bounds_and_canonical_order_are_validated )
{
  auto duplicate_inputs = make_batch( { { "alice", 1 }, { "alice", 1 } }, { { "merchant", 2 } } );
  HIVE_REQUIRE_THROW( duplicate_inputs.validate(), fc::exception );
  auto unsorted = make_batch( { { "bob", 1 }, { "alice", 1 } }, { { "merchant", 2 } } );
  HIVE_REQUIRE_THROW( unsorted.validate(), fc::exception );
  auto self_destination = make_batch( { { "alice", 1 } }, { { "alice", 1 } } );
  HIVE_REQUIRE_THROW( self_destination.validate(), fc::exception );
  auto zero = make_batch( { { "alice", 0 } }, { { "merchant", 0 } } );
  HIVE_REQUIRE_THROW( zero.validate(), fc::exception );
  auto wrong_symbol = make_batch( { { "alice", 1 } }, { { "merchant", 1 } } );
  wrong_symbol.symbol = VESTS_SYMBOL;
  HIVE_REQUIRE_THROW( wrong_symbol.validate(), fc::exception );
}

BOOST_AUTO_TEST_CASE( compressed_batch_can_spend_scoped_grant_with_canonical_recipient_attribution )
{
  ACTORS( DEFAULT_VESTING, (alice)(bob)(merchant)(other)(outside) );
  issue_funds( "alice", HBD_asset( 10000 ) );
  push_transaction( make_hbd_grant( *db, { "merchant", "other" }, 7, 5000, 8000, 10000 ), alice_private_key );
  generate_blocks( 1 );

  auto batch = make_batch( { { "alice", 5000, 7 } }, { { "merchant", 3000 }, { "other", 2000 } } );
  batch.actor = "bob";
  HIVE_REQUIRE_THROW( push_transaction( batch, alice_private_key ), fc::exception );
  push_transaction( batch, bob_private_key );
  generate_blocks( 1 );

  BOOST_REQUIRE_EQUAL( db->get_account( "alice" ).get_hbd_balance(), HBD_asset( 5000 ) );
  BOOST_REQUIRE_EQUAL( db->get_account( "merchant" ).get_hbd_balance(), HBD_asset( 3000 ) );
  BOOST_REQUIRE_EQUAL( db->get_account( "other" ).get_hbd_balance(), HBD_asset( 2000 ) );
  BOOST_REQUIRE_EQUAL( find_grant( *db, "alice", 7 ).lifetime_spent, HBD_asset( 5000 ).to_asset() );
  BOOST_REQUIRE_EQUAL( find_grant( *db, "alice", 7 ).period_spent, HBD_asset( 5000 ).to_asset() );

  signed_transaction atomic_batch;
  atomic_batch.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
  atomic_batch.operations.emplace_back( make_batch( { { "alice", 2000, 7 } }, { { "merchant", 2000 } } ) );
  atomic_batch.operations.back().get< compressed_batch_transfer_operation >().actor = "bob";
  atomic_batch.operations.emplace_back( make_batch( { { "alice", 2000, 7 } }, { { "other", 2000 } } ) );
  atomic_batch.operations.back().get< compressed_batch_transfer_operation >().actor = "bob";
  HIVE_REQUIRE_THROW( push_transaction( atomic_batch, bob_private_key ), fc::exception );
  BOOST_REQUIRE_EQUAL( find_grant( *db, "alice", 7 ).lifetime_spent, HBD_asset( 5000 ).to_asset() );
  BOOST_REQUIRE_EQUAL( db->get_account( "alice" ).get_hbd_balance(), HBD_asset( 5000 ) );
  BOOST_REQUIRE_EQUAL( db->get_account( "merchant" ).get_hbd_balance(), HBD_asset( 3000 ) );
  BOOST_REQUIRE_EQUAL( db->get_account( "other" ).get_hbd_balance(), HBD_asset( 2000 ) );

  // Alice is lexicographically before both destinations, so canonical
  // attribution of the full input reaches both outputs. The allowlist makes
  // that flow explicit and an omitted destination cannot receive pooled funds.
  auto disallowed = make_batch( { { "alice", 1000, 7 } }, { { "outside", 1000 } } );
  disallowed.actor = "bob";
  HIVE_REQUIRE_THROW( push_transaction( disallowed, bob_private_key ), fc::exception );
  BOOST_REQUIRE_EQUAL( find_grant( *db, "alice", 7 ).lifetime_spent, HBD_asset( 5000 ).to_asset() );

  auto wrong_asset = make_batch( { { "alice", 1000, 7 } }, { { "merchant", 1000 } } );
  wrong_asset.actor = "bob";
  wrong_asset.symbol = HIVE_SYMBOL;
  HIVE_REQUIRE_THROW( push_transaction( wrong_asset, bob_private_key ), fc::exception );
  BOOST_REQUIRE_EQUAL( find_grant( *db, "alice", 7 ).lifetime_spent, HBD_asset( 5000 ).to_asset() );

  revoke_scoped_authority_operation revoke;
  revoke.grantor = "alice";
  revoke.grant_id = 7;
  push_transaction( revoke, alice_private_key );
  generate_blocks( 1 );
  HIVE_REQUIRE_THROW( push_transaction( batch, bob_private_key ), fc::exception );
  BOOST_REQUIRE_EQUAL( find_grant( *db, "alice", 7 ).lifetime_spent, HBD_asset( 5000 ).to_asset() );
}

BOOST_AUTO_TEST_CASE( compressed_batch_keeps_multiple_scoped_grant_counters_isolated )
{
  ACTORS( DEFAULT_VESTING, (alice)(bob)(carol)(merchant) );
  issue_funds( "alice", HBD_asset( 10000 ) );
  issue_funds( "carol", HBD_asset( 10000 ) );
  auto alice_grant = make_hbd_grant( *db, { "merchant" }, 1, 5000, 8000, 10000 );
  auto carol_grant = make_hbd_grant( *db, { "merchant" }, 2, 5000, 8000, 10000 );
  carol_grant.grantor = "carol";
  push_transaction( alice_grant, alice_private_key );
  push_transaction( carol_grant, carol_private_key );
  auto batch = make_batch( { { "alice", 3000, 1 }, { "carol", 4000, 2 } }, { { "merchant", 7000 } } );
  batch.actor = "bob";
  push_transaction( batch, bob_private_key );
  BOOST_REQUIRE( find_grant( *db, "alice", 1 ).lifetime_spent.amount.value == 3000 );
  BOOST_REQUIRE( find_grant( *db, "carol", 2 ).lifetime_spent.amount.value == 4000 );
  BOOST_REQUIRE( db->get_account( "merchant" ).get_hbd_balance().amount.value == 7000 );
}

BOOST_AUTO_TEST_CASE( compressed_batch_wire_size_is_smaller_than_equivalent_standard_transfers )
{
  ACTORS( DEFAULT_VESTING, (alice) );
  compressed_batch_transfer_operation batch;
  batch.symbol = HBD_SYMBOL;
  batch.inputs.push_back( { "alice", 100 } );
  signed_transaction standard;
  standard.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
  for( uint32_t i = 0; i < 100; ++i )
  {
    const auto suffix = std::to_string( 1000 + i );
    const account_name_type recipient = "user" + suffix;
    batch.outputs.push_back( { recipient, 1 } );
    transfer_operation transfer;
    transfer.from = "alice";
    transfer.to = recipient;
    transfer.amount = HBD_asset( 1 ).to_asset();
    transfer.memo = "batch";
    standard.operations.emplace_back( transfer );
  }
  batch.inputs.front().amount = 100;
  batch.validate();
  signed_transaction compressed;
  compressed.set_expiration( standard.expiration );
  compressed.operations.emplace_back( batch );
  const auto pack = serialization_mode_controller::get_current_pack();
  const auto standard_signed = build_transaction( standard,
    std::vector<fc::ecc::private_key>{ alice_private_key }, pack );
  const auto compressed_signed = build_transaction( compressed,
    std::vector<fc::ecc::private_key>{ alice_private_key }, pack );
  const auto standard_size = fc::raw::pack_to_vector( standard_signed->get_transaction() ).size();
  const auto compressed_size = fc::raw::pack_to_vector( compressed_signed->get_transaction() ).size();
  BOOST_TEST_MESSAGE( "100-output standard transaction bytes=" << standard_size
    << ", compressed batch bytes=" << compressed_size );
  BOOST_REQUIRE( compressed_size < standard_size );
}

BOOST_AUTO_TEST_CASE( compressed_batch_operation_keeps_legacy_tags_and_has_correct_authority_classification )
{
#ifndef HIVE_PROTOCOL_SKIP_VOPS
  BOOST_REQUIRE_EQUAL( operation::tag< transfer_operation >::value, 2 );
  BOOST_REQUIRE_EQUAL( operation::tag< fill_convert_request_operation >::value, 50 );
  BOOST_REQUIRE_EQUAL( operation::tag< declined_voting_rights_operation >::value, 92 );
  BOOST_REQUIRE_EQUAL( operation::tag< create_scoped_authority_operation >::value, 93 );
  BOOST_REQUIRE_EQUAL( operation::tag< scoped_transfer_operation >::value, 95 );
  BOOST_REQUIRE_EQUAL( operation::tag< compressed_batch_transfer_operation >::value, 96 );
#else
  BOOST_REQUIRE_EQUAL( operation::tag< transfer_operation >::value, 2 );
  BOOST_REQUIRE_EQUAL( operation::tag< create_scoped_authority_operation >::value, 50 );
  BOOST_REQUIRE_EQUAL( operation::tag< scoped_transfer_operation >::value, 52 );
  BOOST_REQUIRE_EQUAL( operation::tag< compressed_batch_transfer_operation >::value, 53 );
#endif

  auto direct = make_batch( { { "alice", 10 }, { "bob", 20 } }, { { "merchant", 30 } } );
  flat_set< account_name_type > required;
  direct.get_required_active_authorities( required );
  BOOST_REQUIRE( required.count( "alice" ) == 1 );
  BOOST_REQUIRE( required.count( "bob" ) == 1 );
  operation packed_direct = direct;
  BOOST_REQUIRE( !is_virtual_operation( packed_direct ) );

  direct.inputs[0].grant_id = 7;
  direct.actor = "carol";
  required.clear();
  direct.get_required_active_authorities( required );
  BOOST_REQUIRE( required.count( "alice" ) == 0 );
  BOOST_REQUIRE( required.count( "bob" ) == 1 );
  BOOST_REQUIRE( required.count( "carol" ) == 1 );

  packed_direct = direct;
  flat_set< account_name_type > impacted;
  hive::app::operation_get_impacted_accounts( packed_direct, impacted );
  BOOST_REQUIRE( impacted.count( "alice" ) == 1 );
  BOOST_REQUIRE( impacted.count( "bob" ) == 1 );
  BOOST_REQUIRE( impacted.count( "merchant" ) == 1 );
  BOOST_REQUIRE( impacted.count( "carol" ) == 1 );
  const auto balance_impacts = hive::app::operation_get_impacted_balances( packed_direct, true );
  BOOST_REQUIRE_EQUAL( balance_impacts.size(), 3u );
}

BOOST_AUTO_TEST_CASE( compressed_batch_100_by_100_bound_is_validated )
{
  compressed_batch_transfer_operation batch;
  batch.symbol = HBD_SYMBOL;
  auto account_for = []( const char* prefix, uint32_t i )
  {
    std::string suffix = std::to_string( i );
    while( suffix.size() < 3 ) suffix.insert( suffix.begin(), '0' );
    return account_name_type( std::string( prefix ) + suffix );
  };
  for( uint32_t i = 0; i < HIVE_MAX_COMPRESSED_BATCH_TRANSFER_INPUTS; ++i )
    batch.inputs.push_back( { account_for( "src", i ), 1 } );
  for( uint32_t i = 0; i < HIVE_MAX_COMPRESSED_BATCH_TRANSFER_OUTPUTS; ++i )
    batch.outputs.push_back( { account_for( "dst", i ), 1 } );
  batch.validate();
  BOOST_REQUIRE( !fc::raw::pack_to_vector( operation( batch ) ).empty() );

  batch.inputs.push_back( { account_for( "src", 100 ), 1 } );
  batch.outputs.back().amount = 2;
  if( !throws_consensus_error( [&] { batch.validate(); } ) )
    BOOST_FAIL( "Compressed batch accepted 101 inputs" );
}

BOOST_AUTO_TEST_CASE( scoped_transfer_rejects_forged_grant_wrong_asset_and_authority_changes )
{
  ACTORS( DEFAULT_VESTING, (alice)(bob)(merchant) );
  issue_funds( "alice", HBD_asset( 100000 ) );
  push_transaction( make_hbd_grant( *db ), alice_private_key );

  auto wrong_asset = make_transfer( "merchant", 1000 );
  wrong_asset.amount = HIVE_asset( 1000 ).to_asset();
  HIVE_REQUIRE_THROW( push_transaction( wrong_asset, bob_private_key ), fc::exception );
  HIVE_REQUIRE_THROW( push_transaction( make_transfer( "merchant", 1000, 999 ), bob_private_key ), fc::exception );

  account_update_operation update;
  update.account = "alice";
  update.active = authority( 1, bob_private_key.get_public_key(), 1 );
  update.memo_key = bob_private_key.get_public_key();
  HIVE_REQUIRE_THROW( push_transaction( update, bob_private_key ), fc::exception );

  BOOST_REQUIRE_EQUAL( find_grant( *db, "alice", 1 ).lifetime_spent, HBD_asset( 0 ).to_asset() );
  BOOST_REQUIRE_EQUAL( db->get_account( "alice" ).get_hbd_balance(), HBD_asset( 100000 ) );
  BOOST_REQUIRE_EQUAL( db->get_account( "merchant" ).get_hbd_balance(), HBD_asset( 0 ) );
}

BOOST_AUTO_TEST_CASE( legacy_transfer_and_historical_operation_tags_are_unchanged )
{
  ACTORS( DEFAULT_VESTING, (alice)(bob) );
  issue_funds( "alice", HBD_asset( 1000 ) );
  transfer_operation legacy;
  legacy.from = "alice";
  legacy.to = "bob";
  legacy.amount = HBD_asset( 100 ).to_asset();
  push_transaction( legacy, alice_private_key );
  BOOST_REQUIRE_EQUAL( db->get_account( "bob" ).get_hbd_balance(), HBD_asset( 100 ) );

  BOOST_REQUIRE_EQUAL( operation::tag< transfer_operation >::value, 2 );
  BOOST_REQUIRE_EQUAL( operation::tag< recurrent_transfer_operation >::value, 49 );
  BOOST_REQUIRE_EQUAL( operation::tag< fill_convert_request_operation >::value, 50 );
  BOOST_REQUIRE_EQUAL( operation::tag< declined_voting_rights_operation >::value, 92 );
  BOOST_REQUIRE_EQUAL( operation::tag< create_scoped_authority_operation >::value, 93 );
  BOOST_REQUIRE_EQUAL( operation::tag< revoke_scoped_authority_operation >::value, 94 );
  BOOST_REQUIRE_EQUAL( operation::tag< scoped_transfer_operation >::value, 95 );
  BOOST_REQUIRE( !is_virtual_operation( operation( create_scoped_authority_operation() ) ) );
  BOOST_REQUIRE( !is_virtual_operation( operation( revoke_scoped_authority_operation() ) ) );
  BOOST_REQUIRE( !is_virtual_operation( operation( scoped_transfer_operation() ) ) );
}

BOOST_AUTO_TEST_SUITE_END()
#endif
