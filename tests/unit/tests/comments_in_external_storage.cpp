#ifdef IS_TEST_NET
#include <boost/test/unit_test.hpp>

#include <hive/chain/hive_fwd.hpp>

#include <hive/protocol/exceptions.hpp>
#include <hive/protocol/hardfork.hpp>

#include <hive/chain/block_summary_object.hpp>
#include <hive/chain/database.hpp>
#include <hive/chain/hive_objects.hpp>

#include <hive/chain/util/reward.hpp>

#include <hive/plugins/debug_node/debug_node_plugin.hpp>

#include <fc/crypto/digest.hpp>

#include "../db_fixture/clean_database_fixture.hpp"

#include <cmath>

using namespace hive;
using namespace hive::chain;
using namespace hive::chain::util;
using namespace hive::protocol;

BOOST_FIXTURE_TEST_SUITE( comments_in_external_storage, clean_database_fixture )

BOOST_AUTO_TEST_CASE( basic_checks )
{
  try
  {
    ACTORS( (alice)(bob) )
    vest( "alice", ASSET( "10.000 TESTS" ) );
    vest( "bob", ASSET( "10.000 TESTS" ) );

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );

    auto _create_or_update_comments = [this]( const std::string& author, const fc::ecc::private_key& post_key )
    {
      signed_transaction tx;
      comment_operation comment_op;
      comment_op.author = author;
      comment_op.permlink = "test";
      comment_op.parent_permlink = "test";
      comment_op.title = "foo";
      comment_op.body = "bar";
      tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
      tx.operations.push_back( comment_op );
      push_transaction( tx, post_key );
    };

    auto _check_comments = [this]( const account_id_type& author, bool before_payout )
    {
      const auto& _alice_comment = db->get_comment( author, string( "test" ) );
      const comment_cashout_object* _alice_comment_cashout = db->find_comment_cashout( *_alice_comment );

      auto _hash = comment_object::compute_author_and_permlink_hash( author, string( "test" ) );
      const auto* _alice_shm_comment = db->find< comment_object, by_permlink >( _hash );

      if( before_payout )
      {
        BOOST_REQUIRE( _alice_comment_cashout != nullptr );
        BOOST_REQUIRE( _alice_shm_comment != nullptr );
      }
      else
      {
        BOOST_REQUIRE( _alice_comment_cashout == nullptr );
        BOOST_REQUIRE( _alice_shm_comment == nullptr );
      }
    };

    _create_or_update_comments( "alice", alice_post_key );
    _check_comments( alice_id, true/*before_payout*/ );

    auto _number_blocks = HIVE_CASHOUT_WINDOW_SECONDS / HIVE_BLOCK_INTERVAL;

    generate_blocks( _number_blocks / 2 );

    _create_or_update_comments( "alice", alice_post_key );
    _check_comments( alice_id, true/*before_payout*/ );

    _create_or_update_comments( "bob", bob_post_key );
    _check_comments( bob_id, true/*before_payout*/ );

    generate_blocks( _number_blocks / 2 );
    generate_blocks( 100 );

    _create_or_update_comments( "alice", alice_post_key );
    _check_comments( alice_id, false/*before_payout*/ );

    _create_or_update_comments( "bob", bob_post_key );
    _check_comments( bob_id, true/*before_payout*/ );

    generate_blocks( _number_blocks / 2 );
    generate_blocks( 100 );

    _create_or_update_comments( "bob", bob_post_key );
    _check_comments( bob_id, false/*before_payout*/ );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( nested_comments )
{
  try
  {
    ACTORS( (alice)(bob)(sam)(dave) )
    vest( "alice", ASSET( "10.000 TESTS" ) );
    vest( "bob", ASSET( "10.000 TESTS" ) );
    vest( "sam", ASSET( "10.000 TESTS" ) );
    vest( "dave", ASSET( "10.000 TESTS" ) );

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );

    auto _create_or_update_comments = [this, &alice_post_key, &bob_post_key, &sam_post_key, &dave_post_key]()
    {
      signed_transaction tx;
      comment_operation comment_op;
      comment_op.author = "alice";
      comment_op.permlink = "test";
      comment_op.parent_permlink = "test";
      comment_op.title = "foo";
      comment_op.body = "bar";
      tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
      tx.operations.push_back( comment_op );
      push_transaction( tx, alice_post_key );

      comment_op.author = "bob";
      comment_op.parent_author = "alice";
      comment_op.parent_permlink = "test";
      tx.operations.clear();
      tx.operations.push_back( comment_op );
      push_transaction( tx, bob_post_key );

      comment_op.author = "sam";
      comment_op.parent_author = "bob";
      tx.operations.clear();
      tx.operations.push_back( comment_op );
      push_transaction( tx, sam_post_key );

      comment_op.author = "dave";
      comment_op.parent_author = "sam";
      tx.operations.clear();
      tx.operations.push_back( comment_op );
      push_transaction( tx, dave_post_key );

      generate_blocks( 1 );
    };

    auto _check_comments = [this, &alice_id, &bob_id, &sam_id, &dave_id ]( bool before_payout )
    {
      const auto _alice_comment = db->get_comment( alice_id, string( "test" ) );
      const comment_cashout_object* _alice_comment_cashout = db->find_comment_cashout( *_alice_comment );
      if( before_payout )
        BOOST_REQUIRE( _alice_comment_cashout != nullptr );
      else
        BOOST_REQUIRE( _alice_comment_cashout == nullptr );

      const auto _bob_comment = db->get_comment( bob_id, string( "test" ) );
      const comment_cashout_object* _bob_comment_cashout = db->find_comment_cashout( *_bob_comment );
      if( before_payout )
        BOOST_REQUIRE( _bob_comment_cashout != nullptr );
      else
        BOOST_REQUIRE( _bob_comment_cashout == nullptr );

      const auto _sam_comment = db->get_comment( sam_id, string( "test" ) );
      const comment_cashout_object* _sam_comment_cashout = db->find_comment_cashout( *_sam_comment );
      if( before_payout )
        BOOST_REQUIRE( _sam_comment_cashout != nullptr );
      else
        BOOST_REQUIRE( _sam_comment_cashout == nullptr );

      const auto _dave_comment = db->get_comment( dave_id, string( "test" ) );
      const comment_cashout_object* _dave_comment_cashout = db->find_comment_cashout( *_dave_comment );
      if( before_payout )
        BOOST_REQUIRE( _dave_comment_cashout != nullptr );
      else
        BOOST_REQUIRE( _dave_comment_cashout == nullptr );
    };

    _create_or_update_comments();
    _check_comments( true/*before_payout*/ );

    auto _number_blocks = HIVE_CASHOUT_WINDOW_SECONDS / HIVE_BLOCK_INTERVAL;

    generate_blocks( _number_blocks / 2 );

    _create_or_update_comments();
    _check_comments( true/*before_payout*/ );

    generate_blocks( _number_blocks / 2 );
    generate_blocks( 100 );

    _create_or_update_comments();
    _check_comments( false/*before_payout*/ );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()
#endif
