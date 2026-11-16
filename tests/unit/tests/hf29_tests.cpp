#ifdef IS_TEST_NET

#include <boost/test/unit_test.hpp>

#include <hive/chain/database_exceptions.hpp>
#include <hive/chain/detail/state/account_object.hpp>
#include <hive/chain/detail/state/account_object_multiindex.hpp>
#include <hive/chain/detail/state/limit_order_object.hpp>
#include <hive/chain/detail/state/limit_order_object_multiindex.hpp>
#include <hive/chain/detail/state/recurrent_transfer_object.hpp>
#include <hive/chain/detail/state/recurrent_transfer_object_multiindex.hpp>
#include <hive/chain/detail/state/global_property_object.hpp>
#include <hive/chain/detail/state/witness_objects.hpp>
#include <hive/chain/rc/rc_objects_multiindex.hpp>
#include <hive/chain/rc/rc_utility.hpp>

#include <hive/protocol/hive_custom_operations.hpp>

#include <hive/plugins/debug_node/debug_node_plugin.hpp>

#include <fc/io/json.hpp>

#include "../db_fixture/clean_database_fixture.hpp"

using namespace hive::chain;
using namespace hive::protocol;

BOOST_FIXTURE_TEST_SUITE( hf29_tests, cluster_database_fixture )

BOOST_AUTO_TEST_CASE( recovery_request_oversized_authority )
{
  try
  {
    bool is_hf29 = false;

    auto _content = [ &is_hf29 ]( ptr_hardfork_database_fixture& executor )
    {
      BOOST_TEST_MESSAGE( "Testing: oversized authority in recovery request edit/cancel" );
      BOOST_REQUIRE_EQUAL( (bool)executor, true );

      ACTORS_EXT( (*executor), DEFAULT_VESTING, (alice)(bob) );
      executor->fund( "alice", HIVE_asset( 1'000'000 ) );
      executor->db_plugin->debug_update( []( database& db )
      {
        const auto& alice = db.get_account( "alice" );
        db.modify( db.get_account( "bob" ), [&]( account_object& a )
        {
          a.set_recovery_account( alice );
        } );
      } );
      executor->generate_block();

      request_account_recovery_operation initial_request;
      initial_request.recovery_account = "alice";
      initial_request.account_to_recover = "bob";
      initial_request.new_owner_authority = authority(
        1, executor->generate_private_key( "k0" ).get_public_key(), 1 );
      executor->push_transaction( initial_request, alice_private_key );
      executor->generate_block();

      const auto& idx = executor->db->get_index< account_recovery_request_index, by_account >();
      BOOST_REQUIRE( idx.find( "bob" ) != idx.end() );
      BOOST_REQUIRE_EQUAL( idx.find( "bob" )->get_new_owner_authority().key_auths.size(), 1u );

      authority oversized;
      oversized.weight_threshold = 1;
      for( int i = 0; i <= HIVE_MAX_AUTHORITY_MEMBERSHIP; ++i )
        oversized.add_authority( executor->generate_private_key( "extra_" + std::to_string( i ) ).get_public_key(), 1 );

      request_account_recovery_operation oversized_edit;
      oversized_edit.recovery_account = "alice";
      oversized_edit.account_to_recover = "bob";
      oversized_edit.new_owner_authority = oversized;

      // regular push_transaction is stopped on check due to "is_in_control"
      HIVE_REQUIRE_ASSERT( executor->push_transaction( oversized_edit, alice_private_key ),
        "size <= HIVE_MAX_AUTHORITY_MEMBERSHIP" );

      // work around "is_in_control" by not actually executing transaction
      executor->db_plugin->debug_push_pending_transaction(
        executor->build_transaction( oversized_edit, alice_private_key ) );

      if( is_hf29 )
      {
        // "p2p" block is stopped on faulty transaction after HF29
        HIVE_REQUIRE_ASSERT( executor->generate_block_from_pending(),
          "size <= HIVE_MAX_AUTHORITY_MEMBERSHIP" );
        executor->db->clear_pending();
      }
      else
      {
        // "p2p" block is ok before HF28, even with faulty transaction
        executor->generate_block_from_pending();
        // oversized authority is included in edited request
        BOOST_REQUIRE_EQUAL( idx.find( "bob" )->get_new_owner_authority().key_auths.size(),
          size_t( HIVE_MAX_AUTHORITY_MEMBERSHIP + 1 ) );
      }

      // weight_threshold == 0 marks cancellation
      oversized.weight_threshold = 0;

      request_account_recovery_operation oversized_cancel;
      oversized_cancel.recovery_account = "alice";
      oversized_cancel.account_to_recover = "bob";
      oversized_cancel.new_owner_authority = oversized;

      HIVE_REQUIRE_ASSERT( executor->push_transaction( oversized_cancel, alice_private_key ),
        "size <= HIVE_MAX_AUTHORITY_MEMBERSHIP" );

      executor->db_plugin->debug_push_pending_transaction(
        executor->build_transaction( oversized_cancel, alice_private_key ) );

      if( is_hf29 )
      {
        HIVE_REQUIRE_ASSERT( executor->generate_block_from_pending(),
          "size <= HIVE_MAX_AUTHORITY_MEMBERSHIP" );
        // request not cancelled - still in the index with the original small authority
        BOOST_REQUIRE( idx.find( "bob" ) != idx.end() );
        BOOST_REQUIRE_EQUAL( idx.find( "bob" )->get_new_owner_authority().key_auths.size(), 1u );
      }
      else
      {
        executor->generate_block_from_pending();
        // oversized cancellation went through - request removed
        BOOST_REQUIRE( idx.find( "bob" ) == idx.end() );
      }
    };

    BOOST_TEST_MESSAGE( "*****HF-28*****" );
    execute_hardfork<28>( _content );

    is_hf29 = true;

    BOOST_TEST_MESSAGE( "*****HF-29*****" );
    execute_hardfork<29>( _content );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( limit_order_with_nonexisting_asset )
{
  try
  {
    bool is_hf29 = false;

    // Stringified evaluator assertion expressions (must exactly match the source).
    const std::string create_assert_expr =
      "( is_asset_type( o.amount_to_sell, HIVE_SYMBOL ) && is_asset_type( o.min_to_receive, HBD_SYMBOL ) ) "
      "|| ( is_asset_type( o.amount_to_sell, HBD_SYMBOL ) && is_asset_type( o.min_to_receive, HIVE_SYMBOL ) )";
    const std::string create2_assert_expr =
      "( is_asset_type( o.amount_to_sell, HIVE_SYMBOL ) && is_asset_type( o.exchange_rate.quote, HBD_SYMBOL ) ) "
      "|| ( is_asset_type( o.amount_to_sell, HBD_SYMBOL ) && is_asset_type( o.exchange_rate.quote, HIVE_SYMBOL ) )";
    const std::string invalid_symbol_expr = "( delta.symbol.asset_num == HIVE_ASSET_NUM_HBD ) && \"liquid\"";

    auto _content = [&]( ptr_hardfork_database_fixture& executor )
    {
      BOOST_TEST_MESSAGE( "Testing: limit_order_create / limit_order_create2 with nonexisting asset" );
      BOOST_REQUIRE_EQUAL( (bool)executor, true );

      ACTORS_EXT( (*executor), DEFAULT_VESTING, (alice)(bob)(carol)(dave) );
      executor->fund( "alice", HIVE_asset( 1'000'000 ) );
      executor->fund( "bob", HIVE_asset( 1'000'000 ) );
      executor->fund( "carol", HIVE_asset( 1'000'000 ) );
      executor->fund( "dave", HIVE_asset( 1'000'000 ) );
      executor->generate_block();

      const auto& limit_order_idx = executor->db->get_index< limit_order_index, by_account >();
      // arbitrary, never-registered SMT NAI - passes validate() since validate() permits SMT/HIVE markets
      const auto smt_symbol = asset_symbol_type::from_nai( 100000006, 3 );
      const auto expiration = executor->db->head_block_time() + fc::seconds( HIVE_MAX_LIMIT_ORDER_EXPIRATION );

      {
        BOOST_TEST_MESSAGE( "--- limit_order_create: buy nonexisting SMT for HIVE (the original bug)" );
        limit_order_create_operation op;
        op.owner = "alice";
        op.orderid = 1;
        op.amount_to_sell = asset( 10'000, HIVE_SYMBOL );
        op.min_to_receive = asset( 1'000, smt_symbol );
        op.expiration = expiration;

        // regular push_transaction is stopped on check due to "is_in_control"
        HIVE_REQUIRE_ASSERT( executor->push_transaction( op, alice_private_key ), create_assert_expr );

        // work around "is_in_control" by not actually executing transaction
        executor->db_plugin->debug_push_pending_transaction(
          executor->build_transaction( op, alice_private_key ) );

        if( is_hf29 )
        {
          // "p2p" block is stopped on faulty transaction after HF29
          HIVE_REQUIRE_ASSERT( executor->generate_block_from_pending(), create_assert_expr );
          BOOST_REQUIRE( limit_order_idx.find( boost::make_tuple( "alice", op.orderid ) ) == limit_order_idx.end() );
          BOOST_REQUIRE_EQUAL( executor->db->get_account( "alice" ).get_hive_balance(), HIVE_asset( 1'000'000 ) );
          executor->db->clear_pending();
        }
        else
        {
          // before HF29 the bug manifests - order in nonexisting asset is created
          executor->generate_block_from_pending();
          auto it = limit_order_idx.find( boost::make_tuple( "alice", op.orderid ) );
          BOOST_REQUIRE( it != limit_order_idx.end() );
          BOOST_REQUIRE_EQUAL( it->amount_for_sale(), asset( 10'000, HIVE_SYMBOL ) );
          BOOST_REQUIRE_EQUAL( executor->db->get_account( "alice" ).get_hive_balance(), HIVE_asset( 990'000 ) );
        }
      }

      {
        BOOST_TEST_MESSAGE( "--- limit_order_create: sell nonexisting SMT for HIVE (self-blocking before HF29)" );
        limit_order_create_operation op;
        op.owner = "bob";
        op.orderid = 2;
        op.amount_to_sell = asset( 1'000, smt_symbol );
        op.min_to_receive = asset( 10'000, HIVE_SYMBOL );
        op.expiration = expiration;

        HIVE_REQUIRE_ASSERT( executor->push_transaction( op, bob_private_key ), create_assert_expr );

        executor->db_plugin->debug_push_pending_transaction(
          executor->build_transaction( op, bob_private_key ) );

        if( is_hf29 )
        {
          HIVE_REQUIRE_ASSERT( executor->generate_block_from_pending(), create_assert_expr );
        }
        else
        {
          // before HF29 the order is blocked anyway, but only by adjust_balance failing on missing SMT
          HIVE_REQUIRE_ASSERT( executor->generate_block_from_pending(), invalid_symbol_expr );
        }
        BOOST_REQUIRE( limit_order_idx.find( boost::make_tuple( "bob", op.orderid ) ) == limit_order_idx.end() );
        BOOST_REQUIRE_EQUAL( executor->db->get_account( "bob" ).get_hive_balance(), HIVE_asset( 1'000'000 ) );
        executor->db->clear_pending();
      }

      {
        BOOST_TEST_MESSAGE( "--- limit_order_create2: buy nonexisting SMT for HIVE (the original bug)" );
        limit_order_create2_operation op;
        op.owner = "carol";
        op.orderid = 3;
        op.amount_to_sell = asset( 10'000, HIVE_SYMBOL );
        op.exchange_rate = price( asset( 10'000, HIVE_SYMBOL ), asset( 1'000, smt_symbol ) );
        op.expiration = expiration;

        HIVE_REQUIRE_ASSERT( executor->push_transaction( op, carol_private_key ), create2_assert_expr );

        executor->db_plugin->debug_push_pending_transaction(
          executor->build_transaction( op, carol_private_key ) );

        if( is_hf29 )
        {
          HIVE_REQUIRE_ASSERT( executor->generate_block_from_pending(), create2_assert_expr );
          BOOST_REQUIRE( limit_order_idx.find( boost::make_tuple( "carol", op.orderid ) ) == limit_order_idx.end() );
          BOOST_REQUIRE_EQUAL( executor->db->get_account( "carol" ).get_hive_balance(), HIVE_asset( 1'000'000 ) );
          executor->db->clear_pending();
        }
        else
        {
          executor->generate_block_from_pending();
          auto it = limit_order_idx.find( boost::make_tuple( "carol", op.orderid ) );
          BOOST_REQUIRE( it != limit_order_idx.end() );
          BOOST_REQUIRE_EQUAL( it->amount_for_sale(), asset( 10'000, HIVE_SYMBOL ) );
          BOOST_REQUIRE_EQUAL( executor->db->get_account( "carol" ).get_hive_balance(), HIVE_asset( 990'000 ) );
        }
      }

      {
        BOOST_TEST_MESSAGE( "--- limit_order_create2: sell nonexisting SMT for HIVE (self-blocking before HF29)" );
        limit_order_create2_operation op;
        op.owner = "dave";
        op.orderid = 4;
        op.amount_to_sell = asset( 1'000, smt_symbol );
        op.exchange_rate = price( asset( 1'000, smt_symbol ), asset( 10'000, HIVE_SYMBOL ) );
        op.expiration = expiration;

        HIVE_REQUIRE_ASSERT( executor->push_transaction( op, dave_private_key ), create2_assert_expr );

        executor->db_plugin->debug_push_pending_transaction(
          executor->build_transaction( op, dave_private_key ) );

        if( is_hf29 )
        {
          HIVE_REQUIRE_ASSERT( executor->generate_block_from_pending(), create2_assert_expr );
        }
        else
        {
          HIVE_REQUIRE_ASSERT( executor->generate_block_from_pending(), invalid_symbol_expr );
        }
        BOOST_REQUIRE( limit_order_idx.find( boost::make_tuple( "dave", op.orderid ) ) == limit_order_idx.end() );
        BOOST_REQUIRE_EQUAL( executor->db->get_account( "dave" ).get_hive_balance(), HIVE_asset( 1'000'000 ) );
        executor->db->clear_pending();
      }
    };

    BOOST_TEST_MESSAGE( "*****HF-28*****" );
    execute_hardfork<28>( _content );

    is_hf29 = true;

    BOOST_TEST_MESSAGE( "*****HF-29*****" );
    execute_hardfork<29>( _content );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( recurrent_transfer_modify_last_execution )
{
  try
  {
    bool is_hf29 = false;

    // Stringified evaluator assertion expressions (must exactly match the source). The create and modify
    // paths intentionally use different expressions so they get distinct assertion ids.
    const std::string create_executions_assert_expr = "op.executions >= 2";
    const std::string modify_executions_assert_expr = "op.executions > 1";

    auto _content = [ &is_hf29, &create_executions_assert_expr, &modify_executions_assert_expr ]( ptr_hardfork_database_fixture& executor )
    {
      BOOST_TEST_MESSAGE( "Testing: modifying a recurrent transfer down to a single remaining execution (issue #786)" );
      BOOST_REQUIRE_EQUAL( (bool)executor, true );

      ACTORS_EXT( (*executor), DEFAULT_VESTING, (alice)(bob) );
      executor->fund( "alice", HIVE_asset( 10'000 ) );
      executor->generate_block();

      BOOST_TEST_MESSAGE( "--- Create a recurrent transfer with 2 executions (valid on every hardfork)" );
      recurrent_transfer_operation op;
      op.from = "alice";
      op.to = "bob";
      op.memo = "original";
      op.amount = ASSET( "1.000 TESTS" );
      op.recurrence = 24;
      op.executions = 2;
      executor->push_transaction( op, alice_private_key );

      // the first execution fires on the next produced block, leaving a single execution remaining
      executor->generate_block();
      BOOST_REQUIRE_EQUAL( executor->get_hive_balance( "bob" ), HIVE_asset( 1'000 ) );
      {
        const auto* rt = executor->db->find< recurrent_transfer_object, by_from_to_id >( boost::make_tuple( alice_id, bob_id ) );
        BOOST_REQUIRE( rt != nullptr );
        BOOST_REQUIRE_EQUAL( rt->remaining_executions, 1 );
      }

      BOOST_TEST_MESSAGE( "--- Creating a NEW recurrent transfer with executions = 1 must fail on every hardfork" );
      recurrent_transfer_operation new_op;
      new_op.from = "alice";
      new_op.to = "bob";
      new_op.memo = "new";
      new_op.amount = ASSET( "1.000 TESTS" );
      new_op.recurrence = 24;
      new_op.executions = 1;
      recurrent_transfer_pair_id rtpi;
      rtpi.pair_id = 1; // different pair_id => a brand new transfer rather than a modification
      new_op.extensions.insert( rtpi );
      HIVE_REQUIRE_ASSERT( executor->push_transaction( new_op, alice_private_key ), create_executions_assert_expr );

      BOOST_TEST_MESSAGE( "--- Modifying the existing transfer with executions = 1" );
      op.memo = "updated_last_payment";
      op.executions = 1;

      if( is_hf29 )
      {
        // since HF29 the modification is allowed so the amount/memo of the last payment can be tweaked
        executor->push_transaction( op, alice_private_key );
        {
          const auto* rt = executor->db->find< recurrent_transfer_object, by_from_to_id >( boost::make_tuple( alice_id, bob_id ) );
          BOOST_REQUIRE( rt != nullptr );
          BOOST_REQUIRE_EQUAL( rt->remaining_executions, 1 );
          BOOST_REQUIRE_EQUAL( rt->memo, "updated_last_payment" );
        }

        BOOST_TEST_MESSAGE( "--- The single remaining execution still fires and removes the transfer" );
        executor->generate_blocks( executor->db->head_block_time() + fc::hours( op.recurrence ) );
        BOOST_REQUIRE_EQUAL( executor->get_hive_balance( "bob" ), HIVE_asset( 2'000 ) );
        const auto* rt_after = executor->db->find< recurrent_transfer_object, by_from_to_id >( boost::make_tuple( alice_id, bob_id ) );
        BOOST_REQUIRE( rt_after == nullptr );
      }
      else
      {
        // old behavior must be preserved before HF29: a single execution is rejected even on modification
        HIVE_REQUIRE_ASSERT( executor->push_transaction( op, alice_private_key ), modify_executions_assert_expr );
        const auto* rt = executor->db->find< recurrent_transfer_object, by_from_to_id >( boost::make_tuple( alice_id, bob_id ) );
        BOOST_REQUIRE( rt != nullptr );
        BOOST_REQUIRE_EQUAL( rt->remaining_executions, 1 ); // unchanged
        BOOST_REQUIRE_EQUAL( rt->memo, "original" ); // unchanged
      }

      executor->validate_database();
    };

    BOOST_TEST_MESSAGE( "*****HF-28*****" );
    execute_hardfork<28>( _content );

    is_hf29 = true;

    BOOST_TEST_MESSAGE( "*****HF-29*****" );
    execute_hardfork<29>( _content );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( recurrent_transfer_delete_with_single_execution )
{
  try
  {
    bool is_hf29 = false;

    // Stringified evaluator assertion expression for the delete (amount == 0) path. It is intentionally
    // textually different from the create ('op.executions >= 2') and modify ('op.executions > 1') paths
    // so it gets its own assertion id.
    const std::string delete_executions_assert_expr = "2 <= op.executions";

    auto _content = [ &is_hf29, &delete_executions_assert_expr ]( ptr_hardfork_database_fixture& executor )
    {
      BOOST_TEST_MESSAGE( "Testing: deleting a recurrent transfer with executions == 1 must match pre-HF29 nodes (issue #786)" );
      BOOST_REQUIRE_EQUAL( (bool)executor, true );

      ACTORS_EXT( (*executor), DEFAULT_VESTING, (alice)(bob) );
      executor->fund( "alice", HIVE_asset( 10'000 ) );
      executor->generate_block();

      BOOST_TEST_MESSAGE( "--- Create a recurrent transfer with 2 executions" );
      recurrent_transfer_operation op;
      op.from = "alice";
      op.to = "bob";
      op.memo = "original";
      op.amount = ASSET( "1.000 TESTS" );
      op.recurrence = 24;
      op.executions = 2;
      executor->push_transaction( op, alice_private_key );

      // the first execution fires on the next produced block, leaving a single execution remaining
      executor->generate_block();
      {
        const auto* rt = executor->db->find< recurrent_transfer_object, by_from_to_id >( boost::make_tuple( alice_id, bob_id ) );
        BOOST_REQUIRE( rt != nullptr );
        BOOST_REQUIRE_EQUAL( rt->remaining_executions, 1 );
      }

      BOOST_TEST_MESSAGE( "--- Deleting the transfer (amount == 0) with executions == 1" );
      recurrent_transfer_operation del_op;
      del_op.from = "alice";
      del_op.to = "bob";
      del_op.memo = "delete";
      del_op.amount = ASSET( "0.000 TESTS" );
      del_op.recurrence = 24;
      del_op.executions = 1;

      if( is_hf29 )
      {
        // since HF29 executions == 1 is accepted, so the deletion goes through
        executor->push_transaction( del_op, alice_private_key );
        const auto* rt = executor->db->find< recurrent_transfer_object, by_from_to_id >( boost::make_tuple( alice_id, bob_id ) );
        BOOST_REQUIRE( rt == nullptr );
      }
      else
      {
        // Old behavior must be preserved before HF29: pre-HF29 nodes reject executions == 1 in validate(),
        // so the evaluator must reject it on the delete path too - otherwise a new node would accept a
        // transaction an old node rejects, splitting the network before the hardfork activates.
        HIVE_REQUIRE_ASSERT( executor->push_transaction( del_op, alice_private_key ), delete_executions_assert_expr );
        const auto* rt = executor->db->find< recurrent_transfer_object, by_from_to_id >( boost::make_tuple( alice_id, bob_id ) );
        BOOST_REQUIRE( rt != nullptr ); // not deleted

        BOOST_TEST_MESSAGE( "--- Deleting with executions >= 2 still works before HF29" );
        del_op.executions = 2;
        executor->push_transaction( del_op, alice_private_key );
        const auto* rt_after = executor->db->find< recurrent_transfer_object, by_from_to_id >( boost::make_tuple( alice_id, bob_id ) );
        BOOST_REQUIRE( rt_after == nullptr );
      }

      executor->validate_database();
    };

    BOOST_TEST_MESSAGE( "*****HF-28*****" );
    execute_hardfork<28>( _content );

    is_hf29 = true;

    BOOST_TEST_MESSAGE( "*****HF-29*****" );
    execute_hardfork<29>( _content );
  }
  FC_LOG_AND_RETHROW()
}

namespace {

custom_json_operation rc_delegation_as_custom_json( const delegate_rc_operation& op )
{
  custom_json_operation custom_op;
  custom_op.required_posting_auths.insert( op.from );
  custom_op.id = HIVE_RC_CUSTOM_OPERATION_ID;
  custom_op.json = fc::json::to_string( rc_custom_operation( op ) );
  return custom_op;
}

const rc_direct_delegation_object* find_rc_delegation( const database& db, const account_id_type& from, const account_id_type& to )
{
  return db.find< rc_direct_delegation_object, by_from_to >( boost::make_tuple( from, to ) );
}

} // namespace

BOOST_AUTO_TEST_CASE( delegate_rc_as_regular_operation )
{
  try
  {
    bool is_hf29 = false;

    const std::string hf_assert_expr = "_db.has_hardfork( HIVE_HARDFORK_1_29_DELEGATE_RC_OPERATION )";

    auto _content = [ &is_hf29, &hf_assert_expr ]( ptr_hardfork_database_fixture& executor )
    {
      BOOST_TEST_MESSAGE( "Testing: delegate_rc_operation used as regular operation (issue #866)" );
      BOOST_REQUIRE_EQUAL( (bool)executor, true );

      ACTORS_EXT( (*executor), DEFAULT_VESTING, (alice)(bob)(carol) );
      executor->vest( "alice", HIVE_asset( 10'000 ) );
      executor->generate_block();

      delegate_rc_operation op;
      op.from = "alice";
      op.delegatees = { "bob" };
      op.max_rc = executor->get_vesting( "alice" ).amount.value / 4;

      if( is_hf29 )
      {
        executor->push_transaction( op, alice_post_key );
        executor->generate_block();
        const auto* delegation = find_rc_delegation( *executor->db, alice_id, bob_id );
        BOOST_REQUIRE( delegation != nullptr );
        BOOST_REQUIRE_EQUAL( delegation->delegated_rc, uint64_t( op.max_rc ) );
      }
      else
      {
        // before HF29 regular delegate_rc_operation is rejected, both in new transaction and in block
        HIVE_REQUIRE_ASSERT( executor->push_transaction( op, alice_post_key ), hf_assert_expr );
        executor->db_plugin->debug_push_pending_transaction( executor->build_transaction( op, alice_post_key ) );
        HIVE_REQUIRE_ASSERT( executor->generate_block_from_pending(), hf_assert_expr );
        executor->db->clear_pending();
        BOOST_REQUIRE( find_rc_delegation( *executor->db, alice_id, bob_id ) == nullptr );
      }

      // delegation embedded in custom_json works regardless of hardfork
      op.delegatees = { "carol" };
      executor->push_transaction( rc_delegation_as_custom_json( op ), alice_post_key );
      executor->generate_block();
      const auto* delegation = find_rc_delegation( *executor->db, alice_id, carol_id );
      BOOST_REQUIRE( delegation != nullptr );
      BOOST_REQUIRE_EQUAL( delegation->delegated_rc, uint64_t( op.max_rc ) );

      executor->validate_database();
    };

    BOOST_TEST_MESSAGE( "*****HF-28*****" );
    execute_hardfork<28>( _content );

    is_hf29 = true;

    BOOST_TEST_MESSAGE( "*****HF-29*****" );
    execute_hardfork<29>( _content );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( rc_delegation_below_minimum_in_block )
{
  try
  {
    bool is_hf29 = false;

    const std::string min_delegation_assert_expr = "op.max_rc >= min_delegation";

    auto _content = [ &is_hf29, &min_delegation_assert_expr ]( ptr_hardfork_database_fixture& executor )
    {
      BOOST_TEST_MESSAGE( "Testing: RC delegation below minimum included in block (issue #866)" );
      BOOST_REQUIRE_EQUAL( (bool)executor, true );

      executor->generate_block();
      executor->set_account_creation_fee( HIVE_asset( 3'000 ) ); //mainnet value - makes minimum RC delegation noticeable
      executor->generate_block();

      ACTORS_EXT( (*executor), DEFAULT_VESTING, (alice)(bob)(carol) );
      executor->vest( "alice", HIVE_asset( 10'000 ) );
      executor->generate_block();

      // minimum delegation is expressed in VESTS, so it changes along with VESTS price - take amounts with big margin
      int64_t min_delegation = 0;
      {
        const auto& gpo = executor->db->get_dynamic_global_properties();
        const auto& wso = executor->db->get_witness_schedule_object();
        min_delegation = ( HIVE_asset( wso.median_props.account_creation_fee.amount / 3 ) * gpo.get_vesting_share_price() ).amount.value;
      }
      const int64_t below_min = min_delegation / 2;
      BOOST_REQUIRE_GT( below_min, 0 );

      delegate_rc_operation op;
      op.from = "alice";
      op.delegatees = { "bob" };
      op.max_rc = below_min;
      custom_json_operation custom_op = rc_delegation_as_custom_json( op );

      // new transaction is rejected regardless of hardfork
      HIVE_REQUIRE_ASSERT( executor->push_transaction( custom_op, alice_post_key ), min_delegation_assert_expr );
      // work around "is_in_control" by not actually executing transaction
      executor->db_plugin->debug_push_pending_transaction( executor->build_transaction( custom_op, alice_post_key ) );
      // block is valid either way (failure of embedded operation is not a failure of custom_json_operation)...
      executor->generate_block_from_pending();
      const auto* delegation = find_rc_delegation( *executor->db, alice_id, bob_id );
      if( is_hf29 )
      {
        // ...but since HF29 the delegation is not effective
        BOOST_REQUIRE( delegation == nullptr );

        // regular operation with the same delegation makes block invalid
        HIVE_REQUIRE_ASSERT( executor->push_transaction( op, alice_post_key ), min_delegation_assert_expr );
        executor->db_plugin->debug_push_pending_transaction( executor->build_transaction( op, alice_post_key ) );
        HIVE_REQUIRE_ASSERT( executor->generate_block_from_pending(), min_delegation_assert_expr );
        executor->db->clear_pending();
        BOOST_REQUIRE( find_rc_delegation( *executor->db, alice_id, bob_id ) == nullptr );
      }
      else
      {
        // ...and before HF29 the delegation is effective
        BOOST_REQUIRE( delegation != nullptr );
        BOOST_REQUIRE_EQUAL( delegation->delegated_rc, uint64_t( below_min ) );
      }

      // delegation above minimal amount is fine
      op.delegatees = { "carol" };
      op.max_rc = min_delegation * 2;
      executor->push_transaction( rc_delegation_as_custom_json( op ), alice_post_key );
      executor->generate_block();
      delegation = find_rc_delegation( *executor->db, alice_id, carol_id );
      BOOST_REQUIRE( delegation != nullptr );
      BOOST_REQUIRE_EQUAL( delegation->delegated_rc, uint64_t( min_delegation * 2 ) );

      executor->validate_database();
    };

    BOOST_TEST_MESSAGE( "*****HF-28*****" );
    execute_hardfork<28>( _content );

    is_hf29 = true;

    BOOST_TEST_MESSAGE( "*****HF-29*****" );
    execute_hardfork<29>( _content );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( rc_delegation_during_pending_removal_in_block )
{
  try
  {
    bool is_hf29 = false;

    const std::string pending_removal_assert_expr = "!strict_rules || !db.rc().has_expired_delegation( from_account )";

    auto _content = [ &is_hf29, &pending_removal_assert_expr ]( ptr_hardfork_database_fixture& executor )
    {
      BOOST_TEST_MESSAGE( "Testing: RC delegation while excess delegations are still being removed included in block (issue #866)" );
      BOOST_REQUIRE_EQUAL( (bool)executor, true );

      executor->generate_block();
      // give actors RC adjustment, so they can transact without vests
      executor->set_account_creation_fee( HIVE_asset( 300'000 ) );
      // only one excess RC delegation is removed at once, rest waits for following blocks
      executor->db_plugin->debug_update( []( database& db )
      {
        db.set_remove_threshold( 1 );
      } );
      executor->generate_block();

      ACTORS_EXT( (*executor), NO_VESTING, (alice)(bob)(carol)(dave)(eve)(frank) );
      executor->generate_block();
      executor->set_account_creation_fee( HIVE_asset( 1 ) ); //turns off minimum RC delegation
      executor->generate_block();
      executor->vest( "alice", HIVE_asset( 10'000 ) );
      executor->generate_block();

      // bob delegates RC he has thanks to vests delegated from alice
      executor->delegate_vest( "alice", "bob", VEST_asset( executor->get_vesting( "alice" ).amount.value / 2 ), alice_private_key );
      executor->generate_block();
      delegate_rc_operation op;
      op.from = "bob";
      op.delegatees = { "carol", "dave", "eve" };
      op.max_rc = 10;
      executor->push_transaction( rc_delegation_as_custom_json( op ), bob_post_key );
      executor->generate_block();

      // alice takes back her vests - bob's RC delegations exceed his RC now; first excess delegation is removed
      // immediately, removal of the rest is spread over following blocks
      executor->delegate_vest( "alice", "bob", VEST_asset( 0 ), alice_private_key );
      // bob gets vests of his own, so he has RC to delegate again (it does not stop removal of excess delegations)
      executor->vest( "bob", HIVE_asset( 10'000 ) );
      const account_object& bob = executor->db->get_account( "bob" );
      BOOST_REQUIRE( executor->db->rc().has_expired_delegation( bob ) );

      op.delegatees = { "frank" };
      custom_json_operation custom_op = rc_delegation_as_custom_json( op );
      // new transaction is rejected regardless of hardfork
      HIVE_REQUIRE_ASSERT( executor->push_transaction( custom_op, bob_post_key ), pending_removal_assert_expr );
      // work around "is_in_control" by not actually executing transaction
      executor->db_plugin->debug_push_pending_transaction( executor->build_transaction( custom_op, bob_post_key ) );
      // block is valid either way (failure of embedded operation is not a failure of custom_json_operation)...
      executor->generate_block_from_pending();
      BOOST_REQUIRE( executor->db->rc().has_expired_delegation( bob ) ); // still one excess delegation to remove
      const auto* delegation = find_rc_delegation( *executor->db, bob_id, frank_id );
      if( is_hf29 )
      {
        // ...but since HF29 the delegation is not effective
        BOOST_REQUIRE( delegation == nullptr );

        // regular operation with the same delegation makes block invalid
        HIVE_REQUIRE_ASSERT( executor->push_transaction( op, bob_post_key ), pending_removal_assert_expr );
        executor->db_plugin->debug_push_pending_transaction( executor->build_transaction( op, bob_post_key ) );
        HIVE_REQUIRE_ASSERT( executor->generate_block_from_pending(), pending_removal_assert_expr );
        executor->db->clear_pending();
        BOOST_REQUIRE( find_rc_delegation( *executor->db, bob_id, frank_id ) == nullptr );
      }
      else
      {
        // ...and before HF29 the delegation is effective
        BOOST_REQUIRE( delegation != nullptr );
        BOOST_REQUIRE_EQUAL( delegation->delegated_rc, 10u );
      }

      // once excess delegations are removed, bob can delegate again
      executor->generate_block();
      BOOST_REQUIRE( !executor->db->rc().has_expired_delegation( bob ) );
      BOOST_REQUIRE( find_rc_delegation( *executor->db, bob_id, carol_id ) == nullptr );
      BOOST_REQUIRE( find_rc_delegation( *executor->db, bob_id, dave_id ) == nullptr );
      BOOST_REQUIRE( find_rc_delegation( *executor->db, bob_id, eve_id ) == nullptr );
      op.max_rc = 20;
      executor->push_transaction( rc_delegation_as_custom_json( op ), bob_post_key );
      executor->generate_block();
      delegation = find_rc_delegation( *executor->db, bob_id, frank_id );
      BOOST_REQUIRE( delegation != nullptr );
      BOOST_REQUIRE_EQUAL( delegation->delegated_rc, 20u );

      executor->validate_database();
    };

    BOOST_TEST_MESSAGE( "*****HF-28*****" );
    execute_hardfork<28>( _content );

    is_hf29 = true;

    BOOST_TEST_MESSAGE( "*****HF-29*****" );
    execute_hardfork<29>( _content );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( rc_usage_of_failed_embedded_rc_delegation_in_block )
{
  try
  {
    bool is_hf29 = false;

    auto _content = [ &is_hf29 ]( ptr_hardfork_database_fixture& executor )
    {
      BOOST_TEST_MESSAGE( "Testing: RC usage of failed RC delegations embedded in custom_json included in block (issue #866)" );
      BOOST_REQUIRE_EQUAL( (bool)executor, true );

      executor->generate_block();
      executor->set_account_creation_fee( HIVE_asset( 1 ) ); //turns off minimum RC delegation
      executor->generate_block();
      ACTORS_EXT( (*executor), DEFAULT_VESTING, (alice)(bob)(carol) );
      executor->vest( "alice", HIVE_asset( 10'000 ) );
      executor->generate_block();

      delegate_rc_operation op;
      op.from = "alice";
      op.delegatees = { "bob" };
      op.max_rc = executor->get_vesting( "alice" ).amount.value / 4;
      const int64_t bob_delegation = op.max_rc;
      executor->push_transaction( rc_delegation_as_custom_json( op ), alice_post_key );
      executor->generate_block();

      auto require_delegations_unchanged = [&]()
      {
        const auto* delegation = find_rc_delegation( *executor->db, alice_id, bob_id );
        BOOST_REQUIRE( delegation != nullptr );
        BOOST_REQUIRE_EQUAL( delegation->delegated_rc, uint64_t( bob_delegation ) );
        BOOST_REQUIRE( find_rc_delegation( *executor->db, alice_id, carol_id ) == nullptr );
      };
      auto usage_in_block = [&]( const custom_json_operation& custom_op )
      {
        // work around "is_in_control" by not actually executing transaction
        executor->db_plugin->debug_push_pending_transaction( executor->build_transaction( custom_op, alice_post_key ) );
        executor->generate_block_from_pending();
        // failure of embedded operations is not a failure of custom_json_operation
        BOOST_REQUIRE_EQUAL( executor->get_block_reader().get_block_by_number( executor->db->head_block_num() )->get_block().transactions.size(), 1u );
        return executor->db->rc().get_tx_info().usage;
      };

      // usage of custom_json_operation itself - for reference
      custom_json_operation custom_op = rc_delegation_as_custom_json( op );
      custom_op.id = "dummy"; // not interpreted
      executor->push_transaction( custom_op, alice_post_key );
      const auto reference = executor->db->rc().get_tx_info().usage;
      executor->generate_block();
      BOOST_REQUIRE_GT( reference[ resource_state_bytes ], 0 );

      // increase of delegation to bob by more than alice can delegate
      const int64_t too_much = executor->db->get_account( "alice" ).get_maximum_rc().value + bob_delegation;

      BOOST_TEST_MESSAGE( "--- failed update of existing delegation (gets state discount before failure)" );
      op.max_rc = too_much;
      auto usage = usage_in_block( rc_delegation_as_custom_json( op ) );
      require_delegations_unchanged();
      BOOST_REQUIRE_EQUAL( usage[ resource_execution_time ], reference[ resource_execution_time ] );
      if( is_hf29 )
        BOOST_REQUIRE_EQUAL( usage[ resource_state_bytes ], reference[ resource_state_bytes ] );
      else
        BOOST_REQUIRE_EQUAL( usage[ resource_state_bytes ], 0 ); // discount for delegation that was not changed

      BOOST_TEST_MESSAGE( "--- successful creation of delegation followed by failed update in the same custom_json" );
      delegate_rc_operation create_op;
      create_op.from = "alice";
      create_op.delegatees = { "carol" };
      create_op.max_rc = bob_delegation;
      custom_op = rc_delegation_as_custom_json( op );
      custom_op.json = "[[\"delegate_rc\"," + fc::json::to_string( create_op ) + "],[\"delegate_rc\"," + fc::json::to_string( op ) + "]]";
      usage = usage_in_block( custom_op );
      // creation of delegation to carol was reverted along with failed update
      require_delegations_unchanged();
      if( is_hf29 )
      {
        BOOST_REQUIRE_EQUAL( usage[ resource_execution_time ], reference[ resource_execution_time ] );
        BOOST_REQUIRE_EQUAL( usage[ resource_state_bytes ], reference[ resource_state_bytes ] );
      }
      else
      {
        // execution of reverted delegation is paid for
        BOOST_REQUIRE_EQUAL( usage[ resource_execution_time ], reference[ resource_execution_time ] + 75000 );
        // new state of reverted delegation is paid for, but there is discount for delegation that was not changed
        BOOST_REQUIRE_EQUAL( usage[ resource_state_bytes ], reference[ resource_state_bytes ] );
      }

      BOOST_TEST_MESSAGE( "--- the same as above, but correct" );
      op.max_rc = bob_delegation / 2;
      custom_op.json = "[[\"delegate_rc\"," + fc::json::to_string( create_op ) + "],[\"delegate_rc\"," + fc::json::to_string( op ) + "]]";
      usage = usage_in_block( custom_op );
      BOOST_REQUIRE_EQUAL( usage[ resource_execution_time ], reference[ resource_execution_time ] + 2 * 75000 );
      BOOST_REQUIRE_EQUAL( usage[ resource_state_bytes ], reference[ resource_state_bytes ] + 1927200 );
      BOOST_REQUIRE( find_rc_delegation( *executor->db, alice_id, carol_id ) != nullptr );

      executor->validate_database();
    };

    BOOST_TEST_MESSAGE( "*****HF-28*****" );
    execute_hardfork<28>( _content );

    is_hf29 = true;

    BOOST_TEST_MESSAGE( "*****HF-29*****" );
    execute_hardfork<29>( _content );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()

#endif
