#ifdef IS_TEST_NET

#include <boost/test/unit_test.hpp>

#include <hive/chain/database_exceptions.hpp>
#include <hive/chain/detail/state/account_object.hpp>
#include <hive/chain/detail/state/account_object_multiindex.hpp>
#include <hive/chain/detail/state/limit_order_object.hpp>
#include <hive/chain/detail/state/limit_order_object_multiindex.hpp>

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

      ACTORS_EXT( (*executor), (alice)(bob) )
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

      ACTORS_EXT( (*executor), (alice)(bob)(carol)(dave) )
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

BOOST_AUTO_TEST_SUITE_END()

#endif
