#ifdef IS_TEST_NET
#include <boost/test/unit_test.hpp>

#include <hive/chain/hive_fwd.hpp>

#include <hive/protocol/exceptions.hpp>
#include <hive/protocol/hardfork.hpp>

#include <hive/chain/database.hpp>
#include <hive/chain/database_exceptions.hpp>
#include <hive/chain/hive_objects.hpp>

#include <hive/chain/util/reward.hpp>

#include <fc/macros.hpp>
#include <fc/crypto/digest.hpp>

#include "../db_fixture/clean_database_fixture.hpp"
#include "../undo_data/undo.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace hive;
using namespace hive::chain;
using namespace hive::protocol;
using fc::string;

BOOST_FIXTURE_TEST_SUITE( undo_tests, clean_database_fixture )

BOOST_AUTO_TEST_CASE( undo_basic )
{
  try
  {
    BOOST_TEST_MESSAGE( "--- Testing: undo_basic" );
    auto time = db->head_block_time();

    undo_db udb( *db );
    undo_scenario< account_object > ao( *db );
    const account_object& pxy = ao.create( "proxy00", time );

    BOOST_TEST_MESSAGE( "--- No object added" );
    ao.remember_old_values< account_index >();
    udb.undo_begin();
    udb.undo_end();
    BOOST_REQUIRE( ao.check< account_index >() );

    BOOST_TEST_MESSAGE( "--- 1 object( create )" );
    ao.remember_old_values< account_index >();
    udb.undo_begin();

    const account_object& obj0 = ao.create( "name00", time );
    BOOST_REQUIRE( std::string( obj0.get_name() ) == "name00" );

    udb.undo_end();
    BOOST_REQUIRE( ao.check< account_index >() );

    BOOST_TEST_MESSAGE( "--- 1 object( create, modify )" );
    ao.remember_old_values< account_index >();
    udb.undo_begin();

    const account_object& obj1 = ao.create( "name00", time );
    BOOST_REQUIRE( std::string( obj1.get_name() ) == "name00" );
    const account_object& obj2 = ao.modify( obj1, [&]( account_object& obj ){ obj.set_name( "name01" ); } );
    BOOST_REQUIRE( std::string( obj2.get_name() ) == "name01" );

    udb.undo_end();
    BOOST_REQUIRE( ao.check< account_index >() );

    BOOST_TEST_MESSAGE( "--- 1 object( create, remove )" );
    ao.remember_old_values< account_index >();
    udb.undo_begin();

    const account_object& obj3 = ao.create( "name00", time );
    ao.remove( obj3 );

    udb.undo_end();
    BOOST_REQUIRE( ao.check< account_index >() );

    BOOST_TEST_MESSAGE( "--- 1 object( create, modify, remove )" );
    ao.remember_old_values< account_index >();
    udb.undo_begin();

    const account_object& obj4 = ao.create( "name00", time );
    ao.modify( obj4, [&]( account_object& obj ){ obj.set_proxy(pxy); } );
    ao.remove( obj4 );

    udb.undo_end();
    BOOST_REQUIRE( ao.check< account_index >() );

    BOOST_TEST_MESSAGE( "--- 1 object( create, remove, create )" );
    ao.remember_old_values< account_index >();
    udb.undo_begin();

    const account_object& obj5 = ao.create( "name00", time );
    ao.remove( obj5 );
    ao.create( "name00", time );

    udb.undo_end();
    BOOST_REQUIRE( ao.check< account_index >() );

    BOOST_TEST_MESSAGE( "--- 1 object( create, modify, remove, create )" );
    ao.remember_old_values< account_index >();
    udb.undo_begin();

    const account_object& obj6 = ao.create( "name00", time );
    ao.modify( obj6, [&]( account_object& obj ){ obj.set_proxy(pxy); } );
    ao.remove( obj6 );
    ao.create( "name00", time );

    udb.undo_end();
    BOOST_REQUIRE( ao.check< account_index >() );

    BOOST_TEST_MESSAGE( "--- 3 objects( create, create/modify, create/remove )" );
    ao.remember_old_values< account_index >();
    udb.undo_begin();

    const account_object& obj_c = ao.create( "name00", time );
    BOOST_REQUIRE( std::string( obj_c.get_name() ) == "name00" );

    const account_object& obj_cm = ao.create( "name01", time );
    BOOST_REQUIRE( std::string( obj_cm.get_name() ) == "name01" );
    ao.modify( obj_cm, [&]( account_object& obj ){ obj.set_name( "name02" ); } );
    BOOST_REQUIRE( std::string( obj_cm.get_name() ) == "name02" );

    const account_object& obj_cr = ao.create( "name03", time );
    BOOST_REQUIRE( std::string( obj_cr.get_name() ) == "name03" );
    ao.remove( obj_cr );

    udb.undo_end();
    BOOST_REQUIRE( ao.check< account_index >() );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( undo_delayed_votes )
{
  try
  {
    BOOST_TEST_MESSAGE( "--- Testing: undo_delayed_votes" );

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    undo_db udb( *db );
    undo_scenario< account_object > ao( *db );
    //const auto& index = db->get_index<account_index>();
    //const size_t initial_allocations = index.get_item_additional_allocation();
    /*
    test used to check additional allocations related to delayed votes, however we are
    not guaranteed to get the same allocation after undo, because it depends on capacity
    which can be different in original object and in its copy; next test (undo_feed_history)
    performs such check on feed history object, where allocation depends on size instead
    */

    ACTOR_DEFAULT_FEE( alice )
    generate_block();
    ISSUE_FUNDS( "alice", ASSET( "100000.000 TESTS" ) );
    BOOST_REQUIRE( compare_delayed_vote_count("alice", {}) );
    //BOOST_REQUIRE_EQUAL(index.get_item_additional_allocation(), initial_allocations);

    ao.remember_old_values< account_index >();
    udb.undo_begin();
    //BOOST_REQUIRE_EQUAL(index.get_item_additional_allocation(), initial_allocations);

    vest( "alice", "alice", ASSET( "100.000 TESTS" ), alice_private_key );
    generate_block();
    BOOST_REQUIRE( compare_delayed_vote_count("alice", { static_cast<uint64_t>(get_vesting( "alice" ).amount.value) }) );
    //BOOST_REQUIRE_GT(index.get_item_additional_allocation(), initial_allocations);

    udb.undo_end();
    BOOST_REQUIRE( ao.check< account_index >() );
    BOOST_REQUIRE( compare_delayed_vote_count("alice", {}) );
    //BOOST_REQUIRE_EQUAL(index.get_item_additional_allocation(), initial_allocations);
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( undo_feed_history )
{
  try
  {
    BOOST_TEST_MESSAGE( "--- Testing: undo_feed_history" );

    generate_blocks( HIVE_MAX_WITNESSES * 2 ); //skip two first schedules

    undo_db udb( *db );
    undo_scenario< feed_history_object > fho( *db );
    const auto& index = db->get_index< feed_history_index >();
    const size_t initial_allocations = index.get_item_additional_allocation();

    generate_block();
    BOOST_REQUIRE_EQUAL( index.get_item_additional_allocation(), initial_allocations );

    fho.remember_old_values< feed_history_index >();
    udb.undo_begin();

    // increase size by adding feed entries
    const auto exchange_rate = price( ASSET( "0.500 TBD" ), ASSET( "1.000 TESTS" ) );
    set_price_feed( exchange_rate, true );
    //generate_block();
    //ABW: note that we can't generate block since migrate_irreversible_state() calls
    //commit() on undo session stack, effectively removing the external session we
    //have here in test - if we produced the block and then tried undo_end(), it
    //would do nothing, not restoring the feed and not restoring allocation; other tests
    //with similar setup work only because they are running in small initial 30 block
    //window where rules of irreversibility are different, but we can't do that in
    //this test, because we need active witnesses to actually set the feed

    size_t increased_allocations = index.get_item_additional_allocation();
    BOOST_REQUIRE_GT( increased_allocations, initial_allocations );

    // restore original feed history
    udb.undo_end();
    BOOST_REQUIRE_EQUAL( index.get_item_additional_allocation(), initial_allocations );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( undo_object_disappear )
{
  try
  {
    BOOST_TEST_MESSAGE( "--- 2 objects. Modifying 1 object - uniqueness of complex index is violated" );
    auto time = db->head_block_time();

    undo_db udb( *db );
    undo_scenario< account_object > ao( *db );

    const account_object& pxy0 = ao.create( "proxy00", time );
    const account_object& pxy1 = ao.create( "proxy01", time );

    uint32_t old_size = ao.size< account_index >();

    const account_object& obj0 = ao.create( "name00", time ); ao.modify( obj0, [&]( account_object& obj ){ obj.set_proxy(pxy0); } );
    BOOST_REQUIRE( old_size + 1 == ao.size< account_index >() );

    const account_object& obj1 = ao.create( "name01", time ); ao.modify( obj1, [&]( account_object& obj ){ obj.set_proxy(pxy1); } );
    BOOST_REQUIRE( old_size + 2 == ao.size< account_index >() );

    ao.remember_old_values< account_index >();
    udb.undo_begin();

    /*
      Important!!!
        Method 'generic_index::modify' works incorrectly - after 'contraint violation', element is removed.
      Solution:
        It's necessary to write fix, according to issue #2154.
      Status:
        Done
    */
    HIVE_REQUIRE_THROW( ao.modify( obj1, [&]( account_object& obj ){ obj.set_name( "name00" ); obj.set_proxy(pxy0); } ), boost::exception );

    udb.undo_end();
    BOOST_REQUIRE( old_size + 2 == ao.size< account_index >() );

    BOOST_REQUIRE( ao.check< account_index >() );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( undo_key_collision )
{
  try
  {
    BOOST_TEST_MESSAGE( "--- Testing: undo_key_collision" );
    auto time = db->head_block_time();

    const auto& fake_account_object = db->create< account_object >( "fake", time );
    const comment_object* fake_parent_comment = nullptr;

    undo_db udb( *db );
    undo_scenario< account_object > ao( *db );

    BOOST_TEST_MESSAGE( "--- 1 object - twice with the same key" );
    ao.remember_old_values< account_index >();
    udb.undo_begin();

    ao.create( "name00", time );
    HIVE_REQUIRE_THROW( ao.create( "name00", time ), boost::exception );

    udb.undo_end();
    BOOST_REQUIRE( ao.check< account_index >() );

    BOOST_TEST_MESSAGE( "Version A - doesn't work without fix" );
    BOOST_TEST_MESSAGE( "--- 2 objects. Object 'obj0' is created before 'undo' and has modified key in next step." );
    BOOST_TEST_MESSAGE( "--- Object 'obj1' retrieves old key from object 'obj0'." );

    const account_object& obj0 = ao.create( "name00", time );

    ao.remember_old_values< account_index >();
    udb.undo_begin();

    uint32_t old_size = ao.size< account_index >();

    ao.modify( obj0, [&]( account_object& obj ){ obj.post_count = 1; } );
    ao.modify( obj0, [&]( account_object& obj ){ obj.set_name( "name01" ); } );

    /*
      Important!!!
        The mechanism 'undo' works incorrectly, when a unique key is changed in object, but such object is saved in 'undo' earlier.
        In next step another object gets identical key what first object had before.
        In such situation, attempt of modification by 'undo', raises 'uniqueness constraint' violation.
        Currently, simple solution used in whole project is adding unique key 'id' to save uniqueness.
      Solution:
        Is necessary to write another version of 'undo'?
    */
    //Temporary. After fix, this line should be enabled.
    //ao.create( "name00", time );

    //Temporary. After fix, this line should be removed.
    ao.create( "nameXYZ", time );

    BOOST_REQUIRE( old_size + 1 == ao.size< account_index >() );

    udb.undo_end(); //ABW: when above lines are commented/uncommented as instructed undo fails because uniqueness constraints are checked
      //with every [un]modified object meaning original obj0 tries to regain its name00 which is in conflict with still not undone new name00 object;
      //in this particular case changing order of undoing to first remove new objects prior to restoring modified ones would help, but it would
      //still be easy to make two objects modified in such a way that undoing changes one by one would still run into uniqueness constraint fail
      //(f.e. they could swap names)
    BOOST_REQUIRE( ao.check< account_index >() );

    BOOST_TEST_MESSAGE( "Version B - work without fix, because key 'cashout_time' is combined with 'id' key." );
    BOOST_TEST_MESSAGE( "--- 2 objects. Object 'obj0' is created before 'undo' and has modified key in next step." );
    BOOST_TEST_MESSAGE( "--- Object 'obj1' retrieves old key from object 'obj0'." );

    undo_scenario< comment_object > co( *db );
    undo_scenario< comment_cashout_object > co_cashout( *db );
    const comment_object& objc0 = co.create( fake_account_object, "permlink", fake_parent_comment );
    const comment_cashout_object& objc0_cashout = co_cashout.create( objc0, fake_account_object, "permlink", time_point_sec( 10 ), time_point_sec( 20 ) );

    BOOST_REQUIRE( objc0_cashout.get_comment_id() == objc0.get_id() );

    co.remember_old_values< comment_index >();
    co_cashout.remember_old_values< comment_cashout_index >();

    udb.undo_begin();

    old_size = co.size< comment_index >();
    uint32_t old_size_cashout = co_cashout.size< comment_cashout_index >();

    co_cashout.modify( objc0_cashout, [&]( comment_cashout_object& obj ){ obj.set_cashout_time( time_point_sec( 21 ) ); } );

    const comment_object& objc1 = co.create( fake_account_object, "permlink2", fake_parent_comment );
    const comment_cashout_object& objc1_cashout = co_cashout.create( objc1, fake_account_object, "permlink2", time_point_sec( 10 ), time_point_sec( 20 ) );

    BOOST_REQUIRE( objc1_cashout.get_comment_id() == objc1.get_id() );

    BOOST_REQUIRE( old_size + 1 == co.size< comment_index >() );
    BOOST_REQUIRE( old_size_cashout + 1 == co_cashout.size< comment_cashout_index >() );

    udb.undo_end();
    BOOST_REQUIRE( co.check< comment_index >() );
    BOOST_REQUIRE( co_cashout.check< comment_cashout_index >() );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( undo_different_indexes )
{
  try
  {
    auto time = db->head_block_time();
    const auto& fake_account_object = db->create< account_object >( "fake", time );
    const comment_object* fake_parent_comment = nullptr;

    BOOST_TEST_MESSAGE( "--- Testing: undo_different_indexes" );

    undo_db udb( *db );
    undo_scenario< account_object > ao( *db );
    undo_scenario< comment_object > co( *db );
    undo_scenario< comment_cashout_object > co_cashout( *db );

    uint32_t old_size_ao;
    uint32_t old_size_co;
    uint32_t old_size_co_cashout;

    BOOST_TEST_MESSAGE( "--- 2 objects( different types )" );
    ao.remember_old_values< account_index >();
    co.remember_old_values< comment_index >();
    old_size_ao = ao.size< account_index >();
    old_size_co = co.size< comment_index >();
    udb.undo_begin();

    const account_object& obja0 = ao.create( "name00", time );
    BOOST_REQUIRE( std::string( obja0.get_name() ) == "name00" );
    BOOST_REQUIRE( old_size_ao + 1 == ao.size< account_index >() );

    const comment_object& objc0 = co.create( fake_account_object, "11", fake_parent_comment );
    BOOST_CHECK_EQUAL( objc0.get_author_and_permlink_hash(), comment_object::compute_author_and_permlink_hash( fake_account_object.get_id(), "11" ) );
    BOOST_REQUIRE( old_size_co + 1 == co.size< comment_index >() );

    udb.undo_end();
    BOOST_REQUIRE( ao.check< account_index >() );
    BOOST_REQUIRE( co.check< comment_index >() );

    BOOST_TEST_MESSAGE( "--- 3 objects 'account_objects' + 2 objects 'comment_objects'." );
    BOOST_TEST_MESSAGE( "--- 'account_objects' - ( obj A )create, ( obj B )create/modify, ( obj C )create/remove." );
    BOOST_TEST_MESSAGE( "--- 'comment_objects' - ( obj A )create, ( obj B )create/remove." );
    ao.remember_old_values< account_index >();
    co.remember_old_values< comment_index >();
    co_cashout.remember_old_values< comment_cashout_index >();
    old_size_ao = ao.size< account_index >();
    old_size_co = co.size< comment_index >();
    old_size_co_cashout = co.size< comment_cashout_index >();
    udb.undo_begin();

    const account_object& pxy = ao.create( "name00", time );
    const account_object& obja1 = ao.create( "name01", time );
    const account_object& obja2 = ao.create( "name02", time );
    BOOST_REQUIRE( old_size_ao + 3 == ao.size< account_index >() );
    ao.modify( obja1, [&]( account_object& obj ){ obj.set_proxy(pxy); } );
    ao.remove( obja2 );
    BOOST_REQUIRE( old_size_ao + 2 == ao.size< account_index >() );

    co.create( fake_account_object, "11", fake_parent_comment );
    const comment_object& objc1 = co.create( fake_account_object, "12", fake_parent_comment );
    const comment_cashout_object& objc1_cashout = co_cashout.create( objc1, fake_account_object, "12", time_point_sec( 10 ), time_point_sec( 20 ) );
    co_cashout.modify( objc1_cashout, [&]( comment_cashout_object& obj ){} );
    BOOST_REQUIRE( old_size_co + 2 == co.size< comment_index >() );
    BOOST_REQUIRE( old_size_co_cashout + 1 == co_cashout.size< comment_cashout_index >() );

    udb.undo_end();
    BOOST_REQUIRE( ao.check< account_index >() );
    BOOST_REQUIRE( co.check< comment_index >() );
    BOOST_REQUIRE( co_cashout.check< comment_cashout_index >() );

    BOOST_TEST_MESSAGE( "--- 3 objects: 'account_object' + 'comment_object' + 'comment_cashout_object'" );
    BOOST_TEST_MESSAGE( "--- 'account_object' - ( obj A )create" );
    BOOST_TEST_MESSAGE( "--- 'comment_object' - ( obj A )create/remove." );
    BOOST_TEST_MESSAGE( "--- 'comment_cashout_object' - ( obj A )create/remove." );
    ao.remember_old_values< account_index >();
    co.remember_old_values< comment_index >();
    co_cashout.remember_old_values< comment_cashout_index >();
    old_size_ao = ao.size< account_index >();
    old_size_co = co.size< comment_index >();
    old_size_co_cashout = co_cashout.size< comment_cashout_index >();
    udb.undo_begin();

    ao.create( "name01", time );
    const comment_object& objc2 = co.create( fake_account_object, "12", fake_parent_comment );
    const comment_cashout_object& objc2_cashout = co_cashout.create( objc2, fake_account_object, "12", time_point_sec( 10 ), time_point_sec( 20 ) );
    BOOST_REQUIRE( old_size_ao + 1 == ao.size< account_index >() );
    BOOST_REQUIRE( old_size_co + 1 == co.size< comment_index >() );
    BOOST_REQUIRE( old_size_co_cashout + 1 == co_cashout.size< comment_cashout_index >() );

    co_cashout.modify( objc2_cashout, [&]( comment_cashout_object& obj ){} );
    BOOST_REQUIRE( old_size_ao + 1 == ao.size< account_index >() );
    BOOST_REQUIRE( old_size_co + 1 == co.size< comment_index >() );
    BOOST_REQUIRE( old_size_co_cashout + 1 == co_cashout.size< comment_cashout_index >() );

    udb.undo_end();
    BOOST_REQUIRE( ao.check< account_index >() );
    BOOST_REQUIRE( co.check< comment_index >() );

    BOOST_TEST_MESSAGE( "--- 3 objects: 'account_object' + 'comment_object' + 'comment_cashout_object'" );
    BOOST_TEST_MESSAGE( "--- Creating is before 'undo_start'." );
    BOOST_TEST_MESSAGE( "--- 'account_object' - create/5*modify" );
    BOOST_TEST_MESSAGE( "--- 'comment_object' - create/remove" );
    BOOST_TEST_MESSAGE( "--- 'comment_cashout_object' - create/5*modify/remove" );

    const comment_object& co1 = co.create( fake_account_object, "12", fake_parent_comment );
    const comment_cashout_object& co1_cashout = co_cashout.create( co1, fake_account_object, "12", time_point_sec( 10 ), time_point_sec( 20 ) );
    const account_object& ao1 = ao.create( std::to_string(0), time );

    ao.remember_old_values< account_index >();
    co.remember_old_values< comment_index >();
    co_cashout.remember_old_values< comment_cashout_index >();
    old_size_ao = ao.size< account_index >();
    old_size_co = co.size< comment_index >();
    old_size_co_cashout = co_cashout.size< comment_cashout_index >();
    udb.undo_begin();

    for( int32_t i=1; i<=5; ++i )
    {
      co_cashout.modify( co1_cashout, [&]( comment_cashout_object& obj ){} );
      ao.modify( ao1, [&]( account_object& obj ){ obj.set_name( std::to_string(0) ); } );

      BOOST_REQUIRE( old_size_ao == ao.size< account_index >() );
      BOOST_REQUIRE( old_size_co_cashout = co_cashout.size< comment_cashout_index >() );
    }

    udb.undo_end();
    BOOST_REQUIRE( old_size_ao == ao.size< account_index >() );
    BOOST_REQUIRE( old_size_co == co.size< comment_index >() );
    BOOST_REQUIRE( old_size_co_cashout == co_cashout.size< comment_cashout_index >() );
    BOOST_REQUIRE( ao.check< account_index >() );
    BOOST_REQUIRE( co.check< comment_index >() );
    BOOST_REQUIRE( co_cashout.check< comment_cashout_index >() );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( undo_generate_blocks )
{
  try
  {
    BOOST_TEST_MESSAGE( "--- Testing: undo_generate_blocks" );

    ACTORS( (alice)(bob)(chuck)(dan) )

    struct _data
    {
      std::string author;
      std::string permlink;
      std::string parent_author;
      std::string parent_permlink;
      std::string title;
      std::string body;;

      _data( std::string a, std::string pl, std::string pa, std::string ppl, std::string t, std::string b )
      : author( a ), permlink( pl ), parent_author( pa ), parent_permlink( ppl ), title( t ), body( b ){}

      void fill( comment_operation& com )
      {
        com.author = author;
        com.permlink = permlink;
        com.parent_author = parent_author;
        com.parent_permlink = parent_permlink;
        com.title = title;
        com.body = body;
      }
    };
    _data data[4]=
    {
      _data( "bob", "post4", std::string( HIVE_ROOT_POST_PARENT ), "pl4", "t4", "b4" ),
      _data( "alice", "post", std::string( HIVE_ROOT_POST_PARENT ), "pl", "t", "b" ),
      _data( "dan", "post2", "bob", "post4", "t2", "b2" ),
      _data( "chuck", "post3", "bob", "post4", "t3", "b3" )
    };

    generate_blocks( 1 );

    comment_operation op;

    undo_db udb( *db );
    undo_scenario< comment_object > co( *db );

    signed_transaction tx;

    BOOST_TEST_MESSAGE( "--- 1 objects: 'comment_object' without undo" );
    data[0].fill( op );
    tx.operations.push_back( op );

    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    push_transaction( tx, bob_post_key );
    generate_blocks( 1 );
    tx.operations.clear();

    generate_blocks( 1 );

    co.remember_old_values< comment_index >();

    uint32_t old_size_co = co.size< comment_index >();

    BOOST_TEST_MESSAGE( "--- 1 objects: 'comment_object' + undo" );
    data[1].fill( op );
    tx.operations.push_back( op );

    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );

    push_transaction( tx, alice_post_key );
    generate_blocks( 1 );
    db->pop_block();

    BOOST_REQUIRE( old_size_co == co.size< comment_index >() );
    BOOST_REQUIRE( co.check< comment_index >() );
    tx.operations.clear();

    generate_blocks( 1 );

    BOOST_TEST_MESSAGE( "--- 2 objects: 'comment_object' + undo" );
    data[2].fill( op );
    tx.operations.push_back( op );
    data[3].fill( op );
    tx.operations.push_back( op );

    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );

    push_transaction( tx, {dan_post_key, chuck_post_key} );
    generate_blocks( 1 );
    db->pop_block();

    BOOST_REQUIRE( old_size_co == co.size< comment_index >() );
    BOOST_REQUIRE( co.check< comment_index >() );
    tx.operations.clear();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()
#endif
