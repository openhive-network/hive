#include <boost/test/unit_test.hpp>

#include <hive/chain/hive_fwd.hpp>

#include <hive/chain/database.hpp>

#include <hive/protocol/protocol.hpp>
#include <hive/protocol/hive_operations.hpp>
#include <hive/protocol/transaction_util.hpp>

#include <hive/chain/account_object.hpp>
#include <hive/chain/hive_objects.hpp>
#include <hive/chain/transaction_object.hpp>

#include "../db_fixture/clean_database_fixture.hpp"

#include <string>

using namespace hive;
using namespace hive::chain;
using namespace hive::protocol;

class test_key_collector : public hive::protocol::signing_keys_collector
{
public:
  struct account_authorities
  {
    authority active;
    authority owner;
    authority posting;
  };

  test_key_collector( database& _db ) : db( _db ) {}
  virtual ~test_key_collector() {}

  virtual void collect_signing_keys( flat_set< public_key_type >* keys, const transaction& tx ) override
  {
    approving_account_lut.clear();
    hive::protocol::signing_keys_collector::collect_signing_keys( keys, tx );
    // approving_account_lut remains filled until next call
  }

  virtual void prepare_account_authority_data( const std::vector< account_name_type >& accounts ) override
  {
    for( const auto& name : accounts )
    {
      const auto* authorities = db.find< account_authority_object, by_name >( name );
      FC_ASSERT( authorities != nullptr, "Account ${name} not found", ( name ) );
      approving_account_lut.emplace( name, account_authorities{ authorities->active, authorities->owner, authorities->posting } );
    }
  }

  bool is_known( const account_name_type& account_name ) const
  {
    auto it = approving_account_lut.find( account_name );
    return it != approving_account_lut.end();
  }

  const account_authorities& get_authorities( const account_name_type& account_name ) const
  {
    auto it = approving_account_lut.find( account_name );
    FC_ASSERT( it != approving_account_lut.end(),
      "Tried to access authority for account ${a} but not cached.", ( "a", account_name ) );
    return it->second;
  }

  virtual const authority& get_active( const account_name_type& account_name ) const override
  {
    return get_authorities( account_name ).active;
  }
  virtual const authority& get_owner( const account_name_type& account_name ) const override
  {
    return get_authorities( account_name ).owner;
  }
  virtual const authority& get_posting( const account_name_type& account_name ) const override
  {
    return get_authorities( account_name ).posting;
  }

private:
  database& db;
  flat_map< account_name_type, account_authorities > approving_account_lut;
};

BOOST_FIXTURE_TEST_SUITE( transaction_util_tests, clean_database_fixture )

#define KEYS( name ) \
  auto name ## _owner = generate_private_key( BOOST_PP_STRINGIZE( name ) "_owner" ).get_public_key(); \
  auto name ## _active = generate_private_key( BOOST_PP_STRINGIZE( name ) "_active" ).get_public_key(); \
  auto name ## _posting = generate_private_key( BOOST_PP_STRINGIZE( name ) "_posting" ).get_public_key(); \
  auto name ## _memo = generate_private_key( BOOST_PP_STRINGIZE( name ) "_memo" ).get_public_key()
#define CREATE_ACCOUNT( name ) \
  do { \
    account_create_operation op; \
    op.creator = HIVE_INIT_MINER_NAME; \
    op.fee = fee; \
    op.new_account_name = BOOST_PP_STRINGIZE( name ); \
    op.owner = authority( 1, name ## _owner, 1 ); \
    op.active = authority( 1, name ## _active, 1 ); \
    op.posting = authority( 1, name ## _posting, 1 ); \
    op.memo_key = name ## _memo; \
    push_transaction( op, init_account_priv_key ); \
  } while( 0 )
#define CHECK_CACHE( name ) BOOST_REQUIRE( collector.is_known( BOOST_PP_STRINGIZE( name ) ) )
#define CHECK_NO_CACHE( name ) BOOST_REQUIRE( not collector.is_known( BOOST_PP_STRINGIZE( name ) ) )

BOOST_AUTO_TEST_CASE( autodetect_keys_simple )
{
  auto fee = db->get_witness_schedule_object().median_props.account_creation_fee;

  KEYS( alice ); CREATE_ACCOUNT( alice ); fund( "alice", ASSET( "1.000 TESTS" ) );
  KEYS( bob ); CREATE_ACCOUNT( bob ); fund( "bob", ASSET( "1.000 TESTS" ) );
  generate_block();

  test_key_collector collector( *db );
  signed_transaction tx;
  flat_set<public_key_type> approving_key_set;

  BOOST_TEST_MESSAGE( "Detecting single active key - single operation" );

  transfer_operation transfer;
  transfer.from = "alice";
  transfer.to = "bob";
  transfer.amount = ASSET( "0.010 TESTS" );
  tx.operations.push_back( transfer );

  approving_key_set.clear();
  // transfer from alice
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice_active ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 1 );
  CHECK_CACHE( alice );
  CHECK_NO_CACHE( bob );
  
  BOOST_TEST_MESSAGE( "Detecting single active key - many operations" );

  transfer.to = "initminer";
  tx.operations.push_back( transfer );

  approving_key_set.clear();
  // two transfers from alice
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice_active ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 1 );
  CHECK_CACHE( alice );
  CHECK_NO_CACHE( bob );

  BOOST_TEST_MESSAGE( "Detecting many active keys - many operations" );

  transfer.from = "bob";
  tx.operations.push_back( transfer );

  approving_key_set.clear();
  // two transfers from alice and one from bob
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice_active ) );
  BOOST_REQUIRE( approving_key_set.contains( bob_active ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 2 );
  CHECK_CACHE( alice );
  CHECK_CACHE( bob );

  BOOST_TEST_MESSAGE( "Detecting active/owner keys - many operations" );

  decline_voting_rights_operation decline_voting;
  decline_voting.account = "bob";
  tx.operations.push_back( decline_voting );

  approving_key_set.clear();
  // two transfers from alice, one from bob and declined voting from bob
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice_active ) );
  BOOST_REQUIRE( approving_key_set.contains( bob_active ) );
  BOOST_REQUIRE( approving_key_set.contains( bob_owner ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 3 );
  CHECK_CACHE( alice );
  CHECK_CACHE( bob );

  BOOST_TEST_MESSAGE( "Detecting single owner key - single operation" );

  tx.operations.clear();
  tx.operations.push_back( decline_voting );

  approving_key_set.clear();
  // declined voting from bob
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( bob_owner ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 1 );
  CHECK_NO_CACHE( alice );
  CHECK_CACHE( bob );

  BOOST_TEST_MESSAGE( "Detecting many owner keys - many operations" );

  decline_voting.account = "alice";
  decline_voting.decline = false;
  tx.operations.push_back( decline_voting );

  approving_key_set.clear();
  // declined voting from bob and alice
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( bob_owner ) );
  BOOST_REQUIRE( approving_key_set.contains( alice_owner ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 2 );
  CHECK_CACHE( alice );
  CHECK_CACHE( bob );

  BOOST_TEST_MESSAGE( "Detecting single posting key - single operation" );

  tx.operations.clear();

  comment_operation comment;
  comment.parent_permlink = "category";
  comment.author = "alice";
  comment.permlink = "test";
  comment.title = "test";
  comment.body = "test";
  tx.operations.push_back( comment );

  approving_key_set.clear();
  // comment from alice
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice_posting ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 1 );
  CHECK_CACHE( alice );
  CHECK_NO_CACHE( bob );

  BOOST_TEST_MESSAGE( "Detecting single posting key - many operations" );

  comment_options_operation comment_opts;
  comment_opts.author = "alice";
  comment_opts.permlink = "test";
  comment_opts.max_accepted_payout = ASSET( "0.000 TBD" );
  tx.operations.push_back( comment_opts );

  approving_key_set.clear();
  // comment and comment options from alice
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice_posting ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 1 );
  CHECK_CACHE( alice );
  CHECK_NO_CACHE( bob );

  BOOST_TEST_MESSAGE( "Detecting many posting keys - many operations" );

  vote_operation vote;
  vote.voter = "bob";
  vote.author = "alice";
  vote.permlink = "test";
  vote.weight = HIVE_100_PERCENT;
  tx.operations.push_back( vote );

  approving_key_set.clear();
  // comment and comment options from alice, vote from bob
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice_posting ) );
  BOOST_REQUIRE( approving_key_set.contains( bob_posting ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 2 );
  CHECK_CACHE( alice );
  CHECK_CACHE( bob );

  BOOST_TEST_MESSAGE( "Detecting many active/posting keys - many operations" );

  transfer.to = "alice";
  transfer.memo = "tip";
  tx.operations.push_back( transfer );

  approving_key_set.clear();
  // comment and comment options from alice, vote and transfer from bob
  // even though such mix is not allowed, it should not cause problems when keys are selected
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice_posting ) );
  BOOST_REQUIRE( approving_key_set.contains( bob_posting ) );
  BOOST_REQUIRE( approving_key_set.contains( bob_active ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 3 );
  CHECK_CACHE( alice );
  CHECK_CACHE( bob );
}

BOOST_AUTO_TEST_CASE( autodetect_keys_1_of_2 )
{
  auto fee = db->get_witness_schedule_object().median_props.account_creation_fee;

#define CREATE_ACCOUNT_1_OF_2( name ) \
  do { \
    account_create_operation op; \
    op.creator = HIVE_INIT_MINER_NAME; \
    op.fee = fee; \
    op.new_account_name = BOOST_PP_STRINGIZE( name ); \
    op.owner = authority( 1, name ## 1_owner, 1, name ## 2_owner, 1 ); \
    op.active = authority( 1, name ## 1_active, 1, name ## 2_active, 1 ); \
    op.posting = authority( 1, name ## 1_posting, 1, name ## 2_posting, 1 ); \
    op.memo_key = name ## 1_memo; \
    push_transaction( op, init_account_priv_key ); \
  } while( 0 )

  KEYS( alice1 ); KEYS( alice2 ); CREATE_ACCOUNT_1_OF_2( alice ); fund( "alice", ASSET( "1.000 TESTS" ) );
  KEYS( bob1 ); KEYS( bob2 ); CREATE_ACCOUNT_1_OF_2( bob ); fund( "bob", ASSET( "1.000 TESTS" ) );
  generate_block();

  test_key_collector collector( *db );
  signed_transaction tx;
  flat_set<public_key_type> approving_key_set;

  BOOST_TEST_MESSAGE( "Detecting many active keys of one account - single operation" );

  transfer_operation transfer;
  transfer.from = "alice";
  transfer.to = "bob";
  transfer.amount = ASSET( "0.010 TESTS" );
  tx.operations.push_back( transfer );

  approving_key_set.clear();
  // transfer from alice
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice1_active ) );
  BOOST_REQUIRE( approving_key_set.contains( alice2_active ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 2 );
  CHECK_CACHE( alice );
  CHECK_NO_CACHE( bob );

  BOOST_TEST_MESSAGE( "Detecting many active keys of one account - many operations" );

  transfer.to = "initminer";
  tx.operations.push_back( transfer );

  approving_key_set.clear();
  // two transfers from alice
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice1_active ) );
  BOOST_REQUIRE( approving_key_set.contains( alice2_active ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 2 );
  CHECK_CACHE( alice );
  CHECK_NO_CACHE( bob );

  BOOST_TEST_MESSAGE( "Detecting many active keys of many accounts - many operations" );

  transfer.from = "bob";
  tx.operations.push_back( transfer );

  approving_key_set.clear();
  // two transfers from alice and one from bob
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice1_active ) );
  BOOST_REQUIRE( approving_key_set.contains( alice2_active ) );
  BOOST_REQUIRE( approving_key_set.contains( bob1_active ) );
  BOOST_REQUIRE( approving_key_set.contains( bob2_active ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 4 );
  CHECK_CACHE( alice );
  CHECK_CACHE( bob );

  BOOST_TEST_MESSAGE( "Detecting active/owner keys - many operations" );

  decline_voting_rights_operation decline_voting;
  decline_voting.account = "bob";
  tx.operations.push_back( decline_voting );

  approving_key_set.clear();
  // two transfers from alice, one from bob and declined voting from bob
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice1_active ) );
  BOOST_REQUIRE( approving_key_set.contains( alice2_active ) );
  BOOST_REQUIRE( approving_key_set.contains( bob1_active ) );
  BOOST_REQUIRE( approving_key_set.contains( bob2_active ) );
  BOOST_REQUIRE( approving_key_set.contains( bob1_owner ) );
  BOOST_REQUIRE( approving_key_set.contains( bob2_owner ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 6 );
  CHECK_CACHE( alice );
  CHECK_CACHE( bob );

  BOOST_TEST_MESSAGE( "Detecting many owner keys of single account - single operation" );

  tx.operations.clear();
  tx.operations.push_back( decline_voting );

  approving_key_set.clear();
  // declined voting from bob
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( bob1_owner ) );
  BOOST_REQUIRE( approving_key_set.contains( bob2_owner ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 2 );
  CHECK_NO_CACHE( alice );
  CHECK_CACHE( bob );

  BOOST_TEST_MESSAGE( "Detecting many owner keys of many accounts - many operations" );

  decline_voting.account = "alice";
  decline_voting.decline = false;
  tx.operations.push_back( decline_voting );

  approving_key_set.clear();
  // declined voting from bob and alice
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( bob1_owner ) );
  BOOST_REQUIRE( approving_key_set.contains( bob2_owner ) );
  BOOST_REQUIRE( approving_key_set.contains( alice1_owner ) );
  BOOST_REQUIRE( approving_key_set.contains( alice2_owner ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 4 );
  CHECK_CACHE( alice );
  CHECK_CACHE( bob );

  BOOST_TEST_MESSAGE( "Detecting many posting keys of single account - single operation" );

  tx.operations.clear();

  comment_operation comment;
  comment.parent_permlink = "category";
  comment.author = "alice";
  comment.permlink = "test";
  comment.title = "test";
  comment.body = "test";
  tx.operations.push_back( comment );

  approving_key_set.clear();
  // comment from alice
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice1_posting ) );
  BOOST_REQUIRE( approving_key_set.contains( alice2_posting ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 2 );
  CHECK_CACHE( alice );
  CHECK_NO_CACHE( bob );

  BOOST_TEST_MESSAGE( "Detecting many posting keys of many accounts - many operations" );

  comment_options_operation comment_opts;
  comment_opts.author = "alice";
  comment_opts.permlink = "test";
  comment_opts.max_accepted_payout = ASSET( "0.000 TBD" );
  tx.operations.push_back( comment_opts );

  approving_key_set.clear();
  // comment and comment options from alice
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice1_posting ) );
  BOOST_REQUIRE( approving_key_set.contains( alice2_posting ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 2 );
  CHECK_CACHE( alice );
  CHECK_NO_CACHE( bob );

  BOOST_TEST_MESSAGE( "Detecting many posting keys of many accounts - many operations" );

  vote_operation vote;
  vote.voter = "bob";
  vote.author = "alice";
  vote.permlink = "test";
  vote.weight = HIVE_100_PERCENT;
  tx.operations.push_back( vote );

  approving_key_set.clear();
  // comment and comment options from alice, vote from bob
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice1_posting ) );
  BOOST_REQUIRE( approving_key_set.contains( alice2_posting ) );
  BOOST_REQUIRE( approving_key_set.contains( bob1_posting ) );
  BOOST_REQUIRE( approving_key_set.contains( bob2_posting ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 4 );
  CHECK_CACHE( alice );
  CHECK_CACHE( bob );

  BOOST_TEST_MESSAGE( "Detecting many active/posting keys - many operations" );

  transfer.to = "alice";
  transfer.memo = "tip";
  tx.operations.push_back( transfer );

  approving_key_set.clear();
  // comment and comment options from alice, vote and transfer from bob
  // even though such mix is not allowed, it should not cause problems when keys are selected
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice1_posting ) );
  BOOST_REQUIRE( approving_key_set.contains( alice2_posting ) );
  BOOST_REQUIRE( approving_key_set.contains( bob1_posting ) );
  BOOST_REQUIRE( approving_key_set.contains( bob2_posting ) );
  BOOST_REQUIRE( approving_key_set.contains( bob1_active ) );
  BOOST_REQUIRE( approving_key_set.contains( bob2_active ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 6 );
  CHECK_CACHE( alice );
  CHECK_CACHE( bob );
}

BOOST_AUTO_TEST_CASE( autodetect_keys_2_of_2 )
{
  auto fee = db->get_witness_schedule_object().median_props.account_creation_fee;

#define CREATE_ACCOUNT_2_OF_2( name ) \
  do { \
    account_create_operation op; \
    op.creator = HIVE_INIT_MINER_NAME; \
    op.fee = fee; \
    op.new_account_name = BOOST_PP_STRINGIZE( name ); \
    op.owner = authority( 2, name ## 1_owner, 1, name ## 2_owner, 1 ); \
    op.active = authority( 2, name ## 1_active, 1, name ## 2_active, 1 ); \
    op.posting = authority( 2, name ## 1_posting, 1, name ## 2_posting, 1 ); \
    op.memo_key = name ## 1_memo; \
    push_transaction( op, init_account_priv_key ); \
  } while( 0 )

  KEYS( alice1 ); KEYS( alice2 ); CREATE_ACCOUNT_2_OF_2( alice ); fund( "alice", ASSET( "1.000 TESTS" ) );
  KEYS( bob1 ); KEYS( bob2 ); CREATE_ACCOUNT_2_OF_2( bob ); fund( "bob", ASSET( "1.000 TESTS" ) );
  generate_block();

  test_key_collector collector( *db );
  signed_transaction tx;
  flat_set<public_key_type> approving_key_set;

  BOOST_TEST_MESSAGE( "Detecting many active keys of one account - single operation" );

  transfer_operation transfer;
  transfer.from = "alice";
  transfer.to = "bob";
  transfer.amount = ASSET( "0.010 TESTS" );
  tx.operations.push_back( transfer );

  approving_key_set.clear();
  // transfer from alice
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice1_active ) );
  BOOST_REQUIRE( approving_key_set.contains( alice2_active ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 2 );
  CHECK_CACHE( alice );
  CHECK_NO_CACHE( bob );

  BOOST_TEST_MESSAGE( "Detecting many active keys of one account - many operations" );

  transfer.to = "initminer";
  tx.operations.push_back( transfer );

  approving_key_set.clear();
  // two transfers from alice
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice1_active ) );
  BOOST_REQUIRE( approving_key_set.contains( alice2_active ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 2 );
  CHECK_CACHE( alice );
  CHECK_NO_CACHE( bob );

  BOOST_TEST_MESSAGE( "Detecting many active keys of many accounts - many operations" );

  transfer.from = "bob";
  tx.operations.push_back( transfer );

  approving_key_set.clear();
  // two transfers from alice and one from bob
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice1_active ) );
  BOOST_REQUIRE( approving_key_set.contains( alice2_active ) );
  BOOST_REQUIRE( approving_key_set.contains( bob1_active ) );
  BOOST_REQUIRE( approving_key_set.contains( bob2_active ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 4 );
  CHECK_CACHE( alice );
  CHECK_CACHE( bob );

  BOOST_TEST_MESSAGE( "Detecting active/owner keys - many operations" );

  decline_voting_rights_operation decline_voting;
  decline_voting.account = "bob";
  tx.operations.push_back( decline_voting );

  approving_key_set.clear();
  // two transfers from alice, one from bob and declined voting from bob
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice1_active ) );
  BOOST_REQUIRE( approving_key_set.contains( alice2_active ) );
  BOOST_REQUIRE( approving_key_set.contains( bob1_active ) );
  BOOST_REQUIRE( approving_key_set.contains( bob2_active ) );
  BOOST_REQUIRE( approving_key_set.contains( bob1_owner ) );
  BOOST_REQUIRE( approving_key_set.contains( bob2_owner ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 6 );
  CHECK_CACHE( alice );
  CHECK_CACHE( bob );

  BOOST_TEST_MESSAGE( "Detecting many owner keys of single account - single operation" );

  tx.operations.clear();
  tx.operations.push_back( decline_voting );

  approving_key_set.clear();
  // declined voting from bob
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( bob1_owner ) );
  BOOST_REQUIRE( approving_key_set.contains( bob2_owner ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 2 );
  CHECK_NO_CACHE( alice );
  CHECK_CACHE( bob );

  BOOST_TEST_MESSAGE( "Detecting many owner keys of many accounts - many operations" );

  decline_voting.account = "alice";
  decline_voting.decline = false;
  tx.operations.push_back( decline_voting );

  approving_key_set.clear();
  // declined voting from bob and alice
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( bob1_owner ) );
  BOOST_REQUIRE( approving_key_set.contains( bob2_owner ) );
  BOOST_REQUIRE( approving_key_set.contains( alice1_owner ) );
  BOOST_REQUIRE( approving_key_set.contains( alice2_owner ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 4 );
  CHECK_CACHE( alice );
  CHECK_CACHE( bob );

  BOOST_TEST_MESSAGE( "Detecting many posting keys of single account - single operation" );

  tx.operations.clear();

  comment_operation comment;
  comment.parent_permlink = "category";
  comment.author = "alice";
  comment.permlink = "test";
  comment.title = "test";
  comment.body = "test";
  tx.operations.push_back( comment );

  approving_key_set.clear();
  // comment from alice
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice1_posting ) );
  BOOST_REQUIRE( approving_key_set.contains( alice2_posting ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 2 );
  CHECK_CACHE( alice );
  CHECK_NO_CACHE( bob );

  BOOST_TEST_MESSAGE( "Detecting many posting keys of many accounts - many operations" );

  comment_options_operation comment_opts;
  comment_opts.author = "alice";
  comment_opts.permlink = "test";
  comment_opts.max_accepted_payout = ASSET( "0.000 TBD" );
  tx.operations.push_back( comment_opts );

  approving_key_set.clear();
  // comment and comment options from alice
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice1_posting ) );
  BOOST_REQUIRE( approving_key_set.contains( alice2_posting ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 2 );
  CHECK_CACHE( alice );
  CHECK_NO_CACHE( bob );

  BOOST_TEST_MESSAGE( "Detecting many posting keys of many accounts - many operations" );

  vote_operation vote;
  vote.voter = "bob";
  vote.author = "alice";
  vote.permlink = "test";
  vote.weight = HIVE_100_PERCENT;
  tx.operations.push_back( vote );

  approving_key_set.clear();
  // comment and comment options from alice, vote from bob
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice1_posting ) );
  BOOST_REQUIRE( approving_key_set.contains( alice2_posting ) );
  BOOST_REQUIRE( approving_key_set.contains( bob1_posting ) );
  BOOST_REQUIRE( approving_key_set.contains( bob2_posting ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 4 );
  CHECK_CACHE( alice );
  CHECK_CACHE( bob );

  BOOST_TEST_MESSAGE( "Detecting many active/posting keys - many operations" );

  transfer.to = "alice";
  transfer.memo = "tip";
  tx.operations.push_back( transfer );

  approving_key_set.clear();
  // comment and comment options from alice, vote and transfer from bob
  // even though such mix is not allowed, it should not cause problems when keys are selected
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice1_posting ) );
  BOOST_REQUIRE( approving_key_set.contains( alice2_posting ) );
  BOOST_REQUIRE( approving_key_set.contains( bob1_posting ) );
  BOOST_REQUIRE( approving_key_set.contains( bob2_posting ) );
  BOOST_REQUIRE( approving_key_set.contains( bob1_active ) );
  BOOST_REQUIRE( approving_key_set.contains( bob2_active ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 6 );
  CHECK_CACHE( alice );
  CHECK_CACHE( bob );
}

BOOST_AUTO_TEST_CASE( autodetect_keys_indirect_1_of_2 )
{
  auto fee = db->get_witness_schedule_object().median_props.account_creation_fee;

  KEYS( alice ); CREATE_ACCOUNT( alice ); fund( "alice", ASSET( "1.000 TESTS" ) );
  KEYS( bob ); CREATE_ACCOUNT( bob ); fund( "bob", ASSET( "1.000 TESTS" ) );
  KEYS( carol ); CREATE_ACCOUNT( carol ); fund( "carol", ASSET( "1.000 TESTS" ) );
  generate_block();

  account_update( "alice", alice_memo, "", authority( 1, alice_owner, 1, "carol", 1 ),
    authority( 1, alice_active, 1, "carol", 1 ), authority( 1, alice_posting, 1, "carol", 1 ),
    generate_private_key( "alice_owner" ) );
  account_update( "bob", bob_memo, "", authority( 1, bob_owner, 1, "carol", 1 ),
    authority( 1, bob_active, 1, "carol", 1 ), authority( 1, bob_posting, 1, "carol", 1 ),
    generate_private_key( "bob_owner" ) );
  generate_block();

  test_key_collector collector( *db );
  signed_transaction tx;
  flat_set<public_key_type> approving_key_set;

  BOOST_TEST_MESSAGE( "Detecting many active keys of direct/indirect accounts - single operation" );

  transfer_operation transfer;
  transfer.from = "alice";
  transfer.to = "bob";
  transfer.amount = ASSET( "0.010 TESTS" );
  tx.operations.push_back( transfer );

  approving_key_set.clear();
  // transfer from alice
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice_active ) );
  BOOST_REQUIRE( approving_key_set.contains( carol_active ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 2 );
  CHECK_CACHE( alice );
  CHECK_NO_CACHE( bob );
  CHECK_CACHE( carol );

  BOOST_TEST_MESSAGE( "Detecting many active keys of direct/indirect accounts - many operations" );

  transfer.to = "initminer";
  tx.operations.push_back( transfer );

  approving_key_set.clear();
  // two transfers from alice
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice_active ) );
  BOOST_REQUIRE( approving_key_set.contains( carol_active ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 2 );
  CHECK_CACHE( alice );
  CHECK_NO_CACHE( bob );
  CHECK_CACHE( carol );

  BOOST_TEST_MESSAGE( "Detecting many active keys of many accounts - many operations" );

  transfer.from = "bob";
  tx.operations.push_back( transfer );

  approving_key_set.clear();
  // two transfers from alice and one from bob
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice_active ) );
  BOOST_REQUIRE( approving_key_set.contains( bob_active ) );
  BOOST_REQUIRE( approving_key_set.contains( carol_active ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 3 );
  CHECK_CACHE( alice );
  CHECK_CACHE( bob );
  CHECK_CACHE( carol );

  BOOST_TEST_MESSAGE( "Detecting active/owner keys - many operations" );

  decline_voting_rights_operation decline_voting;
  decline_voting.account = "bob";
  tx.operations.push_back( decline_voting );

  approving_key_set.clear();
  // two transfers from alice, one from bob and declined voting from bob
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice_active ) );
  BOOST_REQUIRE( approving_key_set.contains( bob_active ) );
  BOOST_REQUIRE( approving_key_set.contains( bob_owner ) );
  BOOST_REQUIRE( approving_key_set.contains( carol_active ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 4 );
  CHECK_CACHE( alice );
  CHECK_CACHE( bob );
  CHECK_CACHE( carol );

  BOOST_TEST_MESSAGE( "Detecting single owner key of single direct account and active of indirect - single operation" );

  tx.operations.clear();
  tx.operations.push_back( decline_voting );

  approving_key_set.clear();
  // declined voting from bob
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( bob_owner ) );
  BOOST_REQUIRE( approving_key_set.contains( carol_active ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 2 );
  CHECK_NO_CACHE( alice );
  CHECK_CACHE( bob );
  CHECK_CACHE( carol );

  BOOST_TEST_MESSAGE( "Detecting many owner keys of many direct accounts and active of indirect - many operations" );

  decline_voting.account = "alice";
  decline_voting.decline = false;
  tx.operations.push_back( decline_voting );

  approving_key_set.clear();
  // declined voting from bob and alice
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( bob_owner ) );
  BOOST_REQUIRE( approving_key_set.contains( alice_owner ) );
  BOOST_REQUIRE( approving_key_set.contains( carol_active ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 3 );
  CHECK_CACHE( alice );
  CHECK_CACHE( bob );
  CHECK_CACHE( carol );

  BOOST_TEST_MESSAGE( "Detecting many posting keys of direct/indirect accounts - single operation" );

  tx.operations.clear();

  comment_operation comment;
  comment.parent_permlink = "category";
  comment.author = "alice";
  comment.permlink = "test";
  comment.title = "test";
  comment.body = "test";
  tx.operations.push_back( comment );

  approving_key_set.clear();
  // comment from alice
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice_posting ) );
  BOOST_REQUIRE( approving_key_set.contains( carol_posting ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 2 );
  CHECK_CACHE( alice );
  CHECK_NO_CACHE( bob );
  CHECK_CACHE( carol );

  BOOST_TEST_MESSAGE( "Detecting many posting keys of direct/indirect accounts - many operations" );

  comment_options_operation comment_opts;
  comment_opts.author = "alice";
  comment_opts.permlink = "test";
  comment_opts.max_accepted_payout = ASSET( "0.000 TBD" );
  tx.operations.push_back( comment_opts );

  approving_key_set.clear();
  // comment and comment options from alice
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice_posting ) );
  BOOST_REQUIRE( approving_key_set.contains( carol_posting ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 2 );
  CHECK_CACHE( alice );
  CHECK_NO_CACHE( bob );
  CHECK_CACHE( carol );

  BOOST_TEST_MESSAGE( "Detecting many posting keys of many direct/indirect accounts - many operations" );

  vote_operation vote;
  vote.voter = "bob";
  vote.author = "alice";
  vote.permlink = "test";
  vote.weight = HIVE_100_PERCENT;
  tx.operations.push_back( vote );

  approving_key_set.clear();
  // comment and comment options from alice, vote from bob
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice_posting ) );
  BOOST_REQUIRE( approving_key_set.contains( bob_posting ) );
  BOOST_REQUIRE( approving_key_set.contains( carol_posting ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 3 );
  CHECK_CACHE( alice );
  CHECK_CACHE( bob );
  CHECK_CACHE( carol );

  BOOST_TEST_MESSAGE( "Detecting many active/posting keys - many operations" );

  transfer.to = "alice";
  transfer.memo = "tip";
  tx.operations.push_back( transfer );

  approving_key_set.clear();
  // comment and comment options from alice, vote and transfer from bob
  // even though such mix is not allowed, it should not cause problems when keys are selected
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice_posting ) );
  BOOST_REQUIRE( approving_key_set.contains( bob_posting ) );
  BOOST_REQUIRE( approving_key_set.contains( carol_posting ) );
  BOOST_REQUIRE( approving_key_set.contains( bob_active ) );
  BOOST_REQUIRE( approving_key_set.contains( carol_active ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 5 );
  CHECK_CACHE( alice );
  CHECK_CACHE( bob );
  CHECK_CACHE( carol );
}

BOOST_AUTO_TEST_CASE( autodetect_keys_indirect_2_of_2 )
{
  auto fee = db->get_witness_schedule_object().median_props.account_creation_fee;

  KEYS( alice ); CREATE_ACCOUNT( alice ); fund( "alice", ASSET( "1.000 TESTS" ) );
  KEYS( bob ); CREATE_ACCOUNT( bob ); fund( "bob", ASSET( "1.000 TESTS" ) );
  KEYS( carol ); CREATE_ACCOUNT( carol ); fund( "carol", ASSET( "1.000 TESTS" ) );
  generate_block();

  account_update( "alice", alice_memo, "", authority( 2, alice_owner, 1, "carol", 1 ),
    authority( 2, alice_active, 1, "carol", 1 ), authority( 2, alice_posting, 1, "carol", 1 ),
    generate_private_key( "alice_owner" ) );
  account_update( "bob", bob_memo, "", authority( 2, bob_owner, 1, "carol", 1 ),
    authority( 2, bob_active, 1, "carol", 1 ), authority( 2, bob_posting, 1, "carol", 1 ),
    generate_private_key( "bob_owner" ) );
  generate_block();

  test_key_collector collector( *db );
  signed_transaction tx;
  flat_set<public_key_type> approving_key_set;

  BOOST_TEST_MESSAGE( "Detecting many active keys of direct/indirect accounts - single operation" );

  transfer_operation transfer;
  transfer.from = "alice";
  transfer.to = "bob";
  transfer.amount = ASSET( "0.010 TESTS" );
  tx.operations.push_back( transfer );

  approving_key_set.clear();
  // transfer from alice
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice_active ) );
  BOOST_REQUIRE( approving_key_set.contains( carol_active ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 2 );
  CHECK_CACHE( alice );
  CHECK_NO_CACHE( bob );
  CHECK_CACHE( carol );

  BOOST_TEST_MESSAGE( "Detecting many active keys of direct/indirect accounts - many operations" );

  transfer.to = "initminer";
  tx.operations.push_back( transfer );

  approving_key_set.clear();
  // two transfers from alice
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice_active ) );
  BOOST_REQUIRE( approving_key_set.contains( carol_active ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 2 );
  CHECK_CACHE( alice );
  CHECK_NO_CACHE( bob );
  CHECK_CACHE( carol );

  BOOST_TEST_MESSAGE( "Detecting many active keys of many accounts - many operations" );

  transfer.from = "bob";
  tx.operations.push_back( transfer );

  approving_key_set.clear();
  // two transfers from alice and one from bob
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice_active ) );
  BOOST_REQUIRE( approving_key_set.contains( bob_active ) );
  BOOST_REQUIRE( approving_key_set.contains( carol_active ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 3 );
  CHECK_CACHE( alice );
  CHECK_CACHE( bob );
  CHECK_CACHE( carol );

  BOOST_TEST_MESSAGE( "Detecting active/owner keys - many operations" );

  decline_voting_rights_operation decline_voting;
  decline_voting.account = "bob";
  tx.operations.push_back( decline_voting );

  approving_key_set.clear();
  // two transfers from alice, one from bob and declined voting from bob
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice_active ) );
  BOOST_REQUIRE( approving_key_set.contains( bob_active ) );
  BOOST_REQUIRE( approving_key_set.contains( bob_owner ) );
  BOOST_REQUIRE( approving_key_set.contains( carol_active ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 4 );
  CHECK_CACHE( alice );
  CHECK_CACHE( bob );
  CHECK_CACHE( carol );

  BOOST_TEST_MESSAGE( "Detecting single owner key of single direct account and active of indirect - single operation" );

  tx.operations.clear();
  tx.operations.push_back( decline_voting );

  approving_key_set.clear();
  // declined voting from bob
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( bob_owner ) );
  BOOST_REQUIRE( approving_key_set.contains( carol_active ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 2 );
  CHECK_NO_CACHE( alice );
  CHECK_CACHE( bob );
  CHECK_CACHE( carol );

  BOOST_TEST_MESSAGE( "Detecting many owner keys of many direct accounts and active of indirect - many operations" );

  decline_voting.account = "alice";
  decline_voting.decline = false;
  tx.operations.push_back( decline_voting );

  approving_key_set.clear();
  // declined voting from bob and alice
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( bob_owner ) );
  BOOST_REQUIRE( approving_key_set.contains( alice_owner ) );
  BOOST_REQUIRE( approving_key_set.contains( carol_active ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 3 );
  CHECK_CACHE( alice );
  CHECK_CACHE( bob );
  CHECK_CACHE( carol );

  BOOST_TEST_MESSAGE( "Detecting many posting keys of direct/indirect accounts - single operation" );

  tx.operations.clear();

  comment_operation comment;
  comment.parent_permlink = "category";
  comment.author = "alice";
  comment.permlink = "test";
  comment.title = "test";
  comment.body = "test";
  tx.operations.push_back( comment );

  approving_key_set.clear();
  // comment from alice
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice_posting ) );
  BOOST_REQUIRE( approving_key_set.contains( carol_posting ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 2 );
  CHECK_CACHE( alice );
  CHECK_NO_CACHE( bob );
  CHECK_CACHE( carol );

  BOOST_TEST_MESSAGE( "Detecting many posting keys of direct/indirect accounts - many operations" );

  comment_options_operation comment_opts;
  comment_opts.author = "alice";
  comment_opts.permlink = "test";
  comment_opts.max_accepted_payout = ASSET( "0.000 TBD" );
  tx.operations.push_back( comment_opts );

  approving_key_set.clear();
  // comment and comment options from alice
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice_posting ) );
  BOOST_REQUIRE( approving_key_set.contains( carol_posting ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 2 );
  CHECK_CACHE( alice );
  CHECK_NO_CACHE( bob );
  CHECK_CACHE( carol );

  BOOST_TEST_MESSAGE( "Detecting many posting keys of many direct/indirect accounts - many operations" );

  vote_operation vote;
  vote.voter = "bob";
  vote.author = "alice";
  vote.permlink = "test";
  vote.weight = HIVE_100_PERCENT;
  tx.operations.push_back( vote );

  approving_key_set.clear();
  // comment and comment options from alice, vote from bob
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice_posting ) );
  BOOST_REQUIRE( approving_key_set.contains( bob_posting ) );
  BOOST_REQUIRE( approving_key_set.contains( carol_posting ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 3 );
  CHECK_CACHE( alice );
  CHECK_CACHE( bob );
  CHECK_CACHE( carol );

  BOOST_TEST_MESSAGE( "Detecting many active/posting keys - many operations" );

  transfer.to = "alice";
  transfer.memo = "tip";
  tx.operations.push_back( transfer );

  approving_key_set.clear();
  // comment and comment options from alice, vote and transfer from bob
  // even though such mix is not allowed, it should not cause problems when keys are selected
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice_posting ) );
  BOOST_REQUIRE( approving_key_set.contains( bob_posting ) );
  BOOST_REQUIRE( approving_key_set.contains( carol_posting ) );
  BOOST_REQUIRE( approving_key_set.contains( bob_active ) );
  BOOST_REQUIRE( approving_key_set.contains( carol_active ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 5 );
  CHECK_CACHE( alice );
  CHECK_CACHE( bob );
  CHECK_CACHE( carol );
}

BOOST_AUTO_TEST_CASE( autodetect_keys_circular_and_deep )
{
  auto fee = db->get_witness_schedule_object().median_props.account_creation_fee;

  KEYS( alice ); CREATE_ACCOUNT( alice ); fund( "alice", ASSET( "1.000 TESTS" ) );
  KEYS( bob ); CREATE_ACCOUNT( bob ); fund( "bob", ASSET( "1.000 TESTS" ) );
  KEYS( carol ); CREATE_ACCOUNT( carol ); fund( "carol", ASSET( "1.000 TESTS" ) );
  KEYS( dan ); CREATE_ACCOUNT( dan ); fund( "dan", ASSET( "1.000 TESTS" ) );
  generate_block();

  // one shallow circle alice <-> bob
  // one deep circle alice -> bob -> carol -> dan -> alice
  account_update( "alice", alice_memo, "", authority( 1, alice_owner, 1, "bob", 1 ),
    authority( 1, alice_active, 1, "bob", 1 ), authority( 1, alice_posting, 1, "bob", 1 ),
    generate_private_key( "alice_owner" ) );
  account_update( "bob", bob_memo, "", authority( 1, bob_owner, 1, "alice", 1, "carol", 1 ),
    authority( 1, bob_active, 1, "alice", 1, "carol", 1 ),
    authority( 1, bob_posting, 1, "alice", 1, "carol", 1 ),
    generate_private_key( "bob_owner" ) );
  account_update( "carol", carol_memo, "", authority( 1, carol_owner, 1, "dan", 1 ),
    authority( 1, carol_active, 1, "dan", 1 ), authority( 1, carol_posting, 1, "dan", 1 ),
    generate_private_key( "carol_owner" ) );
  account_update( "dan", dan_memo, "", authority( 1, dan_owner, 1, "alice", 1 ),
    authority( 1, dan_active, 1, "alice", 1 ), authority( 1, dan_posting, 1, "alice", 1 ),
    generate_private_key( "dan_owner" ) );
  generate_block();

  test_key_collector collector( *db );
  signed_transaction tx;
  flat_set<public_key_type> approving_key_set;

  BOOST_TEST_MESSAGE( "Detecting many active keys of direct/indirect accounts - single operation" );

  transfer_operation transfer;
  transfer.from = "alice";
  transfer.to = "bob";
  transfer.amount = ASSET( "0.010 TESTS" );
  tx.operations.push_back( transfer );

  approving_key_set.clear();
  // transfer from alice
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice_active ) );
  BOOST_REQUIRE( approving_key_set.contains( bob_active ) );
  BOOST_REQUIRE( approving_key_set.contains( carol_active ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 3 );
  CHECK_CACHE( alice );
  CHECK_CACHE( bob );
  CHECK_CACHE( carol );
  CHECK_NO_CACHE( dan );

  BOOST_TEST_MESSAGE( "Detecting many active keys of direct/indirect accounts - many operations" );

  transfer.to = "initminer";
  tx.operations.push_back( transfer );

  approving_key_set.clear();
  // two transfers from alice
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice_active ) );
  BOOST_REQUIRE( approving_key_set.contains( bob_active ) );
  BOOST_REQUIRE( approving_key_set.contains( carol_active ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 3 );
  CHECK_CACHE( alice );
  CHECK_CACHE( bob );
  CHECK_CACHE( carol );
  CHECK_NO_CACHE( dan );

  BOOST_TEST_MESSAGE( "Detecting many active keys of many accounts - many operations" );

  transfer.from = "bob";
  tx.operations.push_back( transfer );

  approving_key_set.clear();
  // two transfers from alice and one from bob
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice_active ) );
  BOOST_REQUIRE( approving_key_set.contains( bob_active ) );
  BOOST_REQUIRE( approving_key_set.contains( carol_active ) );
  BOOST_REQUIRE( approving_key_set.contains( dan_active ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 4 );
  CHECK_CACHE( alice );
  CHECK_CACHE( bob );
  CHECK_CACHE( carol );
  CHECK_CACHE( dan );

  BOOST_TEST_MESSAGE( "Detecting active/owner keys - many operations" );

  decline_voting_rights_operation decline_voting;
  decline_voting.account = "bob";
  tx.operations.push_back( decline_voting );

  approving_key_set.clear();
  // two transfers from alice, one from bob and declined voting from bob
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice_active ) );
  BOOST_REQUIRE( approving_key_set.contains( bob_active ) );
  BOOST_REQUIRE( approving_key_set.contains( bob_owner ) );
  BOOST_REQUIRE( approving_key_set.contains( carol_active ) );
  BOOST_REQUIRE( approving_key_set.contains( dan_active ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 5 );
  CHECK_CACHE( alice );
  CHECK_CACHE( bob );
  CHECK_CACHE( carol );
  CHECK_CACHE( dan );

  BOOST_TEST_MESSAGE( "Detecting single owner key of single direct account and active of indirect - single operation" );

  tx.operations.clear();
  tx.operations.push_back( decline_voting );

  approving_key_set.clear();
  // declined voting from bob
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( bob_owner ) );
  BOOST_REQUIRE( approving_key_set.contains( alice_active ) );
  BOOST_REQUIRE( approving_key_set.contains( bob_active ) );
  BOOST_REQUIRE( approving_key_set.contains( carol_active ) );
  BOOST_REQUIRE( approving_key_set.contains( dan_active ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 5 );
  CHECK_CACHE( alice );
  CHECK_CACHE( bob );
  CHECK_CACHE( carol );
  CHECK_CACHE( dan );

  BOOST_TEST_MESSAGE( "Detecting many owner keys of many direct accounts and active of indirect - many operations" );

  decline_voting.account = "alice";
  decline_voting.decline = false;
  tx.operations.push_back( decline_voting );

  approving_key_set.clear();
  // declined voting from bob and alice
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( bob_owner ) );
  BOOST_REQUIRE( approving_key_set.contains( alice_owner ) );
  BOOST_REQUIRE( approving_key_set.contains( alice_active ) );
  BOOST_REQUIRE( approving_key_set.contains( bob_active ) );
  BOOST_REQUIRE( approving_key_set.contains( carol_active ) );
  BOOST_REQUIRE( approving_key_set.contains( dan_active ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 6 );
  CHECK_CACHE( alice );
  CHECK_CACHE( bob );
  CHECK_CACHE( carol );
  CHECK_CACHE( dan );

  BOOST_TEST_MESSAGE( "Detecting many posting keys of direct/indirect accounts - single operation" );

  tx.operations.clear();

  comment_operation comment;
  comment.parent_permlink = "category";
  comment.author = "alice";
  comment.permlink = "test";
  comment.title = "test";
  comment.body = "test";
  tx.operations.push_back( comment );

  approving_key_set.clear();
  // comment from alice
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice_posting ) );
  BOOST_REQUIRE( approving_key_set.contains( bob_posting ) );
  BOOST_REQUIRE( approving_key_set.contains( carol_posting ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 3 );
  CHECK_CACHE( alice );
  CHECK_CACHE( bob );
  CHECK_CACHE( carol );
  CHECK_NO_CACHE( dan );

  BOOST_TEST_MESSAGE( "Detecting many posting keys of direct/indirect accounts - many operations" );

  comment_options_operation comment_opts;
  comment_opts.author = "alice";
  comment_opts.permlink = "test";
  comment_opts.max_accepted_payout = ASSET( "0.000 TBD" );
  tx.operations.push_back( comment_opts );

  approving_key_set.clear();
  // comment and comment options from alice
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice_posting ) );
  BOOST_REQUIRE( approving_key_set.contains( bob_posting ) );
  BOOST_REQUIRE( approving_key_set.contains( carol_posting ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 3 );
  CHECK_CACHE( alice );
  CHECK_CACHE( bob );
  CHECK_CACHE( carol );
  CHECK_NO_CACHE( dan );

  BOOST_TEST_MESSAGE( "Detecting many posting keys of many direct/indirect accounts - many operations" );

  vote_operation vote;
  vote.voter = "bob";
  vote.author = "alice";
  vote.permlink = "test";
  vote.weight = HIVE_100_PERCENT;
  tx.operations.push_back( vote );

  approving_key_set.clear();
  // comment and comment options from alice, vote from bob
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice_posting ) );
  BOOST_REQUIRE( approving_key_set.contains( bob_posting ) );
  BOOST_REQUIRE( approving_key_set.contains( carol_posting ) );
  BOOST_REQUIRE( approving_key_set.contains( dan_posting ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 4 );
  CHECK_CACHE( alice );
  CHECK_CACHE( bob );
  CHECK_CACHE( carol );
  CHECK_CACHE( dan );

  BOOST_TEST_MESSAGE( "Detecting many active/posting keys - many operations" );

  transfer.to = "alice";
  transfer.memo = "tip";
  tx.operations.push_back( transfer );

  approving_key_set.clear();
  // comment and comment options from alice, vote and transfer from bob
  // even though such mix is not allowed, it should not cause problems when keys are selected
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( alice_posting ) );
  BOOST_REQUIRE( approving_key_set.contains( bob_posting ) );
  BOOST_REQUIRE( approving_key_set.contains( carol_posting ) );
  BOOST_REQUIRE( approving_key_set.contains( dan_posting ) );
  BOOST_REQUIRE( approving_key_set.contains( bob_active ) );
  BOOST_REQUIRE( approving_key_set.contains( alice_active ) );
  BOOST_REQUIRE( approving_key_set.contains( carol_active ) );
  BOOST_REQUIRE( approving_key_set.contains( dan_active ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 8 );
  CHECK_CACHE( alice );
  CHECK_CACHE( bob );
  CHECK_CACHE( carol );
  CHECK_CACHE( dan );
}

BOOST_AUTO_TEST_CASE( autodetect_keys_invalid )
{
  auto fee = db->get_witness_schedule_object().median_props.account_creation_fee;

  KEYS( alice ); CREATE_ACCOUNT( alice ); fund( "alice", ASSET( "1.000 TESTS" ) );
  KEYS( bob ); CREATE_ACCOUNT( bob ); fund( "bob", ASSET( "1.000 TESTS" ) );
  KEYS( carol ); CREATE_ACCOUNT( carol ); fund( "carol", ASSET( "1.000 TESTS" ) );
  KEYS( dan ); CREATE_ACCOUNT( dan ); fund( "dan", ASSET( "1.000 TESTS" ) );
  generate_block();

  // one shallow circle alice <-> bob but not on active level and no keys
  // one deep circle alice -> bob -> carol -> dan -> alice but not on active level and no keys
  optional<authority> no_auth;
  account_update( "alice", alice_memo, "", authority( 1, "bob", 1 ),
    no_auth, authority( 1, "bob", 1 ), generate_private_key( "alice_owner" ) );
  account_update( "bob", bob_memo, "", authority( 1, "alice", 1, "carol", 1 ),
    no_auth, authority( 1, "alice", 1, "carol", 1 ), generate_private_key( "bob_owner" ) );
  account_update( "carol", carol_memo, "", authority( 1, "dan", 1 ),
    no_auth, authority( 1, "dan", 1 ), generate_private_key( "carol_owner" ) );
  account_update( "dan", dan_memo, "", authority( 1, "alice", 1 ),
    no_auth, authority( 1, "alice", 1 ), generate_private_key( "dan_owner" ) );
  generate_block();

  test_key_collector collector( *db );
  signed_transaction tx;
  flat_set<public_key_type> approving_key_set;

  BOOST_TEST_MESSAGE( "Failure in detection due to ill formed account name" );

  transfer_operation transfer;
  transfer.from = "ALICE";
  transfer.to = "bob";
  transfer.amount = ASSET( "0.010 TESTS" );
  tx.operations.push_back( transfer );

  approving_key_set.clear();
  // transfer from ALICE
  HIVE_REQUIRE_ASSERT( collector.collect_signing_keys( &approving_key_set, tx ), "authorities != nullptr" );

  BOOST_TEST_MESSAGE( "Failure in detection due to unknown account" );

  tx.operations.clear();
  transfer.from = "unknown";
  tx.operations.push_back( transfer );

  approving_key_set.clear();
  // transfer from unknown
  HIVE_REQUIRE_ASSERT( collector.collect_signing_keys( &approving_key_set, tx ), "authorities != nullptr" );

  BOOST_TEST_MESSAGE( "Detecting no posting key since there is none - single operation" );

  tx.operations.clear();

  comment_operation comment;
  comment.parent_permlink = "category";
  comment.author = "alice";
  comment.permlink = "test";
  comment.title = "test";
  comment.body = "test";
  tx.operations.push_back( comment );

  approving_key_set.clear();
  // comment from alice
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 0 ); // we could be signing with active, but autodetect does not work that way
  CHECK_CACHE( alice );
  CHECK_CACHE( bob );
  CHECK_CACHE( carol );
  CHECK_NO_CACHE( dan );

  BOOST_TEST_MESSAGE( "Detecting only indirect active key for owner operation - single operation" );

  tx.operations.clear();

  decline_voting_rights_operation decline_voting;
  decline_voting.account = "alice";
  tx.operations.push_back( decline_voting );

  approving_key_set.clear();
  // declined voting by alice
  collector.collect_signing_keys( &approving_key_set, tx );
  BOOST_REQUIRE( approving_key_set.contains( bob_active ) );
  BOOST_REQUIRE_EQUAL( approving_key_set.size(), 1 );
  CHECK_CACHE( alice );
  CHECK_CACHE( bob );
  CHECK_NO_CACHE( carol );
  CHECK_NO_CACHE( dan );
}

BOOST_AUTO_TEST_SUITE_END()
