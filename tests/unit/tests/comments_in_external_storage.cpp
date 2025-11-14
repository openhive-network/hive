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

    auto _check_comments = [this]( const account_id_type& author, bool before_payout, bool before_flush_to_rocksdb )
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
        if( before_flush_to_rocksdb )
          BOOST_REQUIRE( _alice_shm_comment != nullptr );
        else
          BOOST_REQUIRE( _alice_shm_comment == nullptr );
      }
    };

    auto _number_blocks = HIVE_CASHOUT_WINDOW_SECONDS / HIVE_BLOCK_INTERVAL;

    _create_or_update_comments( "alice", alice_post_key );

    generate_blocks( 5 );

    _create_or_update_comments( "bob", bob_post_key );

    generate_blocks( _number_blocks - 5 - 1 );

    //Set artificially a limit to zero in order to check if flushing to rocksdb correctly works
    db->get_comments_handler().on_end_of_syncing();

    _check_comments( alice_id, true/*before_payout*/, true/*before_flush_to_rocksdb*/ );
    _check_comments( bob_id, true/*before_payout*/, true/*before_flush_to_rocksdb*/ );

    generate_blocks( 1 );

    _check_comments( alice_id, false/*before_payout*/, true/*before_flush_to_rocksdb*/ );

    _check_comments( bob_id, true/*before_payout*/, true/*before_flush_to_rocksdb*/ );

    generate_blocks( 5 );

    _check_comments( alice_id, false/*before_payout*/, true/*before_flush_to_rocksdb*/ );
    _check_comments( bob_id, false/*before_payout*/, true/*before_flush_to_rocksdb*/ );

    generate_blocks( 26 );
    _check_comments( alice_id, false/*before_payout*/, false/*before_flush_to_rocksdb*/ );
    _check_comments( bob_id, false/*before_payout*/, false/*before_flush_to_rocksdb*/ );
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

    auto _number_blocks = HIVE_CASHOUT_WINDOW_SECONDS / HIVE_BLOCK_INTERVAL;

    _create_or_update_comments();
    _check_comments( true/*before_payout*/ );

    generate_blocks( _number_blocks - 1 );

    //Set artificially a limit to zero in order to check if flushing to rocksdb correctly works
    db->get_comments_handler().on_end_of_syncing();

    _create_or_update_comments();
    _check_comments( true/*before_payout*/ );

    generate_blocks( 1 );

    _create_or_update_comments();
    _check_comments( false/*before_payout*/ );

    generate_blocks( 21 );
  }
  FC_LOG_AND_RETHROW()
}

void fork_reverts_cashout_scanario( const std::string& comment_archive_type, bool migrate, bool undoable_migration, const char* assertion_message )
{
  configuration_data.set_cashout_related_values( 0, 9, 9 * 2, 9 * 7, 3 );
  autoscope( []() { configuration_data.reset_cashout_values(); } );

  hived_fixture test;
  test.postponed_init( {
    hived_fixture::config_line_t( { "comment-archive", { comment_archive_type } } ),
    hived_fixture::config_line_t( { "plugin", { HIVE_ACCOUNT_HISTORY_ROCKSDB_PLUGIN_NAME } } )
  } );
  test.generate_block();
  test.db->set_hardfork( HIVE_BLOCKCHAIN_VERSION.minor_v() );
  test.generate_block();
  test.db->_log_hardforks = true;

  //Set artificially a limit to zero in order to check if flushing to rocksdb correctly works
  test.db->get_comments_handler().on_end_of_syncing();

  test.vest( HIVE_INIT_MINER_NAME, ASSET( "10.000 TESTS" ) );

  // Fill up the rest of miners
  for( int i = HIVE_NUM_INIT_MINERS; i < HIVE_MAX_WITNESSES; i++ )
  {
    test.account_create( HIVE_INIT_MINER_NAME + fc::to_string( i ), test.init_account_pub_key );
    test.fund( HIVE_INIT_MINER_NAME + fc::to_string( i ), HIVE_MIN_PRODUCER_REWARD );
    test.witness_create( HIVE_INIT_MINER_NAME + fc::to_string( i ), test.init_account_priv_key, "foo.bar", test.init_account_pub_key, HIVE_MIN_PRODUCER_REWARD.amount );
  }
  // disable OBI to allow forking
  test.witness_plugin->disable_fast_confirm();

  // create 'alice'
  fc::ecc::private_key alice_private_key = test.generate_private_key( "alice" );
  fc::ecc::private_key alice_post_key = test.generate_private_key( "alice_post" );
  test.account_create( "alice", alice_private_key.get_public_key(), alice_post_key.get_public_key() );

  test.generate_block();

  const auto& comment_idx = test.db->get_index< comment_index, by_id >();

  BOOST_TEST_MESSAGE( "Testing scenario where fork reverts cashout event" );

  hive::chain::comment comment;
  hive::chain::comment_id_type comment_id;
  fc::time_point_sec cashout_time;
  uint32_t cashout_block_num = 0;
  auto get_comment = [&]( const std::string& author, const std::string& permlink )
  {
    comment = test.get_comment( author, permlink );
    comment_id = comment.get_id();
    cashout_time = test.db->find_comment_cashout( *comment.get() )->get_cashout_time();
  };

  // create comment
  test.post_comment( "alice", "test1", "test", "testtest", "test", alice_post_key );
  test.generate_block();
  get_comment( "alice", "test1" );

  // wait for comment to be near cashout
  test.generate_blocks( cashout_time - fc::seconds( HIVE_BLOCK_INTERVAL ), false );

  // pass cashout - check with vote that comment exists
  test.generate_block();
  BOOST_CHECK_EQUAL( test.get_last_operations(1)[0].which(), operation::tag< comment_payout_update_operation >::value );
  cashout_block_num = test.db->head_block_num();
  test.vote( "alice", "test1", "alice", 100, alice_post_key );

  // pop block that processed cashout
  test.db->pop_block();

  // check with another vote that comment exists
  test.vote( "alice", "test1", "alice", 200, alice_post_key );

  // pass cashout again (like in normal fork, last block is considered missing) - check with third vote that comment exists
  test.generate_block( database::skip_nothing, HIVE_INIT_PRIVATE_KEY, 1 );
  BOOST_CHECK_EQUAL( test.get_last_operations(1)[0].which(), operation::tag< comment_payout_update_operation >::value );
  test.generate_block();
  test.vote( "alice", "test1", "alice", 300, alice_post_key );
  test.generate_block();

  BOOST_TEST_MESSAGE( "Testing scenario where fork reverts LIB change for block that contained cashout event" );

  // check that comment still exists in main index
  BOOST_CHECK( comment_idx.find( comment_id ) != comment_idx.end() );

  // run until block with cashout becomes irreversible
  test.generate_until_irreversible_block( cashout_block_num );
  // comment should migrate to archive (according to 'migrate' test flag - NONE has no migration)
  // ABW: note that this test assumes comments are migrated immediately - for RocksDB version, set volatile_objects_limit to 0 at the start of test once we can control that value)
  BOOST_CHECK_EQUAL( comment_idx.find( comment_id ) == comment_idx.end(), migrate );

  // pop block that processed LIB change (the LIB itself won't revert, but removal of comment from main index will, unless special measures are taken, like in MEMORY)
  test.db->pop_block();
  BOOST_CHECK_EQUAL( comment_idx.find( comment_id ) != comment_idx.end(), !migrate || undoable_migration );

  // reapply the block (popped block is missing)
  test.generate_block( database::skip_nothing, HIVE_INIT_PRIVATE_KEY, 1 );
  // because LIB itself did not revert, above block production did not execute LIB event, we need another block
  test.generate_block();
  // comment should migrate to archive again (be removed from main index again), unless of course there is no migration at all (or migration is not undoable)
  BOOST_CHECK_EQUAL( comment_idx.find( comment_id ) == comment_idx.end(), migrate );

  BOOST_TEST_MESSAGE( "Testing scenario where fork reverts cashout event and comment is deleted before cashout is reapplied" );

  // create comment
  test.post_comment( "alice", "test2", "test", "to be deleted", "test", alice_post_key );
  test.generate_block();
  get_comment( "alice", "test2" );

  // wait for comment to be near cashout
  test.generate_blocks( cashout_time - fc::seconds( HIVE_BLOCK_INTERVAL ), false );

  // pass cashout - check with downvote that comment exists (since we don't want to block comment deletion we need to use downvote, since the vote will be reapplied after pop)
  test.generate_block();
  BOOST_CHECK_EQUAL( test.get_last_operations(1)[0].which(), operation::tag< comment_payout_update_operation >::value );
  cashout_block_num = test.db->head_block_num();
  test.vote( "alice", "test2", "alice", -100, alice_post_key );

  // pop block that processed cashout
  test.db->pop_block();

  // check with another downvote that comment exists
  test.vote( "alice", "test2", "alice", -200, alice_post_key );

  // delete comment so cashout won't happen again
  test.delete_comment( "alice", "test2", alice_post_key );

  // pass cashout point again (last block is considered missing), but this time cashout should not happen - check with third vote that comment is not there
  test.generate_block( database::skip_nothing, HIVE_INIT_PRIVATE_KEY, 1 );
  BOOST_CHECK_EQUAL( test.get_last_operations(1)[0].which(), operation::tag< producer_reward_operation >::value );
  test.generate_block();
  HIVE_REQUIRE_ASSERT( test.vote( "alice", "test2", "alice", -300, alice_post_key ), assertion_message );
  test.generate_block();

  BOOST_TEST_MESSAGE( "Testing scenario where fork reverts cashout event and comment is deleted before cashout is reapplied but new comment is created in its place" );

  // create comment
  test.post_comment( "alice", "test3", "test", "to be deleted", "test", alice_post_key );
  test.generate_block();
  get_comment( "alice", "test3" );

  // wait for comment to be near cashout
  test.generate_blocks( cashout_time - fc::seconds( HIVE_BLOCK_INTERVAL ), false );

  // pass cashout - check with downvote that comment exists
  test.generate_block();
  BOOST_CHECK_EQUAL( test.get_last_operations(1)[0].which(), operation::tag< comment_payout_update_operation >::value );
  cashout_block_num = test.db->head_block_num();
  test.vote( "alice", "test3", "alice", -100, alice_post_key );

  // pop block that processed cashout
  test.db->pop_block();

  // check with another downvote that comment exists
  test.vote( "alice", "test3", "alice", -200, alice_post_key );

  // delete comment so cashout won't happen again
  test.delete_comment( "alice", "test3", alice_post_key );

  // create new comment so it reuses the same author/permlink (that is still new comment, so it can't be archived)
  test.post_comment( "alice", "test3", "test", "replacement comment", "test", alice_post_key );
  auto old_id = comment_id;
  auto old_cashout_time = cashout_time;
  get_comment( "alice", "test3" );
  BOOST_CHECK_LT( old_id, comment_id ); // ABW: that is generally true, except when it wraps
  BOOST_CHECK( old_cashout_time < cashout_time );

  // pass cashout point again (last block is considered missing), but this time cashout should not happen (cashout of replacement comment is in the future)
  test.generate_block( database::skip_nothing, HIVE_INIT_PRIVATE_KEY, 1 );
  BOOST_CHECK_EQUAL( test.get_last_operations(1)[0].which(), operation::tag< producer_reward_operation >::value );
  test.generate_block();
  // we've reused author/permlink, so we can't use vote to see if old version was removed, but new version should not have been archived
  BOOST_CHECK( comment_idx.find( comment_id ) != comment_idx.end() );
  test.generate_block();
};

BOOST_FIXTURE_TEST_CASE( fork_reverts_cashout, empty_fixture )
{
  fork_reverts_cashout_scanario( "NONE", false, false, "not comment_is_required" );
  fork_reverts_cashout_scanario( "MEMORY", true, false, "! comment_is_required" );
  fork_reverts_cashout_scanario( "ROCKSDB", true, true, "!comment_is_required" );
}

BOOST_FIXTURE_TEST_CASE( inconsistent_comment_archive, empty_fixture )
{
  try
  {
    BOOST_TEST_MESSAGE( "Test what happens when comment archive data is inconsistent with rest of state." );
    // What happens in the test:
    // - run a bit
    // - stop the node and save comment archive aside
    // - run a bit more
    // - stop the node
    // - try to start the node again pointing to previously saved comment archive - failure expected
    // - try to start the node again pointing to empty directory as comment archive - failure expected
    // - restart node normally and continue

    configuration_data.set_cashout_related_values( 0, 9, 9 * 2, 9 * 7, 3 );
    autoscope( []() { configuration_data.reset_cashout_values(); } );

    fc::path comment_archive_dir;
    {
      clean_database_fixture fixture( 512U, fc::optional<uint32_t>(), false ); // don't init AH (there is separate test for that)
      //Set artificially a limit to zero in order to check if flushing to rocksdb correctly works
      fixture.db->get_comments_handler().on_end_of_syncing();
      comment_archive_dir = fixture.get_chain_plugin().comment_storage_dir();
      fixture.account_create( "alice", fixture.init_account_pub_key );
      fixture.account_create( "bob", fixture.init_account_pub_key );
      fixture.post_comment( "alice", "test", "test", "test body", "category", fixture.init_account_priv_key );
      // first 30 blocks use different LIB mechanics
      fixture.generate_blocks( 3 * HIVE_MAX_WITNESSES ); // the comment should be archived after that
    }
    fc::temp_directory ca_copy_dir( hive::utilities::temp_directory_path() );
    BOOST_REQUIRE( ca_copy_dir.path().is_absolute() ); // if it wasn't absolute, it would later "try to place itself" inside shm_dir
    fc::copy( comment_archive_dir, ca_copy_dir.path() );
    {
      hived_fixture fixture( false );
      fixture.postponed_init();
      //Set artificially a limit to zero in order to check if flushing to rocksdb correctly works
      fixture.db->get_comments_handler().on_end_of_syncing();
      // check comment exists
      fixture.vote( "alice", "test", "bob", HIVE_100_PERCENT, fixture.init_account_priv_key );
      fixture.generate_block();
      fixture.post_comment( "bob", "test", "test", "test body", "category", fixture.init_account_priv_key );
      fixture.generate_blocks( 2 * HIVE_MAX_WITNESSES ); // second comment should also be archived now
    }
    // try to open with comment archive copied before - LIB will be inconsistent (and content as well)
    {
      hived_fixture fixture( false );
      HIVE_REQUIRE_ASSERT( fixture.postponed_init( { hived_fixture::config_line_t( { "comments-rocksdb-path", { ca_copy_dir.path().generic_string() }} ) } ),
        "lib == _cached_irreversible_block" );
    }
    // try to open with comment archive in completely new directory - LIB will be fresh 0 so also inconsistent
    {
      fc::temp_directory ca_empty_dir( hive::utilities::temp_directory_path() );
      hived_fixture fixture( false );
      HIVE_REQUIRE_ASSERT( fixture.postponed_init( { hived_fixture::config_line_t( { "comments-rocksdb-path", { ca_empty_dir.path().generic_string() }} ) } ),
        "0 == _cached_irreversible_block" );
    }
    // restart node with comment archive in normal location and continue (previous tries should not break anything in state)
    {
      hived_fixture fixture( false );
      fixture.postponed_init();
      //Set artificially a limit to zero in order to check if flushing to rocksdb correctly works
      fixture.db->get_comments_handler().on_end_of_syncing();
      // check both comments exist
      fixture.vote( "bob", "test", "alice", 50 * HIVE_1_PERCENT, fixture.init_account_priv_key );
      fixture.post_comment_to_comment( "bob", "reply", "reply", "I'm replying", "alice", "test", fixture.init_account_priv_key );
      fixture.generate_block();
    }
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()
#endif
