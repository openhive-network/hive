#ifdef IS_TEST_NET
#include <boost/test/unit_test.hpp>

#include <hive/chain/detail/state/scoped_authority_object_multiindex.hpp>
#include <hive/protocol/hardfork.hpp>

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
  if( !throws_consensus_error( [&] { push_transaction( make_transfer( "merchant", 2 ), bob_private_key ); } ) )
    BOOST_FAIL( "Transfer using revoked grant was accepted" );
  const long long revoked_lifetime_after = static_cast< long long >( find_grant( *db, "alice", 1 ).lifetime_spent.amount.value );
  if( revoked_lifetime_after != revoked_lifetime_spent )
    BOOST_FAIL( "Rejected revoked transfer changed lifetime spending from " + std::to_string( revoked_lifetime_spent ) +
      " to " + std::to_string( revoked_lifetime_after ) );

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
