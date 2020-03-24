#include <boost/test/unit_test.hpp>

#include <steem/chain/steem_fwd.hpp>

#include <steem/protocol/exceptions.hpp>
#include <steem/protocol/hardfork.hpp>
#include <steem/protocol/sps_operations.hpp>

#include <steem/chain/database.hpp>
#include <steem/chain/database_exceptions.hpp>
#include <steem/chain/steem_objects.hpp>

#include <steem/chain/util/reward.hpp>

#include <steem/plugins/rc/rc_objects.hpp>
#include <steem/plugins/rc/resource_count.hpp>

#include <fc/macros.hpp>
#include <fc/crypto/digest.hpp>

#include "../db_fixture/database_fixture.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace steem;
using namespace steem::chain;
using namespace steem::protocol;
using fc::string;

BOOST_FIXTURE_TEST_SUITE( hf23_tests, hf23_database_fixture )

BOOST_AUTO_TEST_CASE( basic_test_06 )
{
   try
   {
      BOOST_TEST_MESSAGE( "VEST delegations - object of type `vesting_delegation_expiration_object` is generated" );

      ACTORS( (alice)(bob) )
      generate_block();

      set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
      generate_block();

      auto _10 = ASSET( "10.000 TESTS" );
      auto _20 = ASSET( "20.000 TESTS" );
      auto _1v = ASSET( "1.000000 VESTS" );
      auto _2v = ASSET( "2.000000 VESTS" );
      auto _3v = ASSET( "3.000000 VESTS" );
      auto _1000 = ASSET( "1000.000 TESTS" );

      FUND( "alice", _1000 );
      FUND( "bob", _1000 );

      {
         vest( "alice", "alice", _10, alice_private_key );
         vest( "bob", "bob", _20, bob_private_key );
      }
      {
         delegate_vest( "alice", "bob", _3v, alice_private_key );
         generate_block();

         const auto& idx = db->get_index< vesting_delegation_expiration_index, by_account_expiration >();
         BOOST_REQUIRE( idx.lower_bound( "alice" ) == idx.end() );
         BOOST_REQUIRE( idx.lower_bound( "bob" ) == idx.end() );
      }
      {
         delegate_vest( "alice", "bob", _3v, alice_private_key );

         const auto& idx = db->get_index< vesting_delegation_expiration_index, by_account_expiration >();
         BOOST_REQUIRE( idx.lower_bound( "alice" ) == idx.end() );
         BOOST_REQUIRE( idx.lower_bound( "bob" ) == idx.end() );
      }
      {
         delegate_vest( "alice", "bob", _2v, alice_private_key );

         const auto& idx = db->get_index< vesting_delegation_expiration_index, by_account_expiration >();
         BOOST_REQUIRE( idx.lower_bound( "alice" ) != idx.end() );
         BOOST_REQUIRE( idx.lower_bound( "bob" ) == idx.end() );
      }
      {
         delegate_vest( "alice", "bob", _1v, alice_private_key );

         const auto& idx = db->get_index< vesting_delegation_expiration_index, by_account_expiration >();
         BOOST_REQUIRE( idx.lower_bound( "alice" ) != idx.end() );
         BOOST_REQUIRE( idx.lower_bound( "bob" ) == idx.end() );
      }
      {
         db->clear_account( db->get_account( "bob" ) );

         const auto& idx = db->get_index< vesting_delegation_expiration_index, by_account_expiration >();
         BOOST_REQUIRE( idx.lower_bound( "alice" ) != idx.end() );
         BOOST_REQUIRE( idx.lower_bound( "bob" ) == idx.end() );
      }
      {
         db->clear_account( db->get_account( "alice" ) );

         const auto& idx = db->get_index< vesting_delegation_expiration_index, by_account_expiration >();
         BOOST_REQUIRE( idx.lower_bound( "alice" ) == idx.end() );
         BOOST_REQUIRE( idx.lower_bound( "bob" ) == idx.end() );
      }

      database_fixture::validate_database();
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( basic_test_05 )
{
   try
   {
      BOOST_TEST_MESSAGE( "VEST delegations - more complex scenarios" );

      ACTORS( (alice)(bob)(carol) )
      generate_block();

      set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
      generate_block();

      auto _10 = ASSET( "10.000 TESTS" );
      auto _20 = ASSET( "20.000 TESTS" );
      auto _1v = ASSET( "1.000000 VESTS" );
      auto _2v = ASSET( "2.000000 VESTS" );
      auto _3v = ASSET( "3.000000 VESTS" );
      auto _1000 = ASSET( "1000.000 TESTS" );

      FUND( "alice", _1000 );
      FUND( "bob", _1000 );
      FUND( "carol", _1000 );

      {
         vest( "alice", "alice", _10, alice_private_key );
         vest( "bob", "bob", _20, bob_private_key );
         vest( "carol", "carol", _10, carol_private_key );
      }
      {
         delegate_vest( "alice", "bob", _1v, alice_private_key );
         delegate_vest( "alice", "bob", _2v, alice_private_key );

         BOOST_REQUIRE( db->get_account( "alice" ).delegated_vesting_shares.amount.value == _2v.amount.value );

         BOOST_REQUIRE( db->get_account( "bob" ).received_vesting_shares.amount.value == _2v.amount.value );
      }
      {
         delegate_vest( "alice", "carol", _1v, alice_private_key );

         BOOST_REQUIRE( db->get_account( "alice" ).delegated_vesting_shares.amount.value == _3v.amount.value );

         BOOST_REQUIRE( db->get_account( "bob" ).received_vesting_shares.amount.value == _2v.amount.value );
         BOOST_REQUIRE( db->get_account( "carol" ).received_vesting_shares.amount.value == _1v.amount.value );
      }
      {
         delegate_vest( "carol", "bob", _1v, carol_private_key );

         BOOST_REQUIRE( db->get_account( "alice" ).delegated_vesting_shares.amount.value == _3v.amount.value );
         BOOST_REQUIRE( db->get_account( "carol" ).delegated_vesting_shares.amount.value == _1v.amount.value );

         BOOST_REQUIRE( db->get_account( "bob" ).received_vesting_shares.amount.value == _3v.amount.value );
         BOOST_REQUIRE( db->get_account( "carol" ).received_vesting_shares.amount.value == _1v.amount.value );
      }
      {
         db->clear_account( db->get_account( "carol" ) );

         BOOST_REQUIRE( db->get_account( "alice" ).delegated_vesting_shares.amount.value == _3v.amount.value );
         BOOST_REQUIRE( db->get_account( "carol" ).delegated_vesting_shares.amount.value == 0l );

         BOOST_REQUIRE( db->get_account( "bob" ).received_vesting_shares.amount.value == _2v.amount.value );
         BOOST_REQUIRE( db->get_account( "carol" ).received_vesting_shares.amount.value == _1v.amount.value );
      }
      {
         db->clear_account( db->get_account( "alice" ) );

         BOOST_REQUIRE( db->get_account( "alice" ).delegated_vesting_shares.amount.value == 0l );
         BOOST_REQUIRE( db->get_account( "bob" ).delegated_vesting_shares.amount.value == 0l );
         BOOST_REQUIRE( db->get_account( "carol" ).delegated_vesting_shares.amount.value == 0l );

         BOOST_REQUIRE( db->get_account( "alice" ).received_vesting_shares.amount.value == 0l );
         BOOST_REQUIRE( db->get_account( "bob" ).received_vesting_shares.amount.value == 0l );
         BOOST_REQUIRE( db->get_account( "carol" ).received_vesting_shares.amount.value == 0l );
      }

      database_fixture::validate_database();
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( basic_test_04 )
{
   try
   {
      BOOST_TEST_MESSAGE( "VEST delegations" );

      ACTORS( (alice)(bob)(carol) )
      generate_block();

      set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
      generate_block();

      auto _10 = ASSET( "10.000 TESTS" );
      auto _20 = ASSET( "20.000 TESTS" );
      auto _1v = ASSET( "1.000000 VESTS" );
      auto _2v = ASSET( "2.000000 VESTS" );
      auto _1000 = ASSET( "1000.000 TESTS" );

      FUND( "alice", _1000 );
      FUND( "bob", _1000 );
      FUND( "carol", _1000 );

      {
         vest( "alice", "alice", _10, alice_private_key );
         vest( "alice", "alice", _20, alice_private_key );

         delegate_vest( "alice", "bob", _1v, alice_private_key );
         BOOST_REQUIRE( db->get_account( "alice" ).delegated_vesting_shares.amount.value != 0l );
         BOOST_REQUIRE( db->get_account( "bob" ).delegated_vesting_shares.amount.value == 0l );
         BOOST_REQUIRE( db->get_account( "carol" ).delegated_vesting_shares.amount.value == 0l );

         BOOST_REQUIRE( db->get_account( "alice" ).received_vesting_shares.amount.value == 0l );
         BOOST_REQUIRE( db->get_account( "bob" ).received_vesting_shares.amount.value != 0l );
         BOOST_REQUIRE( db->get_account( "carol" ).received_vesting_shares.amount.value == 0l );

         auto previous = db->get_account( "alice" ).delegated_vesting_shares.amount.value;

         delegate_vest( "alice", "carol", _2v, alice_private_key );

         auto next = db->get_account( "alice" ).delegated_vesting_shares.amount.value;
         BOOST_REQUIRE( next > previous );

         BOOST_REQUIRE( db->get_account( "alice" ).delegated_vesting_shares.amount.value != 0l );
         BOOST_REQUIRE( db->get_account( "bob" ).delegated_vesting_shares.amount.value == 0l );
         BOOST_REQUIRE( db->get_account( "carol" ).delegated_vesting_shares.amount.value == 0l );

         BOOST_REQUIRE( db->get_account( "alice" ).received_vesting_shares.amount.value == 0l );
         BOOST_REQUIRE( db->get_account( "bob" ).received_vesting_shares.amount.value != 0l );
         BOOST_REQUIRE( db->get_account( "carol" ).received_vesting_shares.amount.value != 0l );
      }
      {
         db->clear_account( db->get_account( "alice" ) );
         BOOST_REQUIRE( db->get_account( "alice" ).delegated_vesting_shares.amount.value == 0l );
         BOOST_REQUIRE( db->get_account( "bob" ).delegated_vesting_shares.amount.value == 0l );
         BOOST_REQUIRE( db->get_account( "carol" ).delegated_vesting_shares.amount.value == 0l );

         BOOST_REQUIRE( db->get_account( "alice" ).received_vesting_shares.amount.value == 0l );
         BOOST_REQUIRE( db->get_account( "bob" ).received_vesting_shares.amount.value == 0l );
         BOOST_REQUIRE( db->get_account( "carol" ).received_vesting_shares.amount.value == 0l );

         db->clear_account( db->get_account( "bob" ) );
         BOOST_REQUIRE( db->get_account( "alice" ).delegated_vesting_shares.amount.value == 0l );
         BOOST_REQUIRE( db->get_account( "bob" ).delegated_vesting_shares.amount.value == 0l );
         BOOST_REQUIRE( db->get_account( "carol" ).delegated_vesting_shares.amount.value == 0l );

         BOOST_REQUIRE( db->get_account( "alice" ).received_vesting_shares.amount.value == 0l );
         BOOST_REQUIRE( db->get_account( "bob" ).received_vesting_shares.amount.value == 0l );
         BOOST_REQUIRE( db->get_account( "carol" ).received_vesting_shares.amount.value == 0l );
      }

      database_fixture::validate_database();
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( basic_test_03 )
{
   try
   {
      BOOST_TEST_MESSAGE( "Vesting every account to anothers accounts" );

      ACTORS( (alice)(bob)(carol) )
      generate_block();

      set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
      generate_block();

      auto _1 = ASSET( "1.000 TESTS" );
      auto _2 = ASSET( "2.000 TESTS" );
      auto _3 = ASSET( "3.000 TESTS" );
      auto _1000 = ASSET( "1000.000 TESTS" );

      FUND( "alice", _1000 );
      FUND( "bob", _1000 );
      FUND( "carol", _1000 );

      BOOST_REQUIRE( db->get_account( "alice" ).vesting_shares == db->get_account( "bob" ).vesting_shares );
      BOOST_REQUIRE( db->get_account( "bob" ).vesting_shares == db->get_account( "carol" ).vesting_shares );

      {
         vest( "alice", "bob", _1, alice_private_key );
         vest( "bob", "carol", _2, bob_private_key );
         vest( "carol", "alice", _3, carol_private_key );
         BOOST_REQUIRE_GT( db->get_account( "alice" ).vesting_shares.amount.value, db->get_account( "carol" ).vesting_shares.amount.value );
         BOOST_REQUIRE_GT( db->get_account( "carol" ).vesting_shares.amount.value, db->get_account( "bob" ).vesting_shares.amount.value );
      }
      {
         auto vest_bob = db->get_account( "bob" ).vesting_shares.amount.value;
         auto vest_carol = db->get_account( "carol" ).vesting_shares.amount.value;

         db->clear_account( db->get_account( "alice" ) );
         BOOST_REQUIRE( db->get_account( "alice" ).vesting_shares.amount.value == 0l );
         BOOST_REQUIRE( db->get_account( "bob" ).vesting_shares.amount.value == vest_bob );
         BOOST_REQUIRE( db->get_account( "carol" ).vesting_shares.amount.value == vest_carol );

         db->clear_account( db->get_account( "bob" ) );
         BOOST_REQUIRE( db->get_account( "alice" ).vesting_shares.amount.value == 0l );
         BOOST_REQUIRE( db->get_account( "bob" ).vesting_shares.amount.value == 0l );
         BOOST_REQUIRE( db->get_account( "carol" ).vesting_shares.amount.value == vest_carol );

         db->clear_account( db->get_account( "carol" ) );
         BOOST_REQUIRE( db->get_account( "alice" ).vesting_shares.amount.value == 0l );
         BOOST_REQUIRE( db->get_account( "bob" ).vesting_shares.amount.value == 0l );
         BOOST_REQUIRE( db->get_account( "carol" ).vesting_shares.amount.value == 0l );
      }

      database_fixture::validate_database();
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( basic_test_02 )
{
   try
   {
      BOOST_TEST_MESSAGE( "Vesting" );

      ACTORS( (alice)(bob) )
      generate_block();

      set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
      generate_block();

      auto _1 = ASSET( "1.000 TESTS" );
      auto _1000 = ASSET( "1000.000 TESTS" );

      FUND( "alice", _1000 );
      FUND( "bob", _1000 );

      BOOST_REQUIRE( db->get_account( "alice" ).vesting_shares == db->get_account( "bob" ).vesting_shares );

      {
         vest( "alice", "alice", _1, alice_private_key );
         BOOST_REQUIRE_GT( db->get_account( "alice" ).vesting_shares.amount.value, db->get_account( "bob" ).vesting_shares.amount.value );
      }
      {
         auto vest_bob = db->get_account( "bob" ).vesting_shares.amount.value;

         db->clear_account( db->get_account( "alice" ) );
         BOOST_REQUIRE( db->get_account( "alice" ).vesting_shares.amount.value == 0l );
         BOOST_REQUIRE( db->get_account( "bob" ).vesting_shares.amount.value == vest_bob );

         db->clear_account( db->get_account( "bob" ) );
         BOOST_REQUIRE( db->get_account( "alice" ).vesting_shares.amount.value == 0l );
         BOOST_REQUIRE( db->get_account( "bob" ).vesting_shares.amount.value == 0l );
      }

      database_fixture::validate_database();
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( basic_test_01 )
{
   try
   {
      BOOST_TEST_MESSAGE( "Calling clear_account" );

      ACTORS( (alice) )
      generate_block();

      auto& _alice = db->get_account( "alice" );
      db->clear_account( _alice );

      /*
         Original `clean_database_fixture::validate_database` checks `rc_plugin` as well.
         Is it needed?
      */
      database_fixture::validate_database();
   }
   FC_LOG_AND_RETHROW()
}

#define CLEAR( who ) \
   if( !db->pending_transaction_session().valid() ) \
     db->pending_transaction_session() = db->start_undo_session(); \
   db->clear_account( db->get_account( who ) ); \
   database_fixture::validate_database()
//since generating block runs undo and reapplies transactions since last block (and clear_account is not a transaction)
//it is better to do it explicitly to avoid confusion when reading and debugging test
#define UNDO_CLEAR db->pending_transaction_session().reset()

BOOST_AUTO_TEST_CASE( escrow_cleanup_test )
{

#define REQUIRE_BALANCE( alice, bob, carol, treasury, member, asset ) \
   BOOST_REQUIRE( db->get_account("alice").member == ASSET( alice " " asset ) ); \
   BOOST_REQUIRE( db->get_account("bob").member == ASSET( bob " " asset ) ); \
   BOOST_REQUIRE( db->get_account("carol").member == ASSET( carol " " asset ) ); \
   BOOST_REQUIRE( db->get_account(STEEM_TREASURY_ACCOUNT).member == ASSET( treasury " " asset ) )

   try
   {
      BOOST_TEST_MESSAGE( "Calling escrow_cleanup_test" );

      ACTORS( (alice)(bob)(carol) )
      generate_block();

      fund( "alice", ASSET( "10.000 TESTS" ) ); //<- note! extra 0.1 is in form of vests
      fund( "alice", ASSET( "10.100 TBD" ) ); //<- note! treasury will get extras from interest and sps-fund/inflation
      generate_block();
      REQUIRE_BALANCE( "10.000", "0.000", "0.000", "0.000", balance, "TESTS" );
      REQUIRE_BALANCE( "10.100", "0.000", "0.000", "0.027", sbd_balance, "TBD" );

      signed_transaction tx;
      tx.set_expiration( db->head_block_time() + STEEM_MAX_TIME_UNTIL_EXPIRATION );
      {
         escrow_transfer_operation op;
         op.from = "alice";
         op.to = "bob";
         op.agent = "carol";
         op.steem_amount = ASSET( "10.000 TESTS" );
         op.sbd_amount = ASSET( "10.000 TBD" );
         op.fee = ASSET( "0.100 TBD" );
         op.json_meta = "";
         op.ratification_deadline = db->head_block_time() + fc::seconds( STEEM_BLOCK_INTERVAL * 10 );
         op.escrow_expiration = db->head_block_time() + fc::seconds( STEEM_BLOCK_INTERVAL * 20 );
         tx.operations.push_back( op );
         sign( tx, alice_private_key );
         db->push_transaction( tx, 0 );
      }
      tx.clear();

      REQUIRE_BALANCE( "0.000", "0.000", "0.000", "0.000", balance, "TESTS" );
      REQUIRE_BALANCE( "0.000", "0.000", "0.000", "0.027", sbd_balance, "TBD" );
      BOOST_REQUIRE( db->find_escrow( "alice", 30 ) != nullptr );
      generate_block();

      //escrow transfer requested but neither receiver nor agent approved yet
      CLEAR( "alice" );
      REQUIRE_BALANCE( "0.000", "0.000", "0.000", "10.100", balance, "TESTS" );
      REQUIRE_BALANCE( "0.000", "0.000", "0.000", "10.136", sbd_balance, "TBD" );
      BOOST_REQUIRE( db->find_escrow( "alice", 30 ) == nullptr );
      UNDO_CLEAR;

      {
         escrow_approve_operation op;
         op.from = "alice";
         op.to = "bob";
         op.agent = "carol";
         op.who = "carol";
         tx.operations.push_back( op );
         sign( tx, carol_private_key );
         db->push_transaction( tx, 0 );
      }
      tx.clear();

      REQUIRE_BALANCE( "0.000", "0.000", "0.000", "0.000", balance, "TESTS" );
      REQUIRE_BALANCE( "0.000", "0.000", "0.000", "0.036", sbd_balance, "TBD" );
      BOOST_REQUIRE( db->find_escrow( "alice", 30 ) != nullptr );
      generate_block();

      //escrow transfer approved by agent but not by receiver
      CLEAR( "alice" );
      REQUIRE_BALANCE( "0.000", "0.000", "0.000", "10.100", balance, "TESTS" );
      REQUIRE_BALANCE( "0.000", "0.000", "0.000", "10.145", sbd_balance, "TBD" );
      BOOST_REQUIRE( db->find_escrow( "alice", 30 ) == nullptr );
      UNDO_CLEAR;

      {
         escrow_approve_operation op;
         op.from = "alice";
         op.to = "bob";
         op.agent = "carol";
         op.who = "bob";
         tx.operations.push_back( op );
         sign( tx, bob_private_key );
         db->push_transaction( tx, 0 );
      }
      tx.clear();

      REQUIRE_BALANCE( "0.000", "0.000", "0.000", "0.000", balance, "TESTS" );
      REQUIRE_BALANCE( "0.000", "0.000", "0.100", "0.045", sbd_balance, "TBD" );
      BOOST_REQUIRE( db->find_escrow( "alice", 30 ) != nullptr );
      generate_block();

      //escrow transfer approved by all parties (agent got fee) but the transfer itself wasn't released yet
      CLEAR( "alice" );
      REQUIRE_BALANCE( "0.000", "0.000", "0.000", "10.100", balance, "TESTS" );
      REQUIRE_BALANCE( "0.000", "0.000", "0.100", "10.054", sbd_balance, "TBD" );
      BOOST_REQUIRE( db->find_escrow( "alice", 30 ) == nullptr );
      UNDO_CLEAR;

      {
         escrow_release_operation op;
         op.from = "alice";
         op.to = "bob";
         op.agent = "carol";
         op.who = "alice";
         op.receiver = "bob";
         op.steem_amount = ASSET( "2.000 TESTS" );
         op.sbd_amount = ASSET( "3.000 TBD" );
         tx.operations.push_back( op );
         sign( tx, alice_private_key );
         db->push_transaction( tx, 0 );
      }
      tx.clear();

      REQUIRE_BALANCE( "0.000", "2.000", "0.000", "0.000", balance, "TESTS" );
      REQUIRE_BALANCE( "0.000", "3.000", "0.100", "0.054", sbd_balance, "TBD" );
      BOOST_REQUIRE( db->find_escrow( "alice", 30 ) != nullptr );
      generate_block();

      //escrow transfer released partially by sender prior to escrow expiration
      CLEAR( "alice" );
      REQUIRE_BALANCE( "0.000", "2.000", "0.000", "8.100", balance, "TESTS" );
      REQUIRE_BALANCE( "0.000", "3.000", "0.100", "7.063", sbd_balance, "TBD" );
      BOOST_REQUIRE( db->find_escrow( "alice", 30 ) == nullptr );
      UNDO_CLEAR;

      {
         escrow_release_operation op;
         op.from = "alice";
         op.to = "bob";
         op.agent = "carol";
         op.who = "bob";
         op.receiver = "alice";
         op.steem_amount = ASSET( "2.000 TESTS" );
         op.sbd_amount = ASSET( "3.000 TBD" );
         tx.operations.push_back( op );
         sign( tx, bob_private_key );
         db->push_transaction( tx, 0 );
      }
      tx.clear();

      REQUIRE_BALANCE( "2.000", "2.000", "0.000", "0.000", balance, "TESTS" );
      REQUIRE_BALANCE( "3.000", "3.000", "0.100", "0.063", sbd_balance, "TBD" );
      BOOST_REQUIRE( db->find_escrow( "alice", 30 ) != nullptr );
      generate_block();

      //escrow transfer released partially by receiver prior to escrow expiration
      CLEAR( "alice" );
      REQUIRE_BALANCE( "0.000", "2.000", "0.000", "8.100", balance, "TESTS" );
      REQUIRE_BALANCE( "0.000", "3.000", "0.100", "7.072", sbd_balance, "TBD" );
      BOOST_REQUIRE( db->find_escrow( "alice", 30 ) == nullptr );
      UNDO_CLEAR;

      //after approvals it doesn't matter if the ratification expires

      {
         escrow_dispute_operation op;
         op.from = "alice";
         op.to = "bob";
         op.agent = "carol";
         op.who = "bob";
         tx.operations.push_back( op );
         sign( tx, bob_private_key );
         db->push_transaction( tx, 0 );
      }
      tx.clear();

      REQUIRE_BALANCE( "2.000", "2.000", "0.000", "0.000", balance, "TESTS" );
      REQUIRE_BALANCE( "3.000", "3.000", "0.100", "0.072", sbd_balance, "TBD" );
      BOOST_REQUIRE( db->find_escrow( "alice", 30 ) != nullptr );
      generate_block();

      //escrow transfer disputed by sender
      CLEAR( "alice" );
      REQUIRE_BALANCE( "0.000", "2.000", "0.000", "8.100", balance, "TESTS" );
      REQUIRE_BALANCE( "0.000", "3.000", "0.100", "7.081", sbd_balance, "TBD" );
      BOOST_REQUIRE( db->find_escrow( "alice", 30 ) == nullptr );
      UNDO_CLEAR;

      //after dispute it doesn't matter if the escrow expires
      
      {
         escrow_release_operation op;
         op.from = "alice";
         op.to = "bob";
         op.agent = "carol";
         op.who = "carol";
         op.receiver = "bob";
         op.steem_amount = ASSET( "2.000 TESTS" );
         op.sbd_amount = ASSET( "3.000 TBD" );
         tx.operations.push_back( op );

         op.from = "alice";
         op.to = "bob";
         op.agent = "carol";
         op.who = "carol";
         op.receiver = "alice";
         op.steem_amount = ASSET( "2.000 TESTS" );
         op.sbd_amount = ASSET( "1.000 TBD" );
         tx.operations.push_back( op );

         sign( tx, carol_private_key );
         db->push_transaction( tx, 0 );
      }
      tx.clear();

      REQUIRE_BALANCE( "4.000", "4.000", "0.000", "0.000", balance, "TESTS" );
      REQUIRE_BALANCE( "4.000", "6.000", "0.100", "0.081", sbd_balance, "TBD" );
      BOOST_REQUIRE( db->find_escrow( "alice", 30 ) != nullptr );
      generate_block();

      //escrow transfer released partially by agent to sender and partially to receiver
      CLEAR( "alice" );
      REQUIRE_BALANCE( "0.000", "4.000", "0.000", "6.100", balance, "TESTS" );
      REQUIRE_BALANCE( "0.000", "6.000", "0.100", "4.090", sbd_balance, "TBD" );
      BOOST_REQUIRE( db->find_escrow( "alice", 30 ) == nullptr );
      UNDO_CLEAR;

      {
         escrow_release_operation op;
         op.from = "alice";
         op.to = "bob";
         op.agent = "carol";
         op.who = "carol";
         op.receiver = "bob";
         op.steem_amount = ASSET( "2.000 TESTS" );
         op.sbd_amount = ASSET( "0.000 TBD" );
         tx.operations.push_back( op );
         sign( tx, carol_private_key );
         db->push_transaction( tx, 0 );
      }
      tx.clear();

      REQUIRE_BALANCE( "4.000", "6.000", "0.000", "0.000", balance, "TESTS" );
      REQUIRE_BALANCE( "4.000", "6.000", "0.100", "0.090", sbd_balance, "TBD" );
      BOOST_REQUIRE( db->find_escrow( "alice", 30 ) == nullptr );
      generate_block();

      //escrow transfer released by agent to receiver and finished (transfer fully executed)
      CLEAR( "alice" );
      REQUIRE_BALANCE( "0.000", "6.000", "0.000", "4.100", balance, "TESTS" );
      REQUIRE_BALANCE( "0.000", "6.000", "0.100", "4.099", sbd_balance, "TBD" );
      BOOST_REQUIRE( db->find_escrow( "alice", 30 ) == nullptr );
      UNDO_CLEAR;
   }
   FC_LOG_AND_RETHROW()

#undef REQUIRE_BALANCE

}

BOOST_AUTO_TEST_CASE( limit_order_cleanup_test )
{

#define REQUIRE_BALANCE( alice, bob, treasury, member, asset ) \
   BOOST_REQUIRE( db->get_account("alice").member == ASSET( alice " " asset ) ); \
   BOOST_REQUIRE( db->get_account("bob").member == ASSET( bob " " asset ) ); \
   BOOST_REQUIRE( db->get_account(STEEM_TREASURY_ACCOUNT).member == ASSET( treasury " " asset ) )

   try
   {
      BOOST_TEST_MESSAGE( "Calling limit_order_cleanup_test" );
      ACTORS( (alice)(bob) )
      generate_block();

      fund( "alice", ASSET( "10.000 TESTS" ) ); //<- note! extra 0.1 is in form of vests
      fund( "bob", ASSET( "5.000 TBD" ) ); //<- note! treasury will get extras from interest and sps-fund/inflation
      generate_block();
      REQUIRE_BALANCE( "10.000", "0.000", "0.000", balance, "TESTS" );
      REQUIRE_BALANCE( "0.000", "5.000", "0.027", sbd_balance, "TBD" );

      signed_transaction tx;
      tx.set_expiration( db->head_block_time() + STEEM_MAX_TIME_UNTIL_EXPIRATION );
      {
         limit_order_create_operation op;
         op.owner = "alice";
         op.amount_to_sell = ASSET( "10.000 TESTS" );
         op.min_to_receive = ASSET( "5.000 TBD" );
         op.expiration = db->head_block_time() + fc::seconds( STEEM_BLOCK_INTERVAL * 20 );
         tx.operations.push_back( op );
         sign( tx, alice_private_key );
         db->push_transaction( tx, 0 );
      }
      tx.clear();

      REQUIRE_BALANCE( "0.000", "0.000", "0.000", balance, "TESTS" );
      REQUIRE_BALANCE( "0.000", "5.000", "0.027", sbd_balance, "TBD" );
      BOOST_REQUIRE( db->find_limit_order( "alice", 0 ) != nullptr );
      generate_block();

      //limit order set but not matched yet
      CLEAR( "alice" );
      REQUIRE_BALANCE( "0.000", "0.000", "10.100", balance, "TESTS" );
      REQUIRE_BALANCE( "0.000", "5.000", "0.036", sbd_balance, "TBD" );
      BOOST_REQUIRE( db->find_limit_order( "alice", 0 ) == nullptr );
      UNDO_CLEAR;

      {
         limit_order_create_operation op;
         op.owner = "bob";
         op.amount_to_sell = ASSET( "2.000 TBD" );
         op.min_to_receive = ASSET( "3.500 TESTS" );
         op.fill_or_kill = true;
         op.expiration = db->head_block_time() + fc::seconds( STEEM_BLOCK_INTERVAL * 20 );
         tx.operations.push_back( op );
         sign( tx, bob_private_key );
         db->push_transaction( tx, 0 );
      }
      tx.clear();

      REQUIRE_BALANCE( "0.000", "4.000", "0.000", balance, "TESTS" );
      REQUIRE_BALANCE( "2.000", "3.000", "0.036", sbd_balance, "TBD" );
      BOOST_REQUIRE( db->find_limit_order( "alice", 0 ) != nullptr );
      generate_block();

      //limit order partially matched
      CLEAR( "alice" );
      REQUIRE_BALANCE( "0.000", "4.000", "6.100", balance, "TESTS" );
      REQUIRE_BALANCE( "0.000", "3.000", "2.045", sbd_balance, "TBD" );
      BOOST_REQUIRE( db->find_limit_order( "alice", 0 ) == nullptr );
      UNDO_CLEAR;

      {
         limit_order_create2_operation op;
         op.owner = "bob";
         op.amount_to_sell = ASSET( "3.000 TBD" );
         op.exchange_rate = ASSET( "2.000 TBD" ) / ASSET( "5.000 TESTS" );
         op.fill_or_kill = true;
         op.expiration = db->head_block_time() + fc::seconds( STEEM_BLOCK_INTERVAL * 20 );
         tx.operations.push_back( op );
         sign( tx, bob_private_key );
         STEEM_REQUIRE_THROW( db->push_transaction( tx, 0 ), fc::exception );
      }
      tx.clear();

      REQUIRE_BALANCE( "0.000", "4.000", "0.000", balance, "TESTS" );
      REQUIRE_BALANCE( "2.000", "3.000", "0.045", sbd_balance, "TBD" );
      BOOST_REQUIRE( db->find_limit_order( "alice", 0 ) != nullptr );
      generate_block();

      //limit order partially matched after counter-order in fill_or_kill mode that failed (no change)
      CLEAR( "alice" );
      REQUIRE_BALANCE( "0.000", "4.000", "6.100", balance, "TESTS" );
      REQUIRE_BALANCE( "0.000", "3.000", "2.054", sbd_balance, "TBD" );
      BOOST_REQUIRE( db->find_limit_order( "alice", 0 ) == nullptr );
      UNDO_CLEAR;

      {
         limit_order_create2_operation op;
         op.owner = "bob";
         op.amount_to_sell = ASSET( "3.000 TBD" );
         op.exchange_rate = ASSET( "2.000 TBD" ) / ASSET( "4.000 TESTS" );
         op.fill_or_kill = true;
         op.expiration = db->head_block_time() + fc::seconds( STEEM_BLOCK_INTERVAL * 20 );
         tx.operations.push_back( op );
         sign( tx, bob_private_key );
         db->push_transaction( tx, 0 );
      }
      tx.clear();

      REQUIRE_BALANCE( "0.000", "10.000", "0.000", balance, "TESTS" );
      REQUIRE_BALANCE( "5.000", "0.000", "0.054", sbd_balance, "TBD" );
      BOOST_REQUIRE( db->find_limit_order( "alice", 0 ) == nullptr );
      generate_block();

      //limit order fully matched
      CLEAR( "alice" );
      REQUIRE_BALANCE( "0.000", "10.000", "0.100", balance, "TESTS" );
      REQUIRE_BALANCE( "0.000", "0.000", "5.063", sbd_balance, "TBD" );
      BOOST_REQUIRE( db->find_limit_order( "alice", 0 ) == nullptr );
      UNDO_CLEAR;
   }
   FC_LOG_AND_RETHROW()

#undef REQUIRE_BALANCE

}

BOOST_AUTO_TEST_CASE( convert_request_cleanup_test )
{

#define REQUIRE_BALANCE( alice, treasury, member, asset ) \
   BOOST_REQUIRE( db->get_account("alice").member == ASSET( alice " " asset ) ); \
   BOOST_REQUIRE( db->get_account(STEEM_TREASURY_ACCOUNT).member == ASSET( treasury " " asset ) )

   try
   {
      BOOST_TEST_MESSAGE( "Calling convert_request_cleanup_test" );
      ACTORS( (alice) )
      generate_block();

      //note! extra 0.1 TESTS is in form of vests
      fund( "alice", ASSET( "5.000 TBD" ) ); //<- note! treasury will get extras from interest and sps-fund/inflation
      generate_block();
      REQUIRE_BALANCE( "0.000", "0.000", balance, "TESTS" );
      REQUIRE_BALANCE( "5.000", "0.027", sbd_balance, "TBD" );

      signed_transaction tx;
      tx.set_expiration( db->head_block_time() + STEEM_MAX_TIME_UNTIL_EXPIRATION );
      {
         convert_operation op;
         op.owner = "alice";
         op.amount = ASSET( "5.000 TBD" );
         tx.operations.push_back( op );
         sign( tx, alice_private_key );
         db->push_transaction( tx, 0 );
      }
      tx.clear();

      REQUIRE_BALANCE( "0.000", "0.000", balance, "TESTS" );
      REQUIRE_BALANCE( "0.000", "0.027", sbd_balance, "TBD" );
      generate_block();

      //conversion request created but not executed yet
      CLEAR( "alice" );
      REQUIRE_BALANCE( "0.000", "0.100", balance, "TESTS" );
      REQUIRE_BALANCE( "0.000", "5.036", sbd_balance, "TBD" );
      UNDO_CLEAR;

      generate_blocks( db->head_block_time() + STEEM_CONVERSION_DELAY );

      REQUIRE_BALANCE( "5.000", "0.000", balance, "TESTS" );
      REQUIRE_BALANCE( "0.000", "0.054", sbd_balance, "TBD" );
      generate_block();

      //conversion request executed
      CLEAR( "alice" );
      REQUIRE_BALANCE( "0.000", "5.100", balance, "TESTS" );
      REQUIRE_BALANCE( "0.000", "0.063", sbd_balance, "TBD" );
      UNDO_CLEAR;
   }
   FC_LOG_AND_RETHROW()

#undef REQUIRE_BALANCE

}

BOOST_AUTO_TEST_SUITE_END()
