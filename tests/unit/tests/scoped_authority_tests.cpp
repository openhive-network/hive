#ifdef IS_TEST_NET
#include <boost/test/unit_test.hpp>

#include <hive/chain/detail/state/scoped_authority_object_multiindex.hpp>
#include <hive/chain/rc/resource_count.hpp>
#include <hive/chain/rc/rc_objects.hpp>
#include <hive/chain/rc/rc_objects_multiindex.hpp>
#include <hive/chain/rc/rc_utility.hpp>
#include <hive/chain/rc/resource_user.hpp>
#include <hive/protocol/hardfork.hpp>
#include <hive/protocol/forward_impacted.hpp>

#include "../db_fixture/clean_database_fixture.hpp"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <sys/resource.h>

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

const scoped_authority_grantor_metadata_object& find_grant_metadata( database& db, const account_name_type& grantor )
{
  return db.get< scoped_authority_grantor_metadata_object, by_scoped_grantor >( grantor );
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

BOOST_AUTO_TEST_CASE( scoped_grant_cap_tracks_live_records_and_ids_never_repeat )
{
  ACTORS( DEFAULT_VESTING, (alice)(bob) );
  issue_funds( "alice", HIVE_asset( 100000000 ) );
  vest( "alice", HIVE_asset( 100000000 ) );
  resource_count_type first_create_usage;
  resource_count_type full_set_create_usage;
  for( uint32_t id = 1; id <= HIVE_MAX_SCOPED_AUTHORITIES_PER_GRANTOR; ++id )
  {
    auto grant = make_hbd_grant( *db, {}, id, 100, 1000, 10000, 60, 3600 );
    if( id > 50 )
    {
      grant.max_per_transfer = HIVE_asset( 100 ).to_asset();
      grant.max_per_period = HIVE_asset( 1000 ).to_asset();
      grant.max_lifetime = HIVE_asset( 10000 ).to_asset();
    }
    const auto tx = push_transaction( grant, alice_private_key );
    resource_count_type usage;
    count_resources( tx->get_transaction(), tx->get_transaction_size(), usage, db->head_block_time(), *db );
    if( id == 1 ) first_create_usage = usage;
    if( id == HIVE_MAX_SCOPED_AUTHORITIES_PER_GRANTOR ) full_set_create_usage = usage;
    generate_blocks( 1 );
  }
  BOOST_REQUIRE_GT( full_set_create_usage[resource_execution_time], first_create_usage[resource_execution_time] );
  BOOST_REQUIRE_EQUAL( db->count< scoped_authority_object >(), HIVE_MAX_SCOPED_AUTHORITIES_PER_GRANTOR );
  BOOST_REQUIRE_EQUAL( find_grant_metadata( *db, "alice" ).last_grant_id, 100u );

  issue_funds( "alice", HIVE_asset( 1 ) );
  scoped_transfer_operation spend;
  spend.grantor = "alice";
  spend.actor = "bob";
  spend.to = "bob";
  spend.grant_id = 100;
  spend.amount = HIVE_asset( 100 ).to_asset();
  spend.memo = "lifecycle counter preservation";
  push_transaction( spend, bob_private_key );
  BOOST_REQUIRE_EQUAL( find_grant( *db, "alice", 100 ).lifetime_spent, HIVE_asset( 100 ).to_asset() );

  HIVE_REQUIRE_THROW( push_transaction(
    make_hbd_grant( *db, {}, 101, 100, 1000, 10000, 60, 3600 ), alice_private_key ), fc::exception );

  signed_transaction revoke_tx;
  revoke_tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
  for( uint32_t id = 1; id <= 50; ++id )
  {
    revoke_scoped_authority_operation revoke;
    revoke.grantor = "alice";
    revoke.grant_id = id;
    revoke_tx.operations.emplace_back( revoke );
  }
  push_transaction( revoke_tx, alice_private_key );
  generate_blocks( 1 );

  signed_transaction replace_tx;
  replace_tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
  for( uint32_t id = 101; id <= 150; ++id )
    replace_tx.operations.emplace_back( make_hbd_grant( *db, {}, id, 100, 1000, 10000, 60, 3600 ) );
  push_transaction( replace_tx, alice_private_key );
  generate_blocks( 1 );

  BOOST_REQUIRE_EQUAL( db->count< scoped_authority_object >(), HIVE_MAX_SCOPED_AUTHORITIES_PER_GRANTOR );
  BOOST_REQUIRE_EQUAL( find_grant_metadata( *db, "alice" ).last_grant_id, 150u );
  BOOST_REQUIRE(( db->find< scoped_authority_object, by_grantor_id >(
    boost::make_tuple( "alice", 1u ) ) == nullptr ));
  BOOST_REQUIRE(( db->find< scoped_authority_object, by_grantor_id >(
    boost::make_tuple( "alice", 100u ) ) != nullptr ));
  BOOST_REQUIRE_EQUAL( find_grant( *db, "alice", 100 ).lifetime_spent, HIVE_asset( 100 ).to_asset() );
  HIVE_REQUIRE_THROW( push_transaction(
    make_hbd_grant( *db, {}, 1, 100, 1000, 10000, 60, 3600 ), alice_private_key ), fc::exception );
}

BOOST_AUTO_TEST_CASE( scoped_expired_grants_are_pruned_and_cleanup_rolls_back )
{
  ACTORS( DEFAULT_VESTING, (alice)(bob) );
  issue_funds( "alice", HIVE_asset( 100000000 ) );
  vest( "alice", HIVE_asset( 100000000 ) );
  for( uint32_t id = 1; id <= HIVE_MAX_SCOPED_AUTHORITIES_PER_GRANTOR; ++id )
  {
    auto grant = make_hbd_grant( *db, {}, id, 100, 1000, 10000, 60, 600 );
    if( id > 50 )
    {
      grant.max_per_transfer = HIVE_asset( 100 ).to_asset();
      grant.max_per_period = HIVE_asset( 1000 ).to_asset();
      grant.max_lifetime = HIVE_asset( 10000 ).to_asset();
    }
    push_transaction( grant, alice_private_key );
    generate_blocks( 1 );
  }

  const auto expires_at = find_grant( *db, "alice", HIVE_MAX_SCOPED_AUTHORITIES_PER_GRANTOR ).expires_at;
  const auto blocks_to_expiry = ( expires_at - db->head_block_time() ).to_seconds() / HIVE_BLOCK_INTERVAL;
  for( int32_t i = 0; i < blocks_to_expiry; ++i )
    generate_block_from_pending();
  BOOST_REQUIRE_EQUAL( db->head_block_time(), expires_at );

  const auto replacement = make_hbd_grant( *db, {}, 101, 100, 1000, 10000, 60, 60 );
  signed_transaction failing_tx;
  failing_tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
  failing_tx.operations.emplace_back( replacement );
  revoke_scoped_authority_operation missing_revoke;
  missing_revoke.grantor = "alice";
  missing_revoke.grant_id = 999;
  failing_tx.operations.emplace_back( missing_revoke );
  HIVE_REQUIRE_THROW( push_transaction( failing_tx, alice_private_key ), fc::exception );
  BOOST_REQUIRE_EQUAL( db->count< scoped_authority_object >(), HIVE_MAX_SCOPED_AUTHORITIES_PER_GRANTOR );
  BOOST_REQUIRE_EQUAL( find_grant_metadata( *db, "alice" ).last_grant_id, 100u );
  BOOST_REQUIRE(( db->find< scoped_authority_object, by_grantor_id >(
    boost::make_tuple( "alice", 1u ) ) != nullptr ));

  const auto cleanup_tx = push_transaction( replacement, alice_private_key );
  BOOST_REQUIRE_GT( cleanup_tx->get_rc_cost(), 0 );
  BOOST_REQUIRE_EQUAL( db->count< scoped_authority_object >(), 1u );
  BOOST_REQUIRE(( db->find< scoped_authority_object, by_grantor_id >(
    boost::make_tuple( "alice", 1u ) ) == nullptr ));
  BOOST_REQUIRE(( db->find< scoped_authority_object, by_scoped_actor >(
    boost::make_tuple( "bob", "alice", 1u ) ) == nullptr ));
  HIVE_REQUIRE_THROW( push_transaction( make_transfer( "bob", 1, 1 ), bob_private_key ), fc::exception );
  BOOST_REQUIRE_EQUAL( find_grant( *db, "alice", 101 ).lifetime_spent, HBD_asset( 0 ).to_asset() );
  generate_blocks( 1 );
  db->pop_block();
  BOOST_REQUIRE_EQUAL( db->count< scoped_authority_object >(), HIVE_MAX_SCOPED_AUTHORITIES_PER_GRANTOR );
  BOOST_REQUIRE(( db->find< scoped_authority_object, by_grantor_id >(
    boost::make_tuple( "alice", 1u ) ) != nullptr ));
  BOOST_REQUIRE_EQUAL( find_grant_metadata( *db, "alice" ).last_grant_id, 100u );
  BOOST_REQUIRE(( db->find< scoped_authority_object, by_grantor_id >(
    boost::make_tuple( "alice", 101u ) ) == nullptr ));

  push_transaction( replacement, alice_private_key );
  generate_blocks( 1 );
  BOOST_REQUIRE_EQUAL( db->count< scoped_authority_object >(), 1u );
  BOOST_REQUIRE_EQUAL( find_grant_metadata( *db, "alice" ).last_grant_id, 101u );
  BOOST_REQUIRE(( db->find< scoped_authority_object, by_grantor_id >(
    boost::make_tuple( "alice", 1u ) ) == nullptr ));
}

BOOST_AUTO_TEST_CASE( scoped_grant_id_maximum_is_accepted_once_without_wraparound )
{
  ACTORS( DEFAULT_VESTING, (alice)(bob) );
  const auto max_id = std::numeric_limits<uint32_t>::max();
  push_transaction( make_hbd_grant( *db, {}, max_id, 100, 1000, 10000, 60, 3600 ), alice_private_key );
  BOOST_REQUIRE_EQUAL( find_grant_metadata( *db, "alice" ).last_grant_id, max_id );
  HIVE_REQUIRE_THROW( push_transaction(
    make_hbd_grant( *db, {}, 1, 100, 1000, 10000, 60, 3600 ), alice_private_key ), fc::exception );
  BOOST_REQUIRE_EQUAL( db->count< scoped_authority_object >(), 1u );
}

BOOST_AUTO_TEST_CASE( scoped_grant_metadata_is_created_with_first_grant )
{
  ACTORS( DEFAULT_VESTING, (alice)(bob) );
  issue_funds( "alice", HIVE_asset( 100000000 ) );
  vest( "alice", HIVE_asset( 100000000 ) );
  push_transaction( make_hbd_grant( *db, {}, 7, 100, 1000, 10000, 60, 3600 ), alice_private_key );
  generate_blocks( 1 );
  BOOST_REQUIRE_EQUAL( find_grant_metadata( *db, "alice" ).last_grant_id, 7u );
  const auto& grants = db->get_index< scoped_authority_index, by_grantor_id >();
  BOOST_REQUIRE_EQUAL( grants.size(), 1u );
  BOOST_REQUIRE_EQUAL( grants.begin()->grantor, "alice" );
  BOOST_REQUIRE( grants.begin()->grant_id == uint32_t( 7 ) );
  auto grant = make_hbd_grant( *db, {}, 8, 100, 1000, 10000, 60, 3600 );
  push_transaction( grant, alice_private_key );
  generate_blocks( 1 );
  BOOST_REQUIRE_EQUAL( find_grant_metadata( *db, "alice" ).last_grant_id, 8u );
  revoke_scoped_authority_operation revoke;
  revoke.grantor = "alice";
  revoke.grant_id = 7;
  push_transaction( revoke, alice_private_key );
  generate_blocks( 1 );
  auto replacement = make_hbd_grant( *db, {}, 9, 100, 1000, 10000, 60, 3600 );
  push_transaction( replacement, alice_private_key );
  generate_blocks( 1 );
  BOOST_REQUIRE_EQUAL( find_grant_metadata( *db, "alice" ).last_grant_id, 9u );
  BOOST_REQUIRE(( db->find< scoped_authority_object, by_grantor_id >(
    boost::make_tuple( "alice", 7u ) ) == nullptr ));
  HIVE_REQUIRE_THROW( push_transaction(
    make_hbd_grant( *db, {}, 7, 100, 1000, 10000, 60, 3600 ), alice_private_key ), fc::exception );
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

  // Advance through ordinary empty blocks so expiration uses block time.
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
  auto unsorted_outputs = make_batch( { { "alice", 2 } }, { { "outside", 1 }, { "merchant", 1 } } );
  HIVE_REQUIRE_THROW( unsorted_outputs.validate(), fc::exception );
  auto duplicate_grant_source = make_batch( { { "alice", 1, 7 }, { "alice", 1, 8 } }, { { "merchant", 2 } } );
  HIVE_REQUIRE_THROW( duplicate_grant_source.validate(), fc::exception );
  auto self_destination = make_batch( { { "alice", 1 } }, { { "alice", 1 } } );
  HIVE_REQUIRE_THROW( self_destination.validate(), fc::exception );
  auto zero = make_batch( { { "alice", 0 } }, { { "merchant", 0 } } );
  HIVE_REQUIRE_THROW( zero.validate(), fc::exception );
  auto overflow = make_batch( { { "alice", std::numeric_limits<int64_t>::max() } },
    { { "merchant", std::numeric_limits<int64_t>::max() } } );
  HIVE_REQUIRE_THROW( overflow.validate(), fc::exception );
  auto sum_overflow = make_batch( { { "alice", HIVE_MAX_SHARE_SUPPLY }, { "bob", 1 } },
    { { "merchant", HIVE_MAX_SHARE_SUPPLY }, { "other", 1 } } );
  HIVE_REQUIRE_THROW( sum_overflow.validate(), fc::exception );
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

BOOST_AUTO_TEST_CASE( compressed_batch_attributes_distinct_allowlists_without_cross_routing )
{
  ACTORS( DEFAULT_VESTING, (alice)(bob)(carol)(merchant)(outside) );
  issue_funds( "alice", HBD_asset( 10000 ) );
  issue_funds( "carol", HBD_asset( 10000 ) );

  auto alice_grant = make_hbd_grant( *db, { "merchant" }, 21, 5000, 10000, 10000 );
  auto carol_grant = make_hbd_grant( *db, { "outside" }, 22, 5000, 10000, 10000 );
  carol_grant.grantor = "carol";
  push_transaction( alice_grant, alice_private_key );
  push_transaction( carol_grant, carol_private_key );

  auto permitted = make_batch( { { "alice", 4000, 21 }, { "carol", 4000, 22 } },
    { { "merchant", 4000 }, { "outside", 4000 } } );
  permitted.actor = "bob";
  push_transaction( permitted, bob_private_key );
  generate_blocks( 1 );
  BOOST_REQUIRE_EQUAL( db->get_account( "merchant" ).get_hbd_balance(), HBD_asset( 4000 ) );
  BOOST_REQUIRE_EQUAL( db->get_account( "outside" ).get_hbd_balance(), HBD_asset( 4000 ) );
  BOOST_REQUIRE_EQUAL( find_grant( *db, "alice", 21 ).lifetime_spent, HBD_asset( 4000 ).to_asset() );
  BOOST_REQUIRE_EQUAL( find_grant( *db, "carol", 22 ).lifetime_spent, HBD_asset( 4000 ).to_asset() );

  // Shrinking Alice's contribution makes Carol's deterministic allocation
  // cross the merchant output boundary, which her outside-only grant forbids.
  auto cross_routed = make_batch( { { "alice", 3000, 21 }, { "carol", 5000, 22 } },
    { { "merchant", 4000 }, { "outside", 4000 } } );
  cross_routed.actor = "bob";
  HIVE_REQUIRE_THROW( push_transaction( cross_routed, bob_private_key ), fc::exception );
  BOOST_REQUIRE_EQUAL( find_grant( *db, "alice", 21 ).lifetime_spent, HBD_asset( 4000 ).to_asset() );
  BOOST_REQUIRE_EQUAL( find_grant( *db, "carol", 22 ).lifetime_spent, HBD_asset( 4000 ).to_asset() );
  BOOST_REQUIRE_EQUAL( db->get_account( "alice" ).get_hbd_balance(), HBD_asset( 6000 ) );
  BOOST_REQUIRE_EQUAL( db->get_account( "carol" ).get_hbd_balance(), HBD_asset( 6000 ) );
}

BOOST_AUTO_TEST_CASE( compressed_batch_allows_active_funds_beside_a_restricted_input )
{
  ACTORS( HIVE_asset( 10000 ), (alice)(bob)(carol)(merchant)(outside) );
  issue_funds( "alice", HBD_asset( 10000 ) );
  issue_funds( "bob", HBD_asset( 10000 ) );
  auto grant = make_hbd_grant( *db, { "merchant" }, 31, 5000, 5000, 5000 );
  grant.actor = "carol";
  push_transaction( grant, alice_private_key );

  auto mixed = make_batch( { { "alice", 3000, 31 }, { "bob", 5000, 0 } },
    { { "merchant", 3000 }, { "outside", 5000 } } );
  mixed.actor = "carol";
  signed_transaction mixed_tx;
  mixed_tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
  mixed_tx.operations.emplace_back( mixed );
  try
  {
    push_transaction( mixed_tx, std::vector<fc::ecc::private_key>{ carol_private_key, bob_private_key } );
    generate_blocks( 1 );
  }
  catch( const fc::exception& e )
  {
    BOOST_FAIL( "Mixed active/scoped batch failed: " << e.to_detail_string() );
  }
  BOOST_REQUIRE_EQUAL( find_grant( *db, "alice", 31 ).lifetime_spent, HBD_asset( 3000 ).to_asset() );
  BOOST_REQUIRE_EQUAL( db->get_account( "merchant" ).get_hbd_balance(), HBD_asset( 3000 ) );
  BOOST_REQUIRE_EQUAL( db->get_account( "outside" ).get_hbd_balance(), HBD_asset( 5000 ) );

  auto laundering = make_batch( { { "alice", 2000, 31 } },
    { { "merchant", 1000 }, { "outside", 1000 } } );
  laundering.actor = "carol";
  HIVE_REQUIRE_THROW( push_transaction( laundering, carol_private_key ), fc::exception );
  BOOST_REQUIRE_EQUAL( find_grant( *db, "alice", 31 ).lifetime_spent, HBD_asset( 3000 ).to_asset() );
}

BOOST_AUTO_TEST_CASE( compressed_batches_keep_two_grants_from_one_account_independent )
{
  ACTORS( HIVE_asset( 10000 ), (alice)(bob)(merchant)(other) );
  issue_funds( "alice", HBD_asset( 10000 ) );
  push_transaction( make_hbd_grant( *db, { "merchant" }, 41, 2000, 2000, 2000 ), alice_private_key );
  push_transaction( make_hbd_grant( *db, { "other" }, 42, 2000, 2000, 2000 ), alice_private_key );

  signed_transaction tx;
  tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
  auto first = make_batch( { { "alice", 1000, 41 } }, { { "merchant", 1000 } } );
  first.actor = "bob";
  auto second = make_batch( { { "alice", 1000, 42 } }, { { "other", 1000 } } );
  second.actor = "bob";
  tx.operations.emplace_back( first );
  tx.operations.emplace_back( second );
  push_transaction( tx, bob_private_key );
  generate_blocks( 1 );

  BOOST_REQUIRE_EQUAL( find_grant( *db, "alice", 41 ).lifetime_spent, HBD_asset( 1000 ).to_asset() );
  BOOST_REQUIRE_EQUAL( find_grant( *db, "alice", 42 ).lifetime_spent, HBD_asset( 1000 ).to_asset() );
  BOOST_REQUIRE_EQUAL( db->get_account( "merchant" ).get_hbd_balance(), HBD_asset( 1000 ) );
  BOOST_REQUIRE_EQUAL( db->get_account( "other" ).get_hbd_balance(), HBD_asset( 1000 ) );
}

BOOST_AUTO_TEST_CASE( scoped_batch_resource_cost_is_paid_by_actor )
{
  compressed_batch_transfer_operation active_batch;
  active_batch.symbol = HBD_SYMBOL;
  active_batch.inputs = { { "alice", 1000, 0 }, { "bob", 1000, 0 } };
  active_batch.outputs = { { "merchant", 2000 } };
  signed_transaction active_tx;
  active_tx.operations.emplace_back( active_batch );
  BOOST_REQUIRE_EQUAL( get_resource_user( active_tx ), account_name_type( "alice" ) );

  compressed_batch_transfer_operation scoped_batch;
  scoped_batch.actor = "hivedebit";
  scoped_batch.symbol = HBD_SYMBOL;
  scoped_batch.inputs = { { "alice", 1000, 7 }, { "bob", 1000, 0 } };
  scoped_batch.outputs = { { "merchant", 2000 } };
  signed_transaction scoped_tx;
  scoped_tx.operations.emplace_back( scoped_batch );
  BOOST_REQUIRE_EQUAL( get_resource_user( scoped_tx ), account_name_type( "hivedebit" ) );
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
  BOOST_REQUIRE_EQUAL( operation::tag< revoke_scoped_authority_operation >::value, 94 );
  BOOST_REQUIRE_EQUAL( operation::tag< scoped_transfer_operation >::value, 95 );
  BOOST_REQUIRE_EQUAL( operation::tag< compressed_batch_transfer_operation >::value, 96 );
#else
  BOOST_REQUIRE_EQUAL( operation::tag< transfer_operation >::value, 2 );
  BOOST_REQUIRE_EQUAL( operation::tag< create_scoped_authority_operation >::value, 50 );
  BOOST_REQUIRE_EQUAL( operation::tag< revoke_scoped_authority_operation >::value, 51 );
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

BOOST_AUTO_TEST_CASE( legacy_transfer_operation_binary_fixture_is_unchanged )
{
  transfer_operation transfer;
  transfer.from = "alice";
  transfer.to = "bob";
  transfer.amount = HBD_asset( 1234 ).to_asset();
  transfer.memo = "legacy";
  const auto packed = fc::raw::pack_to_vector( operation( transfer ) );
  BOOST_REQUIRE_EQUAL( packed.front(), 2 );
  BOOST_REQUIRE_EQUAL( fc::to_hex( packed.data(), packed.size() ),
    "0205616c69636503626f62d2040000000000000354424400000000066c6567616379" );
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

BOOST_AUTO_TEST_CASE( compressed_batch_rc_benchmark, *boost::unit_test::disabled() )
{
  // Opt-in benchmark. Run with --run_test=scoped_authority_tests/compressed_batch_rc_benchmark
  // and HIVE_RC_BENCHMARK_CSV=...; excluded from normal consensus-suite execution.
  const char* csv_path = std::getenv( "HIVE_RC_BENCHMARK_CSV" );
  if( csv_path == nullptr )
  {
    BOOST_TEST_MESSAGE( "Set HIVE_RC_BENCHMARK_CSV to run the opt-in RC benchmark" );
    return;
  }

  constexpr uint32_t max_accounts = 100;
  const fc::ecc::private_key actor_key = generate_private_key( "batch_rc_benchmark_actor" );
  std::vector< fc::ecc::private_key > source_keys;
  std::vector< account_name_type > source_names;
  std::vector< account_name_type > destination_names;
  source_keys.reserve( max_accounts );
  source_names.reserve( max_accounts );
  destination_names.reserve( max_accounts );

  for( uint32_t i = 0; i < max_accounts; ++i )
  {
    std::string suffix = std::to_string( i );
    while( suffix.size() < 3 ) suffix.insert( suffix.begin(), '0' );
    const std::string source = "benchsrc" + suffix;
    const std::string destination = "benchdst" + suffix;
    source_keys.push_back( generate_private_key( source ) );
    // Grant creation has a substantial persistent-state RC cost. Give every
    // independent benchmark actor enough development vesting to fund it.
    account_create( source, source_keys.back().get_public_key(), source_keys.back().get_public_key(), HIVE_asset( 1000000 ) );
    account_create( destination, init_account_pub_key, init_account_pub_key, NO_VESTING );
    source_names.emplace_back( source );
    destination_names.emplace_back( destination );
  }
  account_create( "benchdebit", actor_key.get_public_key(), actor_key.get_public_key(), HIVE_asset( 100000 ) );
  generate_blocks( 1 ); // Commit account-creation work and reset the debug-update transaction quota.

  std::ofstream csv( csv_path );
  BOOST_REQUIRE_MESSAGE( csv.good(), "Unable to open RC benchmark CSV output" );
  csv << "shape,mode,inputs,outputs,transactions,serialized_bytes,signatures,history_bytes,new_accounts,market_bytes,state_bytes,execution_units,rc_charged,same_pool_rc_cost,grant_setup_rc,execution_us,max_rss_kb,balance_writes\n";

  struct shape { uint32_t inputs; uint32_t outputs; const char* name; };
  const std::vector< shape > shapes = {
    { 1, 3, "1x3" }, { 1, 10, "1x10" }, { 1, 100, "1x100" },
    { 3, 2, "3x2" }, { 10, 10, "10x10" }, { 100, 100, "100x100" }
  };

  uint32_t grant_id = 10;
  for( const auto& s : shapes )
  {
    BOOST_TEST_MESSAGE( "Starting RC benchmark shape " << s.name );
    generate_blocks( 1 ); // Keep debug-node artificial transactions bounded per block.
    std::vector< HBD_asset > destination_balances_before;
    for( uint32_t i = 0; i < s.outputs; ++i )
      destination_balances_before.emplace_back( db->get_account( destination_names[i] ).get_hbd_balance() );
    std::vector< int64_t > source_balances_before;
    for( uint32_t i = 0; i < s.inputs; ++i )
      source_balances_before.push_back( db->get_account( source_names[i] ).get_hbd_balance().amount.value );
    const int64_t output_unit = 1000;
    std::vector< int64_t > input_amounts( s.inputs, output_unit );
    std::vector< int64_t > output_amounts( s.outputs, output_unit );
    if( s.inputs == 1 ) input_amounts[0] = int64_t( s.outputs ) * output_unit;
    if( s.inputs == 3 && s.outputs == 2 ) output_amounts[1] = 2 * output_unit;
    if( s.inputs > 1 && s.outputs == 1 ) output_amounts[0] = int64_t( s.inputs ) * output_unit;

    for( uint32_t i = 0; i < s.inputs; ++i )
    {
      try
      {
        issue_funds( source_names[i], HBD_asset( input_amounts[i] * 4 ) );
      }
      catch( const fc::exception& e )
      {
        BOOST_FAIL( "Funding RC benchmark source index " << i << " failed: " << e.to_detail_string() );
      }
    }

    std::vector< std::tuple< uint32_t, uint32_t, int64_t > > edges;
    uint32_t output_index = 0;
    auto remaining_outputs = output_amounts;
    for( uint32_t input_index = 0; input_index < s.inputs; ++input_index )
    {
      int64_t remaining = input_amounts[input_index];
      while( remaining > 0 )
      {
        const int64_t portion = std::min( remaining, remaining_outputs[output_index] );
        edges.emplace_back( input_index, output_index, portion );
        remaining -= portion;
        remaining_outputs[output_index] -= portion;
        if( remaining_outputs[output_index] == 0 ) ++output_index;
      }
    }

    // Keep memo bytes identical across all three representations so measured
    // size/RC differences come from transaction and operation encoding only.
    std::vector< std::string > edge_memos;
    edge_memos.reserve( edges.size() );
    for( size_t i = 0; i < edges.size(); ++i )
    {
      std::string edge_id = std::to_string( i );
      while( edge_id.size() < 3 ) edge_id.insert( edge_id.begin(), '0' );
      edge_memos.emplace_back( std::string( "rcbm-" ) + s.name + "-" + edge_id );
    }

    const auto& dgpo = db->get_dynamic_global_properties();
    const int64_t rc_regen = dgpo.get_total_vesting_shares().amount.value / ( HIVE_RC_REGEN_TIME / HIVE_BLOCK_INTERVAL );
    const auto& params = db->get< rc_resource_param_object, by_id >( rc_resource_param_id_type() );
    const auto& pool = db->get< rc_pool_object, by_id >( rc_pool_id_type() );
    auto same_pool_cost = [&]( const resource_count_type& usage )
    {
      int64_t cost = 0;
      for( size_t i = 0; i < HIVE_RC_NUM_RESOURCE_TYPES; ++i )
      {
        const auto& resource = params.resource_param_array[i];
        const int64_t pool_regen_share = fc::uint128_to_int64(
          ( uint128_t( rc_regen ) * pool.get_weight( i ) ) / pool.get_weight_divisor() );
        if( pool_regen_share <= 0 ) continue;
        const int64_t units = usage[i] * int64_t( resource.resource_dynamics_params.resource_unit );
        cost += resource_credits::compute_cost( resource.price_curve_params, pool.get_pool( i ), units, pool_regen_share );
      }
      return cost;
    };
    auto emit_row = [&]( const char* mode, uint64_t transactions, uint64_t bytes, uint64_t signatures,
                         const resource_count_type& usage, int64_t rc_cost, int64_t setup_rc, int64_t elapsed_us, uint64_t writes )
    {
      rusage memory{};
      getrusage( RUSAGE_SELF, &memory );
      csv << s.name << ',' << mode << ',' << s.inputs << ',' << s.outputs << ',' << transactions << ','
          << bytes << ',' << signatures << ',' << usage[resource_history_bytes] << ','
          << usage[resource_new_accounts] << ',' << usage[resource_market_bytes] << ','
          << usage[resource_state_bytes] << ',' << usage[resource_execution_time] << ','
          << rc_cost << ',' << same_pool_cost( usage ) << ',' << setup_rc << ','
          << elapsed_us << ',' << memory.ru_maxrss << ',' << writes << '\n';
    };

    // A: isolated transactions, each with one source signature. Sum the exact
    // cost and resource vector returned/derived for every signed transaction.
    resource_count_type separate_usage;
    int64_t separate_rc = 0;
    std::vector< full_transaction_ptr > separate_transactions;
    uint64_t separate_bytes = 0, separate_signatures = 0;
    int64_t separate_elapsed = 0;
    size_t edge_index = 0;
    for( const auto& edge : edges )
    {
      transfer_operation transfer;
      transfer.from = source_names[std::get<0>( edge )];
      transfer.to = destination_names[std::get<1>( edge )];
      transfer.amount = HBD_asset( std::get<2>( edge ) ).to_asset();
      transfer.memo = edge_memos[edge_index++];
      const auto start = std::chrono::steady_clock::now();
      full_transaction_ptr full_tx;
      try
      {
        full_tx = push_transaction( transfer, source_keys[std::get<0>( edge )] );
      }
      catch( const fc::exception& e )
      {
        BOOST_FAIL( "Separate RC benchmark transfer failed: " << e.to_detail_string() );
      }
      separate_elapsed += std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::steady_clock::now() - start ).count();
      count_resources( full_tx->get_transaction(), full_tx->get_transaction_size(), separate_usage, db->head_block_time(), *db );
      separate_rc += full_tx->get_rc_cost();
      separate_transactions.push_back( full_tx );
      separate_bytes += full_tx->get_transaction_size();
      separate_signatures += full_tx->get_transaction().signatures.size();
    }
    emit_row( "separate", edges.size(), separate_bytes, separate_signatures, separate_usage,
      separate_rc, 0, separate_elapsed, edges.size() * 2 );

    // B: one transaction containing the same ordinary transfer operations.
    signed_transaction standard_tx;
    standard_tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    edge_index = 0;
    for( const auto& edge : edges )
    {
      transfer_operation transfer;
      transfer.from = source_names[std::get<0>( edge )];
      transfer.to = destination_names[std::get<1>( edge )];
      transfer.amount = HBD_asset( std::get<2>( edge ) ).to_asset();
      transfer.memo = edge_memos[edge_index++];
      standard_tx.operations.emplace_back( transfer );
    }
    std::vector< fc::ecc::private_key > standard_keys;
    for( uint32_t i = 0; i < s.inputs; ++i ) standard_keys.push_back( source_keys[i] );
    auto start = std::chrono::steady_clock::now();
    auto standard_full_tx = push_transaction_ex( standard_tx, standard_keys );
    std::vector< full_transaction_ptr > standard_transactions{ standard_full_tx };
    const int64_t standard_elapsed = std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::steady_clock::now() - start ).count();
    resource_count_type standard_usage;
    count_resources( standard_full_tx->get_transaction(), standard_full_tx->get_transaction_size(), standard_usage, db->head_block_time(), *db );
    emit_row( "multi_transfer", 1, standard_full_tx->get_transaction_size(), standard_full_tx->get_transaction().signatures.size(),
      standard_usage, standard_full_tx->get_rc_cost(), 0, standard_elapsed, edges.size() * 2 );

    // C1: compressed operation with direct active-authority inputs.
    compressed_batch_transfer_operation direct_batch;
    direct_batch.symbol = HBD_SYMBOL;
    for( uint32_t i = 0; i < s.inputs; ++i )
      direct_batch.inputs.push_back( { source_names[i], input_amounts[i], 0 } );
    for( uint32_t i = 0; i < s.outputs; ++i )
      direct_batch.outputs.push_back( { destination_names[i], output_amounts[i] } );
    signed_transaction direct_tx;
    direct_tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    direct_tx.operations.emplace_back( direct_batch );
    start = std::chrono::steady_clock::now();
    auto direct_full_tx = push_transaction_ex( direct_tx, standard_keys );
    std::vector< full_transaction_ptr > direct_transactions{ direct_full_tx };
    const int64_t direct_elapsed = std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::steady_clock::now() - start ).count();
    resource_count_type direct_usage;
    count_resources( direct_full_tx->get_transaction(), direct_full_tx->get_transaction_size(), direct_usage, db->head_block_time(), *db );
    BOOST_REQUIRE_EQUAL( direct_usage[resource_history_bytes], direct_full_tx->get_transaction_size() );
    BOOST_REQUIRE_EQUAL( direct_usage[resource_market_bytes], direct_full_tx->get_transaction_size() );
    emit_row( "compressed_active", 1, direct_full_tx->get_transaction_size(), direct_full_tx->get_transaction().signatures.size(),
      direct_usage, direct_full_tx->get_rc_cost(), 0, direct_elapsed, s.inputs + s.outputs );
    csv.flush();

    // C2: same value flow funded by independent scoped grants; the actor signs.
    int64_t grant_setup_rc = 0;
    std::vector< full_transaction_ptr > grant_transactions;
    for( uint32_t i = 0; i < s.inputs; ++i )
    {
      BOOST_TEST_MESSAGE( "Creating scoped RC benchmark grant " << source_names[i] << ":" << grant_id );
      create_scoped_authority_operation grant;
      grant.grantor = source_names[i];
      grant.actor = "benchdebit";
      grant.grant_id = grant_id;
      grant.max_per_transfer = HBD_asset( input_amounts[i] ).to_asset();
      grant.max_per_period = grant.max_per_transfer;
      grant.max_lifetime = grant.max_per_transfer;
      grant.period_seconds = 3600;
      grant.valid_from = db->head_block_time();
      grant.expires_at = grant.valid_from + fc::seconds( 86400 );
      try
      {
        auto grant_tx = push_transaction( grant, source_keys[i] );
        grant_setup_rc += grant_tx->get_rc_cost();
        grant_transactions.push_back( grant_tx );
      }
      catch( const fc::exception& e )
      {
        BOOST_FAIL( "Creating scoped RC benchmark grant failed: " << e.to_detail_string() );
      }
    }
    compressed_batch_transfer_operation scoped_batch = direct_batch;
    scoped_batch.actor = "benchdebit";
    for( auto& input : scoped_batch.inputs ) input.grant_id = grant_id;
    signed_transaction scoped_tx;
    scoped_tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    scoped_tx.operations.emplace_back( scoped_batch );
    BOOST_TEST_MESSAGE( "Submitting scoped RC benchmark batch " << s.name );
    start = std::chrono::steady_clock::now();
    auto scoped_full_tx = push_transaction_ex( scoped_tx, std::vector<fc::ecc::private_key>{ actor_key } );
    std::vector< full_transaction_ptr > scoped_transactions{ scoped_full_tx };
    const int64_t scoped_elapsed = std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::steady_clock::now() - start ).count();
    resource_count_type scoped_usage;
    count_resources( scoped_full_tx->get_transaction(), scoped_full_tx->get_transaction_size(), scoped_usage, db->head_block_time(), *db );
    BOOST_REQUIRE_EQUAL( scoped_usage[resource_history_bytes], scoped_full_tx->get_transaction_size() );
    BOOST_REQUIRE_EQUAL( scoped_usage[resource_market_bytes], scoped_full_tx->get_transaction_size() );
    emit_row( "compressed_scoped", 1, scoped_full_tx->get_transaction_size(), scoped_full_tx->get_transaction().signatures.size(),
      scoped_usage, scoped_full_tx->get_rc_cost(), grant_setup_rc, scoped_elapsed, s.inputs + s.outputs );

    // Confirm all four transfer representations in one produced block, then
    // verify their RC charges and exact source/output state against the actual
    // confirmed block application.
    const uint32_t previous_head = db->head_block_num();
    generate_blocks( 1 );
    BOOST_REQUIRE_EQUAL( db->head_block_num(), previous_head + 1 );
    auto confirmed_cost = []( const std::vector< full_transaction_ptr >& transactions )
    {
      int64_t total = 0;
      for( const auto& tx : transactions )
      {
        BOOST_REQUIRE( tx->get_rc_cost() >= 0 );
        total += tx->get_rc_cost();
      }
      return total;
    };
    BOOST_REQUIRE_EQUAL( confirmed_cost( separate_transactions ), separate_rc );
    BOOST_REQUIRE_EQUAL( confirmed_cost( standard_transactions ), standard_full_tx->get_rc_cost() );
    BOOST_REQUIRE_EQUAL( confirmed_cost( direct_transactions ), direct_full_tx->get_rc_cost() );
    BOOST_REQUIRE_EQUAL( confirmed_cost( scoped_transactions ), scoped_full_tx->get_rc_cost() );
    BOOST_REQUIRE_EQUAL( confirmed_cost( grant_transactions ), grant_setup_rc );
    for( uint32_t i = 0; i < s.inputs; ++i )
      BOOST_REQUIRE_EQUAL( find_grant( *db, source_names[i], grant_id ).lifetime_spent.amount.value, input_amounts[i] );
    for( uint32_t i = 0; i < s.inputs; ++i )
      BOOST_REQUIRE_EQUAL( db->get_account( source_names[i] ).get_hbd_balance().amount.value, source_balances_before[i] );
    for( uint32_t i = 0; i < s.outputs; ++i )
      BOOST_REQUIRE_EQUAL( db->get_account( destination_names[i] ).get_hbd_balance(),
        destination_balances_before[i] + HBD_asset( output_amounts[i] * 4 ) );
    ++grant_id;
  }
  csv.flush();
  BOOST_REQUIRE( csv.good() );
}

BOOST_AUTO_TEST_SUITE_END()
#endif
