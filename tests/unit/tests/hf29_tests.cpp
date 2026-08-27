#ifdef IS_TEST_NET

#include <boost/test/unit_test.hpp>

#include <hive/chain/database_exceptions.hpp>
#include <hive/chain/detail/state/account_object.hpp>
#include <hive/chain/detail/state/account_object_multiindex.hpp>
#include <hive/chain/detail/state/limit_order_object.hpp>
#include <hive/chain/detail/state/limit_order_object_multiindex.hpp>
#include <hive/chain/detail/state/recurrent_transfer_object.hpp>
#include <hive/chain/detail/state/recurrent_transfer_object_multiindex.hpp>

#include <hive/plugins/debug_node/debug_node_plugin.hpp>

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

BOOST_AUTO_TEST_CASE( open_and_impossible_authority )
{
  try
  {
    bool is_hf29 = false;

    auto _content = [ &is_hf29 ]( ptr_hardfork_database_fixture& executor )
    {
      BOOST_TEST_MESSAGE( "Testing: open/impossible authority on account creation and update (issue #586)" );
      BOOST_REQUIRE_EQUAL( (bool)executor, true );

      ACTORS_EXT( (*executor), DEFAULT_VESTING, (alice) );
      executor->fund( "alice", HIVE_asset( 1'000'000 ) );
      executor->generate_block();

      // open and impossible are disjoint states
      authority open_auth; // default constructed - what a mis-nested JSON payload leaves behind
      BOOST_REQUIRE_EQUAL( open_auth.weight_threshold, 0u );
      BOOST_REQUIRE( !open_auth.is_impossible() );

      const private_key_type lonely_key = executor->generate_private_key( "lonely" );
      authority impossible_auth( 2, lonely_key.get_public_key(), 1 ); // needs 2, only 1 can ever be gathered
      BOOST_REQUIRE( impossible_auth.is_impossible() );
      BOOST_REQUIRE( impossible_auth.weight_threshold != 0u );

      // stringified conditions of the asserts in validate_new_authority
      const char* OPEN_ASSERT = "auth.weight_threshold";
      const char* IMPOSSIBLE_ASSERT = "!auth.is_impossible()";

      const authority good_auth( 1, executor->generate_private_key( "good" ).get_public_key(), 1 );

      BOOST_TEST_MESSAGE( "--- account_update / account_update2 reject both, in every role" );

      // "is_in_control" refuses these from a user right away, hardfork or not
      const auto require_update_rejected = [&]( const operation& op, const char* expected )
      {
        HIVE_REQUIRE_ASSERT( executor->push_transaction( op, alice_private_key ), expected );
      };

      for( const authority* bad : { &open_auth, &impossible_auth } )
      {
        const char* expected = bad->weight_threshold == 0 ? OPEN_ASSERT : IMPOSSIBLE_ASSERT;

        { account_update_operation  op; op.account = "alice"; op.owner   = *bad; require_update_rejected( op, expected ); }
        { account_update_operation  op; op.account = "alice"; op.active  = *bad; require_update_rejected( op, expected ); }
        { account_update_operation  op; op.account = "alice"; op.posting = *bad; require_update_rejected( op, expected ); }
        { account_update2_operation op; op.account = "alice"; op.owner   = *bad; require_update_rejected( op, expected ); }
        { account_update2_operation op; op.account = "alice"; op.active  = *bad; require_update_rejected( op, expected ); }
        { account_update2_operation op; op.account = "alice"; op.posting = *bad; require_update_rejected( op, expected ); }
      }

      // stored authorities must be untouched - the #586 erasure must not happen
      const auto& alice_auth = executor->db->get< account_authority_object, by_account >( "alice" );
      BOOST_REQUIRE_EQUAL( alice_auth.get_owner().weight_threshold, 1u );
      BOOST_REQUIRE( !alice_auth.get_owner().key_auths.empty() );

      BOOST_TEST_MESSAGE( "--- a block carrying an open authority is only rejected from HF29 on" );

      // work around "is_in_control" by pushing it as if it arrived from the network
      account_update_operation open_update;
      open_update.account = "alice";
      open_update.posting = open_auth;
      executor->db_plugin->debug_push_pending_transaction(
        executor->build_transaction( open_update, alice_private_key ) );

      if( is_hf29 )
      {
        HIVE_REQUIRE_ASSERT( executor->generate_block_from_pending(), OPEN_ASSERT );
        executor->db->clear_pending();
        BOOST_REQUIRE( alice_auth.get_posting().weight_threshold != 0u ); // authority survived
      }
      else
      {
        // before HF29 the old rule stands, so the erasure still goes through inside a block
        executor->generate_block_from_pending();
        BOOST_REQUIRE_EQUAL( alice_auth.get_posting().weight_threshold, 0u );

        // put alice back together before continuing
        account_update_operation restore;
        restore.account = "alice";
        restore.posting = authority( 1, alice_post_key.get_public_key(), 1 );
        executor->push_transaction( restore, alice_private_key );
        BOOST_REQUIRE( alice_auth.get_posting().weight_threshold != 0u );
      }

      BOOST_TEST_MESSAGE( "--- account_update2's allow_open_authority extension" );

      account_update2_operation opt_in;
      opt_in.account = "alice";
      opt_in.posting = open_auth;
      opt_in.extensions.insert( allow_open_authority() );
      BOOST_REQUIRE( opt_in.is_open_authority_allowed() );

      if( is_hf29 )
      {
        // an open authority set on purpose is allowed again
        executor->push_transaction( opt_in, alice_private_key );
        BOOST_REQUIRE_EQUAL( alice_auth.get_posting().weight_threshold, 0u );

        // ...but the extension only lifts the "open" restriction, never the "impossible" one
        account_update2_operation still_bad;
        still_bad.account = "alice";
        still_bad.active = impossible_auth;
        still_bad.extensions.insert( allow_open_authority() );
        HIVE_REQUIRE_ASSERT( executor->push_transaction( still_bad, alice_private_key ), IMPOSSIBLE_ASSERT );

        // restore a sane posting authority for the final sanity check
        account_update2_operation restore;
        restore.account = "alice";
        restore.posting = good_auth;
        executor->push_transaction( restore, alice_private_key );
      }
      else
      {
        // the extension must not reach a block before the hardfork activates it
        HIVE_REQUIRE_ASSERT( executor->push_transaction( opt_in, alice_private_key ),
          "o.extensions.empty() && \"account_update2\"" );
      }

      BOOST_TEST_MESSAGE( "--- account creation rejects both, with no opt-in of its own" );

      const HIVE_asset creation_fee = executor->db->get_witness_schedule_object().median_props.account_creation_fee;

      const auto require_create_rejected = [&]( const std::string& name, const authority& bad, const char* expected )
      {
        account_create_operation op;
        op.creator = HIVE_INIT_MINER_NAME; // initminer pays the RC; alice has only DEFAULT_VESTING
        op.new_account_name = name;
        op.fee = creation_fee.to_asset();
        op.owner = bad;
        op.active = bad;
        op.posting = bad;
        op.memo_key = executor->generate_private_key( name + "_memo" ).get_public_key();
        HIVE_REQUIRE_ASSERT( executor->push_transaction( op, executor->init_account_priv_key ), expected );
        BOOST_REQUIRE( executor->db->find_account( name ) == nullptr );
      };

      require_create_rejected( "opener", open_auth, OPEN_ASSERT );
      require_create_rejected( "frozen", impossible_auth, IMPOSSIBLE_ASSERT );

      // ...and the same for create_claimed_account, the other way to bring an account into existence
      executor->claim_account( HIVE_INIT_MINER_NAME, creation_fee, executor->init_account_priv_key );

      const auto require_claimed_create_rejected = [&]( const std::string& name, const authority& bad, const char* expected )
      {
        create_claimed_account_operation op;
        op.creator = HIVE_INIT_MINER_NAME;
        op.new_account_name = name;
        op.owner = bad;
        op.active = bad;
        op.posting = bad;
        op.memo_key = executor->generate_private_key( name + "_memo" ).get_public_key();
        HIVE_REQUIRE_ASSERT( executor->push_transaction( op, executor->init_account_priv_key ), expected );
        BOOST_REQUIRE( executor->db->find_account( name ) == nullptr );
      };

      require_claimed_create_rejected( "opener", open_auth, OPEN_ASSERT );
      require_claimed_create_rejected( "frozen", impossible_auth, IMPOSSIBLE_ASSERT );

      BOOST_TEST_MESSAGE( "--- ordinary authorities are unaffected" );

      // the guard must not over-reject
      {
        account_create_operation op;
        op.creator = HIVE_INIT_MINER_NAME;
        op.new_account_name = "carol";
        op.fee = creation_fee.to_asset();
        op.owner = good_auth;
        op.active = good_auth;
        op.posting = good_auth;
        op.memo_key = executor->generate_private_key( "carol_memo" ).get_public_key();
        executor->push_transaction( op, executor->init_account_priv_key );
        BOOST_REQUIRE( executor->db->find_account( "carol" ) != nullptr );
      }
      {
        // done last - it replaces the key alice signs her active-authority transactions with
        account_update_operation op;
        op.account = "alice";
        op.active = good_auth;
        executor->push_transaction( op, alice_private_key );
        BOOST_REQUIRE( alice_auth.get_active() == good_auth );
      }

      executor->generate_block();
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
