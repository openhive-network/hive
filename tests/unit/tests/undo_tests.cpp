#ifdef IS_TEST_NET
#include <boost/test/unit_test.hpp>

#include <hive/chain/hive_fwd.hpp>

#include <hive/protocol/exceptions.hpp>
#include <hive/protocol/hardfork.hpp>

#include <hive/chain/database.hpp>
#include <hive/chain/database_exceptions.hpp>
#include <hive/chain/hive_objects.hpp>
#include <hive/chain/transaction_object.hpp>

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
    const auto& pxy = ao.create( "proxy00", time );

    BOOST_TEST_MESSAGE( "--- No object added" );
    ao.remember_old_values< account_index >();
    udb.undo_begin();
    udb.undo_end();
    BOOST_REQUIRE( ao.check< account_index >() );

    BOOST_TEST_MESSAGE( "--- 1 object( create )" );
    ao.remember_old_values< account_index >();
    udb.undo_begin();

    const auto& obj0 = ao.create( "name00", time );
    BOOST_REQUIRE( std::string( obj0.get_name() ) == "name00" );

    udb.undo_end();
    BOOST_REQUIRE( ao.check< account_index >() );

    BOOST_TEST_MESSAGE( "--- 1 object( create, modify )" );
    ao.remember_old_values< account_index >();
    udb.undo_begin();

    const auto& obj1 = ao.create( "name00", time );
    BOOST_REQUIRE( std::string( obj1.get_name() ) == "name00" );
    const auto& obj2 = ao.modify( obj1, [&]( account_object& obj ){ obj.set_name( "name01" ); } );
    BOOST_REQUIRE( std::string( obj2.get_name() ) == "name01" );

    udb.undo_end();
    BOOST_REQUIRE( ao.check< account_index >() );

    BOOST_TEST_MESSAGE( "--- 1 object( create, remove )" );
    ao.remember_old_values< account_index >();
    udb.undo_begin();

    const auto& obj3 = ao.create( "name00", time );
    ao.remove( obj3 );
    ao.remove<tiny_account_index>( obj3.get_name() );

    udb.undo_end();
    BOOST_REQUIRE( ao.check< account_index >() );

    BOOST_TEST_MESSAGE( "--- 1 object( create, modify, remove )" );
    ao.remember_old_values< account_index >();
    udb.undo_begin();

    const auto& obj4 = ao.create( "name00", time );
    ao.modify( obj4, [&]( account_object& obj ){ obj.set_proxy(pxy); } );
    ao.remove( obj4 );
    ao.remove<tiny_account_index>( obj4.get_name() );

    udb.undo_end();
    BOOST_REQUIRE( ao.check< account_index >() );

    BOOST_TEST_MESSAGE( "--- 1 object( create, remove, create )" );
    ao.remember_old_values< account_index >();
    udb.undo_begin();

    const auto& obj5 = ao.create( "name00", time );
    ao.remove( obj5 );
    ao.remove<tiny_account_index>( obj5.get_name() );
    ao.create( "name00", time );

    udb.undo_end();
    BOOST_REQUIRE( ao.check< account_index >() );

    BOOST_TEST_MESSAGE( "--- 1 object( create, modify, remove, create )" );
    ao.remember_old_values< account_index >();
    udb.undo_begin();

    const auto& obj6 = ao.create( "name00", time );
    ao.modify( obj6, [&]( account_object& obj ){ obj.set_proxy(pxy); } );
    ao.remove( obj6 );
    ao.remove<tiny_account_index>( obj6.get_name() );
    ao.create( "name00", time );

    udb.undo_end();
    BOOST_REQUIRE( ao.check< account_index >() );

    BOOST_TEST_MESSAGE( "--- 3 objects( create, create/modify, create/remove )" );
    ao.remember_old_values< account_index >();
    udb.undo_begin();

    const auto& obj_c = ao.create( "name00", time );
    BOOST_REQUIRE( std::string( obj_c.get_name() ) == "name00" );

    const auto& obj_cm = ao.create( "name01", time );
    BOOST_REQUIRE( std::string( obj_cm.get_name() ) == "name01" );
    ao.modify( obj_cm, [&]( account_object& obj ){ obj.set_name( "name02" ); } );
    BOOST_REQUIRE( std::string( obj_cm.get_name() ) == "name02" );

    const auto& obj_cr = ao.create( "name03", time );
    BOOST_REQUIRE( std::string( obj_cr.get_name() ) == "name03" );
    ao.remove( obj_cr );
    ao.remove<tiny_account_index>( obj_cr.get_name() );

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

    const auto& pxy0 = ao.create( "proxy00", time );
    const auto& pxy1 = ao.create( "proxy01", time );

    uint32_t old_size = ao.size< account_index >();

    const auto& obj0 = ao.create( "name00", time ); ao.modify( obj0, [&]( account_object& obj ){ obj.set_proxy(pxy0); } );
    BOOST_REQUIRE( old_size + 1 == ao.size< account_index >() );

    const auto& obj1 = ao.create( "name01", time ); ao.modify( obj1, [&]( account_object& obj ){ obj.set_proxy(pxy1); } );
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

    const auto& obj0 = ao.create( "name00", time );

    ao.remember_old_values< account_index >();
    udb.undo_begin();

    uint32_t old_size = ao.size< account_index >();

    ao.modify( obj0, [&]( account_object& obj ){ obj.set_post_count( 1 ); } );
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

    const auto& obja0 = ao.create( "name00", time );
    BOOST_REQUIRE( std::string( obja0.get_name() ) == "name00" );
    BOOST_REQUIRE( old_size_ao + 1 == ao.size< account_index >() );

    const comment_object& objc0 = co.create( fake_account_object, "11", fake_parent_comment );
    BOOST_CHECK_EQUAL( objc0.get_author_and_permlink_hash(), comment_object::compute_author_and_permlink_hash( fake_account_object.get_account_id(), "11" ) );
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

    const auto& pxy = ao.create( "name00", time );
    const auto& obja1 = ao.create( "name01", time );
    const auto& obja2 = ao.create( "name02", time );
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
    const auto& ao1 = ao.create( std::to_string(0), time );

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

BOOST_AUTO_TEST_CASE( undo_state_on_squash_and_undo )
{
  try
  {
    BOOST_TEST_MESSAGE( "--- Testing changes to undo state with regular call to squash and undo" );

    // generate couple blocks to push revision up
    generate_blocks( 2 * HIVE_MAX_WITNESSES );
    auto revision_on_empty = db->revision();
    ilog( "Undo revision is ${r}", ( "r", revision_on_empty ) );
    db->commit( revision_on_empty );
    db->clear_pending();
    ilog( "Undo stack is now empty" );
    BOOST_REQUIRE_EQUAL( revision_on_empty, db->revision() );
    auto time = db->head_block_time();

    // since we are not pushing new blocks nor transactions we can use just any index for
    // testing modifications - we are going to use transaction_index
    auto& index = db->get_index< transaction_index >();
    auto next_id = index.get_next_id();
    ilog( "Revision inside specific index is the same as global (${r}), next_id is ${n}",
      ( "r", index.revision() )( "n", next_id ) );
    BOOST_REQUIRE_EQUAL( index.revision(), revision_on_empty );

    auto create = [&]( const std::string& seed ) -> const transaction_object&
    {
      return db->create<transaction_object>( [&]( transaction_object& transaction )
      {
        transaction.trx_id = transaction_id_type::hash( seed );
        transaction.expiration = time;
      } );
    };
    auto modify = [&]( transaction_object_id_type i, const time_point_sec& time )
    {
      db->modify( index.get( i ), [&]( transaction_object& transaction )
      {
        transaction.expiration = time;
      } );
    };
    auto remove = [&]( transaction_object_id_type i )
    {
      db->remove( index.get( i ) );
    };

    // create couple out-of-undo objects to test modifications and removals on
    const int DEPTH = 3;
    std::array<transaction_object_id_type, DEPTH> to_modify;
    std::array<transaction_object_id_type, DEPTH> to_remove;
    std::array<transaction_object_id_type, DEPTH> created;
    int count = 0;
    for( int i = 0; i < DEPTH; ++i )
    {
      to_modify[i] = create( "mod" + std::to_string(i) ).get_id();
      to_remove[i] = create( "rem" + std::to_string(i) ).get_id();
    };
    {
      auto next_id_after_create = index.get_next_id();
      ilog( "Index revision did not change with creation of objects (${r}), but next_id increased (${n})",
        ( "r", index.revision() )( "n", next_id_after_create ) );
      BOOST_REQUIRE_EQUAL( index.revision(), revision_on_empty );
      BOOST_REQUIRE_EQUAL( next_id + 2 * DEPTH, next_id_after_create );
      next_id = next_id_after_create;
    }

    auto saved_next_id = next_id;
    {
      auto base_session = db->start_undo_session();
      auto revision_with_base_session = db->revision();
      ilog( "Creation of first session increases revision (${r}), does not change next_id (${n})",
        ( "r", revision_with_base_session )( "n", index.get_next_id() ) );
      BOOST_REQUIRE_EQUAL( revision_on_empty + 1, revision_with_base_session );
      BOOST_REQUIRE_EQUAL( next_id, index.get_next_id() );
      created[ count ] = create( "new" + std::to_string( count ) ).get_id();
      modify( to_modify[ count ], time + HIVE_BLOCK_INTERVAL * count );
      remove( to_remove[ count ] );
      ++count;
      ilog( "Index revision did not change with creation of objects (${r}), but next_id increased (${n})",
        ( "r", index.revision() )( "n", index.get_next_id() ) );
      BOOST_REQUIRE_EQUAL( next_id + 1, index.get_next_id() );
      ++next_id;
      // test squash of nested session into underlying session
      {
        auto nested_session = db->start_undo_session();
        auto revision_with_nested_session = db->revision();
        ilog( "Creation of nested session increases revision (${r}), does not change next_id (${n})",
          ( "r", revision_with_nested_session )( "n", index.get_next_id() ) );
        BOOST_REQUIRE_EQUAL( revision_with_base_session + 1, revision_with_nested_session );
        BOOST_REQUIRE_EQUAL( next_id, index.get_next_id() );
        created[ count ] = create( "new" + std::to_string( count ) ).get_id();
        modify( to_modify[ count ], time + HIVE_BLOCK_INTERVAL * count );
        remove( to_remove[ count ] );
        ++count;
        ilog( "Index revision did not change with creation of objects (${r}), but next_id increased (${n})",
          ( "r", index.revision() )( "n", index.get_next_id() ) );
        BOOST_REQUIRE_EQUAL( next_id + 1, index.get_next_id() );
        ++next_id;
        nested_session.squash();
        auto revision_after_nested_squash = db->revision();
        ilog( "Squashing nested session into underlying session reduces revision (${r}), does not change next_id (${n})",
          ( "r", revision_after_nested_squash )( "n", index.get_next_id() ) );
        BOOST_REQUIRE_EQUAL( revision_with_base_session, revision_after_nested_squash );
        BOOST_REQUIRE_EQUAL( next_id, index.get_next_id() );
      }
      auto revision_after_nested_removed = db->revision();
      ilog( "Removal of already squashed nested session does not change revision (${r}) nor next_id (${n})",
        ( "r", revision_after_nested_removed )( "n", index.get_next_id() ) );
      BOOST_REQUIRE_EQUAL( revision_with_base_session, revision_after_nested_removed );
      BOOST_REQUIRE_EQUAL( next_id, index.get_next_id() );
      ilog( "Objects from first and squashed nested session are in their modified state" );
      for( int i = count - 1; i >= 0; --i )
      {
        BOOST_REQUIRE( index.find( created[ i ] ) != nullptr );
        BOOST_REQUIRE( index.get( to_modify[ i ] ).expiration == time + HIVE_BLOCK_INTERVAL * i );
        BOOST_REQUIRE( index.find( to_remove[ i ] ) == nullptr );
      }
      saved_next_id = next_id;
      // test explicit undo of nested session
      {
        auto nested_session = db->start_undo_session();
        auto revision_with_nested_session = db->revision();
        ilog( "Creation of nested session increases revision (${r}), does not change next_id (${n})",
          ( "r", revision_with_nested_session )( "n", index.get_next_id() ) );
        BOOST_REQUIRE_EQUAL( revision_with_base_session + 1, revision_with_nested_session );
        BOOST_REQUIRE_EQUAL( next_id, index.get_next_id() );
        created[ count ] = create( "new" + std::to_string( count ) ).get_id();
        modify( to_modify[ count ], time + HIVE_BLOCK_INTERVAL * count );
        remove( to_remove[ count ] );
        ++count;
        ilog( "Index revision did not change with creation of objects (${r}), but next_id increased (${n})",
          ( "r", index.revision() )( "n", index.get_next_id() ) );
        BOOST_REQUIRE_EQUAL( next_id + 1, index.get_next_id() );
        ++next_id;
        nested_session.undo();
        auto revision_after_nested_undo = db->revision();
        ilog( "Undoing nested session reduces revision (${r}), reverts next_id to value from start of session (${n})",
          ( "r", revision_after_nested_undo )( "n", index.get_next_id() ) );
        BOOST_REQUIRE_EQUAL( revision_with_base_session, revision_after_nested_undo );
        BOOST_REQUIRE_EQUAL( saved_next_id, index.get_next_id() );
        next_id = saved_next_id;
        ilog( "Last one newly created object is not accessible, modified and deleted are restored" );
        for( int i = 0; i < 1; ++i )
        {
          --count;
          BOOST_REQUIRE( index.find( created[ count ] ) == nullptr );
          BOOST_REQUIRE( index.get( to_modify[ count ] ).expiration == time );
          BOOST_REQUIRE( index.find( to_remove[ count ] ) != nullptr );
        }
      }
      revision_after_nested_removed = db->revision();
      ilog( "Removal of already undone nested session does not change revision (${r}) nor next_id (${n})",
        ( "r", revision_after_nested_removed )( "n", index.get_next_id() ) );
      BOOST_REQUIRE_EQUAL( revision_with_base_session, revision_after_nested_removed );
      BOOST_REQUIRE_EQUAL( next_id, index.get_next_id() );
      ilog( "Objects from first and squashed nested session are still in their modified state" );
      for( int i = count - 1; i >= 0; --i )
      {
        BOOST_REQUIRE( index.find( created[ i ] ) != nullptr );
        BOOST_REQUIRE( index.get( to_modify[ i ] ).expiration == time + HIVE_BLOCK_INTERVAL * i );
        BOOST_REQUIRE( index.find( to_remove[ i ] ) == nullptr );
      }
      // test implicit undo of nested session
      {
        auto nested_session = db->start_undo_session();
        auto revision_with_nested_session = db->revision();
        ilog( "Creation of nested session increases revision (${r}), does not change next_id (${n})",
          ( "r", revision_with_nested_session )( "n", index.get_next_id() ) );
        BOOST_REQUIRE_EQUAL( revision_with_base_session + 1, revision_with_nested_session );
        BOOST_REQUIRE_EQUAL( next_id, index.get_next_id() );
        auto new_created_id = create( "new" + std::to_string( count ) ).get_id();
        BOOST_REQUIRE_EQUAL( created[ count ], new_created_id ); // reuse id from previously uncreated object
        modify( to_modify[ count ], time + HIVE_BLOCK_INTERVAL * count );
        remove( to_remove[ count ] );
        ++count;
        ilog( "Index revision did not change with creation of objects (${r}), but next_id increased (${n})",
          ( "r", index.revision() )( "n", index.get_next_id() ) );
        BOOST_REQUIRE_EQUAL( next_id + 1, index.get_next_id() );
        ++next_id;
      }
      revision_after_nested_removed = db->revision();
      ilog( "Removal of live nested session reduces revision (${r}), reverts next_id to value from start of session (${n})",
        ( "r", revision_after_nested_removed )( "n", index.get_next_id() ) );
      BOOST_REQUIRE_EQUAL( revision_with_base_session, revision_after_nested_removed );
      BOOST_REQUIRE_EQUAL( saved_next_id, index.get_next_id() );
      next_id = saved_next_id;
      ilog( "Last one newly created object is not accessible, modified and deleted are restored" );
      for( int i = 0; i < 1; ++i )
      {
        --count;
        BOOST_REQUIRE( index.find( created[ count ] ) == nullptr );
        BOOST_REQUIRE( index.get( to_modify[ count ] ).expiration == time );
        BOOST_REQUIRE( index.find( to_remove[ count ] ) != nullptr );
      }
      ilog( "Objects from first and squashed nested session are in their modified state" );
      for( int i = count - 1; i >= 0; --i )
      {
        BOOST_REQUIRE( index.find( created[ i ] ) != nullptr );
        BOOST_REQUIRE( index.get( to_modify[ i ] ).expiration == time + HIVE_BLOCK_INTERVAL * i );
        BOOST_REQUIRE( index.find( to_remove[ i ] ) == nullptr );
      }
      // test squash of the only session into committed state
      base_session.squash();
      auto revision_after_base_squash = db->revision();
      ilog( "Squashing the only session (equivalent of commit) does not change revision (${r}) nor next_id (${n})",
        ( "r", revision_after_base_squash )( "n", index.get_next_id() ) );
      BOOST_REQUIRE_EQUAL( revision_with_base_session, revision_after_base_squash );
      BOOST_REQUIRE_EQUAL( next_id, index.get_next_id() );
    }
    auto revision_after_base_removed = db->revision();
    ilog( "Removal of already squashed the only session does not change revision (${r}) nor next_id (${n})",
      ( "r", revision_after_base_removed )( "n", index.get_next_id() ) );
    BOOST_REQUIRE_EQUAL( revision_on_empty + 1, revision_after_base_removed );
    BOOST_REQUIRE_EQUAL( next_id, index.get_next_id() );
    ilog( "Objects from first and squashed nested session are still in their modified state" );
    for( int i = count - 1; i >= 0; --i )
    {
      BOOST_REQUIRE( index.find( created[ i ] ) != nullptr );
      BOOST_REQUIRE( index.get( to_modify[ i ] ).expiration == time + HIVE_BLOCK_INTERVAL * i );
      BOOST_REQUIRE( index.find( to_remove[ i ] ) == nullptr );
    }

    // since we have no way to reduce revision now, we are correcting revision_on_empty
    ++revision_on_empty;
    saved_next_id = next_id;
    // test explicit undo of the only session
    {
      auto base_session = db->start_undo_session();
      auto revision_with_base_session = db->revision();
      ilog( "Creation of first session increases revision (${r}), does not change next_id (${n})",
        ( "r", revision_with_base_session )( "n", index.get_next_id() ) );
      BOOST_REQUIRE_EQUAL( revision_on_empty + 1, revision_with_base_session );
      BOOST_REQUIRE_EQUAL( next_id, index.get_next_id() );
      auto new_created_id = create( "new" + std::to_string( count ) ).get_id();
      BOOST_REQUIRE_EQUAL( created[ count ], new_created_id ); // reuse id from previously uncreated object
      modify( to_modify[ count ], time + HIVE_BLOCK_INTERVAL * count );
      remove( to_remove[ count ] );
      ++count;
      ilog( "Index revision did not change with creation of objects (${r}), but next_id increased (${n})",
        ( "r", index.revision() )( "n", index.get_next_id() ) );
      BOOST_REQUIRE_EQUAL( next_id + 1, index.get_next_id() );
      ++next_id;
      base_session.undo();
      auto revision_after_base_undo = db->revision();
      ilog( "Undoing the only session reduces revision (${r}), reverts next_id to value from start of session (${n})",
        ( "r", revision_after_base_undo )( "n", index.get_next_id() ) );
      BOOST_REQUIRE_EQUAL( revision_on_empty, revision_after_base_undo );
      BOOST_REQUIRE_EQUAL( saved_next_id, index.get_next_id() );
      next_id = saved_next_id;
      ilog( "Last one newly created object is not accessible, modified and deleted are restored" );
      for( int i = 0; i < 1; ++i )
      {
        --count;
        BOOST_REQUIRE( index.find( created[ count ] ) == nullptr );
        BOOST_REQUIRE( index.get( to_modify[ count ] ).expiration == time );
        BOOST_REQUIRE( index.find( to_remove[ count ] ) != nullptr );
      }
    }
    revision_after_base_removed = db->revision();
    ilog( "Removal of already undone the only session does not change revision (${r}) nor next_id (${n})",
      ( "r", revision_after_base_removed )( "n", index.get_next_id() ) );
    BOOST_REQUIRE_EQUAL( revision_on_empty, revision_after_base_removed );
    BOOST_REQUIRE_EQUAL( next_id, index.get_next_id() );
    ilog( "All previously effectively committed changes are still there" );
    for( int i = count - 1; i >= 0; --i )
    {
      BOOST_REQUIRE( index.find( created[ i ] ) != nullptr );
      BOOST_REQUIRE( index.get( to_modify[ i ] ).expiration == time + HIVE_BLOCK_INTERVAL * i );
      BOOST_REQUIRE( index.find( to_remove[ i ] ) == nullptr );
    }

    // test implicit undo of the only session
    {
      auto base_session = db->start_undo_session();
      auto revision_with_base_session = db->revision();
      ilog( "Creation of first session increases revision (${r}), does not change next_id (${n})",
        ( "r", revision_with_base_session )( "n", index.get_next_id() ) );
      BOOST_REQUIRE_EQUAL( revision_on_empty + 1, revision_with_base_session );
      BOOST_REQUIRE_EQUAL( next_id, index.get_next_id() );
      auto new_created_id = create( "new" + std::to_string( count ) ).get_id();
      BOOST_REQUIRE_EQUAL( created[ count ], new_created_id ); // reuse id from previously uncreated object
      modify( to_modify[ count ], time + HIVE_BLOCK_INTERVAL * count );
      remove( to_remove[ count ] );
      ++count;
      ilog( "Index revision did not change with creation of objects (${r}), but next_id increased (${n})",
        ( "r", index.revision() )( "n", index.get_next_id() ) );
      BOOST_REQUIRE_EQUAL( next_id + 1, index.get_next_id() );
      ++next_id;
    }
    revision_after_base_removed = db->revision();
    ilog( "Removal of the only live session reduces revision (${r}), reverts next_id to value from start of session (${n})",
      ( "r", revision_after_base_removed )( "n", index.get_next_id() ) );
    BOOST_REQUIRE_EQUAL( revision_on_empty, revision_after_base_removed );
    BOOST_REQUIRE_EQUAL( saved_next_id, index.get_next_id() );
    next_id = saved_next_id;
    ilog( "Last one newly created object is not accessible, modified and deleted are restored" );
    for( int i = 0; i < 1; ++i )
    {
      --count;
      BOOST_REQUIRE( index.find( created[ count ] ) == nullptr );
      BOOST_REQUIRE( index.get( to_modify[ count ] ).expiration == time );
      BOOST_REQUIRE( index.find( to_remove[ count ] ) != nullptr );
    }
    ilog( "All previously effectively committed changes are still there" );
    for( int i = count - 1; i >= 0; --i )
    {
      BOOST_REQUIRE( index.find( created[ i ] ) != nullptr );
      BOOST_REQUIRE( index.get( to_modify[ i ] ).expiration == time + HIVE_BLOCK_INTERVAL * i );
      BOOST_REQUIRE( index.find( to_remove[ i ] ) == nullptr );
    }
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( undo_state_on_push_and_commit )
{
  try
  {
    BOOST_TEST_MESSAGE( "--- Testing changes to undo state with call to push and commit" );

    // generate couple blocks to push revision up
    generate_blocks( 2 * HIVE_MAX_WITNESSES );
    auto start_revision = db->revision();
    ilog( "Undo revision is ${r}", ( "r", start_revision ) );
    db->commit( start_revision );
    db->clear_pending();
    ilog( "Undo stack is now empty" );
    BOOST_REQUIRE_EQUAL( start_revision, db->revision() );
    int stack_size = 0;
    auto time = db->head_block_time();

    // since we are not pushing new blocks nor transactions we can use just any index for
    // testing modifications - we are going to use transaction_index
    auto& index = db->get_index< transaction_index >();
    auto next_id = index.get_next_id();
    ilog( "Revision inside specific index is the same as global (${r}), next_id is ${n}",
      ( "r", index.revision() )( "n", next_id ) );
    BOOST_REQUIRE_EQUAL( index.revision(), start_revision );

    auto create = [&]( const std::string& seed ) -> const transaction_object&
    {
      return db->create<transaction_object>( [&]( transaction_object& transaction )
      {
        transaction.trx_id = transaction_id_type::hash( seed );
        transaction.expiration = time;
      } );
    };
    auto modify = [&]( transaction_object_id_type i, const time_point_sec& time )
    {
      db->modify( index.get( i ), [&]( transaction_object& transaction )
      {
        transaction.expiration = time;
      } );
    };
    auto remove = [&]( transaction_object_id_type i )
    {
      db->remove( index.get( i ) );
    };

    // create couple out-of-undo objects to test modifications and removals on
    const int DEPTH = 22;
    std::array<transaction_object_id_type, DEPTH> to_modify;
    std::array<transaction_object_id_type, DEPTH> to_remove;
    std::array<transaction_object_id_type, DEPTH> created;
    int count = 0;
    for( int i = 0; i < DEPTH; ++i )
    {
      to_modify[i] = create( "mod" + std::to_string(i) ).get_id();
      to_remove[i] = create( "rem" + std::to_string(i) ).get_id();
    };
    {
      auto next_id_after_create = index.get_next_id();
      ilog( "Index revision did not change with creation of objects (${r}), but next_id increased (${n})",
        ( "r", index.revision() )( "n", next_id_after_create ) );
      BOOST_REQUIRE_EQUAL( index.revision(), start_revision );
      BOOST_REQUIRE_EQUAL( next_id + 2 * DEPTH, next_id_after_create );
      next_id = next_id_after_create;
    }

    // check push
    {
      auto session = db->start_undo_session();
      auto revision_with_session = db->revision();
      ilog( "Creation of session increases revision (${r}), does not change next_id (${n})",
        ( "r", revision_with_session )( "n", index.get_next_id() ) );
      BOOST_REQUIRE_EQUAL( start_revision + 1, revision_with_session );
      BOOST_REQUIRE_EQUAL( next_id, index.get_next_id() );
      created[ count ] = create( "new" + std::to_string( count ) ).get_id();
      modify( to_modify[ count ], time + HIVE_BLOCK_INTERVAL * count );
      remove( to_remove[ count ] );
      ++count;
      ilog( "Index revision did not change with creation of objects (${r}), but next_id increased (${n})",
        ( "r", index.revision() )( "n", index.get_next_id() ) );
      BOOST_REQUIRE_EQUAL( next_id + 1, index.get_next_id() );
      ++next_id;
      session.push();
      ++stack_size;
      auto revision_after_push = db->revision();
      ilog( "Pushing session does not change revision (${r}) nor next_id (${n})",
        ( "r", revision_after_push )( "n", index.get_next_id() ) );
      BOOST_REQUIRE_EQUAL( revision_with_session, revision_after_push );
      BOOST_REQUIRE_EQUAL( next_id, index.get_next_id() );
    }
    auto revision_after_session_removed = db->revision();
    ilog( "Removing pushed session does not change revision (${r}) nor next_id (${n})",
      ( "r", revision_after_session_removed )( "n", index.get_next_id() ) );
    BOOST_REQUIRE_EQUAL( start_revision + stack_size, revision_after_session_removed );
    BOOST_REQUIRE_EQUAL( next_id, index.get_next_id() );

    // create stack of sessions
    auto saved_next_id = next_id;
    for( int i = 0; i < 20; ++i )
    {
      auto session = db->start_undo_session();
      created[ count ] = create( "new" + std::to_string( count ) ).get_id();
      modify( to_modify[ count ], time + HIVE_BLOCK_INTERVAL * count );
      remove( to_remove[ count ] );
      ++count;
      if( i == 9 )
        saved_next_id = next_id;
      ++next_id;
      session.push();
      ++stack_size;
    }
    auto revision_with_stack = db->revision();
    ilog( "Revision is ${r} after 20 additional push() calls, next_id after 20 creations is ${n}",
      ( "r", revision_with_stack )( "n", index.get_next_id() ) );
    BOOST_REQUIRE_EQUAL( start_revision + stack_size, revision_with_stack );
    BOOST_REQUIRE_EQUAL( next_id, index.get_next_id() );

    auto last_commit = start_revision + 10;
    const int REVERSIBLE_COUNT = 11;
    BOOST_REQUIRE_EQUAL( start_revision + stack_size - last_commit, REVERSIBLE_COUNT );
    db->commit( last_commit );
    auto revision_after_commit = db->revision();
    ilog( "Commit to ${c} does not change revision (${r}) nor next_id (${n})",
      ( "c", last_commit )( "r", revision_after_commit )( "n", index.get_next_id() ) );
    BOOST_REQUIRE_EQUAL( start_revision + stack_size, revision_after_commit );
    BOOST_REQUIRE_EQUAL( next_id, index.get_next_id() );

    db->undo_all();
    auto revision_after_undo_all = db->revision();
    ilog( "Undo all reduces revision down to last commit (${r}), next_id is reverted to value from revision at last commit (${n})",
      ( "r", revision_after_undo_all )( "n", index.get_next_id() ) );
    BOOST_REQUIRE_EQUAL( last_commit, revision_after_undo_all );
    BOOST_REQUIRE_EQUAL( saved_next_id, index.get_next_id() );
    next_id = saved_next_id;
    ilog( "Last ${x} newly created objects are not accessible, modified and deleted are restored",
      ( "x", REVERSIBLE_COUNT ) );
    for( int i = 0; i < REVERSIBLE_COUNT; ++i )
    {
      --count;
      BOOST_REQUIRE( index.find( created[ count ] ) == nullptr );
      BOOST_REQUIRE( index.get( to_modify[ count ] ).expiration == time );
      BOOST_REQUIRE( index.find( to_remove[ count ] ) != nullptr );
    }
    ilog( "All previous objects that were in committed sessions are in their modified state" );
    for( int i = count - 1; i >= 0; --i )
    {
      BOOST_REQUIRE( index.find( created[ i ] ) != nullptr );
      BOOST_REQUIRE( index.get( to_modify[ i ] ).expiration == time + HIVE_BLOCK_INTERVAL * i );
      BOOST_REQUIRE( index.find( to_remove[ i ] ) == nullptr );
    }

    // move start revision to new place
    start_revision = last_commit;
    stack_size = 0;

    // create stack of sessions
    for( int i = 0; i < 3; ++i )
    {
      auto session = db->start_undo_session();
      session.push();
      ++stack_size;
    }
    revision_with_stack = db->revision();
    ilog( "Revision is ${r} after 3 additional push() calls", ( "r", revision_with_stack ) );
    BOOST_REQUIRE_EQUAL( start_revision + stack_size, revision_with_stack );

    last_commit = start_revision + stack_size;
    db->commit( last_commit );
    revision_after_commit = db->revision();
    ilog( "Commit to top revision does not change revision (${r})", ( "r", revision_after_commit ) );
    BOOST_REQUIRE_EQUAL( start_revision + stack_size, revision_after_commit );

    db->undo_all();
    revision_after_undo_all = db->revision();
    ilog( "Undo all on empty stack does not change revision (${r})", ( "r", revision_after_undo_all ) );
    BOOST_REQUIRE_EQUAL( last_commit, revision_after_undo_all );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( undo_state_on_squash_and_undo_with_keep_alive )
{
  try
  {
    BOOST_TEST_MESSAGE( "--- Testing changes to undo state with call to squash and undo with 'keep_alive'" );

    // generate couple blocks to push revision up
    generate_blocks( 2 * HIVE_MAX_WITNESSES );
    auto revision_on_empty = db->revision();
    ilog( "Undo revision is ${r}", ( "r", revision_on_empty ) );
    db->commit( revision_on_empty );
    db->clear_pending();
    ilog( "Undo stack is now empty" );
    BOOST_REQUIRE_EQUAL( revision_on_empty, db->revision() );
    auto time = db->head_block_time();

    // since we are not pushing new blocks nor transactions we can use just any index for
    // testing modifications - we are going to use transaction_index
    auto& index = db->get_index< transaction_index >();
    auto next_id = index.get_next_id();
    ilog( "Revision inside specific index is the same as global (${r}), next_id is ${n}",
      ( "r", index.revision() )( "n", next_id ) );
    BOOST_REQUIRE_EQUAL( index.revision(), revision_on_empty );

    auto create = [&]( const std::string& seed ) -> const transaction_object&
    {
      return db->create<transaction_object>( [&]( transaction_object& transaction )
      {
        transaction.trx_id = transaction_id_type::hash( seed );
        transaction.expiration = time;
      } );
    };
    auto modify = [&]( transaction_object_id_type i, const time_point_sec& time )
    {
      db->modify( index.get( i ), [&]( transaction_object& transaction )
      {
        transaction.expiration = time;
      } );
    };
    auto remove = [&]( transaction_object_id_type i )
    {
      db->remove( index.get( i ) );
    };

    // create couple out-of-undo objects to test modifications and removals on
    const int DEPTH = 6;
    std::array<transaction_object_id_type, DEPTH> to_modify;
    std::array<transaction_object_id_type, DEPTH> to_remove;
    std::array<transaction_object_id_type, DEPTH> created;
    int count = 0;
    for( int i = 0; i < DEPTH; ++i )
    {
      to_modify[i] = create( "mod" + std::to_string(i) ).get_id();
      to_remove[i] = create( "rem" + std::to_string(i) ).get_id();
    };
    {
      auto next_id_after_create = index.get_next_id();
      ilog( "Index revision did not change with creation of objects (${r}), but next_id increased (${n})",
        ( "r", index.revision() )( "n", next_id_after_create ) );
      BOOST_REQUIRE_EQUAL( index.revision(), revision_on_empty );
      BOOST_REQUIRE_EQUAL( next_id + 2 * DEPTH, next_id_after_create );
      next_id = next_id_after_create;
    }

    auto saved_next_id = next_id;
    // open base session
    {
      auto base_session = db->start_undo_session();
      auto revision_with_base_session = db->revision();
      ilog( "Creation of first session increases revision (${r}), does not change next_id (${n})",
        ( "r", revision_with_base_session )( "n", index.get_next_id() ) );
      BOOST_REQUIRE_EQUAL( revision_on_empty + 1, revision_with_base_session );
      BOOST_REQUIRE_EQUAL( next_id, index.get_next_id() );
      // create and change objects within the only session
      created[ count ] = create( "new" + std::to_string( count ) ).get_id();
      modify( to_modify[ count ], time + HIVE_BLOCK_INTERVAL * count );
      remove( to_remove[ count ] );
      ++count;
      ilog( "Index revision did not change with creation of objects (${r}), but next_id increased (${n})",
        ( "r", index.revision() )( "n", index.get_next_id() ) );
      BOOST_REQUIRE_EQUAL( next_id + 1, index.get_next_id() );
      ++next_id;
      // test undo with 'keep_alive' on the only session
      base_session.undo( true );
      auto revision_after_base_undo_alive = db->revision();
      ilog( "Undoing of the only session with 'keep_alive' (equivalent of undo and new session) does not change revision (${r}), reverts next_id to value from start of session (${n})",
        ( "r", revision_after_base_undo_alive )( "n", index.get_next_id() ) );
      BOOST_REQUIRE_EQUAL( revision_with_base_session, revision_after_base_undo_alive );
      BOOST_REQUIRE_EQUAL( saved_next_id, index.get_next_id() );
      next_id = saved_next_id;
      ilog( "Last one newly created object is not accessible, modified and deleted are restored" );
      for( int i = 0; i < 1; ++i )
      {
        --count;
        BOOST_REQUIRE( index.find( created[ count ] ) == nullptr );
        BOOST_REQUIRE( index.get( to_modify[ count ] ).expiration == time );
        BOOST_REQUIRE( index.find( to_remove[ count ] ) != nullptr );
      }
      // create and change objects again within reused session
      created[ count ] = create( "new" + std::to_string( count ) ).get_id();
      modify( to_modify[ count ], time + HIVE_BLOCK_INTERVAL * count );
      remove( to_remove[ count ] );
      ++count;
      ilog( "Index revision did not change with creation of objects (${r}), but next_id increased (${n})",
        ( "r", index.revision() )( "n", index.get_next_id() ) );
      BOOST_REQUIRE_EQUAL( next_id + 1, index.get_next_id() );
      ++next_id;
      // test undo with 'keep_alive' on the only reused session
      base_session.undo( true );
      revision_after_base_undo_alive = db->revision();
      ilog( "You can undo with 'keep_alive' multiple times on the same base session (${r}, ${n})",
        ( "r", revision_after_base_undo_alive )( "n", index.get_next_id() ) );
      BOOST_REQUIRE_EQUAL( revision_with_base_session, revision_after_base_undo_alive );
      BOOST_REQUIRE_EQUAL( saved_next_id, index.get_next_id() );
      next_id = saved_next_id;
      ilog( "Last one newly created object is not accessible, modified and deleted are restored" );
      for( int i = 0; i < 1; ++i )
      {
        --count;
        BOOST_REQUIRE( index.find( created[ count ] ) == nullptr );
        BOOST_REQUIRE( index.get( to_modify[ count ] ).expiration == time );
        BOOST_REQUIRE( index.find( to_remove[ count ] ) != nullptr );
      }
      // create and change objects yet again within reused session
      created[ count ] = create( "new" + std::to_string( count ) ).get_id();
      modify( to_modify[ count ], time + HIVE_BLOCK_INTERVAL * count );
      remove( to_remove[ count ] );
      ++count;
      ilog( "Index revision did not change with creation of objects (${r}), but next_id increased (${n})",
        ( "r", index.revision() )( "n", index.get_next_id() ) );
      BOOST_REQUIRE_EQUAL( next_id + 1, index.get_next_id() );
      ++next_id;
      // test squash with 'keep_alive' on the only session into committed state
      base_session.squash( true );
      auto revision_after_base_squash_alive = db->revision();
      ilog( "Squashing of the only session with 'keep_alive' (equivalent of commit and new session) increases revision (${r}), solidifies next_id after last created object (${n})",
        ( "r", revision_after_base_squash_alive )( "n", index.get_next_id() ) );
      BOOST_REQUIRE_EQUAL( revision_with_base_session + 1, revision_after_base_squash_alive );
      BOOST_REQUIRE_EQUAL( next_id, index.get_next_id() );
      ++revision_with_base_session; ++revision_on_empty; // move start revision since we have effective commit
      saved_next_id = next_id; // also current next_id becomes baseline since last created object was effectively moved to permanent state
      // create and change another layer of objects within reused session
      created[ count ] = create( "new" + std::to_string( count ) ).get_id();
      modify( to_modify[ count ], time + HIVE_BLOCK_INTERVAL * count );
      remove( to_remove[ count ] );
      ++count;
      ilog( "Index revision did not change with creation of objects (${r}), but next_id increased (${n})",
        ( "r", index.revision() )( "n", index.get_next_id() ) );
      BOOST_REQUIRE_EQUAL( next_id + 1, index.get_next_id() );
      ++next_id;
      // test secondary squash with 'keep_alive' on the only session into committed state
      base_session.squash( true );
      revision_after_base_squash_alive = db->revision();
      ilog( "You can squash with 'keep_alive' multiple times on the same base session (${r}), next_id won't change (${n})",
        ( "r", revision_after_base_squash_alive )( "n", index.get_next_id() ) );
      BOOST_REQUIRE_EQUAL( revision_with_base_session + 1, revision_after_base_squash_alive );
      ++revision_with_base_session; ++revision_on_empty; // like above
      saved_next_id = next_id;
      // create and change third layer of objects within reused session
      created[ count ] = create( "new" + std::to_string( count ) ).get_id();
      modify( to_modify[ count ], time + HIVE_BLOCK_INTERVAL * count );
      remove( to_remove[ count ] );
      ++count;
      ilog( "Index revision did not change with creation of objects (${r}), but next_id increased (${n})",
        ( "r", index.revision() )( "n", index.get_next_id() ) );
      BOOST_REQUIRE_EQUAL( next_id + 1, index.get_next_id() );
      ++next_id;
      // open nested session
      auto saved_nested_next_id = next_id;
      {
        auto nested_session = db->start_undo_session();
        auto revision_with_nested_session = db->revision();
        ilog( "Creation of nested session increases revision (${r}), does not change next_id (${n})",
          ( "r", revision_with_nested_session )( "n", index.get_next_id() ) );
        BOOST_REQUIRE_EQUAL( revision_with_base_session + 1, revision_with_nested_session );
        BOOST_REQUIRE_EQUAL( next_id, index.get_next_id() );
        // create and change objects within nested session
        created[ count ] = create( "new" + std::to_string( count ) ).get_id();
        modify( to_modify[ count ], time + HIVE_BLOCK_INTERVAL * count );
        remove( to_remove[ count ] );
        ++count;
        ilog( "Index revision did not change with creation of objects (${r}), but next_id increased (${n})",
          ( "r", index.revision() )( "n", index.get_next_id() ) );
        BOOST_REQUIRE_EQUAL( next_id + 1, index.get_next_id() );
        ++next_id;
        // test undo with 'keep_alive' on nested session
        nested_session.undo( true );
        auto revision_after_nested_undo_alive = db->revision();
        ilog( "Undoing of nested session with 'keep_alive' (equivalent of undo and new session) does not change revision (${r}), reverts next_id to value from start of session (${n})",
          ( "r", revision_after_nested_undo_alive )( "n", index.get_next_id() ) );
        BOOST_REQUIRE_EQUAL( revision_with_nested_session, revision_after_nested_undo_alive );
        BOOST_REQUIRE_EQUAL( saved_nested_next_id, index.get_next_id() );
        next_id = saved_nested_next_id;
        ilog( "Last one newly created object is not accessible, modified and deleted are restored" );
        for( int i = 0; i < 1; ++i )
        {
          --count;
          BOOST_REQUIRE( index.find( created[ count ] ) == nullptr );
          BOOST_REQUIRE( index.get( to_modify[ count ] ).expiration == time );
          BOOST_REQUIRE( index.find( to_remove[ count ] ) != nullptr );
        }
        // create and change objects again within reused nested session
        created[ count ] = create( "new" + std::to_string( count ) ).get_id();
        modify( to_modify[ count ], time + HIVE_BLOCK_INTERVAL * count );
        remove( to_remove[ count ] );
        ++count;
        ilog( "Index revision did not change with creation of objects (${r}), but next_id increased (${n})",
          ( "r", index.revision() )( "n", index.get_next_id() ) );
        BOOST_REQUIRE_EQUAL( next_id + 1, index.get_next_id() );
        ++next_id;
        // test undo with 'keep_alive' on reused nested session
        nested_session.undo( true );
        revision_after_nested_undo_alive = db->revision();
        ilog( "You can undo with 'keep_alive' multiple times on the same nested session (${r}, ${n})",
          ( "r", revision_after_nested_undo_alive )( "n", index.get_next_id() ) );
        BOOST_REQUIRE_EQUAL( revision_with_nested_session, revision_after_nested_undo_alive );
        BOOST_REQUIRE_EQUAL( saved_nested_next_id, index.get_next_id() );
        next_id = saved_nested_next_id;
        ilog( "Last one newly created object is not accessible, modified and deleted are restored" );
        for( int i = 0; i < 1; ++i )
        {
          --count;
          BOOST_REQUIRE( index.find( created[ count ] ) == nullptr );
          BOOST_REQUIRE( index.get( to_modify[ count ] ).expiration == time );
          BOOST_REQUIRE( index.find( to_remove[ count ] ) != nullptr );
        }
        // create and change objects yet again within reused nested session
        created[ count ] = create( "new" + std::to_string( count ) ).get_id();
        modify( to_modify[ count ], time + HIVE_BLOCK_INTERVAL * count );
        remove( to_remove[ count ] );
        ++count;
        ilog( "Index revision did not change with creation of objects (${r}), but next_id increased (${n})",
          ( "r", index.revision() )( "n", index.get_next_id() ) );
        BOOST_REQUIRE_EQUAL( next_id + 1, index.get_next_id() );
        ++next_id;
        // test squash with 'keep_alive' on nested session into base session
        nested_session.squash( true );
        auto revision_after_nested_squash_alive = db->revision();
        ilog( "Squashing of nested session with 'keep_alive' (equivalent of squash and new session) does not change revision (${r}), does not change next_id (${n})",
          ( "r", revision_after_nested_squash_alive )( "n", index.get_next_id() ) );
        BOOST_REQUIRE_EQUAL( revision_with_nested_session, revision_after_nested_squash_alive );
        BOOST_REQUIRE_EQUAL( next_id, index.get_next_id() );
        // create and change another layer of objects within reused nested session
        created[ count ] = create( "new" + std::to_string( count ) ).get_id();
        modify( to_modify[ count ], time + HIVE_BLOCK_INTERVAL * count );
        remove( to_remove[ count ] );
        ++count;
        ilog( "Index revision did not change with creation of objects (${r}), but next_id increased (${n})",
          ( "r", index.revision() )( "n", index.get_next_id() ) );
        BOOST_REQUIRE_EQUAL( next_id + 1, index.get_next_id() );
        ++next_id;
        // test secondary squash with 'keep_alive' on nested session into base session
        nested_session.squash( true );
        revision_after_nested_squash_alive = db->revision();
        ilog( "You can squash with 'keep_alive' multiple times on the same nested session (${r}, ${n})",
          ( "r", revision_after_nested_squash_alive )( "n", index.get_next_id() ) );
        BOOST_REQUIRE_EQUAL( revision_with_nested_session, revision_after_nested_squash_alive );
        BOOST_REQUIRE_EQUAL( next_id, index.get_next_id() );
        saved_nested_next_id = next_id;
        // create and change third layer of objects within reused nested session
        created[ count ] = create( "new" + std::to_string( count ) ).get_id();
        modify( to_modify[ count ], time + HIVE_BLOCK_INTERVAL * count );
        remove( to_remove[ count ] );
        ++count;
        ilog( "Index revision did not change with creation of objects (${r}), but next_id increased (${n})",
          ( "r", index.revision() )( "n", index.get_next_id() ) );
        BOOST_REQUIRE_EQUAL( next_id + 1, index.get_next_id() );
        ++next_id;
      }
      // test implicit undo of nested session
      auto revision_after_nested_removed = db->revision();
      ilog( "Removal of live nested session reduces revision (${r}), restores next_id to value from before last change (${n})",
        ( "r", revision_after_nested_removed )( "n", index.get_next_id() ) );
      BOOST_REQUIRE_EQUAL( revision_with_base_session, revision_after_nested_removed );
      BOOST_REQUIRE_EQUAL( saved_nested_next_id, index.get_next_id() );
      next_id = saved_nested_next_id;
      ilog( "Last one newly created object is not accessible, modified and deleted are restored" );
      for( int i = 0; i < 1; ++i )
      {
        --count;
        BOOST_REQUIRE( index.find( created[ count ] ) == nullptr );
        BOOST_REQUIRE( index.get( to_modify[ count ] ).expiration == time );
        BOOST_REQUIRE( index.find( to_remove[ count ] ) != nullptr );
      }
      ilog( "All previous objects that were in squashed nested sessions and earlier are in their modified state" );
      // the objects are: two squashed from nested into base, one in base handled before nesting and two squashed into committed state
      BOOST_REQUIRE_EQUAL( count, 5 );
      for( int i = count - 1; i >= 0; --i )
      {
        BOOST_REQUIRE( index.find( created[ i ] ) != nullptr );
        BOOST_REQUIRE( index.get( to_modify[ i ] ).expiration == time + HIVE_BLOCK_INTERVAL * i );
        BOOST_REQUIRE( index.find( to_remove[ i ] ) == nullptr );
      }
    }
    // test implicit undo of base session
    auto revision_after_base_removed = db->revision();
    ilog( "Removal of live base session reduces revision (${r}), restores next_id to state from after last base squash (${n})",
      ( "r", revision_after_base_removed )( "n", index.get_next_id() ) );
    BOOST_REQUIRE_EQUAL( revision_on_empty, revision_after_base_removed );
    BOOST_REQUIRE_EQUAL( saved_next_id, index.get_next_id() );
    next_id = saved_next_id;
    ilog( "Last newly created object in base session and two squashed from nexted are not accessible, modified and deleted are restored" );
    for( int i = 0; i < 3; ++i )
    {
      --count;
      BOOST_REQUIRE( index.find( created[ count ] ) == nullptr );
      BOOST_REQUIRE( index.get( to_modify[ count ] ).expiration == time );
      BOOST_REQUIRE( index.find( to_remove[ count ] ) != nullptr );
    }
    ilog( "All previous objects that were squashed into committed state are in their modified state" );
    BOOST_REQUIRE_EQUAL( count, 2 );
    for( int i = count - 1; i >= 0; --i )
    {
      BOOST_REQUIRE( index.find( created[ i ] ) != nullptr );
      BOOST_REQUIRE( index.get( to_modify[ i ] ).expiration == time + HIVE_BLOCK_INTERVAL * i );
      BOOST_REQUIRE( index.find( to_remove[ i ] ) == nullptr );
    }
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( empty_undo_benchmark )
{
  try
  {
    BOOST_TEST_MESSAGE( "--- Benchmarking empty undo sessions in heavy traffic" );

    // generate couple blocks to push revision up
    generate_blocks( 2 * HIVE_MAX_WITNESSES );
    auto start_revision = db->revision();
    ilog( "Undo revision is ${r}", ( "r", start_revision ) );
    db->commit( start_revision );
    db->clear_pending();
    ilog( "Undo stack is now empty" );

    /*
    The test reflects (empty) undo sessions throughout 10 big blocks - we are measuring overhead of
    undo sessions alone, independent of what changes transactions and blocks would add filling up
    the sessions. The same scenario is repeated with use of 'keep_alive' for comparison.
    Before each block there is 10000 new transactions, 8000 are valid. Block is formed out of 7000,
    leaving 1000 in mempool, which means mempool will grow with each block, adding to time of
    reapplication. All pending transactions remain valid until they become part of block. One block is
    "generated" locally (5th one), all others are "from p2p". At 4th block 1st becomes irreversible,
    then at 7th 5th is irreversible, finally at 10th 10th becomes irreversible.
    */
    const int NEW_TX_BEFORE_BLOCK = 10000;
    const int INVALID_TX_FREQUENCY = 5;
    const int BLOCK_CAPACITY = 7000;

    auto start_time = fc::time_point::now();
    {
      int mempool_size = 0;
      fc::optional<chainbase::database::undo_session_guard> pending_tx_session;
      for( int block_num = 1; block_num <= 10; ++block_num )
      {
        // new transactions incoming
        for( int tx = 0; tx < NEW_TX_BEFORE_BLOCK; ++tx )
        {
          // database::_push_transaction start
          if( !pending_tx_session.valid() )
            pending_tx_session = db->start_undo_session();
          auto temp_session = db->start_undo_session();
          // TRANSACTION APPLIED HERE
          if( ( tx % INVALID_TX_FREQUENCY ) > 0 )
          {
            temp_session.squash();
            ++mempool_size;
          }
          // database::_push_transaction end
        }

        if( block_num == 5 ) // block produced locally
        {
          // block_producer::apply_pending_transactions start
          pending_tx_session.reset();
          pending_tx_session = db->start_undo_session();

          for( int tx = 0; tx < BLOCK_CAPACITY; ++tx )
          {
            auto temp_session = db->start_undo_session();
            // TRANSACTION APPLIED HERE
            temp_session.squash();
          }

          pending_tx_session.reset();
          // block_producer::apply_pending_transactions end
        }

        // apply block (even if produced locally)
        {
          // database::clear_pending called from detail::without_pending_transactions:
          pending_tx_session.reset();
          // database::apply_block_extended start
          auto block_session = db->start_undo_session();
          // BLOCK APPLIED HERE
          block_session.push();
          // database::apply_block_extended end
          // reapplication of pending called on exit from detail::without_pending_transactions:
          for( int tx = 0; tx < mempool_size; ++tx )
          {
            if( tx < BLOCK_CAPACITY )
            {
              --mempool_size; // transaction that is part of recent block
              continue;
            }
            // database::_push_transaction start
            if( !pending_tx_session.valid() )
              pending_tx_session = db->start_undo_session();
            auto temp_session = db->start_undo_session();
            // TRANSACTION APPLIED HERE
            temp_session.squash();
            // database::_push_transaction end
          }
        }

        // handle OBI transactions and set irreversible
        switch( block_num )
        {
        case 4: db->commit( 1 ); break;
        case 7: db->commit( 5 ); break;
        case 10: db->commit( 10 ); break;
        }
      }
    }

    fc::microseconds total_time = fc::time_point::now() - start_time;
    ilog( "Total time for undo session handling: ${t}µs, revision ${r}",
      ( "t", total_time )( "r", db->revision() ) );
    BOOST_REQUIRE_EQUAL( start_revision + 10, db->revision() );

    start_revision += 10;

    start_time = fc::time_point::now();
    {
      int mempool_size = 0;
      std::optional<std::pair<chainbase::database::undo_session_guard, chainbase::database::undo_session_guard>> pending_tx_session;
      for( int block_num = 1; block_num <= 10; ++block_num )
      {
        // new transactions incoming
        for( int tx = 0; tx < NEW_TX_BEFORE_BLOCK; ++tx )
        {
          // database::_push_transaction start
          if( !pending_tx_session.has_value() )
            pending_tx_session.emplace( db->start_undo_session(), db->start_undo_session() );
          // TRANSACTION APPLIED HERE
          if( ( tx % INVALID_TX_FREQUENCY ) > 0 )
          {
            pending_tx_session->second.squash( true );
            ++mempool_size;
          }
          else
          {
            pending_tx_session->second.undo( true );
          }
          // database::_push_transaction end
        }

        if( block_num == 5 ) // block produced locally
        {
          // block_producer::apply_pending_transactions start
          pending_tx_session.reset();
          pending_tx_session.emplace( db->start_undo_session(), db->start_undo_session() );

          for( int tx = 0; tx < BLOCK_CAPACITY; ++tx )
          {
            // TRANSACTION APPLIED HERE
            pending_tx_session->second.squash( true );
          }

          pending_tx_session.reset();
          // block_producer::apply_pending_transactions end
        }

        // apply block (even if produced locally)
        {
          // database::clear_pending called from detail::without_pending_transactions:
          pending_tx_session.reset();
          // database::apply_block_extended start
          auto block_session = db->start_undo_session();
          // BLOCK APPLIED HERE
          block_session.push();
          // database::apply_block_extended end
          // reapplication of pending called on exit from detail::without_pending_transactions:
          for( int tx = 0; tx < mempool_size; ++tx )
          {
            if( tx < BLOCK_CAPACITY )
            {
              --mempool_size; // transaction that is part of recent block
              continue;
            }
            // database::_push_transaction start
            if( !pending_tx_session.has_value() )
              pending_tx_session.emplace( db->start_undo_session(), db->start_undo_session() );
            // TRANSACTION APPLIED HERE
            pending_tx_session->second.squash( true );
            // database::_push_transaction end
          }
        }

        // handle OBI transactions and set irreversible
        switch( block_num )
        {
        case 4: db->commit( 1 ); break;
        case 7: db->commit( 5 ); break;
        case 10: db->commit( 10 ); break;
        }
      }
    }

    total_time = fc::time_point::now() - start_time;
    ilog( "Total time for undo session handling with 'keep_alive': ${t}µs, revision ${r}",
      ( "t", total_time )( "r", db->revision() ) );
    BOOST_REQUIRE_EQUAL( start_revision + 10, db->revision() );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( debug_update_undo_bug )
{
  try
  {
    BOOST_TEST_MESSAGE( "--- Test for undo related bug with debug_update" );

    ACTOR(alice)
    generate_block();

    const auto& alice_account = db->get_account( "alice" );
    BOOST_REQUIRE_EQUAL( alice_account.get_balance().amount.value, 0 );

    db->clear_pending();
    auto revision = db->revision();
    ilog( "Undo revision is ${r}", ( "r", revision ) );
    db->commit( revision );
    ilog( "Undo stack is now empty" );
    BOOST_REQUIRE_EQUAL( db->get_index< account_index >().get_undo_depth(), 0 );

    // increase claimed tokens when undo stack is empty
    BOOST_REQUIRE_EQUAL( alice_account.get_pending_claimed_accounts().value, 0 );
    db_plugin->debug_update( [&]( database& db )
    {
      db.modify( alice_account, [&]( account_object& a )
      {
        a.set_pending_claimed_accounts( a.get_pending_claimed_accounts() + 1 );
      } );
    } );
    BOOST_REQUIRE_EQUAL( alice_account.get_pending_claimed_accounts().value, 1 );
    generate_block();
    BOOST_REQUIRE_EQUAL( alice_account.get_pending_claimed_accounts().value, 1 );

    db->clear_pending();
    revision = db->revision();
    ilog( "Undo revision is ${r}", ( "r", revision ) );
    auto previous_revision = revision;
    ilog( "Undo stack contains one block session" );
    BOOST_REQUIRE_EQUAL( db->get_index< account_index >().get_undo_depth(), 1 );

    // increase claimed tokens when undo stack has block session
    BOOST_REQUIRE_EQUAL( alice_account.get_pending_claimed_accounts().value, 1 );
    db_plugin->debug_update( [&]( database& db )
    {
      db.modify( alice_account, [&]( account_object& a )
      {
        a.set_pending_claimed_accounts( a.get_pending_claimed_accounts() + 1 );
      } );
    } );
    BOOST_REQUIRE_EQUAL( alice_account.get_pending_claimed_accounts().value, 2 );
    generate_block();
    BOOST_REQUIRE_EQUAL( alice_account.get_pending_claimed_accounts().value, 2 );

    fund( "alice", ASSET( "1.000 TESTS" ) ); // this makes sure there is pending session open - normally
      // it could happen that it is open when there are some reapplied transactions, but not in unit tests
    BOOST_REQUIRE_EQUAL( alice_account.get_balance().amount.value, 1000 );
    revision = db->revision();
    ilog( "Undo revision is ${r}", ( "r", revision ) );
    ilog( "Undo stack contains block and pending tx sessions" );
    BOOST_REQUIRE_GT( revision, previous_revision + 1 ); // on top of one new revision from block session there should be pending tx related
    ++previous_revision;

    // increase claimed tokens when undo stack has pending tx session
    db_plugin->debug_update( [&]( database& db )
    {
      db.modify( alice_account, [&]( account_object& a )
      {
        a.set_pending_claimed_accounts( a.get_pending_claimed_accounts() + 1 );
      } );
    } );
    BOOST_REQUIRE_EQUAL( alice_account.get_pending_claimed_accounts().value, 3 );
    BOOST_REQUIRE_EQUAL( alice_account.get_balance().amount.value, 1000 );
    generate_block();
    BOOST_REQUIRE_EQUAL( alice_account.get_pending_claimed_accounts().value, 3 );
    BOOST_REQUIRE_EQUAL( alice_account.get_balance().amount.value, 1000 );

    fund( "alice", ASSET( "1.000 TESTS" ) );
    BOOST_REQUIRE_EQUAL( alice_account.get_balance().amount.value, 2000 );
    revision = db->revision();
    ilog( "Undo revision is ${r}", ( "r", revision ) );
    ilog( "Undo stack contains block and pending tx sessions" );
    BOOST_REQUIRE_GT( revision, previous_revision + 1 );
    ++previous_revision;

    // increase claimed tokens when undo stack has pending tx session but then cause exception with another transaction
    db_plugin->debug_update( [&]( database& db )
    {
      db.modify( alice_account, [&]( account_object& a )
      {
        a.set_pending_claimed_accounts( a.get_pending_claimed_accounts() + 1 );
      } );
    } );
    HIVE_REQUIRE_THROW( transfer( "alice", "bob", ASSET( "1.000 TESTS" ), "", alice_private_key ), fc::exception );
    BOOST_REQUIRE_EQUAL( alice_account.get_pending_claimed_accounts().value, 4 );
    BOOST_REQUIRE_EQUAL( alice_account.get_balance().amount.value, 2000 );
    generate_block();
    BOOST_REQUIRE_EQUAL( alice_account.get_pending_claimed_accounts().value, 4 );
    BOOST_REQUIRE_EQUAL( alice_account.get_balance().amount.value, 2000 );

    fund( "alice", ASSET( "1.000 TESTS" ) );
    BOOST_REQUIRE_EQUAL( alice_account.get_balance().amount.value, 3000 );
    revision = db->revision();
    ilog( "Undo revision is ${r}", ( "r", revision ) );
    ilog( "Undo stack contains block and pending tx sessions" );
    BOOST_REQUIRE_GT( revision, previous_revision + 1 );
    ++previous_revision;

    // increase claimed tokens when undo stack has pending tx session but then cause exception within debug_update itself
    HIVE_REQUIRE_THROW( db_plugin->debug_update( [&]( database& db )
    {
      db.modify( alice_account, [&]( account_object& a )
      {
        a.set_pending_claimed_accounts( a.get_pending_claimed_accounts() + 1 );
      } );
      FC_ASSERT( false );
    } ), fc::exception );
    BOOST_REQUIRE_EQUAL( alice_account.get_pending_claimed_accounts().value, 4 );
    BOOST_REQUIRE_EQUAL( alice_account.get_balance().amount.value, 3000 );
    generate_block();
    BOOST_REQUIRE_EQUAL( alice_account.get_pending_claimed_accounts().value, 4 );
    BOOST_REQUIRE_EQUAL( alice_account.get_balance().amount.value, 3000 );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( filled_undo_benchmark )
{
  try
  {
    BOOST_TEST_MESSAGE( "--- Benchmarking allocations of undo session elements" );

    generate_blocks( 2 * HIVE_MAX_WITNESSES );
    db->clear_pending();
    ilog( "Undo stack is now empty" );

    /*
    The test checks time it takes to create/modify/delete small or big objects, without any other
    work. It was mainly done to check influence of shared pool allocator applied to undo objects,
    but can be also used to measure f.e. cutting of account_object into smaller chunks of data.
    */
    fc::optional<chainbase::database::undo_session_guard> undo_session;
    const auto& account_idx = db->get_index< account_index, by_id >();
    size_t account_idx_size = account_idx.size();
    auto lastBuiltinAccountI = --account_idx.end();
    const auto& comment_idx = db->get_index< comment_index, by_id >();
    size_t comment_idx_size = comment_idx.size();

    fc::time_point_sec now = fc::time_point::now();

    const int NUMBER_OF_OBJECTS = 30000;

    // create big objects with subcontainers: account_object (place on undo_state::new_ids)
    {
      auto undo_session = db->start_undo_session();
      auto time_start = std::chrono::high_resolution_clock::now();
      for( int i = 0; i < NUMBER_OF_OBJECTS; ++i )
      {
        auto account_name = "a" + std::to_string( i );
        db->create< account_object >( account_name, now );
      }
      auto duration_ns = std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
      ilog( "Creation of ${x} account_objects took ${t}ns (${o} per object)",
        ( "x", NUMBER_OF_OBJECTS )( "t", duration_ns )( "o", ( duration_ns + NUMBER_OF_OBJECTS - 1 ) / NUMBER_OF_OBJECTS ) );
      BOOST_REQUIRE_EQUAL( account_idx.size(), account_idx_size + NUMBER_OF_OBJECTS );

      // remove big objects from undo session through undo (they are on new_ids, so their size does not matter)
      time_start = std::chrono::high_resolution_clock::now();
      undo_session.undo();
      duration_ns = std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
      ilog( "Removing of ${x} new account_objects from new_ids through undo took ${t}ns (${o} per object)",
        ( "x", NUMBER_OF_OBJECTS )( "t", duration_ns )( "o", ( duration_ns + NUMBER_OF_OBJECTS - 1 ) / NUMBER_OF_OBJECTS ) );
      BOOST_REQUIRE_EQUAL( account_idx.size(), account_idx_size );
    }

    // create big objects again (place on undo_state::new_ids)
    {
      auto undo_session = db->start_undo_session();
      auto time_start = std::chrono::high_resolution_clock::now();
      for( int i = 0; i < NUMBER_OF_OBJECTS; ++i )
      {
        auto account_name = "a" + std::to_string( i );
        db->create< account_object >( account_name, now );
      }
      auto duration_ns = std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
      ilog( "Creation of ${x} account_objects took ${t}ns (${o} per object)",
        ( "x", NUMBER_OF_OBJECTS )( "t", duration_ns )( "o", ( duration_ns + NUMBER_OF_OBJECTS - 1 ) / NUMBER_OF_OBJECTS ) );
      BOOST_REQUIRE_EQUAL( account_idx.size(), account_idx_size + NUMBER_OF_OBJECTS );

      // remove big objects from undo session through commit (they are on new_ids, so their size does not matter)
      time_start = std::chrono::high_resolution_clock::now();
      undo_session.push();
      db->commit( db->revision() );
      duration_ns = std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
      ilog( "Removing of ${x} new account_objects from new_ids through commit took ${t}ns (${o} per object)",
        ( "x", NUMBER_OF_OBJECTS )( "t", duration_ns )( "o", ( duration_ns + NUMBER_OF_OBJECTS - 1 ) / NUMBER_OF_OBJECTS ) );
      BOOST_REQUIRE_EQUAL( account_idx.size(), account_idx_size + NUMBER_OF_OBJECTS );
    }

    // now that we have solidified new accounts, move iterator to actual first new account
    auto firstAccountI = lastBuiltinAccountI;
    ++firstAccountI;

    // create small objects: comment_object (place on undo_state::new_ids)
    {
      auto undo_session = db->start_undo_session();
      auto time_start = std::chrono::high_resolution_clock::now();
      for( auto accountI = firstAccountI; accountI != account_idx.end(); ++accountI )
      {
        db->create< comment_object >( *accountI, "test", nullptr );
      }
      auto duration_ns = std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
      ilog( "Creation of ${x} comment_objects took ${t}ns (${o} per object)",
        ( "x", NUMBER_OF_OBJECTS )( "t", duration_ns )( "o", ( duration_ns + NUMBER_OF_OBJECTS - 1 ) / NUMBER_OF_OBJECTS ) );
      BOOST_REQUIRE_EQUAL( comment_idx.size(), comment_idx_size + NUMBER_OF_OBJECTS );

      // remove small objects from undo session through undo (they are on new_ids, so their size does not matter)
      time_start = std::chrono::high_resolution_clock::now();
      undo_session.undo();
      duration_ns = std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
      ilog( "Removing of ${x} new comment_objects from new_ids through undo took ${t}ns (${o} per object)",
        ( "x", NUMBER_OF_OBJECTS )( "t", duration_ns )( "o", ( duration_ns + NUMBER_OF_OBJECTS - 1 ) / NUMBER_OF_OBJECTS ) );
      BOOST_REQUIRE_EQUAL( comment_idx.size(), comment_idx_size );
    }

    // create small objects again (place on undo_state::new_ids)
    {
      auto undo_session = db->start_undo_session();
      auto time_start = std::chrono::high_resolution_clock::now();
      for( auto accountI = firstAccountI; accountI != account_idx.end(); ++accountI )
      {
        db->create< comment_object >( *accountI, "test", nullptr );
      }
      auto duration_ns = std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
      ilog( "Creation of ${x} comment_objects took ${t}ns (${o} per object)",
        ( "x", NUMBER_OF_OBJECTS )( "t", duration_ns )( "o", ( duration_ns + NUMBER_OF_OBJECTS - 1 ) / NUMBER_OF_OBJECTS ) );
      BOOST_REQUIRE_EQUAL( comment_idx.size(), comment_idx_size + NUMBER_OF_OBJECTS );

      // remove small objects from undo session through commit (they are on new_ids, so their size does not matter)
      time_start = std::chrono::high_resolution_clock::now();
      undo_session.push();
      db->commit( db->revision() );
      duration_ns = std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
      ilog( "Removing of ${x} new comment_objects from new_ids through commit took ${t}ns (${o} per object)",
        ( "x", NUMBER_OF_OBJECTS )( "t", duration_ns )( "o", ( duration_ns + NUMBER_OF_OBJECTS - 1 ) / NUMBER_OF_OBJECTS ) );
      BOOST_REQUIRE_EQUAL( comment_idx.size(), comment_idx_size + NUMBER_OF_OBJECTS );
    }


    // modify big objects (place on undo_state::old_values)
    {
      auto undo_session = db->start_undo_session();
      auto time_start = std::chrono::high_resolution_clock::now();
      for( auto accountI = firstAccountI; accountI != account_idx.end(); ++accountI )
      {
        db->modify< account_object >( *accountI, []( account_object& obj ){} );
      }
      auto duration_ns = std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
      ilog( "Modification of ${x} account_objects took ${t}ns (${o} per object)",
        ( "x", NUMBER_OF_OBJECTS )( "t", duration_ns )( "o", ( duration_ns + NUMBER_OF_OBJECTS - 1 ) / NUMBER_OF_OBJECTS ) );
      BOOST_REQUIRE_EQUAL( account_idx.size(), account_idx_size + NUMBER_OF_OBJECTS );

      // remove big objects from undo session through undo (old_values)
      time_start = std::chrono::high_resolution_clock::now();
      undo_session.undo();
      duration_ns = std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
      ilog( "Removing of ${x} modified account_objects from old_values through undo took ${t}ns (${o} per object)",
        ( "x", NUMBER_OF_OBJECTS )( "t", duration_ns )( "o", ( duration_ns + NUMBER_OF_OBJECTS - 1 ) / NUMBER_OF_OBJECTS ) );
      BOOST_REQUIRE_EQUAL( account_idx.size(), account_idx_size + NUMBER_OF_OBJECTS );
    }

    // modify big objects again (place on undo_state::old_values)
    {
      auto undo_session = db->start_undo_session();
      auto time_start = std::chrono::high_resolution_clock::now();
      for( auto accountI = firstAccountI; accountI != account_idx.end(); ++accountI )
      {
        db->modify< account_object >( *accountI, []( account_object& obj ) {} );
      }
      auto duration_ns = std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
      ilog( "Modification of ${x} account_objects took ${t}ns (${o} per object)",
        ( "x", NUMBER_OF_OBJECTS )( "t", duration_ns )( "o", ( duration_ns + NUMBER_OF_OBJECTS - 1 ) / NUMBER_OF_OBJECTS ) );
      BOOST_REQUIRE_EQUAL( account_idx.size(), account_idx_size + NUMBER_OF_OBJECTS );

      // remove big objects from undo session through commit (old_values)
      time_start = std::chrono::high_resolution_clock::now();
      undo_session.push();
      db->commit( db->revision() );
      duration_ns = std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
      ilog( "Removing of ${x} modified account_objects from old_values through commit took ${t}ns (${o} per object)",
        ( "x", NUMBER_OF_OBJECTS )( "t", duration_ns )( "o", ( duration_ns + NUMBER_OF_OBJECTS - 1 ) / NUMBER_OF_OBJECTS ) );
      BOOST_REQUIRE_EQUAL( account_idx.size(), account_idx_size + NUMBER_OF_OBJECTS );
    }


    // modify small objects (place on undo_state::old_values)
    {
      auto undo_session = db->start_undo_session();
      auto time_start = std::chrono::high_resolution_clock::now();
      for( auto commentI = comment_idx.begin(); commentI != comment_idx.end(); ++commentI )
      {
        db->modify< comment_object >( *commentI, []( comment_object& obj ) {} );
      }
      auto duration_ns = std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
      ilog( "Modification of ${x} comment_objects took ${t}ns (${o} per object)",
        ( "x", NUMBER_OF_OBJECTS )( "t", duration_ns )( "o", ( duration_ns + NUMBER_OF_OBJECTS - 1 ) / NUMBER_OF_OBJECTS ) );
      BOOST_REQUIRE_EQUAL( comment_idx.size(), comment_idx_size + NUMBER_OF_OBJECTS );

      // remove small objects from undo session through undo (old_values)
      time_start = std::chrono::high_resolution_clock::now();
      undo_session.undo();
      duration_ns = std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
      ilog( "Removing of ${x} modified comment_objects from old_values through undo took ${t}ns (${o} per object)",
        ( "x", NUMBER_OF_OBJECTS )( "t", duration_ns )( "o", ( duration_ns + NUMBER_OF_OBJECTS - 1 ) / NUMBER_OF_OBJECTS ) );
      BOOST_REQUIRE_EQUAL( comment_idx.size(), comment_idx_size + NUMBER_OF_OBJECTS );
    }

    // modify small objects again (place on undo_state::old_values)
    {
      auto undo_session = db->start_undo_session();
      auto time_start = std::chrono::high_resolution_clock::now();
      for( auto commentI = comment_idx.begin(); commentI != comment_idx.end(); ++commentI )
      {
        db->modify< comment_object >( *commentI, []( comment_object& obj ) {} );
      }
      auto duration_ns = std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
      ilog( "Modification of ${x} comment_objects took ${t}ns (${o} per object)",
        ( "x", NUMBER_OF_OBJECTS )( "t", duration_ns )( "o", ( duration_ns + NUMBER_OF_OBJECTS - 1 ) / NUMBER_OF_OBJECTS ) );
      BOOST_REQUIRE_EQUAL( comment_idx.size(), comment_idx_size + NUMBER_OF_OBJECTS );

      // remove small objects from undo session through commit (old_values)
      time_start = std::chrono::high_resolution_clock::now();
      undo_session.push();
      db->commit( db->revision() );
      duration_ns = std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
      ilog( "Removing of ${x} modified comment_objects from old_values through commit took ${t}ns (${o} per object)",
        ( "x", NUMBER_OF_OBJECTS )( "t", duration_ns )( "o", ( duration_ns + NUMBER_OF_OBJECTS - 1 ) / NUMBER_OF_OBJECTS ) );
      BOOST_REQUIRE_EQUAL( comment_idx.size(), comment_idx_size + NUMBER_OF_OBJECTS );
    }


    // delete big objects (place on undo_state::removed_values)
    {
      auto undo_session = db->start_undo_session();
      auto time_start = std::chrono::high_resolution_clock::now();
      for( auto accountI = firstAccountI; accountI != account_idx.end(); )
      {
        const auto& accountO = *accountI;
        ++accountI;
        db->remove< account_object >( accountO );
      }
      auto duration_ns = std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
      ilog( "Removal of ${x} account_objects took ${t}ns (${o} per object)",
        ( "x", NUMBER_OF_OBJECTS )( "t", duration_ns )( "o", ( duration_ns + NUMBER_OF_OBJECTS - 1 ) / NUMBER_OF_OBJECTS ) );
      BOOST_REQUIRE_EQUAL( account_idx.size(), account_idx_size );

      // remove big objects from undo session through undo (removed_values)
      time_start = std::chrono::high_resolution_clock::now();
      undo_session.undo();
      duration_ns = std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
      ilog( "Removing of ${x} account_objects from removed_values through undo took ${t}ns (${o} per object)",
        ( "x", NUMBER_OF_OBJECTS )( "t", duration_ns )( "o", ( duration_ns + NUMBER_OF_OBJECTS - 1 ) / NUMBER_OF_OBJECTS ) );
      BOOST_REQUIRE_EQUAL( account_idx.size(), account_idx_size + NUMBER_OF_OBJECTS );
    }

    // we've removed object pointed by firstAccountI - even after undo, the iterator remains invalidated, restore it
    firstAccountI = lastBuiltinAccountI;
    ++firstAccountI;

    // delete big objects again (place on undo_state::removed_values)
    {
      auto undo_session = db->start_undo_session();
      auto time_start = std::chrono::high_resolution_clock::now();
      for( auto accountI = firstAccountI; accountI != account_idx.end(); )
      {
        const auto& accountO = *accountI;
        ++accountI;
        db->remove< account_object >( accountO );
      }
      auto duration_ns = std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
      ilog( "Removal of ${x} account_objects took ${t}ns (${o} per object)",
        ( "x", NUMBER_OF_OBJECTS )( "t", duration_ns )( "o", ( duration_ns + NUMBER_OF_OBJECTS - 1 ) / NUMBER_OF_OBJECTS ) );
      BOOST_REQUIRE_EQUAL( account_idx.size(), account_idx_size );

      // remove big objects from undo session through commit (removed_values)
      time_start = std::chrono::high_resolution_clock::now();
      undo_session.push();
      db->commit( db->revision() );
      duration_ns = std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
      ilog( "Removing of ${x} account_objects from removed_values through commit took ${t}ns (${o} per object)",
        ( "x", NUMBER_OF_OBJECTS )( "t", duration_ns )( "o", ( duration_ns + NUMBER_OF_OBJECTS - 1 ) / NUMBER_OF_OBJECTS ) );
      BOOST_REQUIRE_EQUAL( account_idx.size(), account_idx_size );
    }


    // delete small objects (place on undo_state::removed_values)
    {
      auto undo_session = db->start_undo_session();
      auto time_start = std::chrono::high_resolution_clock::now();
      for( auto commentI = comment_idx.begin(); commentI != comment_idx.end(); )
      {
        const auto& commentO = *commentI;
        ++commentI;
        db->remove< comment_object >( commentO );
      }
      auto duration_ns = std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
      ilog( "Removal of ${x} comment_objects took ${t}ns (${o} per object)",
        ( "x", NUMBER_OF_OBJECTS )( "t", duration_ns )( "o", ( duration_ns + NUMBER_OF_OBJECTS - 1 ) / NUMBER_OF_OBJECTS ) );
      BOOST_REQUIRE_EQUAL( comment_idx.size(), comment_idx_size );

      // remove small objects from undo session through undo (removed_values)
      time_start = std::chrono::high_resolution_clock::now();
      undo_session.undo();
      duration_ns = std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
      ilog( "Removing of ${x} comment_objects from removed_values through undo took ${t}ns (${o} per object)",
        ( "x", NUMBER_OF_OBJECTS )( "t", duration_ns )( "o", ( duration_ns + NUMBER_OF_OBJECTS - 1 ) / NUMBER_OF_OBJECTS ) );
      BOOST_REQUIRE_EQUAL( comment_idx.size(), comment_idx_size + NUMBER_OF_OBJECTS );
    }

    // delete small objects again (place on undo_state::removed_values)
    {
      auto undo_session = db->start_undo_session();
      auto time_start = std::chrono::high_resolution_clock::now();
      for( auto commentI = comment_idx.begin(); commentI != comment_idx.end(); )
      {
        const auto& commentO = *commentI;
        ++commentI;
        db->remove< comment_object >( commentO );
      }
      auto duration_ns = std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
      ilog( "Removal of ${x} comment_objects took ${t}ns (${o} per object)",
        ( "x", NUMBER_OF_OBJECTS )( "t", duration_ns )( "o", ( duration_ns + NUMBER_OF_OBJECTS - 1 ) / NUMBER_OF_OBJECTS ) );
      BOOST_REQUIRE_EQUAL( comment_idx.size(), comment_idx_size );

      // remove small objects from undo session through commit (removed_values)
      time_start = std::chrono::high_resolution_clock::now();
      undo_session.push();
      db->commit( db->revision() );
      duration_ns = std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - time_start ).count();
      ilog( "Removing of ${x} comment_objects from removed_values through commit took ${t}ns (${o} per object)",
        ( "x", NUMBER_OF_OBJECTS )( "t", duration_ns )( "o", ( duration_ns + NUMBER_OF_OBJECTS - 1 ) / NUMBER_OF_OBJECTS ) );
      BOOST_REQUIRE_EQUAL( comment_idx.size(), comment_idx_size );
    }
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()
#endif
