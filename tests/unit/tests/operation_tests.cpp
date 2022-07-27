#ifdef IS_TEST_NET
#include <boost/test/unit_test.hpp>
#include <boost/preprocessor/repetition/repeat.hpp>

#include <hive/chain/hive_fwd.hpp>

#include <hive/protocol/exceptions.hpp>
#include <hive/protocol/hardfork.hpp>

#include <hive/chain/database.hpp>
#include <hive/chain/database_exceptions.hpp>
#include <hive/chain/hive_objects.hpp>

#include <hive/chain/util/reward.hpp>

#include <hive/plugins/rc/rc_objects.hpp>
#include <hive/plugins/rc/resource_count.hpp>

#include <fc/macros.hpp>
#include <fc/crypto/digest.hpp>

#include "../db_fixture/database_fixture.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace hive;
using namespace hive::chain;
using namespace hive::protocol;
using fc::string;

#define VOTING_MANABAR( account_name ) db->get_account( account_name ).voting_manabar
#define DOWNVOTE_MANABAR( account_name ) db->get_account( account_name ).downvote_manabar
#define CHECK_PROXY( account, proxy ) BOOST_REQUIRE( account.get_proxy() == proxy.get_id() )
#define CHECK_NO_PROXY( account ) BOOST_REQUIRE( account.has_proxy() == false )

inline uint16_t get_voting_power( const account_object& a )
{
  return (uint16_t)( a.voting_manabar.current_mana / chain::util::get_effective_vesting_shares( a ) );
}

BOOST_FIXTURE_TEST_SUITE( operation_tests, clean_database_fixture )

BOOST_AUTO_TEST_CASE( account_create_validate )
{
  try
  {

  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( account_create_authorities )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: account_create_authorities" );

    account_create_operation op;
    op.creator = "alice";
    op.new_account_name = "bob";

    flat_set< account_name_type > auths;
    flat_set< account_name_type > expected;

    BOOST_TEST_MESSAGE( "--- Testing owner authority" );
    op.get_required_owner_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    BOOST_TEST_MESSAGE( "--- Testing active authority" );
    expected.insert( "alice" );
    op.get_required_active_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    BOOST_TEST_MESSAGE( "--- Testing posting authority" );
    expected.clear();
    auths.clear();
    op.get_required_posting_authorities( auths );
    BOOST_REQUIRE( auths == expected );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( account_create_apply )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: account_create_apply" );

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );

    db_plugin->debug_update( [=]( database& db )
    {
      db.modify( db.get_witness_schedule_object(), [&]( witness_schedule_object& wso )
      {
        wso.median_props.account_creation_fee = ASSET( "0.100 TESTS" );
      });
    });
    generate_block();

    signed_transaction tx;
    private_key_type priv_key = generate_private_key( "alice" );

    const account_object& init = db->get_account( HIVE_INIT_MINER_NAME );
    asset init_starting_balance = init.get_balance();

    account_create_operation op;

    op.new_account_name = "alice";
    op.creator = HIVE_INIT_MINER_NAME;
    op.owner = authority( 1, priv_key.get_public_key(), 1 );
    op.active = authority( 2, priv_key.get_public_key(), 2 );
    op.memo_key = priv_key.get_public_key();
    op.json_metadata = "{\"foo\":\"bar\"}";

    BOOST_TEST_MESSAGE( "--- Test failure paying more than the fee" );
    op.fee = asset( 101, HIVE_SYMBOL );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.operations.push_back( op );
    tx.validate();
    HIVE_REQUIRE_THROW( push_transaction( tx, init_account_priv_key, 0 ), fc::assert_exception );

    BOOST_TEST_MESSAGE( "--- Test normal account creation" );
    op.fee = asset( 100, HIVE_SYMBOL );
    tx.clear();
    tx.operations.push_back( op );
    tx.validate();
    push_transaction( tx, init_account_priv_key, 0 );

    const account_object& acct = db->get_account( "alice" );
    const account_authority_object& acct_auth = db->get< account_authority_object, by_account >( "alice" );

    BOOST_REQUIRE( acct.name == "alice" );
    BOOST_REQUIRE( acct_auth.owner == authority( 1, priv_key.get_public_key(), 1 ) );
    BOOST_REQUIRE( acct_auth.active == authority( 2, priv_key.get_public_key(), 2 ) );
    BOOST_REQUIRE( acct.memo_key == priv_key.get_public_key() );
    CHECK_NO_PROXY( acct );
    BOOST_REQUIRE( acct.created == db->head_block_time() );
    BOOST_REQUIRE( acct.get_balance().amount.value == ASSET( "0.000 TESTS" ).amount.value );
    BOOST_REQUIRE( acct.get_hbd_balance().amount.value == ASSET( "0.000 TBD" ).amount.value );
    BOOST_REQUIRE( acct.get_id().get_value() == acct_auth.get_id().get_value() );

    BOOST_REQUIRE( acct.get_vesting().amount.value == 0 );
    BOOST_REQUIRE( acct.vesting_withdraw_rate.amount.value == ASSET( "0.000000 VESTS" ).amount.value );
    BOOST_REQUIRE( acct.proxied_vsf_votes_total().value == 0 );
    BOOST_REQUIRE( ( init_starting_balance - ASSET( "0.100 TESTS" ) ).amount.value == init.get_balance().amount.value );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test failure of duplicate account creation" );
    BOOST_REQUIRE_THROW( push_transaction( tx, database::skip_transaction_dupe_check ), fc::exception );

    BOOST_REQUIRE( acct.name == "alice" );
    BOOST_REQUIRE( acct_auth.owner == authority( 1, priv_key.get_public_key(), 1 ) );
    BOOST_REQUIRE( acct_auth.active == authority( 2, priv_key.get_public_key(), 2 ) );
    BOOST_REQUIRE( acct.memo_key == priv_key.get_public_key() );
    CHECK_NO_PROXY( acct );
    BOOST_REQUIRE( acct.created == db->head_block_time() );
    BOOST_REQUIRE( acct.get_balance().amount.value == ASSET( "0.000 TESTS " ).amount.value );
    BOOST_REQUIRE( acct.get_hbd_balance().amount.value == ASSET( "0.000 TBD" ).amount.value );
    BOOST_REQUIRE( acct.get_vesting().amount.value == 0 );
    BOOST_REQUIRE( acct.vesting_withdraw_rate.amount.value == ASSET( "0.000000 VESTS" ).amount.value );
    BOOST_REQUIRE( acct.proxied_vsf_votes_total().value == 0 );
    BOOST_REQUIRE( ( init_starting_balance - ASSET( "0.100 TESTS" ) ).amount.value == init.get_balance().amount.value );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test failure when creator cannot cover fee" );
    tx.signatures.clear();
    tx.operations.clear();
    op.fee = asset( get_balance( HIVE_INIT_MINER_NAME ).amount + 1, HIVE_SYMBOL );
    op.new_account_name = "bob";
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, init_account_priv_key, 0 ), fc::exception );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test failure covering witness fee" );
    generate_block();
    db_plugin->debug_update( [=]( database& db )
    {
      db.modify( db.get_witness_schedule_object(), [&]( witness_schedule_object& wso )
      {
        wso.median_props.account_creation_fee = ASSET( "10.000 TESTS" );
      });
    });
    generate_block();

    tx.clear();
    op.fee = ASSET( "0.100 TESTS" );
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, init_account_priv_key, 0 ), fc::exception );
    validate_database();

    fund( HIVE_TEMP_ACCOUNT, ASSET( "10.000 TESTS" ) );
    vest( HIVE_INIT_MINER_NAME, HIVE_TEMP_ACCOUNT, ASSET( "10.000 TESTS" ) );

    BOOST_TEST_MESSAGE( "--- Test account creation with temp account does not set recovery account" );
    op.creator = HIVE_TEMP_ACCOUNT;
    op.fee = ASSET( "10.000 TESTS" );
    op.new_account_name = "bob";
    tx.clear();
    tx.operations.push_back( op );
    push_transaction( tx, 0 );

    BOOST_REQUIRE( !db->get_account( "bob" ).has_recovery_account() );
    validate_database();

  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( account_update_validate )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: account_update_validate" );

    ACTORS( (alice) )

    account_update_operation op;
    op.account = "alice";
    op.posting = authority();
    op.posting->weight_threshold = 1;
    op.posting->add_authorities( "abcdefghijklmnopq", 1 );

    try
    {
      op.validate();

      signed_transaction tx;
      tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
      tx.operations.push_back( op );
      push_transaction( tx, alice_private_key, 0 );

      BOOST_FAIL( "An exception was not thrown for an invalid account name" );
    }
    catch( fc::exception& ) {}

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( account_update_authorities )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: account_update_authorities" );

    ACTORS( (alice)(bob) )
    private_key_type active_key = generate_private_key( "new_key" );

    db->modify( db->get< account_authority_object, by_account >( "alice" ), [&]( account_authority_object& a )
    {
      a.active = authority( 1, active_key.get_public_key(), 1 );
    });

    account_update_operation op;
    op.account = "alice";
    op.json_metadata = "{\"success\":true}";

    signed_transaction tx;
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );

    BOOST_TEST_MESSAGE( "  Tests when owner authority is not updated ---" );
    BOOST_TEST_MESSAGE( "--- Test failure when no signature" );
    HIVE_REQUIRE_THROW( push_transaction( tx, 0 ), tx_missing_active_auth );

    BOOST_TEST_MESSAGE( "--- Test failure when wrong signature" );
    HIVE_REQUIRE_THROW( push_transaction( tx, bob_private_key, 0 ), tx_missing_active_auth );

    BOOST_TEST_MESSAGE( "--- Test failure when containing additional incorrect signature" );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), tx_irrelevant_sig );

    BOOST_TEST_MESSAGE( "--- Test failure when containing duplicate signatures" );
    tx.signatures.clear();
    sign( tx, active_key );
    sign( tx, active_key );
    HIVE_REQUIRE_THROW( push_transaction( tx, 0 ), tx_duplicate_sig );

    BOOST_TEST_MESSAGE( "--- Test success on active key" );
    tx.signatures.clear();
    push_transaction( tx, active_key, 0 );

    BOOST_TEST_MESSAGE( "--- Test success on owner key alone" );
    tx.signatures.clear();
    push_transaction( tx, alice_private_key, database::skip_transaction_dupe_check );

    BOOST_TEST_MESSAGE( "  Tests when owner authority is updated ---" );
    BOOST_TEST_MESSAGE( "--- Test failure when updating the owner authority with an active key" );
    tx.signatures.clear();
    tx.operations.clear();
    op.owner = authority( 1, active_key.get_public_key(), 1 );
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, active_key, 0 ), tx_missing_owner_auth );

    BOOST_TEST_MESSAGE( "--- Test failure when owner key and active key are present" );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), tx_irrelevant_sig );

    BOOST_TEST_MESSAGE( "--- Test failure when incorrect signature" );
    tx.signatures.clear();
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_post_key, 0), tx_missing_owner_auth );

    BOOST_TEST_MESSAGE( "--- Test failure when duplicate owner keys are present" );
    tx.signatures.clear();
    sign( tx, alice_private_key );
    sign( tx, alice_private_key );
    HIVE_REQUIRE_THROW( push_transaction( tx, 0), tx_duplicate_sig );

    BOOST_TEST_MESSAGE( "--- Test success when updating the owner authority with an owner key" );
    tx.signatures.clear();
    push_transaction( tx, alice_private_key, 0 );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( account_update_apply )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: account_update_apply" );

    ACTORS( (alice) )
    private_key_type new_private_key = generate_private_key( "new_key" );

    BOOST_TEST_MESSAGE( "--- Test normal update" );

    account_update_operation op;
    op.account = "alice";
    op.owner = authority( 1, new_private_key.get_public_key(), 1 );
    op.active = authority( 2, new_private_key.get_public_key(), 2 );
    op.memo_key = new_private_key.get_public_key();
    op.json_metadata = "{\"bar\":\"foo\"}";

    signed_transaction tx;
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    push_transaction( tx, alice_private_key, 0 );

    const account_object& acct = db->get_account( "alice" );
    const account_authority_object& acct_auth = db->get< account_authority_object, by_account >( "alice" );

    BOOST_REQUIRE( acct.name == "alice" );
    BOOST_REQUIRE( acct_auth.owner == authority( 1, new_private_key.get_public_key(), 1 ) );
    BOOST_REQUIRE( acct_auth.active == authority( 2, new_private_key.get_public_key(), 2 ) );
    BOOST_REQUIRE( acct.memo_key == new_private_key.get_public_key() );

    /* This is being moved out of consensus
      BOOST_REQUIRE( acct.json_metadata == "{\"bar\":\"foo\"}" );
    */

    validate_database();

    BOOST_TEST_MESSAGE( "--- Test failure when updating a non-existent account" );
    tx.operations.clear();
    tx.signatures.clear();
    op.account = "bob";
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, new_private_key, 0 ), fc::exception )
    validate_database();


    BOOST_TEST_MESSAGE( "--- Test failure when account authority does not exist" );
    tx.clear();
    op = account_update_operation();
    op.account = "alice";
    op.posting = authority();
    op.posting->weight_threshold = 1;
    op.posting->add_authorities( "dave", 1 );
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, new_private_key, 0 ), fc::exception );
    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( comment_validate )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: comment_validate" );


    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( comment_authorities )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: comment_authorities" );

    ACTORS( (alice)(bob) );
    generate_blocks( 60 / HIVE_BLOCK_INTERVAL );

    comment_operation op;
    op.author = "alice";
    op.permlink = "lorem";
    op.parent_author = "";
    op.parent_permlink = "ipsum";
    op.title = "Lorem Ipsum";
    op.body = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.";
    op.json_metadata = "{\"foo\":\"bar\"}";

    signed_transaction tx;
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );

    BOOST_TEST_MESSAGE( "--- Test failure when no signatures" );
    HIVE_REQUIRE_THROW( push_transaction( tx, 0 ), tx_missing_posting_auth );

    BOOST_TEST_MESSAGE( "--- Test failure when duplicate signatures" );
    sign( tx, alice_post_key );
    sign( tx, alice_post_key );
    HIVE_REQUIRE_THROW( push_transaction( tx, 0 ), tx_duplicate_sig );

    BOOST_TEST_MESSAGE( "--- Test success with post signature" );
    tx.signatures.clear();
    push_transaction( tx, alice_post_key, 0 );

    BOOST_TEST_MESSAGE( "--- Test failure when signed by an additional signature not in the creator's authority" );
    HIVE_REQUIRE_THROW( push_transaction( tx, bob_private_key, database::skip_transaction_dupe_check ), tx_irrelevant_sig );

    BOOST_TEST_MESSAGE( "--- Test failure when signed by a signature not in the creator's authority" );
    tx.signatures.clear();
    HIVE_REQUIRE_THROW( push_transaction( tx, bob_private_key, database::skip_transaction_dupe_check ), tx_missing_posting_auth );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( comment_apply )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: comment_apply" );

    ACTORS( (alice)(bob)(sam) )
    generate_blocks( 60 / HIVE_BLOCK_INTERVAL );

    comment_operation op;
    op.author = "alice";
    op.permlink = "lorem";
    op.parent_author = "";
    op.parent_permlink = "ipsum";
    op.title = "Lorem Ipsum";
    op.body = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.";
    op.json_metadata = "{\"foo\":\"bar\"}";

    signed_transaction tx;
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );

    BOOST_TEST_MESSAGE( "--- Test Alice posting a root comment" );
    tx.operations.push_back( op );
    push_transaction( tx, alice_private_key, 0 );

    const comment_object& alice_comment = db->get_comment( "alice", string( "lorem" ) );
    const comment_cashout_object* alice_comment_cashout = db->find_comment_cashout( alice_comment );

    BOOST_REQUIRE( alice_comment.get_author_and_permlink_hash() == comment_object::compute_author_and_permlink_hash( get_account_id( "alice" ), "lorem" ) );
    BOOST_REQUIRE( alice_comment_cashout != nullptr );
    BOOST_CHECK_EQUAL( alice_comment_cashout->get_comment_id(), alice_comment.get_id() );
    BOOST_CHECK_EQUAL( alice_comment_cashout->get_author_id(), get_account_id( op.author ) );
    BOOST_CHECK_EQUAL( to_string( alice_comment_cashout->get_permlink() ), "lorem" );
    BOOST_REQUIRE( alice_comment.is_root() );
    BOOST_REQUIRE( alice_comment_cashout->get_creation_time() == db->head_block_time() );
    BOOST_REQUIRE( alice_comment_cashout->get_net_rshares() == 0 );
    BOOST_REQUIRE( alice_comment_cashout->get_cashout_time() == fc::time_point_sec( db->head_block_time() + fc::seconds( HIVE_CASHOUT_WINDOW_SECONDS ) ) );

    validate_database();

    BOOST_TEST_MESSAGE( "--- Test Bob posting a comment on a non-existent comment" );
    op.author = "bob";
    op.permlink = "ipsum";
    op.parent_author = "alice";
    op.parent_permlink = "foobar";

    tx.signatures.clear();
    tx.operations.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, bob_private_key, 0 ), fc::exception );

    BOOST_TEST_MESSAGE( "--- Test Bob posting a comment on Alice's comment" );
    op.parent_permlink = "lorem";

    tx.signatures.clear();
    tx.operations.clear();
    tx.operations.push_back( op );
    push_transaction( tx, bob_private_key, 0 );

    const comment_object& bob_comment = db->get_comment( "bob", string( "ipsum" ) );
    const comment_cashout_object* bob_comment_cashout = db->find_comment_cashout( bob_comment );

    BOOST_CHECK_EQUAL( bob_comment.get_author_and_permlink_hash(), comment_object::compute_author_and_permlink_hash( get_account_id( "bob" ), "ipsum" ) );
    BOOST_REQUIRE( bob_comment_cashout != nullptr );
    BOOST_CHECK_EQUAL( bob_comment_cashout->get_comment_id(), bob_comment.get_id() );
    BOOST_CHECK_EQUAL( bob_comment_cashout->get_author_id(), get_account_id( "bob" ) );
    BOOST_CHECK_EQUAL( to_string( bob_comment_cashout->get_permlink() ), "ipsum" );
    BOOST_REQUIRE( bob_comment.get_parent_id() == alice_comment.get_id() );
    BOOST_REQUIRE( bob_comment_cashout->get_creation_time() == db->head_block_time() );
    BOOST_REQUIRE( bob_comment_cashout->get_net_rshares() == 0 );
    BOOST_REQUIRE( bob_comment_cashout->get_cashout_time() == bob_comment_cashout->get_creation_time() + HIVE_CASHOUT_WINDOW_SECONDS );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test Sam posting a comment on Bob's comment" );

    op.author = "sam";
    op.permlink = "dolor";
    op.parent_author = "bob";
    op.parent_permlink = "ipsum";

    tx.signatures.clear();
    tx.operations.clear();
    tx.operations.push_back( op );
    push_transaction( tx, sam_private_key, 0 );

    const comment_object& sam_comment = db->get_comment( "sam", string( "dolor" ) );
    const comment_cashout_object* sam_comment_cashout = db->find_comment_cashout( sam_comment );

    BOOST_REQUIRE( sam_comment.get_author_and_permlink_hash() == comment_object::compute_author_and_permlink_hash( get_account_id( "sam" ), "dolor" ) );
    BOOST_REQUIRE( sam_comment_cashout != nullptr );
    BOOST_CHECK_EQUAL( sam_comment_cashout->get_comment_id(), sam_comment.get_id() );
    BOOST_CHECK_EQUAL( sam_comment_cashout->get_author_id(), get_account_id( "sam" ) );
    BOOST_CHECK_EQUAL( to_string( sam_comment_cashout->get_permlink() ), "dolor" );
    BOOST_REQUIRE( sam_comment.get_parent_id() == bob_comment.get_id() );
    BOOST_REQUIRE( sam_comment_cashout->get_creation_time() == db->head_block_time() );
    BOOST_REQUIRE( sam_comment_cashout->get_net_rshares() == 0 );
    BOOST_REQUIRE( sam_comment_cashout->get_cashout_time() == sam_comment_cashout->get_creation_time() + HIVE_CASHOUT_WINDOW_SECONDS );
    validate_database();

    generate_blocks( 60 * 5 / HIVE_BLOCK_INTERVAL + 1 );

    BOOST_TEST_MESSAGE( "--- Test modifying a comment" );
    const auto& mod_sam_comment = db->get_comment( "sam", string( "dolor" ) );
    const auto& mod_bob_comment = db->get_comment( "bob", string( "ipsum" ) );
    const auto& mod_alice_comment = db->get_comment( "alice", string( "lorem" ) );

    const comment_cashout_object* mod_sam_comment_cashout = db->find_comment_cashout( mod_sam_comment );

    FC_UNUSED(mod_bob_comment, mod_alice_comment);

    fc::time_point_sec created = mod_sam_comment_cashout->get_creation_time();

    db->modify( *mod_sam_comment_cashout, [&]( comment_cashout_object& com )
    {
      com.accumulate_vote_rshares( -com.get_net_rshares() + 10, 0 ); //com.net_rshares = 10;
    });

    db->modify( db->get_dynamic_global_properties(), [&]( dynamic_global_property_object& o)
    {
      o.total_reward_shares2 = hive::chain::util::evaluate_reward_curve( 10 );
    });

    tx.signatures.clear();
    tx.operations.clear();
    op.title = "foo";
    op.body = "bar";
    op.json_metadata = "{\"bar\":\"foo\"}";
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    push_transaction( tx, sam_private_key, 0 );

    BOOST_REQUIRE( mod_sam_comment.get_author_and_permlink_hash() == comment_object::compute_author_and_permlink_hash( get_account_id( "sam" ), "dolor" ) );
    BOOST_REQUIRE( mod_sam_comment_cashout != nullptr );
    BOOST_CHECK_EQUAL( mod_sam_comment_cashout->get_comment_id(), mod_sam_comment.get_id() );
    BOOST_CHECK_EQUAL( mod_sam_comment_cashout->get_author_id(), get_account_id( "sam" ) );
    BOOST_CHECK_EQUAL( to_string( mod_sam_comment_cashout->get_permlink() ), "dolor" );
    BOOST_REQUIRE( mod_sam_comment.get_parent_id() == mod_bob_comment.get_id() );
    BOOST_REQUIRE( mod_sam_comment_cashout->get_creation_time() == created );
    BOOST_REQUIRE( mod_sam_comment_cashout->get_cashout_time() == mod_sam_comment_cashout->get_creation_time() + HIVE_CASHOUT_WINDOW_SECONDS );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test comment edit rate limit" );
    op.body = "edit";
    tx.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, sam_private_key, 0 ), fc::assert_exception );
    generate_block();
    push_transaction( tx, 0 );

    BOOST_TEST_MESSAGE( "--- Test failure posting withing 1 minute" );

    op.permlink = "sit";
    op.parent_author = "";
    op.parent_permlink = "test";
    tx.operations.clear();
    tx.signatures.clear();
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.operations.push_back( op );
    push_transaction( tx, sam_private_key, 0 );

    generate_blocks( 60 * 5 / HIVE_BLOCK_INTERVAL );

    op.permlink = "amet";
    tx.operations.clear();
    tx.signatures.clear();
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, sam_private_key, 0 ), fc::exception );

    validate_database();

    generate_block();
    push_transaction( tx, 0 );
    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( comment_delete_apply )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: comment_delete_apply" );
    ACTORS( (alice) )
    generate_block();

    vest( HIVE_INIT_MINER_NAME, "alice", ASSET( "1000.000 TESTS" ) );

    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );

    signed_transaction tx;
    comment_operation comment;
    vote_operation vote;

    comment.author = "alice";
    comment.permlink = "test1";
    comment.title = "test";
    comment.body = "foo bar";
    comment.parent_permlink = "test";
    vote.voter = "alice";
    vote.author = "alice";
    vote.permlink = "test1";
    vote.weight = HIVE_100_PERCENT;
    tx.operations.push_back( comment );
    tx.operations.push_back( vote );
    tx.set_expiration( db->head_block_time() + HIVE_MIN_TRANSACTION_EXPIRATION_LIMIT );
    push_transaction( tx, alice_private_key, 0 );

    BOOST_TEST_MESSAGE( "--- Test failue deleting a comment with positive rshares" );

    delete_comment_operation op;
    op.author = "alice";
    op.permlink = "test1";
    tx.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::assert_exception );


    BOOST_TEST_MESSAGE( "--- Test success deleting a comment with negative rshares" );

    generate_block();
    vote.weight = -1 * HIVE_100_PERCENT;
    tx.clear();
    tx.operations.push_back( vote );
    tx.operations.push_back( op );
    push_transaction( tx, alice_private_key, 0 );

    auto test_comment = db->find_comment( "alice", string( "test1" ) );
    BOOST_REQUIRE( test_comment == nullptr );


    BOOST_TEST_MESSAGE( "--- Test failure deleting a comment past cashout" );
    generate_blocks( HIVE_MIN_ROOT_COMMENT_INTERVAL.to_seconds() / HIVE_BLOCK_INTERVAL );

    tx.clear();
    tx.operations.push_back( comment );
    tx.set_expiration( db->head_block_time() + HIVE_MIN_TRANSACTION_EXPIRATION_LIMIT );
    push_transaction( tx, alice_private_key, 0 );

    generate_blocks( HIVE_CASHOUT_WINDOW_SECONDS / HIVE_BLOCK_INTERVAL );

    const comment_object& _comment = db->get_comment( "alice", string( "test1" ) );
    const comment_cashout_object* _comment_cashout = db->find_comment_cashout( _comment );
    BOOST_REQUIRE( _comment_cashout == nullptr );

    tx.clear();
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MIN_TRANSACTION_EXPIRATION_LIMIT );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::assert_exception );


    BOOST_TEST_MESSAGE( "--- Test failure deleting a comment with a reply" );

    comment.permlink = "test2";
    comment.parent_author = "alice";
    comment.parent_permlink = "test1";
    tx.clear();
    tx.operations.push_back( comment );
    tx.set_expiration( db->head_block_time() + HIVE_MIN_TRANSACTION_EXPIRATION_LIMIT );
    push_transaction( tx, alice_private_key, 0 );

    generate_blocks( HIVE_MIN_ROOT_COMMENT_INTERVAL.to_seconds() / HIVE_BLOCK_INTERVAL );
    comment.permlink = "test3";
    comment.parent_permlink = "test2";
    tx.clear();
    tx.operations.push_back( comment );
    tx.set_expiration( db->head_block_time() + HIVE_MIN_TRANSACTION_EXPIRATION_LIMIT );
    push_transaction( tx, alice_private_key, 0 );

    op.permlink = "test2";
    tx.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::assert_exception );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( vote_validate )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: vote_validate" );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( vote_authorities )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: vote_authorities" );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( vote_apply )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: vote_apply" );

    ACTORS( (alice)(bob)(sam)(dave) )
    generate_block();

    vest( HIVE_INIT_MINER_NAME, "alice", ASSET( "10.000 TESTS" ) );
    validate_database();
    vest( HIVE_INIT_MINER_NAME, "bob" , ASSET( "10.000 TESTS" ) );
    vest( HIVE_INIT_MINER_NAME, "sam" , ASSET( "10.000 TESTS" ) );
    vest( HIVE_INIT_MINER_NAME, "dave" , ASSET( "10.000 TESTS" ) );
    generate_block();

    const auto& vote_idx = db->get_index< comment_vote_index >().indices().get< by_comment_voter >();

    {
      const auto& alice = db->get_account( "alice" );

      signed_transaction tx;
      comment_operation comment_op;
      comment_op.author = "alice";
      comment_op.permlink = "foo";
      comment_op.parent_permlink = "test";
      comment_op.title = "bar";
      comment_op.body = "foo bar";
      tx.operations.push_back( comment_op );
      tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
      push_transaction( tx, alice_private_key, 0 );

      BOOST_TEST_MESSAGE( "--- Testing voting on a non-existent comment" );

      tx.operations.clear();
      tx.signatures.clear();

      vote_operation op;
      op.voter = "alice";
      op.author = "bob";
      op.permlink = "foo";
      op.weight = HIVE_100_PERCENT;
      tx.operations.push_back( op );

      HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );

      validate_database();

      BOOST_TEST_MESSAGE( "--- Testing voting with a weight of 0" );

      op.weight = (int16_t) 0;
      tx.operations.clear();
      tx.signatures.clear();
      tx.operations.push_back( op );

      HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );

      validate_database();

      BOOST_TEST_MESSAGE( "--- Testing success" );

      auto old_mana = alice.voting_manabar.current_mana;

      op.weight = HIVE_100_PERCENT;
      op.author = "alice";
      tx.operations.clear();
      tx.signatures.clear();
      tx.operations.push_back( op );

      push_transaction( tx, alice_private_key, 0 );

      auto& alice_comment = db->get_comment( "alice", string( "foo" ) );
      const comment_cashout_object* alice_comment_cashout = db->find_comment_cashout( alice_comment );

      auto itr = vote_idx.find( boost::make_tuple( alice_comment.get_id(), alice.get_id() ) );
      int64_t max_vote_denom = ( db->get_dynamic_global_properties().vote_power_reserve_rate * HIVE_VOTING_MANA_REGENERATION_SECONDS ) / (60*60*24);

      BOOST_REQUIRE( alice.last_vote_time == db->head_block_time() );
      BOOST_REQUIRE( alice_comment_cashout->get_net_rshares() == ( old_mana - alice.voting_manabar.current_mana ) - HIVE_VOTE_DUST_THRESHOLD );
      BOOST_REQUIRE( alice_comment_cashout->get_cashout_time() == alice_comment_cashout->get_creation_time() + HIVE_CASHOUT_WINDOW_SECONDS );
      BOOST_REQUIRE( itr->get_rshares() == ( old_mana - alice.voting_manabar.current_mana ) - HIVE_VOTE_DUST_THRESHOLD );
      BOOST_REQUIRE( itr != vote_idx.end() );
      validate_database();

      BOOST_TEST_MESSAGE( "--- Test reduced power for quick voting" );

      generate_blocks( db->head_block_time() + HIVE_MIN_VOTE_INTERVAL_SEC );

      util::manabar old_manabar = VOTING_MANABAR( "alice" );
      util::manabar_params params( util::get_effective_vesting_shares( db->get_account( "alice" ) ), HIVE_VOTING_MANA_REGENERATION_SECONDS );
      old_manabar.regenerate_mana( params, db->head_block_time() );

      comment_op.author = "bob";
      comment_op.permlink = "foo";
      comment_op.title = "bar";
      comment_op.body = "foo bar";
      tx.operations.clear();
      tx.signatures.clear();
      tx.operations.push_back( comment_op );
      push_transaction( tx, bob_private_key, 0 );

      op.weight = HIVE_100_PERCENT / 2;
      op.voter = "alice";
      op.author = "bob";
      op.permlink = "foo";
      tx.operations.clear();
      tx.signatures.clear();
      tx.operations.push_back( op );
      push_transaction( tx, alice_private_key, 0 );

      const auto& bob_comment = db->get_comment( "bob", string( "foo" ) );
      const comment_cashout_object* bob_comment_cashout = db->find_comment_cashout( bob_comment );
      itr = vote_idx.find( boost::make_tuple( bob_comment.get_id(), alice.get_id() ) );

      BOOST_REQUIRE( bob_comment_cashout->get_net_rshares() == ( old_manabar.current_mana - db->get_account( "alice" ).voting_manabar.current_mana ) - HIVE_VOTE_DUST_THRESHOLD );
      BOOST_REQUIRE( bob_comment_cashout->get_cashout_time() == bob_comment_cashout->get_creation_time() + HIVE_CASHOUT_WINDOW_SECONDS );
      BOOST_REQUIRE( itr != vote_idx.end() );
      validate_database();

      BOOST_TEST_MESSAGE( "--- Test payout time extension on vote" ); // such feature was scrapped with HF17

      old_mana = VOTING_MANABAR( "bob" ).current_mana;
      auto old_net_rshares = db->find_comment_cashout( db->get_comment( "alice", string( "foo" ) ) )->get_net_rshares();

      generate_blocks( db->head_block_time() + fc::seconds( ( HIVE_CASHOUT_WINDOW_SECONDS / 2 ) ), true );

      const auto& new_bob = db->get_account( "bob" );
      const auto& new_alice_comment = db->get_comment( "alice", string( "foo" ) );
      const comment_cashout_object* new_alice_comment_cashout = db->find_comment_cashout( new_alice_comment );

      op.weight = HIVE_100_PERCENT;
      op.voter = "bob";
      op.author = "alice";
      op.permlink = "foo";
      tx.operations.clear();
      tx.signatures.clear();
      tx.operations.push_back( op );
      tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
      push_transaction( tx, bob_private_key, 0 );

      itr = vote_idx.find( boost::make_tuple( new_alice_comment.get_id(), new_bob.get_id() ) );

      BOOST_REQUIRE( new_alice_comment_cashout->get_net_rshares() == old_net_rshares + ( old_mana - new_bob.voting_manabar.current_mana ) - HIVE_VOTE_DUST_THRESHOLD );
      BOOST_REQUIRE( new_alice_comment_cashout->get_cashout_time() == new_alice_comment_cashout->get_creation_time() + HIVE_CASHOUT_WINDOW_SECONDS );
      BOOST_REQUIRE( itr != vote_idx.end() );
      validate_database();

      BOOST_TEST_MESSAGE( "--- Test negative vote" );

      const auto& new_sam = db->get_account( "sam" );
      const auto& new_bob_comment = db->get_comment( "bob", string( "foo" ) );
      const comment_cashout_object* new_bob_comment_cashout = db->find_comment_cashout( new_bob_comment );

      old_net_rshares = new_bob_comment_cashout->get_net_rshares();

      old_manabar = VOTING_MANABAR( "sam" );
      params.max_mana = util::get_effective_vesting_shares( db->get_account( "sam" ) );
      old_manabar.regenerate_mana( params, db->head_block_time() );

      op.weight = -1 * HIVE_100_PERCENT / 2;
      op.voter = "sam";
      op.author = "bob";
      op.permlink = "foo";
      tx.operations.clear();
      tx.signatures.clear();
      tx.operations.push_back( op );
      push_transaction( tx, sam_private_key, 0 );

      itr = vote_idx.find( boost::make_tuple( new_bob_comment.get_id(), new_sam.get_id() ) );

      util::manabar old_downvote_manabar;
      util::manabar_params downvote_params( util::get_effective_vesting_shares( db->get_account( "alice" ) ) / 4, HIVE_VOTING_MANA_REGENERATION_SECONDS );
      old_downvote_manabar.regenerate_mana( downvote_params, db->head_block_time() );
      int64_t sam_weight = old_downvote_manabar.current_mana - DOWNVOTE_MANABAR( "sam" ).current_mana - HIVE_VOTE_DUST_THRESHOLD;

      BOOST_REQUIRE( new_bob_comment_cashout->get_net_rshares() == old_net_rshares - sam_weight );
      BOOST_REQUIRE( new_bob_comment_cashout->get_cashout_time() == new_bob_comment_cashout->get_creation_time() + HIVE_CASHOUT_WINDOW_SECONDS );
      BOOST_REQUIRE( itr != vote_idx.end() );
      validate_database();

      BOOST_TEST_MESSAGE( "--- Test nested voting on nested comments" );

      int64_t regenerated_power = (HIVE_100_PERCENT * ( db->head_block_time() - db->get_account( "alice").last_vote_time ).to_seconds() ) / HIVE_VOTING_MANA_REGENERATION_SECONDS;
      int64_t used_power = ( get_voting_power( db->get_account( "alice" ) ) + regenerated_power + max_vote_denom - 1 ) / max_vote_denom;

      comment_op.author = "sam";
      comment_op.permlink = "foo";
      comment_op.title = "bar";
      comment_op.body = "foo bar";
      comment_op.parent_author = "alice";
      comment_op.parent_permlink = "foo";
      tx.operations.clear();
      tx.signatures.clear();
      tx.operations.push_back( comment_op );
      push_transaction( tx, sam_private_key, 0 );

      op.weight = HIVE_100_PERCENT;
      op.voter = "alice";
      op.author = "sam";
      op.permlink = "foo";
      tx.operations.clear();
      tx.signatures.clear();
      tx.operations.push_back( op );
      push_transaction( tx, alice_private_key, 0 );

      int64_t new_rshares = ( ( fc::uint128_t( get_vesting( "alice" ).amount.value ) * used_power ) / HIVE_100_PERCENT ).to_uint64() - HIVE_VOTE_DUST_THRESHOLD;

      {
        auto* alice_cashout = db->find_comment_cashout( db->get_comment( "alice", string( "foo" ) ) );
        BOOST_REQUIRE( alice_cashout->get_cashout_time() == alice_cashout->get_creation_time() + HIVE_CASHOUT_WINDOW_SECONDS );
      }

      validate_database();

      BOOST_TEST_MESSAGE( "--- Test increasing vote rshares" );

      generate_blocks( db->head_block_time() + HIVE_MIN_VOTE_INTERVAL_SEC );

      auto& new_alice = db->get_account( "alice" );
      auto alice_bob_vote = vote_idx.find( boost::make_tuple( new_bob_comment.get_id(), new_alice.get_id() ) );
      auto old_vote_rshares = alice_bob_vote->get_rshares();
      old_net_rshares = new_bob_comment_cashout->get_net_rshares();
      used_power = ( ( HIVE_1_PERCENT * 25 * ( get_voting_power( new_alice ) ) / HIVE_100_PERCENT ) + max_vote_denom - 1 ) / max_vote_denom;
      auto alice_voting_power = get_voting_power( new_alice ) - used_power;

      old_manabar = VOTING_MANABAR( "alice" );
      params.max_mana = util::get_effective_vesting_shares( db->get_account( "alice" ) );
      old_manabar.regenerate_mana( params, db->head_block_time() );

      op.voter = "alice";
      op.weight = HIVE_1_PERCENT * 25;
      op.author = "bob";
      op.permlink = "foo";
      tx.operations.clear();
      tx.signatures.clear();
      tx.operations.push_back( op );
      push_transaction( tx, alice_private_key, 0 );
      alice_bob_vote = vote_idx.find( boost::make_tuple( new_bob_comment.get_id(), new_alice.get_id() ) );

      new_rshares = old_manabar.current_mana - VOTING_MANABAR( "alice" ).current_mana - HIVE_VOTE_DUST_THRESHOLD;

      BOOST_REQUIRE( new_bob_comment_cashout->get_net_rshares() == old_net_rshares - old_vote_rshares + new_rshares );
      BOOST_REQUIRE( new_bob_comment_cashout->get_cashout_time() == new_bob_comment_cashout->get_creation_time() + HIVE_CASHOUT_WINDOW_SECONDS );
      BOOST_REQUIRE( alice_bob_vote->get_rshares() == new_rshares );
      BOOST_REQUIRE( alice_bob_vote->get_last_update() == db->head_block_time() );
      BOOST_REQUIRE( alice_bob_vote->get_vote_percent() == op.weight );
      validate_database();

      BOOST_TEST_MESSAGE( "--- Test decreasing vote rshares" );

      generate_blocks( db->head_block_time() + HIVE_MIN_VOTE_INTERVAL_SEC );

      old_vote_rshares = new_rshares;
      old_net_rshares = new_bob_comment_cashout->get_net_rshares();
      used_power = ( uint64_t( HIVE_1_PERCENT ) * 75 * uint64_t( alice_voting_power ) ) / HIVE_100_PERCENT;
      used_power = ( used_power + max_vote_denom - 1 ) / max_vote_denom;
      alice_voting_power -= used_power;

      old_manabar = VOTING_MANABAR( "alice" );
      params.max_mana = util::get_effective_vesting_shares( db->get_account( "alice" ) );
      old_manabar.regenerate_mana( params, db->head_block_time() );

      old_downvote_manabar = DOWNVOTE_MANABAR( "alice" );
      downvote_params.max_mana = util::get_effective_vesting_shares( db->get_account( "alice" ) ) / 4;
      old_downvote_manabar.regenerate_mana( downvote_params, db->head_block_time() );

      op.weight = HIVE_1_PERCENT * -75;
      tx.operations.clear();
      tx.signatures.clear();
      tx.operations.push_back( op );
      push_transaction( tx, alice_private_key, 0 );
      alice_bob_vote = vote_idx.find( boost::make_tuple( new_bob_comment.get_id(), new_alice.get_id() ) );

      new_rshares = old_downvote_manabar.current_mana - DOWNVOTE_MANABAR( "alice" ).current_mana - HIVE_VOTE_DUST_THRESHOLD;

      BOOST_REQUIRE( new_bob_comment_cashout->get_net_rshares() == old_net_rshares - old_vote_rshares - new_rshares );
      BOOST_REQUIRE( new_bob_comment_cashout->get_cashout_time() == new_bob_comment_cashout->get_creation_time() + HIVE_CASHOUT_WINDOW_SECONDS );
      BOOST_REQUIRE( alice_bob_vote->get_rshares() == -1 * new_rshares );
      BOOST_REQUIRE( alice_bob_vote->get_last_update() == db->head_block_time() );
      BOOST_REQUIRE( alice_bob_vote->get_vote_percent() == op.weight );
      validate_database();

      BOOST_TEST_MESSAGE( "--- Test changing a vote to 0 weight (aka: removing a vote)" );

      generate_blocks( db->head_block_time() + HIVE_MIN_VOTE_INTERVAL_SEC );

      old_vote_rshares = alice_bob_vote->get_rshares();
      old_net_rshares = new_bob_comment_cashout->get_net_rshares();

      op.weight = 0;
      tx.operations.clear();
      tx.signatures.clear();
      tx.operations.push_back( op );
      push_transaction( tx, alice_private_key, 0 );
      alice_bob_vote = vote_idx.find( boost::make_tuple( new_bob_comment.get_id(), new_alice.get_id() ) );

      BOOST_REQUIRE( new_bob_comment_cashout->get_net_rshares() == old_net_rshares - old_vote_rshares );
      BOOST_REQUIRE( new_bob_comment_cashout->get_cashout_time() == new_bob_comment_cashout->get_creation_time() + HIVE_CASHOUT_WINDOW_SECONDS );
      BOOST_REQUIRE( alice_bob_vote->get_rshares() == 0 );
      BOOST_REQUIRE( alice_bob_vote->get_last_update() == db->head_block_time() );
      BOOST_REQUIRE( alice_bob_vote->get_vote_percent() == op.weight );
      validate_database();

      BOOST_TEST_MESSAGE( "--- Test downvote overlap when downvote mana is low" );

      generate_block();
      db_plugin->debug_update( [=]( database& db )
      {
        db.modify( db.get_account( "alice" ), [&]( account_object& a )
        {
          a.downvote_manabar.current_mana /= 30;
          a.downvote_manabar.last_update_time = db.head_block_time().sec_since_epoch();
        });
      });

      int64_t alice_weight = 0;

      {
        const auto& alice = db->get_account( "alice" );
        old_downvote_manabar.current_mana = alice.downvote_manabar.current_mana;
        old_downvote_manabar.last_update_time = alice.downvote_manabar.last_update_time;
        old_manabar.current_mana = alice.voting_manabar.current_mana;
        old_manabar.last_update_time = alice.voting_manabar.last_update_time;
        old_manabar.regenerate_mana( params, db->head_block_time() );
        old_downvote_manabar.regenerate_mana( params, db->head_block_time() );
        alice_weight = old_manabar.current_mana * 60 * 60 * 24;
        auto max_vote_denom = db->get_dynamic_global_properties().vote_power_reserve_rate * HIVE_VOTING_MANA_REGENERATION_SECONDS;
        alice_weight = ( alice_weight + max_vote_denom - 1 ) / max_vote_denom;

        const auto& bob_comment = db->get_comment( "bob", string( "foo" ) );
        const comment_cashout_object* bob_comment_cashout = db->find_comment_cashout( bob_comment );

        old_net_rshares = bob_comment_cashout->get_net_rshares();
      }

      op.weight = -1 * HIVE_100_PERCENT;
      tx.operations.clear();
      tx.signatures.clear();
      tx.operations.push_back( op );
      push_transaction( tx, alice_private_key, 0 );
      alice_bob_vote = vote_idx.find( boost::make_tuple( new_bob_comment.get_id(), new_alice.get_id() ) );

      {
        const auto& bob_comment = db->get_comment( "bob", string( "foo" ) );
        const comment_cashout_object* bob_comment_cashout = db->find_comment_cashout( bob_comment );
        const auto& alice = db->get_account( "alice" );
        BOOST_REQUIRE( bob_comment_cashout->get_net_rshares() == old_net_rshares - alice_weight + HIVE_VOTE_DUST_THRESHOLD );
        BOOST_REQUIRE( alice_bob_vote->get_rshares() == -1 * ( alice_weight - HIVE_VOTE_DUST_THRESHOLD ) );
        BOOST_REQUIRE( alice_bob_vote->get_last_update() == db->head_block_time() );
        BOOST_REQUIRE( alice_bob_vote->get_vote_percent() == op.weight );
        BOOST_REQUIRE( alice.downvote_manabar.current_mana == 0 );
        BOOST_REQUIRE( alice.voting_manabar.current_mana == old_manabar.current_mana - alice_weight + old_downvote_manabar.current_mana );
        validate_database();
      }

      BOOST_TEST_MESSAGE( "--- Test reduced effectiveness when increasing rshares within lockout period" );

      generate_blocks( fc::time_point_sec( ( new_bob_comment_cashout->get_cashout_time() - HIVE_UPVOTE_LOCKOUT_HF17 ).sec_since_epoch() + HIVE_BLOCK_INTERVAL ), true );

      old_manabar = VOTING_MANABAR( "dave" );
      params.max_mana = util::get_effective_vesting_shares( db->get_account( "dave" ) );
      old_manabar.regenerate_mana( params, db->head_block_time() );

      op.voter = "dave";
      op.weight = HIVE_100_PERCENT;
      tx.operations.clear();
      tx.signatures.clear();
      tx.operations.push_back( op );
      push_transaction( tx, dave_private_key, 0 );

      new_rshares = old_manabar.current_mana - VOTING_MANABAR( "dave" ).current_mana - HIVE_VOTE_DUST_THRESHOLD;
      new_rshares = ( new_rshares * ( HIVE_UPVOTE_LOCKOUT_SECONDS - HIVE_BLOCK_INTERVAL ) ) / HIVE_UPVOTE_LOCKOUT_SECONDS;
      account_id_type dave_id = get_account_id( "dave" );
      comment_id_type bob_comment_id = db->get_comment( "bob", string( "foo" ) ).get_id();

      {
        auto& dave_bob_vote = db->get< comment_vote_object, by_comment_voter >( boost::make_tuple( bob_comment_id, dave_id ) );
        BOOST_REQUIRE_EQUAL( dave_bob_vote.get_rshares(), new_rshares );
      }
      validate_database();

      /*
      the following part of test never worked since its inception due to bugs (assignment used instead of comparison);
      since then the mechanism changed a lot (downvote has its own mana pool f.e. but replacing code below with its use didn't fix the test)
      and it doesn't look reasonable to just copy the equations from hf20_vote_evaluator

      BOOST_TEST_MESSAGE( "--- Test reduced effectiveness when reducing rshares within lockout period" );

      generate_block();
      old_manabar = VOTING_MANABAR( "dave" );
      params.max_mana = util::get_effective_vesting_shares( db->get_account( "dave" ) );
      old_manabar.regenerate_mana( params, db->head_block_time() );

      op.weight = -1 * HIVE_100_PERCENT;
      tx.operations.clear();
      tx.signatures.clear();
      tx.operations.push_back( op );
      push_transaction( tx, dave_private_key, 0 );

      new_rshares = old_manabar.current_mana - VOTING_MANABAR( "dave" ).current_mana - HIVE_VOTE_DUST_THRESHOLD;
      new_rshares = ( new_rshares * ( HIVE_UPVOTE_LOCKOUT_SECONDS - HIVE_BLOCK_INTERVAL - HIVE_BLOCK_INTERVAL ) ) / HIVE_UPVOTE_LOCKOUT_SECONDS;

      {
        auto& dave_bob_vote = db->get< comment_vote_object, by_comment_voter >( boost::make_tuple( bob_comment_id, dave_id ) );
        BOOST_REQUIRE_EQUAL( dave_bob_vote.get_rshares(), new_rshares );
      }
      validate_database();
      */
    }
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( vote_weights )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: vote_weights" );

    // test one vote with the same power per one comment in each of possible vote windows:
    // - right with comment itself => this is not tested because since HF25 there is no "reverse_auction_seconds penalty"
    //   for early voting and it would not be possible to have exactly the same vote power after such vote edit
    //   because mana would not have time to regenerate between vote and revote
    // - [0 .. early_voting_seconds)
    // - [early_voting_seconds .. early_voting_seconds+mid_voting_seconds)
    // - [early_voting_seconds+mid_voting_seconds .. cashout-HIVE_UPVOTE_LOCKOUT_SECONDS)
    // - [cashout-HIVE_UPVOTE_LOCKOUT_SECONDS .. cashout)

    ACTORS(
      ( author0 ) ( voter0 ) ( author1 ) ( voter1 ) ( author2 ) ( voter2 ) ( author3 ) ( voter3 ) //pattern upvoters
      ( author4 ) ( voter4 ) ( author5 ) ( voter5 ) ( author6 ) ( voter6 ) ( author7 ) ( voter7 ) //pattern downvoters
      ( author01 ) ( voter01 ) ( author11 ) ( voter11 ) ( author21 ) ( voter21 ) ( author31 ) ( voter31 ) //upvote from small to pattern
      ( author41 ) ( voter41 ) ( author51 ) ( voter51 ) ( author61 ) ( voter61 ) ( author71 ) ( voter71 ) //downvote from small to pattern
      ( author02 ) ( voter02 ) ( author12 ) ( voter12 ) ( author22 ) ( voter22 ) ( author32 ) ( voter32 ) //upvote from big to pattern
      ( author42 ) ( voter42 ) ( author52 ) ( voter52 ) ( author62 ) ( voter62 ) ( author72 ) ( voter72 ) //downvote from big to pattern
      ( author03 ) ( voter03 ) ( author13 ) ( voter13 ) ( author23 ) ( voter23 ) ( author33 ) ( voter33 ) //upvote from big downvote to pattern
      ( author43 ) ( voter43 ) ( author53 ) ( voter53 ) ( author63 ) ( voter63 ) ( author73 ) ( voter73 ) //downvote from big upvote to pattern
      ( author04 ) ( voter04 ) ( author14 ) ( voter14 ) ( author24 ) ( voter24 ) ( author34 ) ( voter34 ) //upvote from big downvote to zero
      ( author44 ) ( voter44 ) ( author54 ) ( voter54 ) ( author64 ) ( voter64 ) ( author74 ) ( voter74 ) //downvote from big upvote to zero
    )
    generate_block();

    // values must be small enough so mana has time to regenerate after initial vote before first edit
    const int16_t PAT_UPVOTE_PERCENT = 3 * HIVE_1_PERCENT; // 3%
    const int16_t PAT_DOWNVOTE_PERCENT = -3 * HIVE_1_PERCENT; // -3%
    const int16_t SMALL_UPVOTE_PERCENT = 1 * HIVE_1_PERCENT; // 1%
    const int16_t SMALL_DOWNVOTE_PERCENT = -1 * HIVE_1_PERCENT; // -1%
    const int16_t BIG_UPVOTE_PERCENT = 5 * HIVE_1_PERCENT; // 5%
    const int16_t BIG_DOWNVOTE_PERCENT = -5 * HIVE_1_PERCENT; // -5%

    struct TActor
    {
      const char* name;
      fc::ecc::private_key key;
    };
    TActor voters[] = {
      {"voter0",voter0_private_key}, {"voter1",voter1_private_key}, {"voter2",voter2_private_key}, {"voter3",voter3_private_key},
      {"voter4",voter4_private_key}, {"voter5",voter5_private_key}, {"voter6",voter6_private_key}, {"voter7",voter7_private_key},
      {"voter01",voter01_private_key}, {"voter11",voter11_private_key}, {"voter21",voter21_private_key}, {"voter31",voter31_private_key},
      {"voter41",voter41_private_key}, {"voter51",voter51_private_key}, {"voter61",voter61_private_key}, {"voter71",voter71_private_key},
      {"voter02",voter02_private_key}, {"voter12",voter12_private_key}, {"voter22",voter22_private_key}, {"voter32",voter32_private_key},
      {"voter42",voter42_private_key}, {"voter52",voter52_private_key}, {"voter62",voter62_private_key}, {"voter72",voter72_private_key},
      {"voter03",voter03_private_key}, {"voter13",voter13_private_key}, {"voter23",voter23_private_key}, {"voter33",voter33_private_key},
      {"voter43",voter43_private_key}, {"voter53",voter53_private_key}, {"voter63",voter63_private_key}, {"voter73",voter73_private_key},
      {"voter04",voter04_private_key}, {"voter14",voter14_private_key}, {"voter24",voter24_private_key}, {"voter34",voter34_private_key},
      {"voter44",voter44_private_key}, {"voter54",voter54_private_key}, {"voter64",voter64_private_key}, {"voter74",voter74_private_key}
    };
    TActor authors[] = {
      {"author0",author0_private_key}, {"author1",author1_private_key}, {"author2",author2_private_key}, {"author3",author3_private_key},
      {"author4",author4_private_key}, {"author5",author5_private_key}, {"author6",author6_private_key}, {"author7",author7_private_key},
      {"author01",author01_private_key}, {"author11",author11_private_key}, {"author21",author21_private_key}, {"author31",author31_private_key},
      {"author41",author41_private_key}, {"author51",author51_private_key}, {"author61",author61_private_key}, {"author71",author71_private_key},
      {"author02",author02_private_key}, {"author12",author12_private_key}, {"author22",author22_private_key}, {"author32",author32_private_key},
      {"author42",author42_private_key}, {"author52",author52_private_key}, {"author62",author62_private_key}, {"author72",author72_private_key},
      {"author03",author03_private_key}, {"author13",author13_private_key}, {"author23",author23_private_key}, {"author33",author33_private_key},
      {"author43",author43_private_key}, {"author53",author53_private_key}, {"author63",author63_private_key}, {"author73",author73_private_key},
      {"author04",author04_private_key}, {"author14",author14_private_key}, {"author24",author24_private_key}, {"author34",author34_private_key},
      {"author44",author44_private_key}, {"author54",author54_private_key}, {"author64",author64_private_key}, {"author74",author74_private_key}
    };

    for( auto& voter : voters )
      vest( HIVE_INIT_MINER_NAME, voter.name, ASSET( "1000.000 TESTS" ) );
    generate_block();

    comment_operation comment_op;
    comment_op.permlink = "permlink";
    comment_op.parent_permlink = "test";
    comment_op.title = "test";
    comment_op.body = "text";
    for( auto& author : authors )
    {
      comment_op.author = author.name;
      push_transaction( comment_op, author.key );
    }

    vote_operation vote_op;
    vote_op.permlink = "permlink";
    int i;
    const auto& VOTE = [&]( int i )
    {
      vote_op.voter = voters[i].name;
      vote_op.author = authors[i].name;
      push_transaction( vote_op, voters[i].key );
    };
    vote_op.weight = SMALL_UPVOTE_PERCENT; //initial small upvote
    for( i = 8; i < 12; ++i )
      VOTE( i );
    vote_op.weight = SMALL_DOWNVOTE_PERCENT; //initial small downvote
    for( i = 12; i < 16; ++i )
      VOTE( i );
    vote_op.weight = BIG_UPVOTE_PERCENT; //initial big upvote
    for( i = 16; i < 20; ++i )
      VOTE( i );
    vote_op.weight = BIG_DOWNVOTE_PERCENT; //initial big downvote
    for( i = 20; i < 24; ++i )
      VOTE( i );
    vote_op.weight = BIG_DOWNVOTE_PERCENT; //initial big downvote
    for( i = 24; i < 28; ++i )
      VOTE( i );
    vote_op.weight = BIG_UPVOTE_PERCENT; //initial big upvote
    for( i = 28; i < 32; ++i )
      VOTE( i );
    vote_op.weight = BIG_DOWNVOTE_PERCENT; //initial big downvote
    for( i = 32; i < 36; ++i )
      VOTE( i );
    vote_op.weight = BIG_UPVOTE_PERCENT; //initial big upvote
    for( i = 36; i < 40; ++i )
      VOTE( i );
    static_assert( sizeof( voters ) == 40 * sizeof( TActor ) );

    generate_block();

    const auto& dgpo = db->get_dynamic_global_properties();
    fc::time_point_sec creation_time, cashout_time;
    {
      const auto& comment = db->get_comment( "author0", string( "permlink" ) );
      const auto* cashout = db->find_comment_cashout( comment );
      creation_time = cashout->get_creation_time();
      cashout_time = cashout->get_cashout_time();
    }

    //votes in early window
    generate_blocks( creation_time + fc::seconds( dgpo.early_voting_seconds - 6 ), false );
    vote_op.weight = PAT_UPVOTE_PERCENT; //pattern vote and vote edits into pattern upvote
    for( auto i : { 0,8,16,24 } )
      VOTE( i );
    vote_op.weight = 0; //vote deleted
    VOTE( 32 );
    vote_op.weight = PAT_DOWNVOTE_PERCENT; //pattern vote and vote edits into pattern downvote
    for( auto i : { 4,12,20,28 } )
      VOTE( i );
    vote_op.weight = 0; //vote deleted
    VOTE( 36 );
    generate_block();

    //votes in mid window
    generate_blocks( creation_time + fc::seconds( dgpo.early_voting_seconds + dgpo.mid_voting_seconds / 2 ), false );
    vote_op.weight = PAT_UPVOTE_PERCENT; //pattern vote and edits into pattern upvote
    for( auto i : { 1,9,17,25 } )
      VOTE( i );
    vote_op.weight = 0; //vote deleted
    VOTE( 33 );
    vote_op.weight = PAT_DOWNVOTE_PERCENT; //pattern vote and vote edits into pattern downvote
    for( auto i : { 5,13,21,29 } )
      VOTE( i );
    vote_op.weight = 0; //vote deleted
    VOTE( 37 );
    generate_block();

    //votes in late window
    generate_blocks( cashout_time - fc::seconds( HIVE_UPVOTE_LOCKOUT_SECONDS + 6 ), false );
    vote_op.weight = PAT_UPVOTE_PERCENT; //pattern vote and vote edits into pattern upvote
    for( auto i : { 2,10,18,26 } )
      VOTE( i );
    vote_op.weight = 0; //vote deleted
    VOTE( 34 );
    vote_op.weight = PAT_DOWNVOTE_PERCENT; //pattern vote and vote edits into pattern downvote
    for( auto i : { 6,14,22,30 } )
      VOTE( i );
    vote_op.weight = 0; //vote deleted
    VOTE( 38 );
    generate_block();

    //votes in upvote lockout (that is also within late window)
    generate_blocks( cashout_time - fc::seconds( HIVE_UPVOTE_LOCKOUT_SECONDS / 2 ), false );
    vote_op.weight = PAT_UPVOTE_PERCENT; //pattern vote and vote edits into pattern upvote
    for( auto i : { 3,11,19,27 } )
      VOTE( i );
    vote_op.weight = 0; //vote deleted
    VOTE( 35 );
    vote_op.weight = PAT_DOWNVOTE_PERCENT; //pattern vote and vote edits into pattern downvote
    for( auto i : { 7,15,23,31 } )
      VOTE( i );
    vote_op.weight = 0; //vote deleted
    VOTE( 39 );
    generate_block();

    const auto& vote_idx = db->get_index< comment_vote_index, by_comment_voter >();
    std::vector<const hive::chain::comment_object*> commentObjs;
    std::vector<const hive::chain::account_object*> voterObjs;
    std::vector<const hive::chain::comment_vote_object*> voteObjs;
    for( i = 0; i < 40; ++i )
    {
      const auto& comment = db->get_comment( authors[i].name, string( "permlink" ) );
      commentObjs.emplace_back( &comment );
      const auto& cashout = *( db->find_comment_cashout( comment ) );
      const auto& voter = db->get_account( voters[i].name );
      voterObjs.emplace_back( &voter );
      const auto& vote = *( vote_idx.find( boost::make_tuple( comment.get_id(), voter.get_id() ) ) );
      voteObjs.emplace_back( &vote );
      BOOST_REQUIRE_EQUAL( vote.get_weight(), cashout.get_total_vote_weight() );
    }

    for( auto i : { 8,16,24 } ) //upvotes edited in early window are the same as early window pattern
      BOOST_REQUIRE_EQUAL( voteObjs[i]->get_weight(), voteObjs[0]->get_weight() );
    for( auto i : { 12,20,28 } ) //downvotes edited in early window are the same as early window pattern
      BOOST_REQUIRE_EQUAL( voteObjs[i]->get_weight(), voteObjs[4]->get_weight() );
    for( auto i : { 9,17,25 } ) //upvotes edited in mid window are the same as mid window pattern
      BOOST_REQUIRE_EQUAL( voteObjs[i]->get_weight(), voteObjs[1]->get_weight() );
    for( auto i : { 13,21,29 } ) //downvotes edited in mid window are the same as mid window pattern
      BOOST_REQUIRE_EQUAL( voteObjs[i]->get_weight(), voteObjs[5]->get_weight() );
    for( auto i : { 10,18,26 } ) //upvotes edited in late window are the same as late window pattern
      BOOST_REQUIRE_EQUAL( voteObjs[i]->get_weight(), voteObjs[2]->get_weight() );
    for( auto i : { 14,22,30 } ) //downvotes edited in late window are the same as late window pattern
      BOOST_REQUIRE_EQUAL( voteObjs[i]->get_weight(), voteObjs[6]->get_weight() );
    for( auto i : { 11,19,27 } ) //upvotes edited in lockout window are the same as lockout window pattern (only because the same block)
      BOOST_REQUIRE_EQUAL( voteObjs[i]->get_weight(), voteObjs[3]->get_weight() );
    for( auto i : { 15,23,31 } ) //downvotes edited in lockout window are the same as lockout window pattern (only because the same block)
      BOOST_REQUIRE_EQUAL( voteObjs[i]->get_weight(), voteObjs[7]->get_weight() );
    for( i = 32; i < 40; ++i ) //deleted votes have zero weight
      BOOST_REQUIRE_EQUAL( voteObjs[i]->get_weight(), 0 );
    
    generate_blocks( cashout_time, false );

    for( auto i : { 8,16,24 } ) //upvotes edited in early window have the same rewards as early window pattern
      BOOST_REQUIRE_EQUAL( voterObjs[i]->get_vest_rewards().amount.value, voterObjs[0]->get_vest_rewards().amount.value );
    for( auto i : { 4,12,20,28 } ) //downvotes have no rewards
      BOOST_REQUIRE_EQUAL( voterObjs[i]->get_vest_rewards().amount.value, 0 );
    for( auto i : { 9,17,25 } ) //upvotes edited in mid window have the same rewards as mid window pattern
      BOOST_REQUIRE_EQUAL( voterObjs[i]->get_vest_rewards().amount.value, voterObjs[1]->get_vest_rewards().amount.value );
    for( auto i : { 5,13,21,29 } ) //downvotes have no rewards
      BOOST_REQUIRE_EQUAL( voterObjs[i]->get_vest_rewards().amount.value, 0 );
    for( auto i : { 10,18,26 } ) //upvotes edited in late window have the same rewards as late window pattern
      BOOST_REQUIRE_EQUAL( voterObjs[i]->get_vest_rewards().amount.value, voterObjs[2]->get_vest_rewards().amount.value );
    for( auto i : { 6,14,22,30 } ) //downvotes have no rewards
      BOOST_REQUIRE_EQUAL( voterObjs[i]->get_vest_rewards().amount.value, 0 );
    for( auto i : { 11,19,27 } ) //upvotes edited in lockout window have the same rewards as lockout window pattern (only because the same block)
      BOOST_REQUIRE_EQUAL( voterObjs[i]->get_vest_rewards().amount.value, voterObjs[3]->get_vest_rewards().amount.value );
    for( auto i : { 7,15,23,31 } ) //downvotes have no rewards
      BOOST_REQUIRE_EQUAL( voterObjs[i]->get_vest_rewards().amount.value, 0 );
    for( i = 32; i < 40; ++i ) //deleted votes have no rewards
      BOOST_REQUIRE_EQUAL( voterObjs[i]->get_vest_rewards().amount.value, 0 );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( transfer_validate )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: transfer_validate" );

    transfer_operation op;
    op.from = "alice";
    op.to = "bob";
    op.memo = "Memo";
    op.amount = asset( 100, HIVE_SYMBOL );
    op.validate();

    BOOST_TEST_MESSAGE( " --- Invalid from account" );
    op.from = "alice-";
    HIVE_REQUIRE_THROW( op.validate(), fc::assert_exception );
    op.from = "alice";

    BOOST_TEST_MESSAGE( " --- Invalid to account" );
    op.to = "bob-";
    HIVE_REQUIRE_THROW( op.validate(), fc::assert_exception );
    op.to = "bob";

    BOOST_TEST_MESSAGE( " --- Memo too long" );
    std::string memo;
    for ( int i = 0; i < HIVE_MAX_MEMO_SIZE + 1; i++ )
      memo += "x";
    op.memo = memo;
    HIVE_REQUIRE_THROW( op.validate(), fc::assert_exception );
    op.memo = "Memo";

    BOOST_TEST_MESSAGE( " --- Negative amount" );
    op.amount = -op.amount;
    HIVE_REQUIRE_THROW( op.validate(), fc::assert_exception );
    op.amount = -op.amount;

    BOOST_TEST_MESSAGE( " --- Transferring vests" );
    op.amount = asset( 100, VESTS_SYMBOL );
    HIVE_REQUIRE_THROW( op.validate(), fc::assert_exception );
    op.amount = asset( 100, HIVE_SYMBOL );

    op.validate();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( transfer_authorities )
{
  try
  {
    ACTORS( (alice)(bob) )
    fund( "alice", 10000 );

    BOOST_TEST_MESSAGE( "Testing: transfer_authorities" );

    transfer_operation op;
    op.from = "alice";
    op.to = "bob";
    op.amount = ASSET( "2.500 TESTS" );

    signed_transaction tx;
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.operations.push_back( op );

    BOOST_TEST_MESSAGE( "--- Test failure when no signatures" );
    HIVE_REQUIRE_THROW( push_transaction( tx, 0 ), tx_missing_active_auth );

    BOOST_TEST_MESSAGE( "--- Test failure when signed by a signature not in the account's authority" );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_post_key, 0 ), tx_missing_active_auth );

    BOOST_TEST_MESSAGE( "--- Test failure when duplicate signatures" );
    tx.signatures.clear();
    sign( tx, alice_private_key );
    sign( tx, alice_private_key );
    HIVE_REQUIRE_THROW( push_transaction( tx, 0 ), tx_duplicate_sig );

    BOOST_TEST_MESSAGE( "--- Test failure when signed by an additional signature not in the creator's authority" );
    tx.signatures.clear();
    sign( tx, alice_private_key );
    sign( tx, bob_private_key );
    HIVE_REQUIRE_THROW( push_transaction( tx, 0 ), tx_irrelevant_sig );

    BOOST_TEST_MESSAGE( "--- Test success with witness signature" );
    tx.signatures.clear();
    push_transaction( tx, alice_private_key, 0 );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( signature_stripping )
{
  try
  {
    // Alice, Bob and Sam all have 2-of-3 multisig on corp.
    // Legitimate tx signed by (Alice, Bob) goes through.
    // Sam shouldn't be able to add or remove signatures to get the transaction to process multiple times.

    ACTORS( (alice)(bob)(sam)(corp) )
    fund( "corp", 10000 );

    account_update_operation update_op;
    update_op.account = "corp";
    update_op.active = authority( 2, "alice", 1, "bob", 1, "sam", 1 );

    signed_transaction tx;
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.operations.push_back( update_op );

    push_transaction( tx, corp_private_key, 0 );

    tx.operations.clear();
    tx.signatures.clear();

    transfer_operation transfer_op;
    transfer_op.from = "corp";
    transfer_op.to = "sam";
    transfer_op.amount = ASSET( "1.000 TESTS" );

    tx.operations.push_back( transfer_op );

    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), tx_missing_active_auth );
    signature_type alice_sig = tx.signatures.back();
    sign( tx, bob_private_key );
    signature_type bob_sig = tx.signatures.back();
    sign( tx, sam_private_key );
    signature_type sam_sig = tx.signatures.back();
    HIVE_REQUIRE_THROW( push_transaction( tx, 0 ), tx_irrelevant_sig );

    tx.signatures.clear();
    tx.signatures.push_back( alice_sig );
    tx.signatures.push_back( bob_sig );
    push_transaction( tx, 0 );

    tx.signatures.clear();
    tx.signatures.push_back( alice_sig );
    tx.signatures.push_back( sam_sig );
    HIVE_REQUIRE_THROW( push_transaction( tx, 0 ), fc::exception );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( transfer_apply )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: transfer_apply" );

    ACTORS( (alice)(bob) )
    generate_block();
    fund( "alice", 10000 );
    fund( "bob", ASSET( "1.000 TBD" ) );

    BOOST_REQUIRE( get_balance( "alice" ).amount.value == ASSET( "10.000 TESTS" ).amount.value );
    BOOST_REQUIRE( get_balance( "bob" ).amount.value == ASSET(" 0.000 TESTS" ).amount.value );

    signed_transaction tx;
    transfer_operation op;

    op.from = "alice";
    op.to = "bob";
    op.amount = ASSET( "5.000 TESTS" );

    BOOST_TEST_MESSAGE( "--- Test normal transaction" );
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    push_transaction( tx, alice_private_key, 0 );

    BOOST_REQUIRE( get_balance( "alice" ).amount.value == ASSET( "5.000 TESTS" ).amount.value );
    BOOST_REQUIRE( get_balance( "bob" ).amount.value == ASSET( "5.000 TESTS" ).amount.value );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Generating a block" );
    generate_block();

    const auto& new_alice = db->get_account( "alice" );
    const auto& new_bob = db->get_account( "bob" );

    BOOST_REQUIRE( new_alice.get_balance().amount.value == ASSET( "5.000 TESTS" ).amount.value );
    BOOST_REQUIRE( new_bob.get_balance().amount.value == ASSET( "5.000 TESTS" ).amount.value );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test emptying an account" );
    tx.signatures.clear();
    tx.operations.clear();
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    push_transaction( tx, alice_private_key, database::skip_transaction_dupe_check );

    BOOST_REQUIRE( new_alice.get_balance().amount.value == ASSET( "0.000 TESTS" ).amount.value );
    BOOST_REQUIRE( new_bob.get_balance().amount.value == ASSET( "10.000 TESTS" ).amount.value );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test transferring non-existent funds" );
    tx.signatures.clear();
    tx.operations.clear();
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, database::skip_transaction_dupe_check ), fc::exception );

    BOOST_REQUIRE( new_alice.get_balance().amount.value == ASSET( "0.000 TESTS" ).amount.value );
    BOOST_REQUIRE( new_bob.get_balance().amount.value == ASSET( "10.000 TESTS" ).amount.value );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test successfully transfering HIVE to treasury and converting it to HBD" );
    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();
    auto treasury_hbd_balance = db->get_treasury().get_hbd_balance();
    op.from = "bob";
    op.to = db->get_treasury_name();
    op.amount = ASSET( "1.000 TESTS" );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.signatures.clear();
    tx.operations.clear();
    tx.operations.push_back( op );
    push_transaction( tx, bob_private_key, 0 );
    BOOST_REQUIRE( get_balance( "bob" ) == ASSET( "9.000 TESTS" ) );
    BOOST_REQUIRE( db->get_treasury().get_hbd_balance() == treasury_hbd_balance + ASSET( "1.000 TBD" ) );
    validate_database();

    BOOST_TEST_MESSAGE( "Checking Virtual Operation Correctness" );
    auto dhf_conversion_op = get_last_operations( 1 )[0].get< dhf_conversion_operation >();
    BOOST_REQUIRE( dhf_conversion_op.treasury == op.to );
    BOOST_REQUIRE( dhf_conversion_op.hive_amount_in == op.amount );
    BOOST_REQUIRE( dhf_conversion_op.hbd_amount_out == ASSET( "1.000 TBD" ) );

    BOOST_TEST_MESSAGE( "--- Test transfering HBD to treasury" );
    treasury_hbd_balance = db->get_treasury().get_hbd_balance();
    op.amount = ASSET( "1.000 TBD" );
    tx.signatures.clear();
    tx.operations.clear();
    tx.operations.push_back( op );
    push_transaction( tx, bob_private_key, 0 );

    BOOST_REQUIRE( get_hbd_balance( "bob" ) == ASSET( "0.000 TBD" ) );
    BOOST_REQUIRE( db->get_treasury().get_hbd_balance() == treasury_hbd_balance + ASSET( "1.000 TBD" ) );
    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( transfer_to_vesting_validate )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: transfer_to_vesting_validate" );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( transfer_to_vesting_authorities )
{
  try
  {
    ACTORS( (alice)(bob) )
    fund( "alice", 10000 );

    BOOST_TEST_MESSAGE( "Testing: transfer_to_vesting_authorities" );

    transfer_to_vesting_operation op;
    op.from = "alice";
    op.to = "bob";
    op.amount = ASSET( "2.500 TESTS" );

    signed_transaction tx;
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.operations.push_back( op );

    BOOST_TEST_MESSAGE( "--- Test failure when no signatures" );
    HIVE_REQUIRE_THROW( push_transaction( tx, 0 ), tx_missing_active_auth );

    BOOST_TEST_MESSAGE( "--- Test failure when signed by a signature not in the account's authority" );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_post_key, 0 ), tx_missing_active_auth );

    BOOST_TEST_MESSAGE( "--- Test failure when duplicate signatures" );
    tx.signatures.clear();
    sign( tx, alice_private_key );
    sign( tx, alice_private_key );
    HIVE_REQUIRE_THROW( push_transaction( tx, 0 ), tx_duplicate_sig );

    BOOST_TEST_MESSAGE( "--- Test failure when signed by an additional signature not in the creator's authority" );
    tx.signatures.clear();
    sign( tx, alice_private_key );
    sign( tx, bob_private_key );
    HIVE_REQUIRE_THROW( push_transaction( tx, 0 ), tx_irrelevant_sig );

    BOOST_TEST_MESSAGE( "--- Test success with from signature" );
    tx.signatures.clear();
    push_transaction( tx, alice_private_key, 0 );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( transfer_to_vesting_apply )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: transfer_to_vesting_apply" );

    ACTORS( (alice)(bob) )
    fund( "alice", 10000 );

    const auto& gpo = db->get_dynamic_global_properties();

    BOOST_REQUIRE( alice.get_balance() == ASSET( "10.000 TESTS" ) );

    auto shares = asset( gpo.get_total_vesting_shares().amount, VESTS_SYMBOL );
    auto vests = asset( gpo.get_total_vesting_fund_hive().amount, HIVE_SYMBOL );
    auto alice_shares = alice.get_vesting();
    auto bob_shares = bob.get_vesting();

    transfer_to_vesting_operation op;
    op.from = "alice";
    op.to = db->get_treasury_name();
    op.amount = ASSET( "7.500 TESTS" );

    signed_transaction tx;
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );

    op.to = "";
    tx.clear();
    tx.operations.push_back( op );
    push_transaction( tx, alice_private_key, 0 );

    auto new_vest = op.amount * price( shares, vests );
    shares += new_vest;
    vests += op.amount;
    alice_shares += new_vest;

    BOOST_REQUIRE( alice.get_balance().amount.value == ASSET( "2.500 TESTS" ).amount.value );
    BOOST_REQUIRE( alice.get_vesting().amount.value == alice_shares.amount.value );
    BOOST_REQUIRE( gpo.get_total_vesting_fund_hive().amount.value == vests.amount.value );
    BOOST_REQUIRE( gpo.get_total_vesting_shares().amount.value == shares.amount.value );
    validate_database();

    op.to = "bob";
    op.amount = asset( 2000, HIVE_SYMBOL );
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    push_transaction( tx, alice_private_key, 0 );

    new_vest = asset( ( op.amount * price( shares, vests ) ).amount, VESTS_SYMBOL );
    shares += new_vest;
    vests += op.amount;
    bob_shares += new_vest;

    BOOST_REQUIRE( alice.get_balance().amount.value == ASSET( "0.500 TESTS" ).amount.value );
    BOOST_REQUIRE( alice.get_vesting().amount.value == alice_shares.amount.value );
    BOOST_REQUIRE( bob.get_balance().amount.value == ASSET( "0.000 TESTS" ).amount.value );
    BOOST_REQUIRE( bob.get_vesting().amount.value == bob_shares.amount.value );
    BOOST_REQUIRE( gpo.get_total_vesting_fund_hive().amount.value == vests.amount.value );
    BOOST_REQUIRE( gpo.get_total_vesting_shares().amount.value == shares.amount.value );
    validate_database();

    HIVE_REQUIRE_THROW( push_transaction( tx, database::skip_transaction_dupe_check ), fc::exception );

    BOOST_REQUIRE( alice.get_balance().amount.value == ASSET( "0.500 TESTS" ).amount.value );
    BOOST_REQUIRE( alice.get_vesting().amount.value == alice_shares.amount.value );
    BOOST_REQUIRE( bob.get_balance().amount.value == ASSET( "0.000 TESTS" ).amount.value );
    BOOST_REQUIRE( bob.get_vesting().amount.value == bob_shares.amount.value );
    BOOST_REQUIRE( gpo.get_total_vesting_fund_hive().amount.value == vests.amount.value );
    BOOST_REQUIRE( gpo.get_total_vesting_shares().amount.value == shares.amount.value );
    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( withdraw_vesting_validate )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: withdraw_vesting_validate" );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( withdraw_vesting_authorities )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: withdraw_vesting_authorities" );

    ACTORS( (alice)(bob) )
    fund( "alice", 10000 );
    vest( "alice", 10000 );

    withdraw_vesting_operation op;
    op.account = "alice";
    op.vesting_shares = ASSET( "0.001000 VESTS" );

    signed_transaction tx;
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );

    BOOST_TEST_MESSAGE( "--- Test failure when no signature." );
    HIVE_REQUIRE_THROW( push_transaction( tx, database::skip_transaction_dupe_check ), tx_missing_active_auth );

    BOOST_TEST_MESSAGE( "--- Test success with account signature" );
    push_transaction( tx, alice_private_key, database::skip_transaction_dupe_check );

    BOOST_TEST_MESSAGE( "--- Test failure with duplicate signature" );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, database::skip_transaction_dupe_check ), tx_duplicate_sig );

    BOOST_TEST_MESSAGE( "--- Test failure with additional incorrect signature" );
    tx.signatures.clear();
    sign( tx, alice_private_key );
    sign( tx, bob_private_key );
    HIVE_REQUIRE_THROW( push_transaction( tx, database::skip_transaction_dupe_check ), tx_irrelevant_sig );

    BOOST_TEST_MESSAGE( "--- Test failure with incorrect signature" );
    tx.signatures.clear();
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_post_key, database::skip_transaction_dupe_check ), tx_missing_active_auth );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( withdraw_vesting_apply )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: withdraw_vesting_apply" );

    ACTORS( (alice)(bob) )
    generate_block();
    vest( HIVE_INIT_MINER_NAME, "alice", ASSET( "10.000 TESTS" ) );

    BOOST_TEST_MESSAGE( "--- Test failure withdrawing negative VESTS" );

    {
    const auto& alice = db->get_account( "alice" );

    withdraw_vesting_operation op;
    op.account = "alice";
    op.vesting_shares = asset( -1, VESTS_SYMBOL );

    signed_transaction tx;
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::assert_exception );


    BOOST_TEST_MESSAGE( "--- Test withdraw of existing VESTS" );
    op.vesting_shares = asset( alice.get_vesting().amount / 2, VESTS_SYMBOL );

    auto old_vesting_shares = alice.get_vesting();

    tx.clear();
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    push_transaction( tx, alice_private_key, 0 );

    BOOST_REQUIRE( alice.get_vesting().amount.value == old_vesting_shares.amount.value );
    BOOST_REQUIRE( alice.vesting_withdraw_rate.amount.value == ( old_vesting_shares.amount / ( HIVE_VESTING_WITHDRAW_INTERVALS * 2 ) ).value + 1 );
    BOOST_REQUIRE( alice.to_withdraw.value == op.vesting_shares.amount.value );
    BOOST_REQUIRE( alice.next_vesting_withdrawal == db->head_block_time() + HIVE_VESTING_WITHDRAW_INTERVAL_SECONDS );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test changing vesting withdrawal" );
    tx.operations.clear();
    tx.signatures.clear();

    op.vesting_shares = asset( alice.get_vesting().amount / 3, VESTS_SYMBOL );
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    push_transaction( tx, alice_private_key, 0 );

    BOOST_REQUIRE( alice.get_vesting().amount.value == old_vesting_shares.amount.value );
    BOOST_REQUIRE( alice.vesting_withdraw_rate.amount.value == ( old_vesting_shares.amount / ( HIVE_VESTING_WITHDRAW_INTERVALS * 3 ) ).value + 1 );
    BOOST_REQUIRE( alice.to_withdraw.value == op.vesting_shares.amount.value );
    BOOST_REQUIRE( alice.next_vesting_withdrawal == db->head_block_time() + HIVE_VESTING_WITHDRAW_INTERVAL_SECONDS );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test withdrawing more vests than available" );
    //auto old_withdraw_amount = alice.to_withdraw;
    tx.operations.clear();
    tx.signatures.clear();

    op.vesting_shares = asset( alice.get_vesting().amount * 2, VESTS_SYMBOL );
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );

    BOOST_REQUIRE( alice.get_vesting().amount.value == old_vesting_shares.amount.value );
    BOOST_REQUIRE( alice.vesting_withdraw_rate.amount.value == ( old_vesting_shares.amount / ( HIVE_VESTING_WITHDRAW_INTERVALS * 3 ) ).value + 1 );
    BOOST_REQUIRE( alice.next_vesting_withdrawal == db->head_block_time() + HIVE_VESTING_WITHDRAW_INTERVAL_SECONDS );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test withdrawing 0 to reset vesting withdraw" );
    tx.operations.clear();
    tx.signatures.clear();

    op.vesting_shares = asset( 0, VESTS_SYMBOL );
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    push_transaction( tx, alice_private_key, 0 );

    BOOST_REQUIRE( alice.get_vesting().amount.value == old_vesting_shares.amount.value );
    BOOST_REQUIRE( alice.vesting_withdraw_rate.amount.value == 0 );
    BOOST_REQUIRE( alice.to_withdraw.value == 0 );
    BOOST_REQUIRE( alice.next_vesting_withdrawal == fc::time_point_sec::maximum() );


    BOOST_TEST_MESSAGE( "--- Test cancelling a withdraw when below the account creation fee" );
    op.vesting_shares = alice.get_vesting();
    tx.clear();
    tx.operations.push_back( op );
    push_transaction( tx, alice_private_key, 0 );
    generate_block();
    }

    db_plugin->debug_update( [=]( database& db )
    {
      auto& wso = db.get_witness_schedule_object();

      db.modify( wso, [&]( witness_schedule_object& w )
      {
        w.median_props.account_creation_fee = ASSET( "10.000 TESTS" );
      });

      db.modify( db.get_dynamic_global_properties(), [&]( dynamic_global_property_object& gpo )
      {
        gpo.current_supply += wso.median_props.account_creation_fee - ASSET( "0.001 TESTS" ) - gpo.get_total_vesting_fund_hive();
        gpo.total_vesting_fund_hive = wso.median_props.account_creation_fee - ASSET( "0.001 TESTS" );
      });

      db.update_virtual_supply();
    }, database::skip_witness_signature );

    withdraw_vesting_operation op;
    signed_transaction tx;
    op.account = "alice";
    op.vesting_shares = ASSET( "0.000000 VESTS" );
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    push_transaction( tx, alice_private_key, 0 );

    BOOST_REQUIRE( db->get_account( "alice" ).vesting_withdraw_rate == ASSET( "0.000000 VESTS" ) );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test withdrawing minimal VESTS" );
    op.account = "bob";
    op.vesting_shares = get_vesting( "bob" );
    tx.clear();
    tx.operations.push_back( op );
    push_transaction( tx, bob_private_key, 0 ); // We do not need to test the result of this, simply that it works.
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( witness_update_validate )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: withness_update_validate" );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( witness_update_authorities )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: witness_update_authorities" );

    ACTORS( (alice)(bob) );
    fund( "alice", 10000 );

    private_key_type signing_key = generate_private_key( "new_key" );

    witness_update_operation op;
    op.owner = "alice";
    op.url = "foo.bar";
    op.fee = ASSET( "1.000 TESTS" );
    op.block_signing_key = signing_key.get_public_key();

    signed_transaction tx;
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.operations.push_back( op );

    BOOST_TEST_MESSAGE( "--- Test failure when no signatures" );
    HIVE_REQUIRE_THROW( push_transaction( tx, 0 ), tx_missing_active_auth );

    BOOST_TEST_MESSAGE( "--- Test failure when signed by a signature not in the account's authority" );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_post_key, 0 ), tx_missing_active_auth );

    BOOST_TEST_MESSAGE( "--- Test failure when duplicate signatures" );
    tx.signatures.clear();
    sign( tx, alice_private_key );
    sign( tx, alice_private_key );
    HIVE_REQUIRE_THROW( push_transaction( tx, 0 ), tx_duplicate_sig );

    BOOST_TEST_MESSAGE( "--- Test failure when signed by an additional signature not in the creator's authority" );
    tx.signatures.clear();
    sign( tx, alice_private_key );
    sign( tx, bob_private_key );
    HIVE_REQUIRE_THROW( push_transaction( tx, 0 ), tx_irrelevant_sig );

    BOOST_TEST_MESSAGE( "--- Test success with witness signature" );
    tx.signatures.clear();
    push_transaction( tx, alice_private_key, 0 );

    tx.signatures.clear();
    HIVE_REQUIRE_THROW( push_transaction( tx, signing_key, database::skip_transaction_dupe_check ), tx_missing_active_auth );
    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( witness_update_apply )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: witness_update_apply" );

    ACTORS( (alice) )
    fund( "alice", 10000 );

    private_key_type signing_key = generate_private_key( "new_key" );

    BOOST_TEST_MESSAGE( "--- Test upgrading an account to a witness" );

    witness_update_operation op;
    op.owner = "alice";
    op.url = "foo.bar";
    op.fee = ASSET( "1.000 TESTS" );
    op.block_signing_key = signing_key.get_public_key();
    op.props.account_creation_fee = legacy_hive_asset::from_asset( asset(HIVE_MIN_ACCOUNT_CREATION_FEE + 10, HIVE_SYMBOL) );
    op.props.maximum_block_size = HIVE_MIN_BLOCK_SIZE_LIMIT + 100;

    signed_transaction tx;
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.operations.push_back( op );

    push_transaction( tx, alice_private_key, 0 );

    const witness_object& alice_witness = db->get_witness( "alice" );

    BOOST_REQUIRE( alice_witness.owner == "alice" );
    BOOST_REQUIRE( alice_witness.created == db->head_block_time() );
    BOOST_REQUIRE( to_string( alice_witness.url ) == op.url );
    BOOST_REQUIRE( alice_witness.signing_key == op.block_signing_key );
    BOOST_REQUIRE( alice_witness.props.account_creation_fee == op.props.account_creation_fee.to_asset<true>() );
    BOOST_REQUIRE( alice_witness.props.maximum_block_size == op.props.maximum_block_size );
    BOOST_REQUIRE( alice_witness.total_missed == 0 );
    BOOST_REQUIRE( alice_witness.last_aslot == 0 );
    BOOST_REQUIRE( alice_witness.last_confirmed_block_num == 0 );
    BOOST_REQUIRE( alice_witness.pow_worker == 0 );
    BOOST_REQUIRE( alice_witness.votes.value == 0 );
    BOOST_REQUIRE( alice_witness.virtual_last_update == 0 );
    BOOST_REQUIRE( alice_witness.virtual_position == 0 );
    BOOST_REQUIRE( alice_witness.virtual_scheduled_time == fc::uint128_t::max_value() );
    BOOST_REQUIRE( alice.get_balance().amount.value == ASSET( "10.000 TESTS" ).amount.value ); // No fee
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test updating a witness" );

    tx.signatures.clear();
    tx.operations.clear();
    op.url = "bar.foo";
    tx.operations.push_back( op );

    push_transaction( tx, alice_private_key, 0 );

    BOOST_REQUIRE( alice_witness.owner == "alice" );
    BOOST_REQUIRE( alice_witness.created == db->head_block_time() );
    BOOST_REQUIRE( to_string( alice_witness.url ) == "bar.foo" );
    BOOST_REQUIRE( alice_witness.signing_key == op.block_signing_key );
    BOOST_REQUIRE( alice_witness.props.account_creation_fee == op.props.account_creation_fee.to_asset<true>() );
    BOOST_REQUIRE( alice_witness.props.maximum_block_size == op.props.maximum_block_size );
    BOOST_REQUIRE( alice_witness.total_missed == 0 );
    BOOST_REQUIRE( alice_witness.last_aslot == 0 );
    BOOST_REQUIRE( alice_witness.last_confirmed_block_num == 0 );
    BOOST_REQUIRE( alice_witness.pow_worker == 0 );
    BOOST_REQUIRE( alice_witness.votes.value == 0 );
    BOOST_REQUIRE( alice_witness.virtual_last_update == 0 );
    BOOST_REQUIRE( alice_witness.virtual_position == 0 );
    BOOST_REQUIRE( alice_witness.virtual_scheduled_time == fc::uint128_t::max_value() );
    BOOST_REQUIRE( alice.get_balance().amount.value == ASSET( "10.000 TESTS" ).amount.value );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test failure when upgrading a non-existent account" );

    tx.signatures.clear();
    tx.operations.clear();
    op.owner = "bob";
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );
    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( account_witness_vote_validate )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: account_witness_vote_validate" );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( account_witness_vote_authorities )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: account_witness_vote_authorities" );

    ACTORS( (alice)(bob)(sam) )

    fund( "alice", 1000 );
    private_key_type alice_witness_key = generate_private_key( "alice_witness" );
    witness_create( "alice", alice_private_key, "foo.bar", alice_witness_key.get_public_key(), 1000 );

    account_witness_vote_operation op;
    op.account = "bob";
    op.witness = "alice";

    signed_transaction tx;
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.operations.push_back( op );

    BOOST_TEST_MESSAGE( "--- Test failure when no signatures" );
    HIVE_REQUIRE_THROW( push_transaction( tx, 0 ), tx_missing_active_auth );

    BOOST_TEST_MESSAGE( "--- Test failure when signed by a signature not in the account's authority" );
    HIVE_REQUIRE_THROW( push_transaction( tx, bob_post_key, 0 ), tx_missing_active_auth );

    BOOST_TEST_MESSAGE( "--- Test failure when duplicate signatures" );
    tx.signatures.clear();
    sign( tx, bob_private_key );
    sign( tx, bob_private_key );
    HIVE_REQUIRE_THROW( push_transaction( tx, 0 ), tx_duplicate_sig );

    BOOST_TEST_MESSAGE( "--- Test failure when signed by an additional signature not in the creator's authority" );
    tx.signatures.clear();
    sign( tx, bob_private_key );
    sign( tx, alice_private_key );
    HIVE_REQUIRE_THROW( push_transaction( tx, 0 ), tx_irrelevant_sig );

    BOOST_TEST_MESSAGE( "--- Test success with witness signature" );
    tx.signatures.clear();
    push_transaction( tx, bob_private_key, 0 );

    BOOST_TEST_MESSAGE( "--- Test failure with proxy signature" );
    proxy( "bob", "sam" );
    tx.signatures.clear();
    HIVE_REQUIRE_THROW( push_transaction( tx, sam_private_key, database::skip_transaction_dupe_check ), tx_missing_active_auth );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( account_witness_vote_apply )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: account_witness_vote_apply" );

    ACTORS( (alice)(bob)(sam) )
    fund( "alice" , 5000 );
    vest( "alice", 5000 );
    fund( "sam", 1000 );

    private_key_type sam_witness_key = generate_private_key( "sam_key" );
    witness_create( "sam", sam_private_key, "foo.bar", sam_witness_key.get_public_key(), 1000 );
    const witness_object& sam_witness = db->get_witness( "sam" );

    const auto& witness_vote_idx = db->get_index< witness_vote_index >().indices().get< by_witness_account >();

    BOOST_TEST_MESSAGE( "--- Test normal vote" );
    account_witness_vote_operation op;
    op.account = "alice";
    op.witness = "sam";
    op.approve = true;

    signed_transaction tx;
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.operations.push_back( op );

    push_transaction( tx, alice_private_key, 0 );

    BOOST_REQUIRE( sam_witness.votes == alice.get_real_vesting_shares() );
    BOOST_REQUIRE( witness_vote_idx.find( boost::make_tuple( sam_witness.owner, alice.name ) ) != witness_vote_idx.end() );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test revoke vote" );
    op.approve = false;
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );

    push_transaction( tx, alice_private_key, 0 );
    BOOST_REQUIRE( sam_witness.votes.value == 0 );
    BOOST_REQUIRE( witness_vote_idx.find( boost::make_tuple( sam_witness.owner, alice.name ) ) == witness_vote_idx.end() );

    BOOST_TEST_MESSAGE( "--- Test failure when attempting to revoke a non-existent vote" );

    HIVE_REQUIRE_THROW( push_transaction( tx, database::skip_transaction_dupe_check ), fc::exception );
    BOOST_REQUIRE( sam_witness.votes.value == 0 );
    BOOST_REQUIRE( witness_vote_idx.find( boost::make_tuple( sam_witness.owner, alice.name ) ) == witness_vote_idx.end() );

    BOOST_TEST_MESSAGE( "--- Test proxied vote" );
    proxy( "alice", "bob" );
    tx.operations.clear();
    tx.signatures.clear();
    op.approve = true;
    op.account = "bob";
    tx.operations.push_back( op );

    push_transaction( tx, bob_private_key, 0 );

    BOOST_REQUIRE( sam_witness.votes == ( bob.proxied_vsf_votes_total() + bob.get_real_vesting_shares() ) );
    BOOST_REQUIRE( witness_vote_idx.find( boost::make_tuple( sam_witness.owner, bob.name ) ) != witness_vote_idx.end() );
    BOOST_REQUIRE( witness_vote_idx.find( boost::make_tuple( sam_witness.owner, alice.name ) ) == witness_vote_idx.end() );

    BOOST_TEST_MESSAGE( "--- Test vote from a proxied account" );
    tx.operations.clear();
    tx.signatures.clear();
    op.account = "alice";
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, database::skip_transaction_dupe_check ), fc::exception );

    BOOST_REQUIRE( sam_witness.votes == ( bob.proxied_vsf_votes_total() + bob.get_real_vesting_shares() ) );
    BOOST_REQUIRE( witness_vote_idx.find( boost::make_tuple( sam_witness.owner, bob.name ) ) != witness_vote_idx.end() );
    BOOST_REQUIRE( witness_vote_idx.find( boost::make_tuple( sam_witness.owner, alice.name ) ) == witness_vote_idx.end() );

    BOOST_TEST_MESSAGE( "--- Test revoke proxied vote" );
    tx.operations.clear();
    tx.signatures.clear();
    op.account = "bob";
    op.approve = false;
    tx.operations.push_back( op );

    push_transaction( tx, bob_private_key, 0 );

    BOOST_REQUIRE( sam_witness.votes.value == 0 );
    BOOST_REQUIRE( witness_vote_idx.find( boost::make_tuple( sam_witness.owner, bob.name ) ) == witness_vote_idx.end() );
    BOOST_REQUIRE( witness_vote_idx.find( boost::make_tuple( sam_witness.owner, alice.name ) ) == witness_vote_idx.end() );

    BOOST_TEST_MESSAGE( "--- Test failure when voting for a non-existent account" );
    tx.operations.clear();
    tx.signatures.clear();
    op.witness = "dave";
    op.approve = true;
    tx.operations.push_back( op );

    HIVE_REQUIRE_THROW( push_transaction( tx, bob_private_key, 0 ), fc::exception );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test failure when voting for an account that is not a witness" );
    tx.operations.clear();
    tx.signatures.clear();
    op.witness = "alice";
    tx.operations.push_back( op );

    HIVE_REQUIRE_THROW( push_transaction( tx, bob_private_key, 0 ), fc::exception );
    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(account_witness_vote_apply_delay)
{
  //copy of account_witness_vote_apply with extra checks to account for delayed voting effect
  //note that call to generate_block() invalidates everything that was done by push_transaction,
  //in particular local variables like `alice` and `sam_witness` that point to objects created
  //by push_transaction (and thus present inside pending block state) become invalid;
  //generate_block call first calls `undo` on pending block, then reapplies all transactions,
  //so all the objects are present in chain, but have to be acquired again from final state
  try
  {
    BOOST_TEST_MESSAGE("Testing: account_witness_vote_apply_delay");

    ACTORS((alice)(bob)(sam))
    fund("alice", 5000);
    vest("alice", 5000);
    //vests are going to vote after HIVE_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS
    //note that account creation also vests a bit for new account
    fund("sam", 1000);

    private_key_type sam_witness_key = generate_private_key("sam_key");
    witness_create("sam", sam_private_key, "foo.bar", sam_witness_key.get_public_key(), 1000);

    generate_block();
    //above variables: alice/bob/sam are invalid past that point
    //they need to be reacquired from chain
    const account_object& _alice = db->get_account("alice");
    const account_object& _bob = db->get_account("bob");
    //const account_object& _sam = db->get_account("sam");
    const witness_object& _sam_witness = db->get_witness("sam");
    const auto& witness_vote_idx = db->get_index< witness_vote_index >().indices().get< by_witness_account >();

    BOOST_TEST_MESSAGE("--- Test normal vote");
    account_witness_vote_operation op;
    op.account = "alice";
    op.witness = "sam";
    op.approve = true;

    signed_transaction tx;
    tx.set_expiration(db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION);
    tx.operations.push_back(op);
    sign(tx, alice_private_key);

    push_transaction(tx, 0);

    BOOST_REQUIRE(_sam_witness.votes == 0);
    BOOST_REQUIRE(_alice.get_vesting().amount == _alice.sum_delayed_votes.value);
    BOOST_REQUIRE(witness_vote_idx.find(boost::make_tuple(_sam_witness.owner, _alice.name)) != witness_vote_idx.end());
    validate_database();
    generate_blocks(db->head_block_time() + fc::seconds(HIVE_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS));
    BOOST_REQUIRE(_sam_witness.votes == _alice.get_vesting().amount);
    BOOST_REQUIRE(_alice.sum_delayed_votes == 0);
    validate_database();

    BOOST_TEST_MESSAGE("--- Test revoke vote");
    tx.set_expiration(db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION);
    op.approve = false;
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back(op);
    sign(tx, alice_private_key);

    push_transaction(tx, 0);
    BOOST_REQUIRE(_sam_witness.votes.value == 0);
    BOOST_REQUIRE(witness_vote_idx.find(boost::make_tuple(_sam_witness.owner, _alice.name)) == witness_vote_idx.end());

    BOOST_TEST_MESSAGE("--- Test failure when attempting to revoke a non-existent vote");

    HIVE_REQUIRE_THROW(push_transaction(tx, database::skip_transaction_dupe_check), fc::exception);
    BOOST_REQUIRE(_sam_witness.votes.value == 0);
    BOOST_REQUIRE(witness_vote_idx.find(boost::make_tuple(_sam_witness.owner, _alice.name)) == witness_vote_idx.end());

    BOOST_TEST_MESSAGE("--- Test proxied vote");
    proxy("alice", "bob");
    tx.operations.clear();
    tx.signatures.clear();
    op.approve = true;
    op.account = "bob";
    tx.operations.push_back(op);
    sign(tx, bob_private_key);

    push_transaction(tx, 0);

    //since all vests are already mature voting has immediate effect
    BOOST_REQUIRE(_alice.get_vesting().amount == _bob.proxied_vsf_votes_total());
    BOOST_REQUIRE(_sam_witness.votes == (_alice.get_vesting().amount + _bob.get_vesting().amount));
    BOOST_REQUIRE(witness_vote_idx.find(boost::make_tuple(_sam_witness.owner, _bob.name)) != witness_vote_idx.end());
    BOOST_REQUIRE(witness_vote_idx.find(boost::make_tuple(_sam_witness.owner, _alice.name)) == witness_vote_idx.end());
    validate_database();

    BOOST_TEST_MESSAGE("--- Test vote from a proxied account");
    tx.operations.clear();
    tx.signatures.clear();
    op.account = "alice";
    tx.operations.push_back(op);
    sign(tx, alice_private_key);
    HIVE_REQUIRE_THROW(push_transaction(tx, database::skip_transaction_dupe_check), fc::exception);

    BOOST_REQUIRE(_alice.get_vesting().amount == _bob.proxied_vsf_votes_total());
    BOOST_REQUIRE(_sam_witness.votes == (_alice.get_vesting().amount + _bob.get_vesting().amount));
    BOOST_REQUIRE(witness_vote_idx.find(boost::make_tuple(_sam_witness.owner, _bob.name)) != witness_vote_idx.end());
    BOOST_REQUIRE(witness_vote_idx.find(boost::make_tuple(_sam_witness.owner, _alice.name)) == witness_vote_idx.end());

    BOOST_TEST_MESSAGE("--- Test revoke proxied vote");
    tx.operations.clear();
    tx.signatures.clear();
    op.account = "bob";
    op.approve = false;
    tx.operations.push_back(op);
    sign(tx, bob_private_key);

    push_transaction(tx, 0);

    BOOST_REQUIRE(_sam_witness.votes.value == 0);
    BOOST_REQUIRE(witness_vote_idx.find(boost::make_tuple(_sam_witness.owner, _bob.name)) == witness_vote_idx.end());
    BOOST_REQUIRE(witness_vote_idx.find(boost::make_tuple(_sam_witness.owner, _alice.name)) == witness_vote_idx.end());

    BOOST_TEST_MESSAGE("--- Test failure when voting for a non-existent account");
    tx.operations.clear();
    tx.signatures.clear();
    op.witness = "dave";
    op.approve = true;
    tx.operations.push_back(op);
    sign(tx, bob_private_key);

    HIVE_REQUIRE_THROW(push_transaction(tx, 0), fc::exception);
    validate_database();

    BOOST_TEST_MESSAGE("--- Test failure when voting for an account that is not a witness");
    tx.operations.clear();
    tx.signatures.clear();
    op.witness = "alice";
    tx.operations.push_back(op);
    sign(tx, bob_private_key);

    HIVE_REQUIRE_THROW(push_transaction(tx, 0), fc::exception);
    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( account_object_by_governance_vote_expiration_ts_idx )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: account_object_by_governance_vote_expiration_ts_idx" );

    ACTORS( (acc1)(acc2)(acc3)(acc4)(accw) )
    signed_transaction tx;
    private_key_type accw_witness_key = generate_private_key( "accw_key" );
    witness_create( "accw", accw_private_key, "foo.bar", accw_witness_key.get_public_key(), 1000 );

    //Cannot use vote_proposal() and witness_vote() because of differ DB Fixture
    auto witness_vote = [&](std::string voter, const fc::ecc::private_key& key) {
      account_witness_vote_operation op;
      op.account = voter;
      op.witness = "accw";
      op.approve = true;

      tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
      tx.operations.push_back( op );
      push_transaction( tx, key, 0 );
      tx.clear();
    };

    auto proposal_vote = [&](std::string voter, const std::vector<int64_t>& proposals, const fc::ecc::private_key& key) {
      update_proposal_votes_operation op;
      op.voter = voter;
      op.proposal_ids.insert(proposals.cbegin(), proposals.cend());  //doesn't matter which ids, order in the container matters in this test case.
      op.approve = true;

      tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
      tx.operations.push_back( op );
      push_transaction( tx, key, 0 );
      tx.clear();
    };

    generate_block();
    witness_vote("acc1", acc1_private_key);
    generate_block();
    witness_vote("acc2", acc2_private_key);
    proposal_vote("acc3", {154,357,987}, acc3_private_key);
    generate_block();
    proposal_vote("acc4", {111,222,333,444,555}, acc4_private_key);
    generate_block();


    BOOST_REQUIRE (db->get_account( "acc1" ).get_governance_vote_expiration_ts() != db->get_account( "acc2" ).get_governance_vote_expiration_ts());
    BOOST_REQUIRE (db->get_account( "acc2" ).get_governance_vote_expiration_ts() == db->get_account( "acc3" ).get_governance_vote_expiration_ts());
    BOOST_REQUIRE (db->get_account( "acc4" ).get_governance_vote_expiration_ts() != db->get_account( "acc3" ).get_governance_vote_expiration_ts());

    const auto& accounts = db->get_index< account_index, by_governance_vote_expiration_ts >();
    time_point_sec governance_vote_expiration_ts = accounts.begin()->get_governance_vote_expiration_ts();

    for (const auto&ac : accounts)
    {
      time_point_sec curr_last_vote = ac.get_governance_vote_expiration_ts();
      BOOST_REQUIRE (governance_vote_expiration_ts <= curr_last_vote);
      governance_vote_expiration_ts = curr_last_vote;
    }
    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( account_witness_proxy_validate )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: account_witness_proxy_validate" );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( account_witness_proxy_authorities )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: account_witness_proxy_authorities" );

    ACTORS( (alice)(bob) )

    account_witness_proxy_operation op;
    op.account = "bob";
    op.proxy = "alice";

    signed_transaction tx;
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.operations.push_back( op );

    BOOST_TEST_MESSAGE( "--- Test failure when no signatures" );
    HIVE_REQUIRE_THROW( push_transaction( tx, 0 ), tx_missing_active_auth );

    BOOST_TEST_MESSAGE( "--- Test failure when signed by a signature not in the account's authority" );
    HIVE_REQUIRE_THROW( push_transaction( tx, bob_post_key, 0 ), tx_missing_active_auth );

    BOOST_TEST_MESSAGE( "--- Test failure when duplicate signatures" );
    tx.signatures.clear();
    sign( tx, bob_private_key );
    sign( tx, bob_private_key );
    HIVE_REQUIRE_THROW( push_transaction( tx, 0 ), tx_duplicate_sig );

    BOOST_TEST_MESSAGE( "--- Test failure when signed by an additional signature not in the creator's authority" );
    tx.signatures.clear();
    sign( tx, bob_private_key );
    sign( tx, alice_private_key );
    HIVE_REQUIRE_THROW( push_transaction( tx, 0 ), tx_irrelevant_sig );

    BOOST_TEST_MESSAGE( "--- Test success with witness signature" );
    tx.signatures.clear();
    push_transaction( tx, bob_private_key, 0 );

    BOOST_TEST_MESSAGE( "--- Test failure with proxy signature" );
    tx.signatures.clear();
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, database::skip_transaction_dupe_check ), tx_missing_active_auth );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( account_witness_proxy_apply )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: account_witness_proxy_apply" );

    ACTORS( (alice)(bob)(sam)(dave) )
    fund( "alice", 1000 );
    vest( "alice", 1000 );
    fund( "bob", 3000 );
    vest( "bob", 3000 );
    fund( "sam", 5000 );
    vest( "sam", 5000 );
    fund( "dave", 7000 );
    vest( "dave", 7000 );

    BOOST_TEST_MESSAGE( "--- Test setting proxy to another account from self." );
    // bob -> alice

    account_witness_proxy_operation op;
    op.account = "bob";
    op.proxy = "alice";

    signed_transaction tx;
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );

    push_transaction( tx, bob_private_key, 0 );

    CHECK_PROXY( bob, alice );
    BOOST_REQUIRE( bob.proxied_vsf_votes_total().value == 0 );
    CHECK_NO_PROXY( alice );
    BOOST_REQUIRE( alice.proxied_vsf_votes_total() == bob.get_real_vesting_shares() );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test changing proxy" );
    // bob->sam

    tx.operations.clear();
    tx.signatures.clear();
    op.proxy = "sam";
    tx.operations.push_back( op );

    push_transaction( tx, bob_private_key, 0 );

    CHECK_PROXY( bob, sam );
    BOOST_REQUIRE( bob.proxied_vsf_votes_total().value == 0 );
    BOOST_REQUIRE( alice.proxied_vsf_votes_total().value == 0 );
    CHECK_NO_PROXY( sam );
    BOOST_REQUIRE( sam.proxied_vsf_votes_total().value == bob.get_real_vesting_shares() );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test failure when changing proxy to existing proxy" );

    HIVE_REQUIRE_THROW( push_transaction( tx, database::skip_transaction_dupe_check ), fc::exception );

    CHECK_PROXY( bob, sam );
    BOOST_REQUIRE( bob.proxied_vsf_votes_total().value == 0 );
    CHECK_NO_PROXY( sam );
    BOOST_REQUIRE( sam.proxied_vsf_votes_total() == bob.get_real_vesting_shares() );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test adding a grandparent proxy" );
    // bob->sam->dave

    tx.operations.clear();
    tx.signatures.clear();
    op.proxy = "dave";
    op.account = "sam";
    tx.operations.push_back( op );

    push_transaction( tx, sam_private_key, 0 );

    CHECK_PROXY( bob, sam );
    BOOST_REQUIRE( bob.proxied_vsf_votes_total().value == 0 );
    CHECK_PROXY( sam, dave );
    BOOST_REQUIRE( sam.proxied_vsf_votes_total() == bob.get_real_vesting_shares() );
    CHECK_NO_PROXY( dave );
    BOOST_REQUIRE( dave.proxied_vsf_votes_total() == ( sam.get_real_vesting_shares() + bob.get_real_vesting_shares() ) );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test adding a grandchild proxy" );
    //       alice
    //         |
    // bob->  sam->dave

    tx.operations.clear();
    tx.signatures.clear();
    op.proxy = "sam";
    op.account = "alice";
    tx.operations.push_back( op );

    push_transaction( tx, alice_private_key, 0 );

    CHECK_PROXY( alice, sam );
    BOOST_REQUIRE( alice.proxied_vsf_votes_total().value == 0 );
    CHECK_PROXY( bob, sam );
    BOOST_REQUIRE( bob.proxied_vsf_votes_total().value == 0 );
    CHECK_PROXY( sam, dave );
    BOOST_REQUIRE( sam.proxied_vsf_votes_total() == ( bob.get_real_vesting_shares() + alice.get_real_vesting_shares() ) );
    CHECK_NO_PROXY( dave );
    BOOST_REQUIRE( dave.proxied_vsf_votes_total() == ( sam.get_real_vesting_shares() + bob.get_real_vesting_shares() + alice.get_real_vesting_shares() ) );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test removing a grandchild proxy" );
    // alice->sam->dave

    tx.operations.clear();
    tx.signatures.clear();
    op.proxy = HIVE_PROXY_TO_SELF_ACCOUNT;
    op.account = "bob";
    tx.operations.push_back( op );

    push_transaction( tx, bob_private_key, 0 );

    CHECK_PROXY( alice, sam );
    BOOST_REQUIRE( alice.proxied_vsf_votes_total().value == 0 );
    CHECK_NO_PROXY( bob );
    BOOST_REQUIRE( bob.proxied_vsf_votes_total().value == 0 );
    CHECK_PROXY( sam, dave );
    BOOST_REQUIRE( sam.proxied_vsf_votes_total() == alice.get_real_vesting_shares() );
    CHECK_NO_PROXY( dave );
    BOOST_REQUIRE( dave.proxied_vsf_votes_total() == ( sam.get_real_vesting_shares() + alice.get_real_vesting_shares() ) );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test votes are transferred when a proxy is added" );
    account_witness_vote_operation vote;
    vote.account= "bob";
    vote.witness = HIVE_INIT_MINER_NAME;
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( vote );

    push_transaction( tx, bob_private_key, 0 );

    tx.operations.clear();
    tx.signatures.clear();
    op.account = "alice";
    op.proxy = "bob";
    tx.operations.push_back( op );

    push_transaction( tx, alice_private_key, 0 );

    BOOST_REQUIRE( db->get_witness( HIVE_INIT_MINER_NAME ).votes == ( alice.get_real_vesting_shares() + bob.get_real_vesting_shares() ) );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test votes are removed when a proxy is removed" );
    op.proxy = HIVE_PROXY_TO_SELF_ACCOUNT;
    tx.signatures.clear();
    tx.operations.clear();
    tx.operations.push_back( op );

    push_transaction( tx, alice_private_key, 0 );

    BOOST_REQUIRE( db->get_witness( HIVE_INIT_MINER_NAME ).votes == bob.get_real_vesting_shares() );
    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( account_witness_proxy_apply_delay )
{
  //copy of account_witness_proxy_apply with extra checks to account for delayed voting effect
  //see comment in account_witness_vote_apply_delay
  try
  {
    BOOST_TEST_MESSAGE( "Testing: account_witness_proxy_apply_delay" );

    ACTORS( (alice)(bob)(sam)(dave) )
    fund( "alice", 1000 );
    vest( "alice", 1000 );
    //vests are going to vote after HIVE_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS
    //note that account creation also vests a bit for new account
    fund( "bob", 3000 );
    vest( "bob", 3000 );
    fund( "sam", 5000 );
    vest( "sam", 5000 );
    fund( "dave", 7000 );
    vest( "dave", 7000 );

    generate_block();
    //above variables: alice etc. are invalid past that point
    //they need to be reacquired from chain
    const account_object& _alice = db->get_account("alice");
    const account_object& _bob = db->get_account("bob");
    const account_object& _sam = db->get_account("sam");
    const account_object& _dave = db->get_account("dave");

    BOOST_TEST_MESSAGE( "--- Test setting proxy to another account from self." );
    // bob -> alice

    account_witness_proxy_operation op;
    op.account = "bob";
    op.proxy = "alice";

    signed_transaction tx;
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );

    push_transaction( tx, bob_private_key, 0 );

    CHECK_PROXY( _bob, _alice );
    BOOST_REQUIRE( _bob.proxied_vsf_votes_total().value == 0 );
    CHECK_NO_PROXY( _alice );
    BOOST_REQUIRE( _alice.proxied_vsf_votes_total().value == 0 );
    validate_database();
    generate_blocks(db->head_block_time() + fc::seconds(HIVE_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS));
    BOOST_REQUIRE(_alice.proxied_vsf_votes_total() == _bob.get_vesting().amount);

    BOOST_TEST_MESSAGE( "--- Test changing proxy" );
    // bob->sam

    tx.set_expiration(db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION);
    tx.operations.clear();
    tx.signatures.clear();
    op.proxy = "sam";
    tx.operations.push_back( op );

    push_transaction( tx, bob_private_key, 0 );

    CHECK_PROXY( _bob, _sam );
    BOOST_REQUIRE( _bob.proxied_vsf_votes_total().value == 0 );
    BOOST_REQUIRE( _alice.proxied_vsf_votes_total().value == 0 );
    CHECK_NO_PROXY( _sam );
    //all vests are now mature so changes in voting power are immediate
    BOOST_REQUIRE( _sam.proxied_vsf_votes_total().value == _bob.get_vesting().amount );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test failure when changing proxy to existing proxy" );

    HIVE_REQUIRE_THROW( push_transaction( tx, database::skip_transaction_dupe_check ), fc::exception );

    CHECK_PROXY( _bob, _sam );
    BOOST_REQUIRE( _bob.proxied_vsf_votes_total().value == 0 );
    CHECK_NO_PROXY( _sam );
    BOOST_REQUIRE( _sam.proxied_vsf_votes_total() == _bob.get_vesting().amount );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test adding a grandparent proxy" );
    // bob->sam->dave

    tx.operations.clear();
    tx.signatures.clear();
    op.proxy = "dave";
    op.account = "sam";
    tx.operations.push_back( op );

    push_transaction( tx, sam_private_key, 0 );

    CHECK_PROXY( _bob, _sam );
    BOOST_REQUIRE( _bob.proxied_vsf_votes_total().value == 0 );
    CHECK_PROXY( _sam, _dave );
    BOOST_REQUIRE( _sam.proxied_vsf_votes_total() == _bob.get_vesting().amount );
    CHECK_NO_PROXY( _dave );
    BOOST_REQUIRE( _dave.proxied_vsf_votes_total() == ( _sam.get_vesting() + _bob.get_vesting() ).amount );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test adding a grandchild proxy" );
    //       alice
    //         |
    // bob->  sam->dave

    tx.operations.clear();
    tx.signatures.clear();
    op.proxy = "sam";
    op.account = "alice";
    tx.operations.push_back( op );

    push_transaction( tx, alice_private_key, 0 );

    CHECK_PROXY( _alice, _sam );
    BOOST_REQUIRE( _alice.proxied_vsf_votes_total().value == 0 );
    CHECK_PROXY( _bob, _sam );
    BOOST_REQUIRE( _bob.proxied_vsf_votes_total().value == 0 );
    CHECK_PROXY( _sam, _dave );
    BOOST_REQUIRE( _sam.proxied_vsf_votes_total() == ( _bob.get_vesting() + _alice.get_vesting() ).amount );
    CHECK_NO_PROXY( _dave );
    BOOST_REQUIRE( _dave.proxied_vsf_votes_total() == ( _sam.get_vesting() + _bob.get_vesting() + _alice.get_vesting() ).amount );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test removing a grandchild proxy" );
    // alice->sam->dave

    tx.operations.clear();
    tx.signatures.clear();
    op.proxy = HIVE_PROXY_TO_SELF_ACCOUNT;
    op.account = "bob";
    tx.operations.push_back( op );

    push_transaction( tx, bob_private_key, 0 );

    CHECK_PROXY( _alice, _sam );
    BOOST_REQUIRE( _alice.proxied_vsf_votes_total().value == 0 );
    CHECK_NO_PROXY( _bob );
    BOOST_REQUIRE( _bob.proxied_vsf_votes_total().value == 0 );
    CHECK_PROXY( _sam, _dave );
    BOOST_REQUIRE( _sam.proxied_vsf_votes_total() == _alice.get_vesting().amount );
    CHECK_NO_PROXY( _dave );
    BOOST_REQUIRE( _dave.proxied_vsf_votes_total() == ( _sam.get_vesting() + _alice.get_vesting() ).amount );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test votes are transferred when a proxy is added" );
    account_witness_vote_operation vote;
    vote.account= "bob";
    vote.witness = HIVE_INIT_MINER_NAME;
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( vote );

    push_transaction( tx, bob_private_key, 0 );

    tx.operations.clear();
    tx.signatures.clear();
    op.account = "alice";
    op.proxy = "bob";
    tx.operations.push_back( op );

    push_transaction( tx, alice_private_key, 0 );

    BOOST_REQUIRE( db->get_witness( HIVE_INIT_MINER_NAME ).votes == ( _alice.get_vesting() + _bob.get_vesting() ).amount );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test votes are removed when a proxy is removed" );
    op.proxy = HIVE_PROXY_TO_SELF_ACCOUNT;
    tx.signatures.clear();
    tx.operations.clear();
    tx.operations.push_back( op );

    push_transaction( tx, alice_private_key, 0 );

    BOOST_REQUIRE( db->get_witness( HIVE_INIT_MINER_NAME ).votes == _bob.get_vesting().amount );
    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( custom_authorities )
{
  custom_operation op;
  op.required_auths.insert( "alice" );
  op.required_auths.insert( "bob" );

  flat_set< account_name_type > auths;
  flat_set< account_name_type > expected;

  op.get_required_owner_authorities( auths );
  BOOST_REQUIRE( auths == expected );

  op.get_required_posting_authorities( auths );
  BOOST_REQUIRE( auths == expected );

  expected.insert( "alice" );
  expected.insert( "bob" );
  op.get_required_active_authorities( auths );
  BOOST_REQUIRE( auths == expected );
}

BOOST_AUTO_TEST_CASE( custom_json_authorities )
{
  custom_json_operation op;
  op.required_auths.insert( "alice" );
  op.required_posting_auths.insert( "bob" );

  flat_set< account_name_type > auths;
  flat_set< account_name_type > expected;

  op.get_required_owner_authorities( auths );
  BOOST_REQUIRE( auths == expected );

  expected.insert( "alice" );
  op.get_required_active_authorities( auths );
  BOOST_REQUIRE( auths == expected );

  auths.clear();
  expected.clear();
  expected.insert( "bob" );
  op.get_required_posting_authorities( auths );
  BOOST_REQUIRE( auths == expected );
}

BOOST_AUTO_TEST_CASE( custom_json_rate_limit )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: custom_json_rate_limit" );

    ACTORS( (alice)(bob)(sam) )

    BOOST_TEST_MESSAGE( "--- Testing 5 custom json ops as separate transactions" );

    custom_json_operation op;
    signed_transaction tx;
    op.required_posting_auths.insert( "alice" );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );

    for( int i = 0; i < 5; i++ )
    {
      op.json = fc::to_string( i );
      tx.clear();
      tx.operations.push_back( op );
      push_transaction( tx, alice_private_key, 0 );
    }


    BOOST_TEST_MESSAGE( "--- Testing failure pushing 6th custom json op tx" );

    op.json = "toomany";
    tx.clear();
    tx.operations.push_back( op );

    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), plugin_exception );


    BOOST_TEST_MESSAGE( "--- Testing 5 custom json ops in one transaction" );
    tx.clear();
    op.json = "foobar";
    op.required_posting_auths.clear();
    op.required_posting_auths.insert( "bob" );

    for( int i = 0; i < 5; i++ )
    {
      tx.operations.push_back( op );
    }

    push_transaction( tx, bob_private_key, 0 );


    BOOST_TEST_MESSAGE( "--- Testing failure pushing 6th custom json op tx" );

    op.json = "toomany";
    tx.clear();
    tx.operations.push_back( op );

    HIVE_REQUIRE_THROW( push_transaction( tx, bob_private_key, 0 ), plugin_exception );


    BOOST_TEST_MESSAGE( "--- Testing failure of 6 custom json ops in one transaction" );
    tx.clear();
    op.required_posting_auths.clear();
    op.required_posting_auths.insert( "sam" );

    for( int i = 0; i < 6; i++ )
    {
      tx.operations.push_back( op );
    }

    HIVE_REQUIRE_THROW( push_transaction( tx, sam_private_key, 0 ), plugin_exception );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( custom_binary_authorities )
{
  ACTORS( (alice) )

  custom_binary_operation op;
  op.required_owner_auths.insert( "alice" );
  op.required_active_auths.insert( "bob" );
  op.required_posting_auths.insert( "sam" );
  op.required_auths.push_back( db->get< account_authority_object, by_account >( "alice" ).posting );

  flat_set< account_name_type > acc_auths;
  flat_set< account_name_type > acc_expected;
  vector< authority > auths;
  vector< authority > expected;

  acc_expected.insert( "alice" );
  op.get_required_owner_authorities( acc_auths );
  BOOST_REQUIRE( acc_auths == acc_expected );

  acc_auths.clear();
  acc_expected.clear();
  acc_expected.insert( "bob" );
  op.get_required_active_authorities( acc_auths );
  BOOST_REQUIRE( acc_auths == acc_expected );

  acc_auths.clear();
  acc_expected.clear();
  acc_expected.insert( "sam" );
  op.get_required_posting_authorities( acc_auths );
  BOOST_REQUIRE( acc_auths == acc_expected );

  expected.push_back( db->get< account_authority_object, by_account >( "alice" ).posting );
  op.get_required_authorities( auths );
  BOOST_REQUIRE( auths == expected );
}

BOOST_AUTO_TEST_CASE( feed_publish_validate )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: feed_publish_validate" );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( feed_publish_authorities )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: feed_publish_authorities" );

    ACTORS( (alice)(bob) )
    fund( "alice", 10000 );
    witness_create( "alice", alice_private_key, "foo.bar", alice_private_key.get_public_key(), 1000 );

    feed_publish_operation op;
    op.publisher = "alice";
    op.exchange_rate = price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) );

    signed_transaction tx;
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );

    BOOST_TEST_MESSAGE( "--- Test failure when no signature." );
    HIVE_REQUIRE_THROW( push_transaction( tx, database::skip_transaction_dupe_check ), tx_missing_active_auth );

    BOOST_TEST_MESSAGE( "--- Test failure with incorrect signature" );
    tx.signatures.clear();
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_post_key, database::skip_transaction_dupe_check ), tx_missing_active_auth );

    BOOST_TEST_MESSAGE( "--- Test failure with duplicate signature" );
    sign( tx, alice_private_key );
    sign( tx, alice_private_key );
    HIVE_REQUIRE_THROW( push_transaction( tx, database::skip_transaction_dupe_check ), tx_duplicate_sig );

    BOOST_TEST_MESSAGE( "--- Test failure with additional incorrect signature" );
    tx.signatures.clear();
    sign( tx, alice_private_key );
    sign( tx, bob_private_key );
    HIVE_REQUIRE_THROW( push_transaction( tx, database::skip_transaction_dupe_check ), tx_irrelevant_sig );

    BOOST_TEST_MESSAGE( "--- Test success with witness account signature" );
    tx.signatures.clear();
    push_transaction( tx, alice_private_key, database::skip_transaction_dupe_check );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( feed_publish_apply )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: feed_publish_apply" );

    ACTORS( (alice) )
    fund( "alice", 10000 );
    witness_create( "alice", alice_private_key, "foo.bar", alice_private_key.get_public_key(), 1000 );

    BOOST_TEST_MESSAGE( "--- Test publishing price feed" );
    feed_publish_operation op;
    op.publisher = "alice";
    op.exchange_rate = price( ASSET( "1.000 TBD" ), ASSET( "1000.000 TESTS" ) ); // 1000 HIVE : 1 HBD

    signed_transaction tx;
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.operations.push_back( op );

    push_transaction( tx, alice_private_key, 0 );

    {
      auto& alice_witness = db->get_witness( "alice" );

      BOOST_REQUIRE( alice_witness.get_hbd_exchange_rate() == op.exchange_rate );
      BOOST_REQUIRE( alice_witness.get_last_hbd_exchange_update() == db->head_block_time() );
      validate_database();
    }

    BOOST_TEST_MESSAGE( "--- Test failure publishing to non-existent witness" );

    tx.operations.clear();
    tx.signatures.clear();
    op.publisher = "bob";

    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test failure publishing with HBD base symbol" );

    tx.operations.clear();
    tx.signatures.clear();
    op.exchange_rate = price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) );

    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::assert_exception );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test updating price feed" );

    tx.operations.clear();
    tx.signatures.clear();
    op.exchange_rate = price( ASSET(" 1.000 TBD" ), ASSET( "1500.000 TESTS" ) );
    op.publisher = "alice";
    tx.operations.push_back( op );

    push_transaction( tx, alice_private_key, 0 );

    auto& alice_witness = db->get_witness( "alice" );
    // BOOST_REQUIRE( std::abs( alice_witness.get_hbd_exchange_rate().to_real() - op.exchange_rate.to_real() ) < 0.0000005 );
    BOOST_REQUIRE( alice_witness.get_last_hbd_exchange_update() == db->head_block_time() );
    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( convert_validate )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: convert_validate" );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( convert_authorities )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: convert_authorities" );

    ACTORS( (alice)(bob) )
    fund( "alice", 10000 );

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );

    convert( "alice", ASSET( "2.500 TESTS" ) );

    validate_database();

    convert_operation op;
    op.owner = "alice";
    op.amount = ASSET( "2.500 TBD" );

    signed_transaction tx;
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.operations.push_back( op );

    BOOST_TEST_MESSAGE( "--- Test failure when no signatures" );
    HIVE_REQUIRE_THROW( push_transaction( tx, 0 ), tx_missing_active_auth );

    BOOST_TEST_MESSAGE( "--- Test failure when signed by a signature not in the account's authority" );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_post_key, 0 ), tx_missing_active_auth );

    BOOST_TEST_MESSAGE( "--- Test failure when duplicate signatures" );
    tx.signatures.clear();
    sign( tx, alice_private_key );
    sign( tx, alice_private_key );
    HIVE_REQUIRE_THROW( push_transaction( tx, 0 ), tx_duplicate_sig );

    BOOST_TEST_MESSAGE( "--- Test failure when signed by an additional signature not in the creator's authority" );
    tx.signatures.clear();
    sign( tx, alice_private_key );
    sign( tx, bob_private_key );
    HIVE_REQUIRE_THROW( push_transaction( tx, 0 ), tx_irrelevant_sig );

    BOOST_TEST_MESSAGE( "--- Test success with owner signature" );
    tx.signatures.clear();
    push_transaction( tx, alice_private_key, 0 );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( convert_apply )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: convert_apply" );
    ACTORS( (alice)(bob) );
    fund( "alice", 10000 );
    fund( "bob" , 10000 );

    convert_operation op;
    signed_transaction tx;
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );

    const auto& convert_request_idx = db->get_index< convert_request_index, by_owner >();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );

    convert( "alice", ASSET( "2.500 TESTS" ) );
    convert( "bob", ASSET( "7.000 TESTS" ) );

    const auto& new_alice = db->get_account( "alice" );
    const auto& new_bob = db->get_account( "bob" );

    BOOST_TEST_MESSAGE( "--- Test failure when account does not have the required TESTS" );
    op.owner = "bob";
    op.amount = ASSET( "5.000 TESTS" );
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, bob_private_key, 0 ), fc::exception );

    BOOST_REQUIRE( new_bob.get_balance().amount.value == ASSET( "3.000 TESTS" ).amount.value );
    BOOST_REQUIRE( new_bob.get_hbd_balance().amount.value == ASSET( "7.000 TBD" ).amount.value );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test failure when account does not have the required TBD" );
    op.owner = "alice";
    op.amount = ASSET( "5.000 TBD" );
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );

    BOOST_REQUIRE( new_alice.get_balance().amount.value == ASSET( "7.500 TESTS" ).amount.value );
    BOOST_REQUIRE( new_alice.get_hbd_balance().amount.value == ASSET( "2.500 TBD" ).amount.value );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test failure when account does not exist" );
    op.owner = "sam";
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );

    BOOST_TEST_MESSAGE( "--- Test success converting HBD to TESTS" );
    op.owner = "bob";
    op.amount = ASSET( "3.000 TBD" );
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    push_transaction( tx, bob_private_key, 0 );

    BOOST_REQUIRE( new_bob.get_balance().amount.value == ASSET( "3.000 TESTS" ).amount.value );
    BOOST_REQUIRE( new_bob.get_hbd_balance().amount.value == ASSET( "4.000 TBD" ).amount.value );

    auto convert_request = convert_request_idx.find( boost::make_tuple( get_account_id( op.owner ), op.requestid ) );
    BOOST_REQUIRE( convert_request != convert_request_idx.end() );
    BOOST_REQUIRE( convert_request->get_owner() == get_account_id( op.owner ) );
    BOOST_REQUIRE( convert_request->get_request_id() == op.requestid );
    BOOST_REQUIRE( convert_request->get_convert_amount().amount.value == op.amount.amount.value );
    //BOOST_REQUIRE( convert_request->premium == 100000 );
    BOOST_REQUIRE( convert_request->get_conversion_date() == db->head_block_time() + HIVE_CONVERSION_DELAY );

    BOOST_TEST_MESSAGE( "--- Test failure from repeated id" );
    op.amount = ASSET( "2.000 TESTS" );
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );

    BOOST_REQUIRE( new_bob.get_balance().amount.value == ASSET( "3.000 TESTS" ).amount.value );
    BOOST_REQUIRE( new_bob.get_hbd_balance().amount.value == ASSET( "4.000 TBD" ).amount.value );

    convert_request = convert_request_idx.find( boost::make_tuple( get_account_id( op.owner ), op.requestid ) );
    BOOST_REQUIRE( convert_request != convert_request_idx.end() );
    BOOST_REQUIRE( convert_request->get_owner() == get_account_id( op.owner ) );
    BOOST_REQUIRE( convert_request->get_request_id() == op.requestid );
    BOOST_REQUIRE( convert_request->get_convert_amount().amount.value == ASSET( "3.000 TBD" ).amount.value );
    //BOOST_REQUIRE( convert_request->premium == 100000 );
    BOOST_REQUIRE( convert_request->get_conversion_date() == db->head_block_time() + HIVE_CONVERSION_DELAY );
    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( fixture_convert_checks_balance )
{
  // This actually tests the convert() method of the database fixture can't result in negative
  //   balances, see issue #1825
  try
  {
    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    ACTORS( (dany) )

    fund( "dany", 5000 );
    HIVE_REQUIRE_THROW( convert( "dany", ASSET( "5000.000 TESTS" ) ), fc::exception );
  }
  FC_LOG_AND_RETHROW()

}

BOOST_AUTO_TEST_CASE( collateralized_convert_authorities )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: collateralized_convert_authorities" );

    ACTORS( (alice)(bob) )

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "4.000 TESTS" ) ) );

    fund( "alice", ASSET( "300.000 TBD" ) );
    fund( "alice", ASSET( "1000.000 TESTS" ) );

    collateralized_convert_operation op;
    op.owner = "alice";
    op.amount = ASSET( "200.000 TESTS" );

    signed_transaction tx;
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.operations.push_back( op );

    BOOST_TEST_MESSAGE( "--- Test failure when no signatures" );
    HIVE_REQUIRE_THROW( push_transaction( tx, 0 ), tx_missing_active_auth );

    BOOST_TEST_MESSAGE( "--- Test failure when signed by a signature not in the account's authority" );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_post_key, 0 ), tx_missing_active_auth );

    BOOST_TEST_MESSAGE( "--- Test failure when duplicate signatures" );
    tx.signatures.clear();
    sign( tx, alice_private_key );
    sign( tx, alice_private_key );
    HIVE_REQUIRE_THROW( push_transaction( tx, 0 ), tx_duplicate_sig );

    BOOST_TEST_MESSAGE( "--- Test failure when signed by an additional signature not in the creator's authority" );
    tx.signatures.clear();
    sign( tx, alice_private_key );
    sign( tx, bob_private_key );
    HIVE_REQUIRE_THROW( push_transaction( tx, 0 ), tx_irrelevant_sig );

    BOOST_TEST_MESSAGE( "--- Test success with owner signature" );
    tx.signatures.clear();
    push_transaction( tx, alice_private_key, 0 );

    validate_database();
    generate_block();

    //remove price feed to check one unlikely scenario
    db_plugin->debug_update( [=]( database& db )
    {
      db.modify( db.get_feed_history(), [&]( feed_history_object& feed )
      {
        feed.current_median_history = feed.market_median_history =
          feed.current_min_history = feed.current_max_history = price();
      } );
    } );

    BOOST_TEST_MESSAGE( "--- Test failure without price feed" );
    tx.signatures.clear();
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    HIVE_REQUIRE_ASSERT( push_transaction( tx, alice_private_key, 0 ), "!fhistory.current_median_history.is_null()" );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( collateralized_convert_apply )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: collateralized_convert_apply" );
    ACTORS( (alice)(bob) );

    generate_block();

    const auto& dgpo = db->get_dynamic_global_properties();
    const auto& feed = db->get_feed_history();
    db->skip_price_feed_limit_check = false;
    
    price price_1_for_4 = price( ASSET( "1.000 TBD" ), ASSET( "4.000 TESTS" ) );
    set_price_feed( price_1_for_4 );
    BOOST_REQUIRE( feed.current_median_history == price_1_for_4 );

    //prevent HBD interest from interfering with the test
    flat_map< string, vector<char> > props;
    props[ "hbd_interest_rate" ] = fc::raw::pack_to_vector( 0 );
    set_witness_props( props );

    fund( "alice", ASSET( "100.000 TBD" ) );
    fund( "alice", ASSET( "1000.000 TESTS" ) );
    fund( "bob", ASSET( "100.000 TBD" ) );

    BOOST_TEST_MESSAGE( "--- Test failure sending non-HIVE collateral" );
    collateralized_convert_operation op;
    op.owner = "alice";
    op.amount = ASSET( "5.000 TBD" );
    HIVE_REQUIRE_ASSERT( push_transaction( op, alice_private_key ), "is_asset_type( amount, HIVE_SYMBOL )" );
    transfer( "alice", db->get_treasury_name(), get_hbd_balance( "alice" ) );

    BOOST_TEST_MESSAGE( "--- Test failure sending negative collateral" );
    op.amount = ASSET( "-5.000 TESTS" );
    HIVE_REQUIRE_ASSERT( push_transaction( op, alice_private_key ), "amount.amount > 0" );

    BOOST_TEST_MESSAGE( "--- Test failure sending zero collateral" );
    op.amount = ASSET( "0.000 TESTS" );
    HIVE_REQUIRE_ASSERT( push_transaction( op, alice_private_key ), "amount.amount > 0" );

    BOOST_TEST_MESSAGE( "--- Test failure sending too small collateral" );
    op.amount = ASSET( "0.009 TESTS" ); //0.004 TESTS for immediate conversion which would give 0.001 TBD if there was no extra 5% fee
    HIVE_REQUIRE_ASSERT( push_transaction( op, alice_private_key ), "converted_amount.amount > 0" );

    BOOST_TEST_MESSAGE( "--- Test failure sending collateral exceeding balance" );
    op.amount = ASSET( "1000.001 TESTS" );
    HIVE_REQUIRE_ASSERT( push_transaction( op, alice_private_key ), "available >= -delta" );

    //give alice enough for further tests
    transfer( HIVE_INIT_MINER_NAME, "alice", ASSET( "9000.000 TESTS" ) );
    auto alice_balance = get_balance( "alice" );
    BOOST_REQUIRE( alice_balance == ASSET( "10000.000 TESTS" ) );

    BOOST_TEST_MESSAGE( "--- Test ok - conversion at 25 cents per HIVE, both initial and actual" );
    op.amount = ASSET( "1000.000 TESTS" );
    auto conversion_time = db->head_block_time();
    push_transaction( op, alice_private_key );
    {
      auto recent_ops = get_last_operations( 1 );
      auto convert_op = recent_ops.back().get< collateralized_convert_immediate_conversion_operation >();
      BOOST_REQUIRE( convert_op.owner == "alice" );
      BOOST_REQUIRE( convert_op.requestid == 0 );
      BOOST_REQUIRE( convert_op.hbd_out == ASSET( "119.047 TBD" ) );
    }

    alice_balance -= ASSET( "1000.000 TESTS" );
    BOOST_REQUIRE( get_balance( "alice" ) == alice_balance );
    BOOST_REQUIRE( get_hbd_balance( "alice" ) == ASSET( "119.047 TBD" ) ); // 1000/2 collateral * 10/42 price with fee
    transfer( "alice", db->get_treasury_name(), get_hbd_balance( "alice" ) );

    generate_block();

    BOOST_TEST_MESSAGE( "--- Test failure - conversion with duplicate id" );
    op.amount = ASSET( "1000.000 TESTS" );
    HIVE_REQUIRE_CHAINBASE_ASSERT( push_transaction( op, alice_private_key ), "could not insert object, most likely a uniqueness constraint was violated inside index holding types: hive::chain::collateralized_convert_request_object" );

    BOOST_REQUIRE( get_balance( "alice" ) == alice_balance );
    BOOST_REQUIRE( get_hbd_balance( "alice" ) == ASSET( "0.000 TBD" ) );

    generate_blocks( conversion_time + HIVE_COLLATERALIZED_CONVERSION_DELAY - fc::seconds( HIVE_BLOCK_INTERVAL ) );

    BOOST_REQUIRE( get_balance( "alice" ) == alice_balance );
    generate_block(); //actual conversion
    alice_balance += ASSET( "500.003 TESTS" ); //getting back excess collateral (1000 - 119.047 * 42/10)
    BOOST_REQUIRE( get_balance( "alice" ) == alice_balance );
    BOOST_REQUIRE( get_hbd_balance( "alice" ) == ASSET( "0.000 TBD" ) ); //actual conversion does not give any more HBD
    {
      auto recent_ops = get_last_operations( 1 );
      auto convert_op = recent_ops.back().get< fill_collateralized_convert_request_operation >();
      BOOST_REQUIRE( convert_op.owner == "alice" );
      BOOST_REQUIRE( convert_op.requestid == 0 );
      BOOST_REQUIRE( convert_op.amount_in == ASSET( "499.997 TESTS" ) );
      BOOST_REQUIRE( convert_op.amount_out == ASSET( "119.047 TBD" ) );
      BOOST_REQUIRE( convert_op.excess_collateral == ASSET( "500.003 TESTS" ) );
    }

    BOOST_TEST_MESSAGE( "--- Test ok - conversion at 25 cents initial, 12.5 cents per HIVE actual" );
    op.amount = ASSET( "1000.000 TESTS" );
    auto conversion_2_time = db->head_block_time();
    push_transaction( op, alice_private_key );
    {
      auto recent_ops = get_last_operations( 1 );
      auto convert_op = recent_ops.back().get< collateralized_convert_immediate_conversion_operation >();
      BOOST_REQUIRE( convert_op.owner == "alice" );
      BOOST_REQUIRE( convert_op.requestid == 0 );
      BOOST_REQUIRE( convert_op.hbd_out == ASSET( "119.047 TBD" ) );
    }

    alice_balance -= ASSET( "1000.000 TESTS" );
    BOOST_REQUIRE( get_balance( "alice" ) == alice_balance );
    BOOST_REQUIRE( get_hbd_balance( "alice" ) == ASSET( "119.047 TBD" ) ); // 1000/2 collateral * 10/42 price with fee
    transfer( "alice", db->get_treasury_name(), get_hbd_balance( "alice" ) );

    price price_1_for_8 = price( ASSET( "1.000 TBD" ), ASSET( "8.000 TESTS" ) );
    set_price_feed( price_1_for_8 );
    set_price_feed( price_1_for_8 ); //need to do it twice or median won't be the one required
    BOOST_REQUIRE( feed.current_median_history == price_1_for_8 );

    generate_blocks( conversion_2_time + HIVE_COLLATERALIZED_CONVERSION_DELAY - fc::seconds( HIVE_BLOCK_INTERVAL ) );

    BOOST_REQUIRE( get_balance( "alice" ) == alice_balance );
    generate_block(); //actual conversion
    alice_balance += ASSET( "0.006 TESTS" ); //almost no excess collateral (1000 - 119.047 * 84/10)
    BOOST_REQUIRE( get_balance( "alice" ) == alice_balance );
    BOOST_REQUIRE( get_hbd_balance( "alice" ) == ASSET( "0.000 TBD" ) );
    {
      auto recent_ops = get_last_operations( 1 );
      auto convert_op = recent_ops.back().get< fill_collateralized_convert_request_operation >();
      BOOST_REQUIRE( convert_op.owner == "alice" );
      BOOST_REQUIRE( convert_op.requestid == 0 );
      BOOST_REQUIRE( convert_op.amount_in == ASSET( "999.994 TESTS" ) );
      BOOST_REQUIRE( convert_op.amount_out == ASSET( "119.047 TBD" ) );
      BOOST_REQUIRE( convert_op.excess_collateral == ASSET( "0.006 TESTS" ) );
    }

    BOOST_TEST_MESSAGE( "--- Test ok - conversion at 12.5 cents initial, 5 cents per HIVE actual" );
    op.amount = ASSET( "1000.000 TESTS" );
    auto conversion_3_time = db->head_block_time();
    push_transaction( op, alice_private_key );
    {
      auto recent_ops = get_last_operations( 1 );
      auto convert_op = recent_ops.back().get< collateralized_convert_immediate_conversion_operation >();
      BOOST_REQUIRE( convert_op.owner == "alice" );
      BOOST_REQUIRE( convert_op.requestid == 0 );
      BOOST_REQUIRE( convert_op.hbd_out == ASSET( "59.523 TBD" ) );
    }

    alice_balance -= ASSET( "1000.000 TESTS" );
    BOOST_REQUIRE( get_balance( "alice" ) == alice_balance );
    BOOST_REQUIRE( get_hbd_balance( "alice" ) == ASSET( "59.523 TBD" ) ); // 1000/2 collateral * 10/84 price with fee
    //transfer( "alice", db->get_treasury_name(), get_hbd_balance( "alice" ) ); leave it this time on account

    price price_1_for_20 = price( ASSET( "1.000 TBD" ), ASSET( "20.000 TESTS" ) );
    set_price_feed( price_1_for_20 );
    set_price_feed( price_1_for_20 );
    set_price_feed( price_1_for_20 );
    set_price_feed( price_1_for_20 ); //four times required to override three previous records as median
    BOOST_REQUIRE( feed.current_median_history == price_1_for_20 );

    generate_blocks( conversion_3_time + HIVE_COLLATERALIZED_CONVERSION_DELAY - fc::seconds( HIVE_BLOCK_INTERVAL ) );

    BOOST_REQUIRE( get_balance( "alice" ) == alice_balance );
    generate_block(); //actual conversion
    BOOST_REQUIRE( get_balance( "alice" ) == alice_balance ); //no excess collateral
    BOOST_REQUIRE( get_hbd_balance( "alice" ) == ASSET( "59.523 TBD" ) ); //even though there was too little collateral we still don't try to take back produced HBD
    {
      auto recent_ops = get_last_operations( 2 );
      auto convert_op = recent_ops.front().get< fill_collateralized_convert_request_operation >();
      BOOST_REQUIRE( convert_op.owner == "alice" );
      BOOST_REQUIRE( convert_op.requestid == 0 );
      BOOST_REQUIRE( convert_op.amount_in == ASSET( "1000.000 TESTS" ) );
      BOOST_REQUIRE( convert_op.amount_out == ASSET( "59.523 TBD" ) );
      BOOST_REQUIRE( convert_op.excess_collateral == ASSET( "0.000 TESTS" ) );
      auto sys_warn_op = recent_ops.back().get< system_warning_operation >();
      BOOST_REQUIRE( sys_warn_op.message.compare( 0, 23, "Insufficient collateral" ) == 0 );
    }
    transfer( "alice", db->get_treasury_name(), get_hbd_balance( "alice" ) );

    BOOST_TEST_MESSAGE( "--- Setting amount of HBD in the system to the edge of upper soft limit" );
    {
      fc::uint128_t amount( dgpo.get_current_supply().amount.value );
      uint16_t limit2 = 2 * dgpo.hbd_stop_percent + HIVE_1_BASIS_POINT; //there is rounding when percent is calculated, hence some strange correction
      amount = ( amount * limit2 ) / ( 2 * HIVE_100_PERCENT - limit2 );
      auto new_hbd = asset( amount.to_uint64(), HIVE_SYMBOL ) * feed.current_median_history;
      new_hbd -= dgpo.get_current_hbd_supply() - db->get_treasury().get_hbd_balance();
      fund( "alice", new_hbd, false );
      uint16_t percent = db->calculate_HBD_percent();
      BOOST_REQUIRE_EQUAL( percent, dgpo.hbd_stop_percent );
    }

    BOOST_TEST_MESSAGE( "--- Test failure on too many HBD in the system" );
    op.amount = ASSET( "0.042 TESTS" ); //minimal amount of collateral that gives nonzero HBD at current price
    HIVE_REQUIRE_ASSERT( push_transaction( op, alice_private_key ), "percent_hbd <= dgpo.hbd_stop_percent" );

    const auto& collateralized_convert_request_idx = db->get_index< collateralized_convert_request_index, by_owner >();
    BOOST_REQUIRE( collateralized_convert_request_idx.empty() );

    //let's make some room for conversion (treasury HBD does not count)
    transfer( "alice", db->get_treasury_name(), ASSET( "25.000 TBD" ) );

    BOOST_TEST_MESSAGE( "--- Test ok - conversion at 5 cents initial, 5 cents per HIVE actual" );
    op.amount = ASSET( "1000.000 TESTS" );
    auto conversion_4_time = db->head_block_time();
    auto alice_hbd_balance = get_hbd_balance( "alice" );
    push_transaction( op, alice_private_key );

    alice_balance -= ASSET( "1000.000 TESTS" );
    BOOST_REQUIRE( get_balance( "alice" ) == alice_balance );
    alice_hbd_balance += ASSET( "23.809 TBD" ); // 1000/2 collateral * 1/21 price with fee
    BOOST_REQUIRE( get_hbd_balance( "alice" ) == alice_hbd_balance );

    BOOST_TEST_MESSAGE( "--- Test ok - regular conversion" );
    {
      //let's schedule conversion from HBD to hive
      convert_operation op;
      op.owner = "bob";
      op.amount = ASSET( "20.000 TBD" );
      push_transaction( op, bob_private_key );
    }
    generate_block();
    BOOST_REQUIRE_EQUAL( dgpo.hbd_print_rate, 0 );

    BOOST_TEST_MESSAGE( "--- Test failed - conversion initiated while at or above upper soft limit" );
    op.amount = ASSET( "1000.000 TESTS" );
    HIVE_REQUIRE_ASSERT( push_transaction( op, alice_private_key ), "dgpo.hbd_print_rate > 0" );

    //since we are already on the edge of HBD upper soft limit which means we can't convert more anyway,
    //let's use the opportunity and test what happens when we try to cross hard limit
    {
      auto extra_hbd = dgpo.get_current_hbd_supply();
      int16_t diff = HIVE_HBD_HARD_LIMIT - dgpo.hbd_stop_percent;
      if( diff < 0 )
        diff = 0; //just in case we'd have incorrect configuration with hard limit below upper soft limit
      extra_hbd.amount *= diff;
      extra_hbd.amount /= dgpo.hbd_stop_percent; //HBD supply multiplied by the same factor as a difference between hard and upper soft limit
      extra_hbd += dgpo.get_current_hbd_supply(); //to always have some value above hard limit
      fund( "bob", extra_hbd );

      generate_blocks( HIVE_FEED_INTERVAL_BLOCKS - ( dgpo.head_block_number % HIVE_FEED_INTERVAL_BLOCKS ) );
      //last feed update should've put up artificial price of HIVE
      auto recent_ops = get_last_operations( 2 );
      auto sys_warn_op = recent_ops.back().get< system_warning_operation >();
      BOOST_REQUIRE( sys_warn_op.message.compare( 0, 27, "HIVE price corrected upward" ) == 0 );
    }

    BOOST_REQUIRE( feed.current_median_history > price_1_for_20 );
    BOOST_REQUIRE( feed.current_max_history == price_1_for_4 ); //it is hard to force artificial correction of max price when it is so high to begin with
    BOOST_REQUIRE( feed.market_median_history == price_1_for_20 ); //market driven median price should be intact
    BOOST_REQUIRE( feed.current_min_history == price_1_for_20 ); //minimal price should be intact

    generate_blocks( conversion_4_time + HIVE_COLLATERALIZED_CONVERSION_DELAY - fc::seconds( HIVE_BLOCK_INTERVAL ) );

    BOOST_REQUIRE( get_balance( "alice" ) == alice_balance );
    BOOST_REQUIRE( get_balance( "bob" ) == ASSET( "0.000 TESTS" ) );
    generate_block(); //actual conversion
    alice_balance += ASSET( "500.011 TESTS" ); //excess collateral (1000 - 23.809 * 21/1) - alice used fee corrected market_median_price...
    BOOST_REQUIRE( get_balance( "alice" ) == alice_balance );
    BOOST_REQUIRE( get_balance( "bob" ) == ASSET( "273.625 TESTS" ) ); //...but bob used artificial current_median_history
      //(for HF25 values of 9%/10%/10% lower/upper/hard cap expected value is 199.102)
      //(for 9%/10%/30% expected value is 383.222)
      //with different limits value will change, but should be less than 400.000 TESTS

    //put HBD on treasury where it does not count, but try to make some conversion with artificial price before it is changed back down
    transfer( "alice", db->get_treasury_name(), get_hbd_balance( "alice" ) );
    transfer( "bob", db->get_treasury_name(), get_hbd_balance( "bob" ) );

    BOOST_TEST_MESSAGE( "--- Test failed - conversion initiated while artificial price is active" );
    op.amount = ASSET( "1000.000 TESTS" );
    HIVE_REQUIRE_ASSERT( push_transaction( op, alice_private_key ), "dgpo.hbd_print_rate > 0" );

    generate_blocks( HIVE_FEED_INTERVAL_BLOCKS - ( dgpo.head_block_number % HIVE_FEED_INTERVAL_BLOCKS ) );

    //since HBD on treasury does not count the price should now be back to normal price
    BOOST_REQUIRE( feed.current_median_history == price_1_for_20 );
    BOOST_REQUIRE( feed.market_median_history == price_1_for_20 );
    BOOST_REQUIRE( feed.current_min_history == price_1_for_20 );
    BOOST_REQUIRE( feed.current_max_history == price_1_for_4 );

    BOOST_TEST_MESSAGE( "--- Test ok - conversion at 5 cents initial, 50 cents per HIVE actual" );
    op.amount = ASSET( "1000.000 TESTS" );
    auto conversion_5_time = db->head_block_time();
    push_transaction( op, alice_private_key );

    alice_balance -= ASSET( "1000.000 TESTS" );
    BOOST_REQUIRE( get_balance( "alice" ) == alice_balance );
    BOOST_REQUIRE( get_hbd_balance( "alice" ) == ASSET( "23.809 TBD" ) ); // 1000/2 collateral * 1/21 price with fee

    price price_1_for_2 = price( ASSET( "1.000 TBD" ), ASSET( "2.000 TESTS" ) );
    set_price_feed( price_1_for_2 );
    set_price_feed( price_1_for_2 );
    set_price_feed( price_1_for_2 );
    set_price_feed( price_1_for_2 );
    set_price_feed( price_1_for_2 );
    set_price_feed( price_1_for_2 );
    set_price_feed( price_1_for_2 );
    set_price_feed( price_1_for_2 );
    set_price_feed( price_1_for_2 );
    BOOST_REQUIRE( feed.current_median_history == price_1_for_2 );

    generate_blocks( conversion_5_time + HIVE_COLLATERALIZED_CONVERSION_DELAY - fc::seconds( HIVE_BLOCK_INTERVAL ) );

    BOOST_REQUIRE( get_balance( "alice" ) == alice_balance );
    generate_block(); //actual conversion
    alice_balance += ASSET( "950.002 TESTS" ); //a lot of excess collateral (1000 - 23.809 * 21/10)
    BOOST_REQUIRE( get_balance( "alice" ) == alice_balance );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( collateralized_convert_narrow_price )
{
  //test covers bug where fee was not applied, because price scaling code was truncating it
  try
  {
    BOOST_TEST_MESSAGE( "Testing: collateralized_convert_narrow_price" );
    ACTORS( (alice) );

    generate_block();

    const auto& feed = db->get_feed_history();
    db->skip_price_feed_limit_check = false;

    price price_1_for_2 = price( ASSET( "0.001 TBD" ), ASSET( "0.002 TESTS" ) );
    set_price_feed( price_1_for_2 );
    BOOST_REQUIRE( feed.current_median_history == price_1_for_2 );

    //prevent HBD interest from interfering with the test
    flat_map< string, vector<char> > props;
    props[ "hbd_interest_rate" ] = fc::raw::pack_to_vector( 0 );
    set_witness_props( props );

    fund( "alice", ASSET( "0.042 TESTS" ) );

    BOOST_TEST_MESSAGE( "--- Test ok - conversion at 50 cents per HIVE, both initial and actual" );
    collateralized_convert_operation op;
    op.owner = "alice";
    op.amount = ASSET( "0.042 TESTS" );
    auto conversion_time = db->head_block_time();
    push_transaction( op, alice_private_key );

    BOOST_REQUIRE( get_balance( "alice" ) == ASSET( "0.000 TESTS" ) );
    BOOST_REQUIRE( get_hbd_balance( "alice" ) == ASSET( "0.010 TBD" ) ); // 0.042/2 collateral * 10/21 price with fee

    generate_blocks( conversion_time + HIVE_COLLATERALIZED_CONVERSION_DELAY - fc::seconds( HIVE_BLOCK_INTERVAL ) );

    BOOST_REQUIRE( get_balance( "alice" ) == ASSET( "0.000 TESTS" ) );
    BOOST_REQUIRE( get_hbd_balance( "alice" ) == ASSET( "0.010 TBD" ) );
    generate_block(); //actual conversion
    BOOST_REQUIRE( get_balance( "alice" ) == ASSET( "0.021 TESTS" ) );
    BOOST_REQUIRE( get_hbd_balance( "alice" ) == ASSET( "0.010 TBD" ) );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( collateralized_convert_wide_price )
{
  //test covers potential bug where application of fee to price would exceed 64bit
  try
  {
    BOOST_TEST_MESSAGE( "Testing: collateralized_convert_wide_price" );
    ACTORS( ( alice ) );

    generate_block();

    const auto& feed = db->get_feed_history();
    db->skip_price_feed_limit_check = false;

    price price_1_for_2 = price( ASSET( "4611686018427387.000 TBD" ), ASSET( "9223372036854774.000 TESTS" ) );
    set_price_feed( price_1_for_2 );
    BOOST_REQUIRE( feed.current_median_history == price_1_for_2 );

    //prevent HBD interest from interfering with the test
    flat_map< string, vector<char> > props;
    props[ "hbd_interest_rate" ] = fc::raw::pack_to_vector( 0 );
    set_witness_props( props );

    fund( "alice", ASSET( "50000000.000 TESTS" ) );

    BOOST_TEST_MESSAGE( "--- Test failure on too many HBD in the system" );
    collateralized_convert_operation op;
    op.owner = "alice";
    op.amount = ASSET( "42000000.000 TESTS" );
    HIVE_REQUIRE_ASSERT( push_transaction( op, alice_private_key ), "percent_hbd <= dgpo.hbd_stop_percent" );

    BOOST_TEST_MESSAGE( "--- Test ok - conversion at 50 cents per HIVE, both initial and actual" );
    op.amount = ASSET( "4200000.000 TESTS" );
    auto conversion_time = db->head_block_time();
    auto alice_balance = get_balance( "alice" );
    push_transaction( op, alice_private_key );
    alice_balance -= ASSET( "4200000.000 TESTS" );

    BOOST_REQUIRE( get_balance( "alice" ) == alice_balance );
    BOOST_REQUIRE( get_hbd_balance( "alice" ) == ASSET( "1000000.000 TBD" ) );

    generate_blocks( conversion_time + HIVE_COLLATERALIZED_CONVERSION_DELAY - fc::seconds( HIVE_BLOCK_INTERVAL ) );

    BOOST_REQUIRE( get_balance( "alice" ) == alice_balance );
    BOOST_REQUIRE( get_hbd_balance( "alice" ) == ASSET( "1000000.000 TBD" ) );
    generate_block(); //actual conversion
    alice_balance += ASSET( "2100000.000 TESTS" );
    BOOST_REQUIRE( get_balance( "alice" ) == alice_balance );
    BOOST_REQUIRE( get_hbd_balance( "alice" ) == ASSET( "1000000.000 TBD" ) );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( limit_order_create_validate )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: limit_order_create_validate" );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( limit_order_create_authorities )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: limit_order_create_authorities" );

    ACTORS( (alice)(bob) )
    fund( "alice", 10000 );

    limit_order_create_operation op;
    op.owner = "alice";
    op.amount_to_sell = ASSET( "1.000 TESTS" );
    op.min_to_receive = ASSET( "1.000 TBD" );
    op.expiration = db->head_block_time() + fc::seconds( HIVE_MAX_LIMIT_ORDER_EXPIRATION );

    signed_transaction tx;
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );

    BOOST_TEST_MESSAGE( "--- Test failure when no signature." );
    HIVE_REQUIRE_THROW( push_transaction( tx, database::skip_transaction_dupe_check ), tx_missing_active_auth );

    BOOST_TEST_MESSAGE( "--- Test success with account signature" );
    push_transaction( tx, alice_private_key, database::skip_transaction_dupe_check );

    BOOST_TEST_MESSAGE( "--- Test failure with duplicate signature" );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, database::skip_transaction_dupe_check ), tx_duplicate_sig );

    BOOST_TEST_MESSAGE( "--- Test failure with additional incorrect signature" );
    tx.signatures.clear();
    sign( tx, alice_private_key );
    sign( tx, bob_private_key );
    HIVE_REQUIRE_THROW( push_transaction( tx, database::skip_transaction_dupe_check ), tx_irrelevant_sig );

    BOOST_TEST_MESSAGE( "--- Test failure with incorrect signature" );
    tx.signatures.clear();
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_post_key, database::skip_transaction_dupe_check ), tx_missing_active_auth );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( limit_order_create_apply )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: limit_order_create_apply" );

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );

    ACTORS( (alice)(bob) )
    fund( "alice", 1000000 );
    fund( "bob", 1000000 );
    convert( "bob", ASSET("1000.000 TESTS" ) );

    const auto& limit_order_idx = db->get_index< limit_order_index >().indices().get< by_account >();

    BOOST_TEST_MESSAGE( "--- Test failure when account does not have required funds" );
    limit_order_create_operation op;
    signed_transaction tx;

    op.owner = "bob";
    op.orderid = 1;
    op.amount_to_sell = ASSET( "10.000 TESTS" );
    op.min_to_receive = ASSET( "10.000 TBD" );
    op.fill_or_kill = false;
    op.expiration = db->head_block_time() + fc::seconds( HIVE_MAX_LIMIT_ORDER_EXPIRATION );
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    HIVE_REQUIRE_THROW( push_transaction( tx, bob_private_key, 0 ), fc::exception );

    BOOST_REQUIRE( limit_order_idx.find( boost::make_tuple( "bob", op.orderid ) ) == limit_order_idx.end() );
    BOOST_REQUIRE( bob.get_balance().amount.value == ASSET( "0.000 TESTS" ).amount.value );
    BOOST_REQUIRE( bob.get_hbd_balance().amount.value == ASSET( "1000.000 TBD" ).amount.value );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test failure when amount to receive is 0" );

    op.owner = "alice";
    op.min_to_receive = ASSET( "0.000 TBD" );
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );

    BOOST_REQUIRE( limit_order_idx.find( boost::make_tuple( "alice", op.orderid ) ) == limit_order_idx.end() );
    BOOST_REQUIRE( alice.get_balance().amount.value == ASSET( "1000.000 TESTS" ).amount.value );
    BOOST_REQUIRE( alice.get_hbd_balance().amount.value == ASSET( "0.000 TBD" ).amount.value );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test failure when amount to sell is 0" );

    op.amount_to_sell = ASSET( "0.000 TESTS" );
    op.min_to_receive = ASSET( "10.000 TBD" ) ;
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );

    BOOST_REQUIRE( limit_order_idx.find( boost::make_tuple( "alice", op.orderid ) ) == limit_order_idx.end() );
    BOOST_REQUIRE( alice.get_balance().amount.value == ASSET( "1000.000 TESTS" ).amount.value );
    BOOST_REQUIRE( alice.get_hbd_balance().amount.value == ASSET( "0.000 TBD" ).amount.value );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test failure when expiration is too long" );
    op.amount_to_sell = ASSET( "10.000 TESTS" );
    op.min_to_receive = ASSET( "15.000 TBD" );
    op.expiration = db->head_block_time() + fc::seconds( HIVE_MAX_LIMIT_ORDER_EXPIRATION + 1 );
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );

    BOOST_TEST_MESSAGE( "--- Test success creating limit order that will not be filled" );

    op.expiration = db->head_block_time() + fc::seconds( HIVE_MAX_LIMIT_ORDER_EXPIRATION );
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    push_transaction( tx, alice_private_key, 0 );

    auto limit_order = limit_order_idx.find( boost::make_tuple( "alice", op.orderid ) );
    BOOST_REQUIRE( limit_order != limit_order_idx.end() );
    BOOST_REQUIRE( limit_order->seller == op.owner );
    BOOST_REQUIRE( limit_order->orderid == op.orderid );
    BOOST_REQUIRE( limit_order->for_sale == op.amount_to_sell.amount );
    BOOST_REQUIRE( limit_order->sell_price == price( op.amount_to_sell, op.min_to_receive ) );
    BOOST_REQUIRE( limit_order->get_market() == std::make_pair( HBD_SYMBOL, HIVE_SYMBOL ) );
    BOOST_REQUIRE( alice.get_balance().amount.value == ASSET( "990.000 TESTS" ).amount.value );
    BOOST_REQUIRE( alice.get_hbd_balance().amount.value == ASSET( "0.000 TBD" ).amount.value );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test failure creating limit order with duplicate id" );

    op.amount_to_sell = ASSET( "20.000 TESTS" );
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );

    limit_order = limit_order_idx.find( boost::make_tuple( "alice", op.orderid ) );
    BOOST_REQUIRE( limit_order != limit_order_idx.end() );
    BOOST_REQUIRE( limit_order->seller == op.owner );
    BOOST_REQUIRE( limit_order->orderid == op.orderid );
    BOOST_REQUIRE( limit_order->for_sale == 10000 );
    BOOST_REQUIRE( limit_order->sell_price == price( ASSET( "10.000 TESTS" ), op.min_to_receive ) );
    BOOST_REQUIRE( limit_order->get_market() == std::make_pair( HBD_SYMBOL, HIVE_SYMBOL ) );
    BOOST_REQUIRE( alice.get_balance().amount.value == ASSET( "990.000 TESTS" ).amount.value );
    BOOST_REQUIRE( alice.get_hbd_balance().amount.value == ASSET( "0.000 TBD" ).amount.value );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test sucess killing an order that will not be filled" );

    op.orderid = 2;
    op.fill_or_kill = true;
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );

    BOOST_REQUIRE( limit_order_idx.find( boost::make_tuple( "alice", op.orderid ) ) == limit_order_idx.end() );
    BOOST_REQUIRE( alice.get_balance().amount.value == ASSET( "990.000 TESTS" ).amount.value );
    BOOST_REQUIRE( alice.get_hbd_balance().amount.value == ASSET( "0.000 TBD" ).amount.value );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test having a partial match to limit order" );
    // Alice has order for 15 HBD at a price of 2:3
    // Fill 5 HIVE for 7.5 HBD

    op.owner = "bob";
    op.orderid = 1;
    op.amount_to_sell = ASSET( "7.500 TBD" );
    op.min_to_receive = ASSET( "5.000 TESTS" );
    op.fill_or_kill = false;
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    push_transaction( tx, bob_private_key, 0 );

    auto recent_ops = get_last_operations( 1 );
    auto fill_order_op = recent_ops[0].get< fill_order_operation >();

    limit_order = limit_order_idx.find( boost::make_tuple( "alice", 1 ) );
    BOOST_REQUIRE( limit_order != limit_order_idx.end() );
    BOOST_REQUIRE( limit_order->seller == "alice" );
    BOOST_REQUIRE( limit_order->orderid == op.orderid );
    BOOST_REQUIRE( limit_order->for_sale == 5000 );
    BOOST_REQUIRE( limit_order->sell_price == price( ASSET( "10.000 TESTS" ), ASSET( "15.000 TBD" ) ) );
    BOOST_REQUIRE( limit_order->get_market() == std::make_pair( HBD_SYMBOL, HIVE_SYMBOL ) );
    BOOST_REQUIRE( limit_order_idx.find( boost::make_tuple( "bob", op.orderid ) ) == limit_order_idx.end() );
    BOOST_REQUIRE( alice.get_balance().amount.value == ASSET( "990.000 TESTS" ).amount.value );
    BOOST_REQUIRE( alice.get_hbd_balance().amount.value == ASSET( "7.500 TBD" ).amount.value );
    BOOST_REQUIRE( bob.get_balance().amount.value == ASSET( "5.000 TESTS" ).amount.value );
    BOOST_REQUIRE( bob.get_hbd_balance().amount.value == ASSET( "992.500 TBD" ).amount.value );
    BOOST_REQUIRE( fill_order_op.open_owner == "alice" );
    BOOST_REQUIRE( fill_order_op.open_orderid == 1 );
    BOOST_REQUIRE( fill_order_op.open_pays.amount.value == ASSET( "5.000 TESTS").amount.value );
    BOOST_REQUIRE( fill_order_op.current_owner == "bob" );
    BOOST_REQUIRE( fill_order_op.current_orderid == 1 );
    BOOST_REQUIRE( fill_order_op.current_pays.amount.value == ASSET( "7.500 TBD" ).amount.value );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test filling an existing order fully, but the new order partially" );

    op.amount_to_sell = ASSET( "15.000 TBD" );
    op.min_to_receive = ASSET( "10.000 TESTS" );
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    push_transaction( tx, bob_private_key, 0 );

    limit_order = limit_order_idx.find( boost::make_tuple( "bob", 1 ) );
    BOOST_REQUIRE( limit_order != limit_order_idx.end() );
    BOOST_REQUIRE( limit_order->seller == "bob" );
    BOOST_REQUIRE( limit_order->orderid == 1 );
    BOOST_REQUIRE( limit_order->for_sale.value == 7500 );
    BOOST_REQUIRE( limit_order->sell_price == price( ASSET( "15.000 TBD" ), ASSET( "10.000 TESTS" ) ) );
    BOOST_REQUIRE( limit_order->get_market() == std::make_pair( HBD_SYMBOL, HIVE_SYMBOL ) );
    BOOST_REQUIRE( limit_order_idx.find( boost::make_tuple( "alice", 1 ) ) == limit_order_idx.end() );
    BOOST_REQUIRE( alice.get_balance().amount.value == ASSET( "990.000 TESTS" ).amount.value );
    BOOST_REQUIRE( alice.get_hbd_balance().amount.value == ASSET( "15.000 TBD" ).amount.value );
    BOOST_REQUIRE( bob.get_balance().amount.value == ASSET( "10.000 TESTS" ).amount.value );
    BOOST_REQUIRE( bob.get_hbd_balance().amount.value == ASSET( "977.500 TBD" ).amount.value );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test filling an existing order and new order fully" );

    op.owner = "alice";
    op.orderid = 3;
    op.amount_to_sell = ASSET( "5.000 TESTS" );
    op.min_to_receive = ASSET( "7.500 TBD" );
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    push_transaction( tx, alice_private_key, 0 );

    BOOST_REQUIRE( limit_order_idx.find( boost::make_tuple( "alice", 3 ) ) == limit_order_idx.end() );
    BOOST_REQUIRE( limit_order_idx.find( boost::make_tuple( "bob", 1 ) ) == limit_order_idx.end() );
    BOOST_REQUIRE( alice.get_balance().amount.value == ASSET( "985.000 TESTS" ).amount.value );
    BOOST_REQUIRE( alice.get_hbd_balance().amount.value == ASSET( "22.500 TBD" ).amount.value );
    BOOST_REQUIRE( bob.get_balance().amount.value == ASSET( "15.000 TESTS" ).amount.value );
    BOOST_REQUIRE( bob.get_hbd_balance().amount.value == ASSET( "977.500 TBD" ).amount.value );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test filling limit order with better order when partial order is better." );

    op.owner = "alice";
    op.orderid = 4;
    op.amount_to_sell = ASSET( "10.000 TESTS" );
    op.min_to_receive = ASSET( "11.000 TBD" );
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    push_transaction( tx, alice_private_key, 0 );

    op.owner = "bob";
    op.orderid = 4;
    op.amount_to_sell = ASSET( "12.000 TBD" );
    op.min_to_receive = ASSET( "10.000 TESTS" );
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    push_transaction( tx, bob_private_key, 0 );

    limit_order = limit_order_idx.find( boost::make_tuple( "bob", 4 ) );
    BOOST_REQUIRE( limit_order != limit_order_idx.end() );
    BOOST_REQUIRE( limit_order_idx.find(boost::make_tuple( "alice", 4 ) ) == limit_order_idx.end() );
    BOOST_REQUIRE( limit_order->seller == "bob" );
    BOOST_REQUIRE( limit_order->orderid == 4 );
    BOOST_REQUIRE( limit_order->for_sale.value == 1000 );
    BOOST_REQUIRE( limit_order->sell_price == price( ASSET( "12.000 TBD" ), ASSET( "10.000 TESTS" ) ) );
    BOOST_REQUIRE( limit_order->get_market() == std::make_pair( HBD_SYMBOL, HIVE_SYMBOL ) );
    BOOST_REQUIRE( alice.get_balance().amount.value == ASSET( "975.000 TESTS" ).amount.value );
    BOOST_REQUIRE( alice.get_hbd_balance().amount.value == ASSET( "33.500 TBD" ).amount.value );
    BOOST_REQUIRE( bob.get_balance().amount.value == ASSET( "25.000 TESTS" ).amount.value );
    BOOST_REQUIRE( bob.get_hbd_balance().amount.value == ASSET( "965.500 TBD" ).amount.value );
    validate_database();

    limit_order_cancel_operation can;
    can.owner = "bob";
    can.orderid = 4;
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( can );
    push_transaction( tx, bob_private_key, 0 );

    BOOST_TEST_MESSAGE( "--- Test filling limit order with better order when partial order is worse." );

    //auto& gpo = db->get_dynamic_global_properties();
    //auto start_hbd = gpo.get_current_hbd_supply();

    op.owner = "alice";
    op.orderid = 5;
    op.amount_to_sell = ASSET( "20.000 TESTS" );
    op.min_to_receive = ASSET( "22.000 TBD" );
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    push_transaction( tx, alice_private_key, 0 );

    op.owner = "bob";
    op.orderid = 5;
    op.amount_to_sell = ASSET( "12.000 TBD" );
    op.min_to_receive = ASSET( "10.000 TESTS" );
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    push_transaction( tx, bob_private_key, 0 );

    limit_order = limit_order_idx.find( boost::make_tuple( "alice", 5 ) );
    BOOST_REQUIRE( limit_order != limit_order_idx.end() );
    BOOST_REQUIRE( limit_order_idx.find(boost::make_tuple( "bob", 5 ) ) == limit_order_idx.end() );
    BOOST_REQUIRE( limit_order->seller == "alice" );
    BOOST_REQUIRE( limit_order->orderid == 5 );
    BOOST_REQUIRE( limit_order->for_sale.value == 9091 );
    BOOST_REQUIRE( limit_order->sell_price == price( ASSET( "20.000 TESTS" ), ASSET( "22.000 TBD" ) ) );
    BOOST_REQUIRE( limit_order->get_market() == std::make_pair( HBD_SYMBOL, HIVE_SYMBOL ) );
    BOOST_REQUIRE( alice.get_balance().amount.value == ASSET( "955.000 TESTS" ).amount.value );
    BOOST_REQUIRE( alice.get_hbd_balance().amount.value == ASSET( "45.500 TBD" ).amount.value );
    BOOST_REQUIRE( bob.get_balance().amount.value == ASSET( "35.909 TESTS" ).amount.value );
    BOOST_REQUIRE( bob.get_hbd_balance().amount.value == ASSET( "954.500 TBD" ).amount.value );
    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( limit_order_create2_authorities )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: limit_order_create2_authorities" );

    ACTORS( (alice)(bob) )
    fund( "alice", 10000 );

    limit_order_create2_operation op;
    op.owner = "alice";
    op.amount_to_sell = ASSET( "1.000 TESTS" );
    op.exchange_rate = price( ASSET( "1.000 TESTS" ), ASSET( "1.000 TBD" ) );
    op.expiration = db->head_block_time() + fc::seconds( HIVE_MAX_LIMIT_ORDER_EXPIRATION );

    signed_transaction tx;
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );

    BOOST_TEST_MESSAGE( "--- Test failure when no signature." );
    HIVE_REQUIRE_THROW( push_transaction( tx, database::skip_transaction_dupe_check ), tx_missing_active_auth );

    BOOST_TEST_MESSAGE( "--- Test success with account signature" );
    push_transaction( tx, alice_private_key, database::skip_transaction_dupe_check );

    BOOST_TEST_MESSAGE( "--- Test failure with duplicate signature" );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, database::skip_transaction_dupe_check ), tx_duplicate_sig );

    BOOST_TEST_MESSAGE( "--- Test failure with additional incorrect signature" );
    tx.signatures.clear();
    sign( tx, alice_private_key );
    sign( tx, bob_private_key );
    HIVE_REQUIRE_THROW( push_transaction( tx, database::skip_transaction_dupe_check ), tx_irrelevant_sig );

    BOOST_TEST_MESSAGE( "--- Test failure with incorrect signature" );
    tx.signatures.clear();
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_post_key, database::skip_transaction_dupe_check ), tx_missing_active_auth );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( limit_order_create2_apply )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: limit_order_create2_apply" );

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );

    ACTORS( (alice)(bob) )
    fund( "alice", 1000000 );
    fund( "bob", 1000000 );
    convert( "bob", ASSET("1000.000 TESTS" ) );

    const auto& limit_order_idx = db->get_index< limit_order_index >().indices().get< by_account >();

    BOOST_TEST_MESSAGE( "--- Test failure when account does not have required funds" );
    limit_order_create2_operation op;
    signed_transaction tx;

    op.owner = "bob";
    op.orderid = 1;
    op.amount_to_sell = ASSET( "10.000 TESTS" );
    op.exchange_rate = price( ASSET( "1.000 TESTS" ), ASSET( "1.000 TBD" ) );
    op.fill_or_kill = false;
    op.expiration = db->head_block_time() + fc::seconds( HIVE_MAX_LIMIT_ORDER_EXPIRATION );
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    HIVE_REQUIRE_THROW( push_transaction( tx, bob_private_key, 0 ), fc::exception );

    BOOST_REQUIRE( limit_order_idx.find( boost::make_tuple( "bob", op.orderid ) ) == limit_order_idx.end() );
    BOOST_REQUIRE( bob.get_balance().amount.value == ASSET( "0.000 TESTS" ).amount.value );
    BOOST_REQUIRE( bob.get_hbd_balance().amount.value == ASSET( "1000.000 TBD" ).amount.value );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test failure when price is 0" );

    /// First check validation on price constructor level:
    {
      price broken_price;
      /// Invalid base value
      HIVE_REQUIRE_THROW(broken_price=price(ASSET("0.000 TESTS"), ASSET("1.000 TBD")),
        fc::exception);
      /// Invalid quote value
      HIVE_REQUIRE_THROW(broken_price=price(ASSET("1.000 TESTS"), ASSET("0.000 TBD")),
        fc::exception);
      /// Invalid symbol (same in base & quote)
      HIVE_REQUIRE_THROW(broken_price=price(ASSET("1.000 TESTS"), ASSET("1.000 TESTS")),
        fc::exception);
    }

    op.owner = "alice";
    /** Here intentionally price has assigned its members directly, to skip validation
        inside price constructor, and force the one performed at tx push.
    */
    op.exchange_rate = price();
    op.exchange_rate.base = ASSET("0.000 TESTS");
    op.exchange_rate.quote = ASSET("1.000 TBD");

    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );

    BOOST_REQUIRE( limit_order_idx.find( boost::make_tuple( "alice", op.orderid ) ) == limit_order_idx.end() );
    BOOST_REQUIRE( alice.get_balance().amount.value == ASSET( "1000.000 TESTS" ).amount.value );
    BOOST_REQUIRE( alice.get_hbd_balance().amount.value == ASSET( "0.000 TBD" ).amount.value );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test failure when amount to sell is 0" );

    op.amount_to_sell = ASSET( "0.000 TESTS" );
    op.exchange_rate = price( ASSET( "1.000 TESTS" ), ASSET( "1.000 TBD" ) );
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );

    BOOST_REQUIRE( limit_order_idx.find( boost::make_tuple( "alice", op.orderid ) ) == limit_order_idx.end() );
    BOOST_REQUIRE( alice.get_balance().amount.value == ASSET( "1000.000 TESTS" ).amount.value );
    BOOST_REQUIRE( alice.get_hbd_balance().amount.value == ASSET( "0.000 TBD" ).amount.value );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test failure when expiration is too long" );
    op.amount_to_sell = ASSET( "10.000 TESTS" );
    op.exchange_rate = price( ASSET( "2.000 TESTS" ), ASSET( "3.000 TBD" ) );
    op.expiration = db->head_block_time() + fc::seconds( HIVE_MAX_LIMIT_ORDER_EXPIRATION + 1 );
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );

    BOOST_TEST_MESSAGE( "--- Test success creating limit order that will not be filled" );

    op.expiration = db->head_block_time() + fc::seconds( HIVE_MAX_LIMIT_ORDER_EXPIRATION );
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    push_transaction( tx, alice_private_key, 0 );

    auto limit_order = limit_order_idx.find( boost::make_tuple( "alice", op.orderid ) );
    BOOST_REQUIRE( limit_order != limit_order_idx.end() );
    BOOST_REQUIRE( limit_order->seller == op.owner );
    BOOST_REQUIRE( limit_order->orderid == op.orderid );
    BOOST_REQUIRE( limit_order->for_sale == op.amount_to_sell.amount );
    BOOST_REQUIRE( limit_order->sell_price == op.exchange_rate );
    BOOST_REQUIRE( limit_order->get_market() == std::make_pair( HBD_SYMBOL, HIVE_SYMBOL ) );
    BOOST_REQUIRE( alice.get_balance().amount.value == ASSET( "990.000 TESTS" ).amount.value );
    BOOST_REQUIRE( alice.get_hbd_balance().amount.value == ASSET( "0.000 TBD" ).amount.value );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test failure creating limit order with duplicate id" );

    op.amount_to_sell = ASSET( "20.000 TESTS" );
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );

    limit_order = limit_order_idx.find( boost::make_tuple( "alice", op.orderid ) );
    BOOST_REQUIRE( limit_order != limit_order_idx.end() );
    BOOST_REQUIRE( limit_order->seller == op.owner );
    BOOST_REQUIRE( limit_order->orderid == op.orderid );
    BOOST_REQUIRE( limit_order->for_sale == 10000 );
    BOOST_REQUIRE( limit_order->sell_price == op.exchange_rate );
    BOOST_REQUIRE( limit_order->get_market() == std::make_pair( HBD_SYMBOL, HIVE_SYMBOL ) );
    BOOST_REQUIRE( alice.get_balance().amount.value == ASSET( "990.000 TESTS" ).amount.value );
    BOOST_REQUIRE( alice.get_hbd_balance().amount.value == ASSET( "0.000 TBD" ).amount.value );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test sucess killing an order that will not be filled" );

    op.orderid = 2;
    op.fill_or_kill = true;
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );

    BOOST_REQUIRE( limit_order_idx.find( boost::make_tuple( "alice", op.orderid ) ) == limit_order_idx.end() );
    BOOST_REQUIRE( alice.get_balance().amount.value == ASSET( "990.000 TESTS" ).amount.value );
    BOOST_REQUIRE( alice.get_hbd_balance().amount.value == ASSET( "0.000 TBD" ).amount.value );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test having a partial match to limit order" );
    // Alice has order for 15 HBD at a price of 2:3
    // Fill 5 HIVE for 7.5 HBD

    op.owner = "bob";
    op.orderid = 1;
    op.amount_to_sell = ASSET( "7.500 TBD" );
    op.exchange_rate = price( ASSET( "3.000 TBD" ), ASSET( "2.000 TESTS" ) );
    op.fill_or_kill = false;
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    push_transaction( tx, bob_private_key, 0 );

    auto recent_ops = get_last_operations( 1 );
    auto fill_order_op = recent_ops[0].get< fill_order_operation >();

    limit_order = limit_order_idx.find( boost::make_tuple( "alice", 1 ) );
    BOOST_REQUIRE( limit_order != limit_order_idx.end() );
    BOOST_REQUIRE( limit_order->seller == "alice" );
    BOOST_REQUIRE( limit_order->orderid == op.orderid );
    BOOST_REQUIRE( limit_order->for_sale == 5000 );
    BOOST_REQUIRE( limit_order->sell_price == price( ASSET( "2.000 TESTS" ), ASSET( "3.000 TBD" ) ) );
    BOOST_REQUIRE( limit_order->get_market() == std::make_pair( HBD_SYMBOL, HIVE_SYMBOL ) );
    BOOST_REQUIRE( limit_order_idx.find( boost::make_tuple( "bob", op.orderid ) ) == limit_order_idx.end() );
    BOOST_REQUIRE( alice.get_balance().amount.value == ASSET( "990.000 TESTS" ).amount.value );
    BOOST_REQUIRE( alice.get_hbd_balance().amount.value == ASSET( "7.500 TBD" ).amount.value );
    BOOST_REQUIRE( bob.get_balance().amount.value == ASSET( "5.000 TESTS" ).amount.value );
    BOOST_REQUIRE( bob.get_hbd_balance().amount.value == ASSET( "992.500 TBD" ).amount.value );
    BOOST_REQUIRE( fill_order_op.open_owner == "alice" );
    BOOST_REQUIRE( fill_order_op.open_orderid == 1 );
    BOOST_REQUIRE( fill_order_op.open_pays.amount.value == ASSET( "5.000 TESTS").amount.value );
    BOOST_REQUIRE( fill_order_op.current_owner == "bob" );
    BOOST_REQUIRE( fill_order_op.current_orderid == 1 );
    BOOST_REQUIRE( fill_order_op.current_pays.amount.value == ASSET( "7.500 TBD" ).amount.value );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test filling an existing order fully, but the new order partially" );

    op.amount_to_sell = ASSET( "15.000 TBD" );
    op.exchange_rate = price( ASSET( "3.000 TBD" ), ASSET( "2.000 TESTS" ) );
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    push_transaction( tx, bob_private_key, 0 );

    limit_order = limit_order_idx.find( boost::make_tuple( "bob", 1 ) );
    BOOST_REQUIRE( limit_order != limit_order_idx.end() );
    BOOST_REQUIRE( limit_order->seller == "bob" );
    BOOST_REQUIRE( limit_order->orderid == 1 );
    BOOST_REQUIRE( limit_order->for_sale.value == 7500 );
    BOOST_REQUIRE( limit_order->sell_price == price( ASSET( "3.000 TBD" ), ASSET( "2.000 TESTS" ) ) );
    BOOST_REQUIRE( limit_order->get_market() == std::make_pair( HBD_SYMBOL, HIVE_SYMBOL ) );
    BOOST_REQUIRE( limit_order_idx.find( boost::make_tuple( "alice", 1 ) ) == limit_order_idx.end() );
    BOOST_REQUIRE( alice.get_balance().amount.value == ASSET( "990.000 TESTS" ).amount.value );
    BOOST_REQUIRE( alice.get_hbd_balance().amount.value == ASSET( "15.000 TBD" ).amount.value );
    BOOST_REQUIRE( bob.get_balance().amount.value == ASSET( "10.000 TESTS" ).amount.value );
    BOOST_REQUIRE( bob.get_hbd_balance().amount.value == ASSET( "977.500 TBD" ).amount.value );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test filling an existing order and new order fully" );

    op.owner = "alice";
    op.orderid = 3;
    op.amount_to_sell = ASSET( "5.000 TESTS" );
    op.exchange_rate = price( ASSET( "2.000 TESTS" ), ASSET( "3.000 TBD" ) );
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    push_transaction( tx, alice_private_key, 0 );

    BOOST_REQUIRE( limit_order_idx.find( boost::make_tuple( "alice", 3 ) ) == limit_order_idx.end() );
    BOOST_REQUIRE( limit_order_idx.find( boost::make_tuple( "bob", 1 ) ) == limit_order_idx.end() );
    BOOST_REQUIRE( alice.get_balance().amount.value == ASSET( "985.000 TESTS" ).amount.value );
    BOOST_REQUIRE( alice.get_hbd_balance().amount.value == ASSET( "22.500 TBD" ).amount.value );
    BOOST_REQUIRE( bob.get_balance().amount.value == ASSET( "15.000 TESTS" ).amount.value );
    BOOST_REQUIRE( bob.get_hbd_balance().amount.value == ASSET( "977.500 TBD" ).amount.value );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test filling limit order with better order when partial order is better." );

    op.owner = "alice";
    op.orderid = 4;
    op.amount_to_sell = ASSET( "10.000 TESTS" );
    op.exchange_rate = price( ASSET( "1.000 TESTS" ), ASSET( "1.100 TBD" ) );
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    push_transaction( tx, alice_private_key, 0 );

    op.owner = "bob";
    op.orderid = 4;
    op.amount_to_sell = ASSET( "12.000 TBD" );
    op.exchange_rate = price( ASSET( "1.200 TBD" ), ASSET( "1.000 TESTS" ) );
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    push_transaction( tx, bob_private_key, 0 );

    limit_order = limit_order_idx.find( boost::make_tuple( "bob", 4 ) );
    BOOST_REQUIRE( limit_order != limit_order_idx.end() );
    BOOST_REQUIRE( limit_order_idx.find(boost::make_tuple( "alice", 4 ) ) == limit_order_idx.end() );
    BOOST_REQUIRE( limit_order->seller == "bob" );
    BOOST_REQUIRE( limit_order->orderid == 4 );
    BOOST_REQUIRE( limit_order->for_sale.value == 1000 );
    BOOST_REQUIRE( limit_order->sell_price == op.exchange_rate );
    BOOST_REQUIRE( limit_order->get_market() == std::make_pair( HBD_SYMBOL, HIVE_SYMBOL ) );
    BOOST_REQUIRE( alice.get_balance().amount.value == ASSET( "975.000 TESTS" ).amount.value );
    BOOST_REQUIRE( alice.get_hbd_balance().amount.value == ASSET( "33.500 TBD" ).amount.value );
    BOOST_REQUIRE( bob.get_balance().amount.value == ASSET( "25.000 TESTS" ).amount.value );
    BOOST_REQUIRE( bob.get_hbd_balance().amount.value == ASSET( "965.500 TBD" ).amount.value );
    validate_database();

    limit_order_cancel_operation can;
    can.owner = "bob";
    can.orderid = 4;
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( can );
    push_transaction( tx, bob_private_key, 0 );

    BOOST_TEST_MESSAGE( "--- Test filling limit order with better order when partial order is worse." );

    //auto& gpo = db->get_dynamic_global_properties();
    //auto start_hbd = gpo.get_current_hbd_supply();


    op.owner = "alice";
    op.orderid = 5;
    op.amount_to_sell = ASSET( "20.000 TESTS" );
    op.exchange_rate = price( ASSET( "1.000 TESTS" ), ASSET( "1.100 TBD" ) );
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    push_transaction( tx, alice_private_key, 0 );

    op.owner = "bob";
    op.orderid = 5;
    op.amount_to_sell = ASSET( "12.000 TBD" );
    op.exchange_rate = price( ASSET( "1.200 TBD" ), ASSET( "1.000 TESTS" ) );
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    push_transaction( tx, bob_private_key, 0 );

    limit_order = limit_order_idx.find( boost::make_tuple( "alice", 5 ) );
    BOOST_REQUIRE( limit_order != limit_order_idx.end() );
    BOOST_REQUIRE( limit_order_idx.find(boost::make_tuple( "bob", 5 ) ) == limit_order_idx.end() );
    BOOST_REQUIRE( limit_order->seller == "alice" );
    BOOST_REQUIRE( limit_order->orderid == 5 );
    BOOST_REQUIRE( limit_order->for_sale.value == 9091 );
    BOOST_REQUIRE( limit_order->sell_price == price( ASSET( "1.000 TESTS" ), ASSET( "1.100 TBD" ) ) );
    BOOST_REQUIRE( limit_order->get_market() == std::make_pair( HBD_SYMBOL, HIVE_SYMBOL ) );
    BOOST_REQUIRE( alice.get_balance().amount.value == ASSET( "955.000 TESTS" ).amount.value );
    BOOST_REQUIRE( alice.get_hbd_balance().amount.value == ASSET( "45.500 TBD" ).amount.value );
    BOOST_REQUIRE( bob.get_balance().amount.value == ASSET( "35.909 TESTS" ).amount.value );
    BOOST_REQUIRE( bob.get_hbd_balance().amount.value == ASSET( "954.500 TBD" ).amount.value );

    BOOST_TEST_MESSAGE( "--- Test filling best order with multiple matches." );
    ACTORS( (sam)(dave) )
    fund( "sam", 1000000 );
    fund( "dave", 1000000 );
    convert( "dave", ASSET("1000.000 TESTS" ) );

    op.owner = "bob";
    op.orderid = 6;
    op.amount_to_sell = ASSET( "20.000 TESTS" );
    op.exchange_rate = price( ASSET( "1.000 TESTS" ), ASSET( "1.000 TBD" ) );
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    push_transaction( tx, bob_private_key, 0 );

    op.owner = "sam";
    op.orderid = 1;
    op.amount_to_sell = ASSET( "20.000 TESTS" );
    op.exchange_rate = price( ASSET( "1.000 TESTS" ), ASSET( "0.500 TBD" ) );
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    push_transaction( tx, sam_private_key, 0 );

    op.owner = "alice";
    op.orderid = 6;
    op.amount_to_sell = ASSET( "20.000 TESTS" );
    op.exchange_rate = price( ASSET( "1.000 TESTS" ), ASSET( "2.000 TBD" ) );
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    push_transaction( tx, alice_private_key, 0 );

    op.owner = "dave";
    op.orderid = 1;
    op.amount_to_sell = ASSET( "25.000 TBD" );
    op.exchange_rate = price( ASSET( "1.000 TBD" ), ASSET( "0.010 TESTS" ) );
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    push_transaction( tx, dave_private_key, 0 );

    recent_ops = get_last_operations( 2 );
    fill_order_op = recent_ops[1].get< fill_order_operation >();
    BOOST_REQUIRE( fill_order_op.open_owner == "sam" );
    BOOST_REQUIRE( fill_order_op.open_orderid == 1 );
    BOOST_REQUIRE( fill_order_op.open_pays == ASSET( "20.000 TESTS") );
    BOOST_REQUIRE( fill_order_op.current_owner == "dave" );
    BOOST_REQUIRE( fill_order_op.current_orderid == 1 );
    BOOST_REQUIRE( fill_order_op.current_pays == ASSET( "10.000 TBD" ) );

    fill_order_op = recent_ops[0].get< fill_order_operation >();
    BOOST_REQUIRE( fill_order_op.open_owner == "bob" );
    BOOST_REQUIRE( fill_order_op.open_orderid == 6 );
    BOOST_REQUIRE( fill_order_op.open_pays == ASSET( "15.000 TESTS") );
    BOOST_REQUIRE( fill_order_op.current_owner == "dave" );
    BOOST_REQUIRE( fill_order_op.current_orderid == 1 );
    BOOST_REQUIRE( fill_order_op.current_pays == ASSET( "15.000 TBD" ) );

    limit_order = limit_order_idx.find( boost::make_tuple( "bob", 6 ) );
    BOOST_REQUIRE( limit_order->seller == "bob" );
    BOOST_REQUIRE( limit_order->orderid == 6 );
    BOOST_REQUIRE( limit_order->for_sale.value == 5000 );
    BOOST_REQUIRE( limit_order->sell_price == price( ASSET( "1.000 TESTS" ), ASSET( "1.000 TBD" ) ) );

    limit_order = limit_order_idx.find( boost::make_tuple( "alice", 6 ) );
    BOOST_REQUIRE( limit_order->seller == "alice" );
    BOOST_REQUIRE( limit_order->orderid == 6 );
    BOOST_REQUIRE( limit_order->for_sale.value == 20000 );
    BOOST_REQUIRE( limit_order->sell_price == price( ASSET( "1.000 TESTS" ), ASSET( "2.000 TBD" ) ) );
    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( limit_order_cancel_validate )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: limit_order_cancel_validate" );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( limit_order_cancel_authorities )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: limit_order_cancel_authorities" );

    ACTORS( (alice)(bob) )
    fund( "alice", 10000 );

    limit_order_create_operation c;
    c.owner = "alice";
    c.orderid = 1;
    c.amount_to_sell = ASSET( "1.000 TESTS" );
    c.min_to_receive = ASSET( "1.000 TBD" );
    c.expiration = db->head_block_time() + fc::seconds( HIVE_MAX_LIMIT_ORDER_EXPIRATION );

    signed_transaction tx;
    tx.operations.push_back( c );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    push_transaction( tx, alice_private_key, 0 );

    limit_order_cancel_operation op;
    op.owner = "alice";
    op.orderid = 1;

    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );

    BOOST_TEST_MESSAGE( "--- Test failure when no signature." );
    HIVE_REQUIRE_THROW( push_transaction( tx, database::skip_transaction_dupe_check ), tx_missing_active_auth );

    BOOST_TEST_MESSAGE( "--- Test success with account signature" );
    push_transaction( tx, alice_private_key, database::skip_transaction_dupe_check );

    BOOST_TEST_MESSAGE( "--- Test failure with duplicate signature" );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, database::skip_transaction_dupe_check ), tx_duplicate_sig );

    BOOST_TEST_MESSAGE( "--- Test failure with additional incorrect signature" );
    tx.signatures.clear();
    sign( tx, alice_private_key );
    sign( tx, bob_private_key );
    HIVE_REQUIRE_THROW( push_transaction( tx, database::skip_transaction_dupe_check ), tx_irrelevant_sig );

    BOOST_TEST_MESSAGE( "--- Test failure with incorrect signature" );
    tx.signatures.clear();
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_post_key, database::skip_transaction_dupe_check ), tx_missing_active_auth );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( limit_order_cancel_apply )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: limit_order_cancel_apply" );

    ACTORS( (alice) )
    fund( "alice", 10000 );

    const auto& limit_order_idx = db->get_index< limit_order_index >().indices().get< by_account >();

    BOOST_TEST_MESSAGE( "--- Test cancel non-existent order" );

    limit_order_cancel_operation op;
    signed_transaction tx;

    op.owner = "alice";
    op.orderid = 5;
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );

    BOOST_TEST_MESSAGE( "--- Test cancel order" );

    limit_order_create_operation create;
    create.owner = "alice";
    create.orderid = 5;
    create.amount_to_sell = ASSET( "5.000 TESTS" );
    create.min_to_receive = ASSET( "7.500 TBD" );
    create.expiration = db->head_block_time() + fc::seconds( HIVE_MAX_LIMIT_ORDER_EXPIRATION );
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( create );
    push_transaction( tx, alice_private_key, 0 );

    BOOST_REQUIRE( limit_order_idx.find( boost::make_tuple( "alice", 5 ) ) != limit_order_idx.end() );

    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    push_transaction( tx, alice_private_key, 0 );

    BOOST_REQUIRE( limit_order_idx.find( boost::make_tuple( "alice", 5 ) ) == limit_order_idx.end() );
    BOOST_REQUIRE( alice.get_balance().amount.value == ASSET( "10.000 TESTS" ).amount.value );
    BOOST_REQUIRE( alice.get_hbd_balance().amount.value == ASSET( "0.000 TBD" ).amount.value );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( pow_validate )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: pow_validate" );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( pow_authorities )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: pow_authorities" );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( pow_apply )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: pow_apply" );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( account_recovery )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: account recovery" );

    ACTORS( (alice) );
    fund( "alice", 1000000 );
    generate_block();

    db_plugin->debug_update( [=]( database& db )
    {
      db.modify( db.get_witness_schedule_object(), [&]( witness_schedule_object& wso )
      {
        wso.median_props.account_creation_fee = ASSET( "0.100 TESTS" );
      });
    });

    generate_block();

    BOOST_TEST_MESSAGE( "Creating account bob with alice" );

    account_create_operation acc_create;
    acc_create.fee = ASSET( "0.100 TESTS" );
    acc_create.creator = "alice";
    acc_create.new_account_name = "bob";
    acc_create.owner = authority( 1, generate_private_key( "bob_owner" ).get_public_key(), 1 );
    acc_create.active = authority( 1, generate_private_key( "bob_active" ).get_public_key(), 1 );
    acc_create.posting = authority( 1, generate_private_key( "bob_posting" ).get_public_key(), 1 );
    acc_create.memo_key = generate_private_key( "bob_memo" ).get_public_key();
    acc_create.json_metadata = "";


    signed_transaction tx;
    tx.operations.push_back( acc_create );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    push_transaction( tx, alice_private_key, 0 );

    vest( HIVE_INIT_MINER_NAME, "bob", asset( 1000, HIVE_SYMBOL ) );

    const auto& bob_auth = db->get< account_authority_object, by_account >( "bob" );
    BOOST_REQUIRE( bob_auth.owner == acc_create.owner );


    BOOST_TEST_MESSAGE( "Changing bob's owner authority" );

    account_update_operation acc_update;
    acc_update.account = "bob";
    acc_update.owner = authority( 1, generate_private_key( "bad_key" ).get_public_key(), 1 );
    acc_update.memo_key = acc_create.memo_key;
    acc_update.json_metadata = "";

    tx.operations.clear();
    tx.signatures.clear();

    tx.operations.push_back( acc_update );
    push_transaction( tx, generate_private_key( "bob_owner" ), 0 );

    BOOST_REQUIRE( bob_auth.owner == *acc_update.owner );


    BOOST_TEST_MESSAGE( "Creating recover request for bob with alice" );

    request_account_recovery_operation request;
    request.recovery_account = "alice";
    request.account_to_recover = "bob";
    request.new_owner_authority = authority( 1, generate_private_key( "new_key" ).get_public_key(), 1 );

    tx.operations.clear();
    tx.signatures.clear();

    tx.operations.push_back( request );
    push_transaction( tx, alice_private_key, 0 );

    BOOST_REQUIRE( bob_auth.owner == *acc_update.owner );


    BOOST_TEST_MESSAGE( "Recovering bob's account with original owner auth and new secret" );

    generate_blocks( db->head_block_time() + HIVE_OWNER_UPDATE_LIMIT );

    recover_account_operation recover;
    recover.account_to_recover = "bob";
    recover.new_owner_authority = request.new_owner_authority;
    recover.recent_owner_authority = acc_create.owner;

    tx.operations.clear();
    tx.signatures.clear();

    tx.operations.push_back( recover );
    sign( tx, generate_private_key( "bob_owner" ) );
    sign( tx, generate_private_key( "new_key" ) );
    push_transaction( tx, 0 );
    const auto& owner1 = db->get< account_authority_object, by_account >("bob").owner;

    BOOST_REQUIRE( owner1 == recover.new_owner_authority );


    BOOST_TEST_MESSAGE( "Creating new recover request for a bogus key" );

    request.new_owner_authority = authority( 1, generate_private_key( "foo bar" ).get_public_key(), 1 );

    tx.operations.clear();
    tx.signatures.clear();

    tx.operations.push_back( request );
    push_transaction( tx, alice_private_key, 0 );


    BOOST_TEST_MESSAGE( "Testing failure when bob does not have new authority" );

    generate_blocks( db->head_block_time() + HIVE_OWNER_UPDATE_LIMIT + fc::seconds( HIVE_BLOCK_INTERVAL ) );

    recover.new_owner_authority = authority( 1, generate_private_key( "idontknow" ).get_public_key(), 1 );

    tx.operations.clear();
    tx.signatures.clear();

    tx.operations.push_back( recover );
    sign( tx, generate_private_key( "bob_owner" ) );
    sign( tx, generate_private_key( "idontknow" ) );
    HIVE_REQUIRE_THROW( push_transaction( tx, 0 ), fc::exception );
    const auto& owner2 = db->get< account_authority_object, by_account >("bob").owner;
    BOOST_REQUIRE( owner2 == authority( 1, generate_private_key( "new_key" ).get_public_key(), 1 ) );


    BOOST_TEST_MESSAGE( "Testing failure when bob does not have old authority" );

    recover.recent_owner_authority = authority( 1, generate_private_key( "idontknow" ).get_public_key(), 1 );
    recover.new_owner_authority = authority( 1, generate_private_key( "foo bar" ).get_public_key(), 1 );

    tx.operations.clear();
    tx.signatures.clear();

    tx.operations.push_back( recover );
    sign( tx, generate_private_key( "foo bar" ) );
    sign( tx, generate_private_key( "idontknow" ) );
    HIVE_REQUIRE_THROW( push_transaction( tx, 0 ), fc::exception );
    const auto& owner3 = db->get< account_authority_object, by_account >("bob").owner;
    BOOST_REQUIRE( owner3 == authority( 1, generate_private_key( "new_key" ).get_public_key(), 1 ) );


    BOOST_TEST_MESSAGE( "Testing using the same old owner auth again for recovery" );

    recover.recent_owner_authority = authority( 1, generate_private_key( "bob_owner" ).get_public_key(), 1 );
    recover.new_owner_authority = authority( 1, generate_private_key( "foo bar" ).get_public_key(), 1 );

    tx.operations.clear();
    tx.signatures.clear();

    tx.operations.push_back( recover );
    sign( tx, generate_private_key( "bob_owner" ) );
    sign( tx, generate_private_key( "foo bar" ) );
    push_transaction( tx, 0 );

    const auto& owner4 = db->get< account_authority_object, by_account >("bob").owner;
    BOOST_REQUIRE( owner4 == recover.new_owner_authority );

    BOOST_TEST_MESSAGE( "Creating a recovery request that will expire" );

    request.new_owner_authority = authority( 1, generate_private_key( "expire" ).get_public_key(), 1 );

    tx.operations.clear();
    tx.signatures.clear();

    tx.operations.push_back( request );
    push_transaction( tx, alice_private_key, 0 );

    const auto& request_idx = db->get_index< account_recovery_request_index >().indices();
    auto req_itr = request_idx.begin();

    BOOST_REQUIRE( req_itr->get_account_to_recover() == "bob" );
    BOOST_REQUIRE( req_itr->get_new_owner_authority() == authority( 1, generate_private_key( "expire" ).get_public_key(), 1 ) );
    auto expires = req_itr->get_expiration_time();
    BOOST_REQUIRE( expires == db->head_block_time() + HIVE_ACCOUNT_RECOVERY_REQUEST_EXPIRATION_PERIOD );
    ++req_itr;
    BOOST_REQUIRE( req_itr == request_idx.end() );

    generate_blocks( time_point_sec( expires - HIVE_BLOCK_INTERVAL ), true );

    const auto& new_request_idx = db->get_index< account_recovery_request_index >().indices();
    BOOST_REQUIRE( new_request_idx.begin() != new_request_idx.end() );

    generate_block();

    BOOST_REQUIRE( new_request_idx.begin() == new_request_idx.end() );

    recover.new_owner_authority = authority( 1, generate_private_key( "expire" ).get_public_key(), 1 );
    recover.recent_owner_authority = authority( 1, generate_private_key( "bob_owner" ).get_public_key(), 1 );

    tx.operations.clear();
    tx.signatures.clear();

    tx.operations.push_back( recover );
    tx.set_expiration( db->head_block_time() );
    sign( tx, generate_private_key( "expire" ) );
    sign( tx, generate_private_key( "bob_owner" ) );
    HIVE_REQUIRE_THROW( push_transaction( tx, 0 ), fc::exception );
    const auto& owner5 = db->get< account_authority_object, by_account >("bob").owner;
    BOOST_REQUIRE( owner5 == authority( 1, generate_private_key( "foo bar" ).get_public_key(), 1 ) );

    BOOST_TEST_MESSAGE( "Expiring owner authority history" );

    acc_update.owner = authority( 1, generate_private_key( "new_key" ).get_public_key(), 1 );

    tx.operations.clear();
    tx.signatures.clear();

    tx.operations.push_back( acc_update );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    push_transaction( tx, generate_private_key( "foo bar" ), 0 );

    generate_blocks( db->head_block_time() + ( HIVE_OWNER_AUTH_RECOVERY_PERIOD - HIVE_ACCOUNT_RECOVERY_REQUEST_EXPIRATION_PERIOD ) );
    generate_block();

    request.new_owner_authority = authority( 1, generate_private_key( "last key" ).get_public_key(), 1 );

    tx.operations.clear();
    tx.signatures.clear();

    tx.operations.push_back( request );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    push_transaction( tx, alice_private_key, 0 );

    recover.new_owner_authority = request.new_owner_authority;
    recover.recent_owner_authority = authority( 1, generate_private_key( "bob_owner" ).get_public_key(), 1 );

    tx.operations.clear();
    tx.signatures.clear();

    tx.operations.push_back( recover );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    sign( tx, generate_private_key( "bob_owner" ) );
    sign( tx, generate_private_key( "last key" ) );
    HIVE_REQUIRE_THROW( push_transaction( tx, 0 ), fc::exception );
    const auto& owner6 = db->get< account_authority_object, by_account >("bob").owner;
    BOOST_REQUIRE( owner6 == authority( 1, generate_private_key( "new_key" ).get_public_key(), 1 ) );

    recover.recent_owner_authority = authority( 1, generate_private_key( "foo bar" ).get_public_key(), 1 );

    tx.operations.clear();
    tx.signatures.clear();

    tx.operations.push_back( recover );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    sign( tx, generate_private_key( "foo bar" ) );
    sign( tx, generate_private_key( "last key" ) );
    push_transaction( tx, 0 );
    const auto& owner7 = db->get< account_authority_object, by_account >("bob").owner;
    BOOST_REQUIRE( owner7 == authority( 1, generate_private_key( "last key" ).get_public_key(), 1 ) );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( change_recovery_account )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing change_recovery_account_operation" );

    ACTORS( (alice)(bob)(sam)(tyler) )

    auto change_recovery_account = [&]( const std::string& account_to_recover, const std::string& new_recovery_account )
    {
      change_recovery_account_operation op;
      op.account_to_recover = account_to_recover;
      op.new_recovery_account = new_recovery_account;

      signed_transaction tx;
      tx.operations.push_back( op );
      tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
      push_transaction( tx, alice_private_key, 0 );
    };

    auto recover_account = [&]( const std::string& account_to_recover, const fc::ecc::private_key& new_owner_key, const fc::ecc::private_key& recent_owner_key )
    {
      recover_account_operation op;
      op.account_to_recover = account_to_recover;
      op.new_owner_authority = authority( 1, public_key_type( new_owner_key.get_public_key() ), 1 );
      op.recent_owner_authority = authority( 1, public_key_type( recent_owner_key.get_public_key() ), 1 );

      signed_transaction tx;
      tx.operations.push_back( op );
      tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
      // only Alice -> throw
      HIVE_REQUIRE_THROW( push_transaction( tx, recent_owner_key, 0 ), fc::exception );
      tx.signatures.clear();
      // only Sam -> throw
      HIVE_REQUIRE_THROW( push_transaction( tx, new_owner_key, 0 ), fc::exception );
      // Alice+Sam -> OK
      push_transaction( tx, recent_owner_key, 0 );
    };

    auto request_account_recovery = [&]( const std::string& recovery_account, const fc::ecc::private_key& recovery_account_key, const std::string& account_to_recover, const public_key_type& new_owner_key )
    {
      request_account_recovery_operation op;
      op.recovery_account    = recovery_account;
      op.account_to_recover  = account_to_recover;
      op.new_owner_authority = authority( 1, new_owner_key, 1 );

      signed_transaction tx;
      tx.operations.push_back( op );
      tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
      push_transaction( tx, recovery_account_key, 0 );
    };

    auto change_owner = [&]( const std::string& account, const fc::ecc::private_key& old_private_key, const public_key_type& new_public_key )
    {
      account_update_operation op;
      op.account = account;
      op.owner = authority( 1, new_public_key, 1 );

      signed_transaction tx;
      tx.operations.push_back( op );
      tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
      push_transaction( tx, old_private_key, 0 );
    };

    // if either/both users do not exist, we shouldn't allow it
    HIVE_REQUIRE_THROW( change_recovery_account("alice", "nobody"), fc::exception );
    HIVE_REQUIRE_THROW( change_recovery_account("haxer", "sam"   ), fc::exception );
    HIVE_REQUIRE_THROW( change_recovery_account("haxer", "nobody"), fc::exception );
    change_recovery_account("alice", "sam");

    fc::ecc::private_key alice_priv1 = fc::ecc::private_key::regenerate( fc::sha256::hash( "alice_k1" ) );
    fc::ecc::private_key alice_priv2 = fc::ecc::private_key::regenerate( fc::sha256::hash( "alice_k2" ) );
    public_key_type alice_pub1 = public_key_type( alice_priv1.get_public_key() );

    generate_blocks( db->head_block_time() + HIVE_OWNER_AUTH_RECOVERY_PERIOD - fc::seconds( HIVE_BLOCK_INTERVAL ), true );
    // cannot request account recovery until recovery account is approved
    HIVE_REQUIRE_THROW( request_account_recovery( "sam", sam_private_key, "alice", alice_pub1 ), fc::exception );
    generate_blocks(1);
    // cannot finish account recovery until requested
    HIVE_REQUIRE_THROW( recover_account( "alice", alice_priv1, alice_private_key ), fc::exception );
    // do the request
    request_account_recovery( "sam", sam_private_key, "alice", alice_pub1 );
    // can't recover with the current owner key
    HIVE_REQUIRE_THROW( recover_account( "alice", alice_priv1, alice_private_key ), fc::exception );
    // unless we change it!
    change_owner( "alice", alice_private_key, public_key_type( alice_priv2.get_public_key() ) );
    recover_account( "alice", alice_priv1, alice_private_key );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( escrow_transfer_validate )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: escrow_transfer_validate" );

    escrow_transfer_operation op;
    op.from = "alice";
    op.to = "bob";
    op.hbd_amount = ASSET( "1.000 TBD" );
    op.hive_amount = ASSET( "1.000 TESTS" );
    op.escrow_id = 0;
    op.agent = "sam";
    op.fee = ASSET( "0.100 TESTS" );
    op.json_meta = "";
    op.ratification_deadline = db->head_block_time() + 100;
    op.escrow_expiration = db->head_block_time() + 200;

    BOOST_TEST_MESSAGE( "--- failure when hbd symbol != HBD" );
    op.hbd_amount.symbol = HIVE_SYMBOL;
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );

    BOOST_TEST_MESSAGE( "--- failure when hive symbol != HIVE" );
    op.hbd_amount.symbol = HBD_SYMBOL;
    op.hive_amount.symbol = HBD_SYMBOL;
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );

    BOOST_TEST_MESSAGE( "--- failure when fee symbol != HBD and fee symbol != HIVE" );
    op.hive_amount.symbol = HIVE_SYMBOL;
    op.fee.symbol = VESTS_SYMBOL;
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );

    BOOST_TEST_MESSAGE( "--- failure when hbd == 0 and hive == 0" );
    op.fee.symbol = HIVE_SYMBOL;
    op.hbd_amount.amount = 0;
    op.hive_amount.amount = 0;
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );

    BOOST_TEST_MESSAGE( "--- failure when hbd < 0" );
    op.hbd_amount.amount = -100;
    op.hive_amount.amount = 1000;
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );

    BOOST_TEST_MESSAGE( "--- failure when hive < 0" );
    op.hbd_amount.amount = 1000;
    op.hive_amount.amount = -100;
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );

    BOOST_TEST_MESSAGE( "--- failure when fee < 0" );
    op.hive_amount.amount = 1000;
    op.fee.amount = -100;
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );

    BOOST_TEST_MESSAGE( "--- failure when ratification deadline == escrow expiration" );
    op.fee.amount = 100;
    op.ratification_deadline = op.escrow_expiration;
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );

    BOOST_TEST_MESSAGE( "--- failure when ratification deadline > escrow expiration" );
    op.ratification_deadline = op.escrow_expiration + 100;
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );

    BOOST_TEST_MESSAGE( "--- success" );
    op.ratification_deadline = op.escrow_expiration - 100;
    op.validate();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( escrow_transfer_authorities )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: escrow_transfer_authorities" );

    escrow_transfer_operation op;
    op.from = "alice";
    op.to = "bob";
    op.hbd_amount = ASSET( "1.000 TBD" );
    op.hive_amount = ASSET( "1.000 TESTS" );
    op.escrow_id = 0;
    op.agent = "sam";
    op.fee = ASSET( "0.100 TESTS" );
    op.json_meta = "";
    op.ratification_deadline = db->head_block_time() + 100;
    op.escrow_expiration = db->head_block_time() + 200;

    flat_set< account_name_type > auths;
    flat_set< account_name_type > expected;

    op.get_required_owner_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    op.get_required_posting_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    op.get_required_active_authorities( auths );
    expected.insert( "alice" );
    BOOST_REQUIRE( auths == expected );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( escrow_transfer_apply )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: escrow_transfer_apply" );

    ACTORS( (alice)(bob)(sam) )

    fund( "alice", 10000 );

    escrow_transfer_operation op;
    op.from = "alice";
    op.to = "bob";
    op.hbd_amount = ASSET( "1.000 TBD" );
    op.hive_amount = ASSET( "1.000 TESTS" );
    op.escrow_id = 0;
    op.agent = "sam";
    op.fee = ASSET( "0.100 TESTS" );
    op.json_meta = "";
    op.ratification_deadline = db->head_block_time() + 100;
    op.escrow_expiration = db->head_block_time() + 200;

    BOOST_TEST_MESSAGE( "--- failure when from cannot cover hbd amount" );
    signed_transaction tx;
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );

    BOOST_TEST_MESSAGE( "--- falure when from cannot cover amount + fee" );
    op.hbd_amount.amount = 0;
    op.hive_amount.amount = 10000;
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );

    BOOST_TEST_MESSAGE( "--- failure when ratification deadline is in the past" );
    op.hive_amount.amount = 1000;
    op.ratification_deadline = db->head_block_time() - 200;
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );

    BOOST_TEST_MESSAGE( "--- failure when expiration is in the past" );
    op.escrow_expiration = db->head_block_time() - 100;
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );

    BOOST_TEST_MESSAGE( "--- success" );
    op.ratification_deadline = db->head_block_time() + 100;
    op.escrow_expiration = db->head_block_time() + 200;
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );

    auto alice_hive_balance = alice.get_balance() - op.hive_amount - op.fee;
    auto alice_hbd_balance = alice.get_hbd_balance() - op.hbd_amount;
    auto bob_hive_balance = bob.get_balance();
    auto bob_hbd_balance = bob.get_hbd_balance();
    auto sam_hive_balance = sam.get_balance();
    auto sam_hbd_balance = sam.get_hbd_balance();

    push_transaction( tx, alice_private_key, 0 );

    const auto& escrow = db->get_escrow( op.from, op.escrow_id );

    BOOST_REQUIRE( escrow.escrow_id == op.escrow_id );
    BOOST_REQUIRE( escrow.from == op.from );
    BOOST_REQUIRE( escrow.to == op.to );
    BOOST_REQUIRE( escrow.agent == op.agent );
    BOOST_REQUIRE( escrow.ratification_deadline == op.ratification_deadline );
    BOOST_REQUIRE( escrow.escrow_expiration == op.escrow_expiration );
    BOOST_REQUIRE( escrow.get_hbd_balance() == op.hbd_amount );
    BOOST_REQUIRE( escrow.get_hive_balance() == op.hive_amount );
    BOOST_REQUIRE( escrow.get_fee() == op.fee );
    BOOST_REQUIRE( !escrow.to_approved );
    BOOST_REQUIRE( !escrow.agent_approved );
    BOOST_REQUIRE( !escrow.disputed );
    BOOST_REQUIRE( alice.get_balance() == alice_hive_balance );
    BOOST_REQUIRE( alice.get_hbd_balance() == alice_hbd_balance );
    BOOST_REQUIRE( bob.get_balance() == bob_hive_balance );
    BOOST_REQUIRE( bob.get_hbd_balance() == bob_hbd_balance );
    BOOST_REQUIRE( sam.get_balance() == sam_hive_balance );
    BOOST_REQUIRE( sam.get_hbd_balance() == sam_hbd_balance );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( escrow_approve_validate )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: escrow_approve_validate" );

    escrow_approve_operation op;

    op.from = "alice";
    op.to = "bob";
    op.agent = "sam";
    op.who = "bob";
    op.escrow_id = 0;
    op.approve = true;

    BOOST_TEST_MESSAGE( "--- failure when who is not to or agent" );
    op.who = "dave";
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );

    BOOST_TEST_MESSAGE( "--- success when who is to" );
    op.who = op.to;
    op.validate();

    BOOST_TEST_MESSAGE( "--- success when who is agent" );
    op.who = op.agent;
    op.validate();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( escrow_approve_authorities )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: escrow_approve_authorities" );

    escrow_approve_operation op;

    op.from = "alice";
    op.to = "bob";
    op.agent = "sam";
    op.who = "bob";
    op.escrow_id = 0;
    op.approve = true;

    flat_set< account_name_type > auths;
    flat_set< account_name_type > expected;

    op.get_required_owner_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    op.get_required_posting_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    op.get_required_active_authorities( auths );
    expected.insert( "bob" );
    BOOST_REQUIRE( auths == expected );

    expected.clear();
    auths.clear();

    op.who = "sam";
    op.get_required_active_authorities( auths );
    expected.insert( "sam" );
    BOOST_REQUIRE( auths == expected );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( escrow_approve_apply )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: escrow_approve_apply" );
    ACTORS( (alice)(bob)(sam)(dave) )
    fund( "alice", 10000 );

    escrow_transfer_operation et_op;
    et_op.from = "alice";
    et_op.to = "bob";
    et_op.agent = "sam";
    et_op.hive_amount = ASSET( "1.000 TESTS" );
    et_op.fee = ASSET( "0.100 TESTS" );
    et_op.json_meta = "";
    et_op.ratification_deadline = db->head_block_time() + 100;
    et_op.escrow_expiration = db->head_block_time() + 200;

    signed_transaction tx;
    tx.operations.push_back( et_op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    push_transaction( tx, alice_private_key, 0 );
    tx.operations.clear();
    tx.signatures.clear();


    BOOST_TEST_MESSAGE( "---failure when to does not match escrow" );
    escrow_approve_operation op;
    op.from = "alice";
    op.to = "dave";
    op.agent = "sam";
    op.who = "dave";
    op.approve = true;

    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, dave_private_key, 0 ), fc::exception );


    BOOST_TEST_MESSAGE( "--- failure when agent does not match escrow" );
    op.to = "bob";
    op.agent = "dave";

    tx.operations.clear();
    tx.signatures.clear();

    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, dave_private_key, 0 ), fc::exception );


    BOOST_TEST_MESSAGE( "--- success approving to" );
    op.agent = "sam";
    op.who = "bob";

    tx.operations.clear();
    tx.signatures.clear();

    tx.operations.push_back( op );
    push_transaction( tx, bob_private_key, 0 );

    auto& escrow = db->get_escrow( op.from, op.escrow_id );
    BOOST_REQUIRE( escrow.to == "bob" );
    BOOST_REQUIRE( escrow.agent == "sam" );
    BOOST_REQUIRE( escrow.ratification_deadline == et_op.ratification_deadline );
    BOOST_REQUIRE( escrow.escrow_expiration == et_op.escrow_expiration );
    BOOST_REQUIRE( escrow.get_hbd_balance() == ASSET( "0.000 TBD" ) );
    BOOST_REQUIRE( escrow.get_hive_balance() == ASSET( "1.000 TESTS" ) );
    BOOST_REQUIRE( escrow.get_fee() == ASSET( "0.100 TESTS" ) );
    BOOST_REQUIRE( escrow.to_approved );
    BOOST_REQUIRE( !escrow.agent_approved );
    BOOST_REQUIRE( !escrow.disputed );


    BOOST_TEST_MESSAGE( "--- failure on repeat approval" );
    tx.signatures.clear();

    tx.set_expiration( db->head_block_time() + HIVE_BLOCK_INTERVAL );
    HIVE_REQUIRE_THROW( push_transaction( tx, bob_private_key, 0 ), fc::exception );

    BOOST_REQUIRE( escrow.to == "bob" );
    BOOST_REQUIRE( escrow.agent == "sam" );
    BOOST_REQUIRE( escrow.ratification_deadline == et_op.ratification_deadline );
    BOOST_REQUIRE( escrow.escrow_expiration == et_op.escrow_expiration );
    BOOST_REQUIRE( escrow.get_hbd_balance() == ASSET( "0.000 TBD" ) );
    BOOST_REQUIRE( escrow.get_hive_balance() == ASSET( "1.000 TESTS" ) );
    BOOST_REQUIRE( escrow.get_fee() == ASSET( "0.100 TESTS" ) );
    BOOST_REQUIRE( escrow.to_approved );
    BOOST_REQUIRE( !escrow.agent_approved );
    BOOST_REQUIRE( !escrow.disputed );


    BOOST_TEST_MESSAGE( "--- failure trying to repeal after approval" );
    tx.signatures.clear();
    tx.operations.clear();

    op.approve = false;

    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, bob_private_key, 0 ), fc::exception );

    BOOST_REQUIRE( escrow.to == "bob" );
    BOOST_REQUIRE( escrow.agent == "sam" );
    BOOST_REQUIRE( escrow.ratification_deadline == et_op.ratification_deadline );
    BOOST_REQUIRE( escrow.escrow_expiration == et_op.escrow_expiration );
    BOOST_REQUIRE( escrow.get_hbd_balance() == ASSET( "0.000 TBD" ) );
    BOOST_REQUIRE( escrow.get_hive_balance() == ASSET( "1.000 TESTS" ) );
    BOOST_REQUIRE( escrow.get_fee() == ASSET( "0.100 TESTS" ) );
    BOOST_REQUIRE( escrow.to_approved );
    BOOST_REQUIRE( !escrow.agent_approved );
    BOOST_REQUIRE( !escrow.disputed );


    BOOST_TEST_MESSAGE( "--- success refunding from because of repeal" );
    tx.signatures.clear();
    tx.operations.clear();

    op.who = op.agent;

    tx.operations.push_back( op );
    push_transaction( tx, sam_private_key, 0 );

    {
      auto recent_ops = get_last_operations( 1 );
      auto reject_op = recent_ops.back().get< escrow_rejected_operation >();

      BOOST_REQUIRE( reject_op.from == et_op.from );
      BOOST_REQUIRE( reject_op.to == et_op.to );
      BOOST_REQUIRE( reject_op.agent == et_op.agent );
      BOOST_REQUIRE( reject_op.escrow_id == et_op.escrow_id );
      BOOST_REQUIRE( reject_op.hive_amount == et_op.hive_amount );
      BOOST_REQUIRE( reject_op.hbd_amount == et_op.hbd_amount );
      BOOST_REQUIRE( reject_op.fee == et_op.fee );
    }

    HIVE_REQUIRE_THROW( db->get_escrow( op.from, op.escrow_id ), fc::exception );
    BOOST_REQUIRE( alice.get_balance() == ASSET( "10.000 TESTS" ) );
    validate_database();


    BOOST_TEST_MESSAGE( "--- test automatic refund when escrow is not ratified before deadline" );
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( et_op );
    push_transaction( tx, alice_private_key, 0 );

    generate_blocks( et_op.ratification_deadline, true );
    generate_block();

    {
      auto recent_ops = get_last_operations( 1 );
      auto reject_op = recent_ops.back().get< escrow_rejected_operation >();

      BOOST_REQUIRE( reject_op.from == et_op.from );
      BOOST_REQUIRE( reject_op.to == et_op.to );
      BOOST_REQUIRE( reject_op.agent == et_op.agent );
      BOOST_REQUIRE( reject_op.escrow_id == et_op.escrow_id );
      BOOST_REQUIRE( reject_op.hive_amount == et_op.hive_amount );
      BOOST_REQUIRE( reject_op.hbd_amount == et_op.hbd_amount );
      BOOST_REQUIRE( reject_op.fee == et_op.fee );
    }

    HIVE_REQUIRE_THROW( db->get_escrow( op.from, op.escrow_id ), fc::exception );
    BOOST_REQUIRE( get_balance( "alice" ) == ASSET( "10.000 TESTS" ) );
    validate_database();


    BOOST_TEST_MESSAGE( "--- test ratification expiration when escrow is only approved by to" );
    tx.operations.clear();
    tx.signatures.clear();
    et_op.ratification_deadline = db->head_block_time() + 100;
    et_op.escrow_expiration = db->head_block_time() + 200;
    tx.operations.push_back( et_op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    push_transaction( tx, alice_private_key, 0 );

    tx.operations.clear();
    tx.signatures.clear();
    op.who = op.to;
    op.approve = true;
    tx.operations.push_back( op );
    push_transaction( tx, bob_private_key, 0 );

    generate_blocks( et_op.ratification_deadline, true );
    generate_block();

    {
      auto recent_ops = get_last_operations( 1 );
      auto reject_op = recent_ops.back().get< escrow_rejected_operation >();

      BOOST_REQUIRE( reject_op.from == et_op.from );
      BOOST_REQUIRE( reject_op.to == et_op.to );
      BOOST_REQUIRE( reject_op.agent == et_op.agent );
      BOOST_REQUIRE( reject_op.escrow_id == et_op.escrow_id );
      BOOST_REQUIRE( reject_op.hive_amount == et_op.hive_amount );
      BOOST_REQUIRE( reject_op.hbd_amount == et_op.hbd_amount );
      BOOST_REQUIRE( reject_op.fee == et_op.fee );
    }

    HIVE_REQUIRE_THROW( db->get_escrow( op.from, op.escrow_id ), fc::exception );
    BOOST_REQUIRE( get_balance( "alice" ) == ASSET( "10.000 TESTS" ) );
    validate_database();


    BOOST_TEST_MESSAGE( "--- test ratification expiration when escrow is only approved by agent" );
    tx.operations.clear();
    tx.signatures.clear();
    et_op.ratification_deadline = db->head_block_time() + 100;
    et_op.escrow_expiration = db->head_block_time() + 200;
    tx.operations.push_back( et_op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    push_transaction( tx, alice_private_key, 0 );

    tx.operations.clear();
    tx.signatures.clear();
    op.who = op.agent;
    tx.operations.push_back( op );
    push_transaction( tx, sam_private_key, 0 );

    generate_blocks( et_op.ratification_deadline, true );
    generate_block();

    {
      auto recent_ops = get_last_operations( 1 );
      auto reject_op = recent_ops.back().get< escrow_rejected_operation >();

      BOOST_REQUIRE( reject_op.from == et_op.from );
      BOOST_REQUIRE( reject_op.to == et_op.to );
      BOOST_REQUIRE( reject_op.agent == et_op.agent );
      BOOST_REQUIRE( reject_op.escrow_id == et_op.escrow_id );
      BOOST_REQUIRE( reject_op.hive_amount == et_op.hive_amount );
      BOOST_REQUIRE( reject_op.hbd_amount == et_op.hbd_amount );
      BOOST_REQUIRE( reject_op.fee == et_op.fee );
    }

    HIVE_REQUIRE_THROW( db->get_escrow( op.from, op.escrow_id ), fc::exception );
    BOOST_REQUIRE( get_balance( "alice" ) == ASSET( "10.000 TESTS" ) );
    validate_database();


    BOOST_TEST_MESSAGE( "--- success approving escrow" );
    tx.operations.clear();
    tx.signatures.clear();
    et_op.ratification_deadline = db->head_block_time() + 100;
    et_op.escrow_expiration = db->head_block_time() + 200;
    tx.operations.push_back( et_op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    push_transaction( tx, alice_private_key, 0 );

    tx.operations.clear();
    tx.signatures.clear();
    op.who = op.to;
    tx.operations.push_back( op );
    push_transaction( tx, bob_private_key, 0 );

    tx.operations.clear();
    tx.signatures.clear();
    op.who = op.agent;
    tx.operations.push_back( op );
    push_transaction( tx, sam_private_key, 0 );

    {
      auto recent_ops = get_last_operations( 1 );
      auto accept_op = recent_ops.back().get< escrow_approved_operation >();

      BOOST_REQUIRE( accept_op.from == et_op.from );
      BOOST_REQUIRE( accept_op.to == et_op.to );
      BOOST_REQUIRE( accept_op.agent == et_op.agent );
      BOOST_REQUIRE( accept_op.escrow_id == et_op.escrow_id );
      BOOST_REQUIRE( accept_op.fee == et_op.fee );

      const auto& escrow = db->get_escrow( op.from, op.escrow_id );
      BOOST_REQUIRE( escrow.to == "bob" );
      BOOST_REQUIRE( escrow.agent == "sam" );
      BOOST_REQUIRE( escrow.ratification_deadline == et_op.ratification_deadline );
      BOOST_REQUIRE( escrow.escrow_expiration == et_op.escrow_expiration );
      BOOST_REQUIRE( escrow.get_hbd_balance() == ASSET( "0.000 TBD" ) );
      BOOST_REQUIRE( escrow.get_hive_balance() == ASSET( "1.000 TESTS" ) );
      BOOST_REQUIRE( escrow.get_fee() == ASSET( "0.000 TESTS" ) );
      BOOST_REQUIRE( escrow.to_approved );
      BOOST_REQUIRE( escrow.agent_approved );
      BOOST_REQUIRE( !escrow.disputed );
    }

    BOOST_REQUIRE( get_balance( "sam" ) == et_op.fee );
    validate_database();


    BOOST_TEST_MESSAGE( "--- ratification expiration does not remove an approved escrow" );

    generate_blocks( et_op.ratification_deadline + HIVE_BLOCK_INTERVAL, true );
    {
      const auto& escrow = db->get_escrow( op.from, op.escrow_id );
      BOOST_REQUIRE( escrow.to == "bob" );
      BOOST_REQUIRE( escrow.agent == "sam" );
      BOOST_REQUIRE( escrow.ratification_deadline == et_op.ratification_deadline );
      BOOST_REQUIRE( escrow.escrow_expiration == et_op.escrow_expiration );
      BOOST_REQUIRE( escrow.get_hbd_balance() == ASSET( "0.000 TBD" ) );
      BOOST_REQUIRE( escrow.get_hive_balance() == ASSET( "1.000 TESTS" ) );
      BOOST_REQUIRE( escrow.get_fee() == ASSET( "0.000 TESTS" ) );
      BOOST_REQUIRE( escrow.to_approved );
      BOOST_REQUIRE( escrow.agent_approved );
      BOOST_REQUIRE( !escrow.disputed );
    }

    BOOST_REQUIRE( get_balance( "sam" ) == et_op.fee );
    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( escrow_dispute_validate )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: escrow_dispute_validate" );
    escrow_dispute_operation op;
    op.from = "alice";
    op.to = "bob";
    op.agent = "alice";
    op.who = "alice";

    BOOST_TEST_MESSAGE( "failure when who is not from or to" );
    op.who = "sam";
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );

    BOOST_TEST_MESSAGE( "success" );
    op.who = "alice";
    op.validate();

    op.who = "bob";
    op.validate();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( escrow_dispute_authorities )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: escrow_dispute_authorities" );
    escrow_dispute_operation op;
    op.from = "alice";
    op.to = "bob";
    op.who = "alice";

    flat_set< account_name_type > auths;
    flat_set< account_name_type > expected;

    op.get_required_owner_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    op.get_required_posting_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    op.get_required_active_authorities( auths );
    expected.insert( "alice" );
    BOOST_REQUIRE( auths == expected );

    auths.clear();
    expected.clear();
    op.who = "bob";
    op.get_required_active_authorities( auths );
    expected.insert( "bob" );
    BOOST_REQUIRE( auths == expected );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( escrow_dispute_apply )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: escrow_dispute_apply" );

    ACTORS( (alice)(bob)(sam)(dave) )
    fund( "alice", 10000 );

    escrow_transfer_operation et_op;
    et_op.from = "alice";
    et_op.to = "bob";
    et_op.agent = "sam";
    et_op.hive_amount = ASSET( "1.000 TESTS" );
    et_op.fee = ASSET( "0.100 TESTS" );
    et_op.ratification_deadline = db->head_block_time() + HIVE_BLOCK_INTERVAL;
    et_op.escrow_expiration = db->head_block_time() + 2 * HIVE_BLOCK_INTERVAL;

    escrow_approve_operation ea_b_op;
    ea_b_op.from = "alice";
    ea_b_op.to = "bob";
    ea_b_op.agent = "sam";
    ea_b_op.who = "bob";
    ea_b_op.approve = true;

    signed_transaction tx;
    tx.operations.push_back( et_op );
    tx.operations.push_back( ea_b_op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    sign( tx, alice_private_key );
    sign( tx, bob_private_key );
    push_transaction( tx, 0 );


    BOOST_TEST_MESSAGE( "--- failure when escrow has not been approved" );
    escrow_dispute_operation op;
    op.from = "alice";
    op.to = "bob";
    op.agent = "sam";
    op.who = "bob";

    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, bob_private_key, 0 ), fc::exception );

    const auto& escrow = db->get_escrow( et_op.from, et_op.escrow_id );
    BOOST_REQUIRE( escrow.to == "bob" );
    BOOST_REQUIRE( escrow.agent == "sam" );
    BOOST_REQUIRE( escrow.ratification_deadline == et_op.ratification_deadline );
    BOOST_REQUIRE( escrow.escrow_expiration == et_op.escrow_expiration );
    BOOST_REQUIRE( escrow.get_hbd_balance() == et_op.hbd_amount );
    BOOST_REQUIRE( escrow.get_hive_balance() == et_op.hive_amount );
    BOOST_REQUIRE( escrow.get_fee() == et_op.fee );
    BOOST_REQUIRE( escrow.to_approved );
    BOOST_REQUIRE( !escrow.agent_approved );
    BOOST_REQUIRE( !escrow.disputed );


    BOOST_TEST_MESSAGE( "--- failure when to does not match escrow" );
    escrow_approve_operation ea_s_op;
    ea_s_op.from = "alice";
    ea_s_op.to = "bob";
    ea_s_op.agent = "sam";
    ea_s_op.who = "sam";
    ea_s_op.approve = true;

    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( ea_s_op );
    push_transaction( tx, sam_private_key, 0 );

    op.to = "dave";
    op.who = "alice";
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );

    BOOST_REQUIRE( escrow.to == "bob" );
    BOOST_REQUIRE( escrow.agent == "sam" );
    BOOST_REQUIRE( escrow.ratification_deadline == et_op.ratification_deadline );
    BOOST_REQUIRE( escrow.escrow_expiration == et_op.escrow_expiration );
    BOOST_REQUIRE( escrow.get_hbd_balance() == et_op.hbd_amount );
    BOOST_REQUIRE( escrow.get_hive_balance() == et_op.hive_amount );
    BOOST_REQUIRE( escrow.get_fee() == ASSET( "0.000 TESTS" ) );
    BOOST_REQUIRE( escrow.to_approved );
    BOOST_REQUIRE( escrow.agent_approved );
    BOOST_REQUIRE( !escrow.disputed );


    BOOST_TEST_MESSAGE( "--- failure when agent does not match escrow" );
    op.to = "bob";
    op.who = "alice";
    op.agent = "dave";
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );

    BOOST_REQUIRE( escrow.to == "bob" );
    BOOST_REQUIRE( escrow.agent == "sam" );
    BOOST_REQUIRE( escrow.ratification_deadline == et_op.ratification_deadline );
    BOOST_REQUIRE( escrow.escrow_expiration == et_op.escrow_expiration );
    BOOST_REQUIRE( escrow.get_hbd_balance() == et_op.hbd_amount );
    BOOST_REQUIRE( escrow.get_hive_balance() == et_op.hive_amount );
    BOOST_REQUIRE( escrow.get_fee() == ASSET( "0.000 TESTS" ) );
    BOOST_REQUIRE( escrow.to_approved );
    BOOST_REQUIRE( escrow.agent_approved );
    BOOST_REQUIRE( !escrow.disputed );


    BOOST_TEST_MESSAGE( "--- failure when escrow is expired" );
    generate_blocks( 2 );

    tx.operations.clear();
    tx.signatures.clear();
    op.agent = "sam";
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );

    {
      const auto& escrow = db->get_escrow( et_op.from, et_op.escrow_id );
      BOOST_REQUIRE( escrow.to == "bob" );
      BOOST_REQUIRE( escrow.agent == "sam" );
      BOOST_REQUIRE( escrow.ratification_deadline == et_op.ratification_deadline );
      BOOST_REQUIRE( escrow.escrow_expiration == et_op.escrow_expiration );
      BOOST_REQUIRE( escrow.get_hbd_balance() == et_op.hbd_amount );
      BOOST_REQUIRE( escrow.get_hive_balance() == et_op.hive_amount );
      BOOST_REQUIRE( escrow.get_fee() == ASSET( "0.000 TESTS" ) );
      BOOST_REQUIRE( escrow.to_approved );
      BOOST_REQUIRE( escrow.agent_approved );
      BOOST_REQUIRE( !escrow.disputed );
    }


    BOOST_TEST_MESSAGE( "--- success disputing escrow" );
    et_op.escrow_id = 1;
    et_op.ratification_deadline = db->head_block_time() + HIVE_BLOCK_INTERVAL;
    et_op.escrow_expiration = db->head_block_time() + 2 * HIVE_BLOCK_INTERVAL;
    ea_b_op.escrow_id = et_op.escrow_id;
    ea_s_op.escrow_id = et_op.escrow_id;

    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( et_op );
    tx.operations.push_back( ea_b_op );
    tx.operations.push_back( ea_s_op );
    sign( tx, alice_private_key );
    sign( tx, bob_private_key );
    sign( tx, sam_private_key );
    push_transaction( tx, 0 );

    tx.operations.clear();
    tx.signatures.clear();
    op.escrow_id = et_op.escrow_id;
    tx.operations.push_back( op );
    push_transaction( tx, alice_private_key, 0 );

    {
      const auto& escrow = db->get_escrow( et_op.from, et_op.escrow_id );
      BOOST_REQUIRE( escrow.to == "bob" );
      BOOST_REQUIRE( escrow.agent == "sam" );
      BOOST_REQUIRE( escrow.ratification_deadline == et_op.ratification_deadline );
      BOOST_REQUIRE( escrow.escrow_expiration == et_op.escrow_expiration );
      BOOST_REQUIRE( escrow.get_hbd_balance() == et_op.hbd_amount );
      BOOST_REQUIRE( escrow.get_hive_balance() == et_op.hive_amount );
      BOOST_REQUIRE( escrow.get_fee() == ASSET( "0.000 TESTS" ) );
      BOOST_REQUIRE( escrow.to_approved );
      BOOST_REQUIRE( escrow.agent_approved );
      BOOST_REQUIRE( escrow.disputed );
    }


    BOOST_TEST_MESSAGE( "--- failure when escrow is already under dispute" );
    tx.operations.clear();
    tx.signatures.clear();
    op.who = "bob";
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, bob_private_key, 0 ), fc::exception );

    {
      const auto& escrow = db->get_escrow( et_op.from, et_op.escrow_id );
      BOOST_REQUIRE( escrow.to == "bob" );
      BOOST_REQUIRE( escrow.agent == "sam" );
      BOOST_REQUIRE( escrow.ratification_deadline == et_op.ratification_deadline );
      BOOST_REQUIRE( escrow.escrow_expiration == et_op.escrow_expiration );
      BOOST_REQUIRE( escrow.get_hbd_balance() == et_op.hbd_amount );
      BOOST_REQUIRE( escrow.get_hive_balance() == et_op.hive_amount );
      BOOST_REQUIRE( escrow.get_fee() == ASSET( "0.000 TESTS" ) );
      BOOST_REQUIRE( escrow.to_approved );
      BOOST_REQUIRE( escrow.agent_approved );
      BOOST_REQUIRE( escrow.disputed );
    }
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( escrow_release_validate )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: escrow release validate" );
    escrow_release_operation op;
    op.from = "alice";
    op.to = "bob";
    op.who = "alice";
    op.agent = "sam";
    op.receiver = "bob";


    BOOST_TEST_MESSAGE( "--- failure when hive < 0" );
    op.hive_amount.amount = -1;
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );


    BOOST_TEST_MESSAGE( "--- failure when hbd < 0" );
    op.hive_amount.amount = 0;
    op.hbd_amount.amount = -1;
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );


    BOOST_TEST_MESSAGE( "--- failure when hive == 0 and hbd == 0" );
    op.hbd_amount.amount = 0;
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );


    BOOST_TEST_MESSAGE( "--- failure when hbd is not HBD symbol" );
    op.hbd_amount = ASSET( "1.000 TESTS" );
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );


    BOOST_TEST_MESSAGE( "--- failure when hive is not HIVE symbol" );
    op.hbd_amount.symbol = HBD_SYMBOL;
    op.hive_amount = ASSET( "1.000 TBD" );
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );


    BOOST_TEST_MESSAGE( "--- success" );
    op.hive_amount.symbol = HIVE_SYMBOL;
    op.validate();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( escrow_release_authorities )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: escrow_release_authorities" );
    escrow_release_operation op;
    op.from = "alice";
    op.to = "bob";
    op.who = "alice";

    flat_set< account_name_type > auths;
    flat_set< account_name_type > expected;

    op.get_required_owner_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    op.get_required_posting_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    expected.insert( "alice" );
    op.get_required_active_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    op.who = "bob";
    auths.clear();
    expected.clear();
    expected.insert( "bob" );
    op.get_required_active_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    op.who = "sam";
    auths.clear();
    expected.clear();
    expected.insert( "sam" );
    op.get_required_active_authorities( auths );
    BOOST_REQUIRE( auths == expected );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( escrow_release_apply )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: escrow_release_apply" );

    ACTORS( (alice)(bob)(sam)(dave) )
    fund( "alice", 10000 );

    escrow_transfer_operation et_op;
    et_op.from = "alice";
    et_op.to = "bob";
    et_op.agent = "sam";
    et_op.hive_amount = ASSET( "1.000 TESTS" );
    et_op.fee = ASSET( "0.100 TESTS" );
    et_op.ratification_deadline = db->head_block_time() + HIVE_BLOCK_INTERVAL;
    et_op.escrow_expiration = db->head_block_time() + 2 * HIVE_BLOCK_INTERVAL;

    signed_transaction tx;
    tx.operations.push_back( et_op );

    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    push_transaction( tx, alice_private_key, 0 );


    BOOST_TEST_MESSAGE( "--- failure releasing funds prior to approval" );
    escrow_release_operation op;
    op.from = et_op.from;
    op.to = et_op.to;
    op.agent = et_op.agent;
    op.who = et_op.from;
    op.receiver = et_op.to;
    op.hive_amount = ASSET( "0.100 TESTS" );

    tx.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );

    escrow_approve_operation ea_b_op;
    ea_b_op.from = "alice";
    ea_b_op.to = "bob";
    ea_b_op.agent = "sam";
    ea_b_op.who = "bob";

    escrow_approve_operation ea_s_op;
    ea_s_op.from = "alice";
    ea_s_op.to = "bob";
    ea_s_op.agent = "sam";
    ea_s_op.who = "sam";

    tx.clear();
    tx.operations.push_back( ea_b_op );
    tx.operations.push_back( ea_s_op );
    sign( tx, bob_private_key );
    sign( tx, sam_private_key );
    push_transaction( tx, 0 );

    BOOST_TEST_MESSAGE( "--- failure when 'agent' attempts to release non-disputed escrow to 'to'" );
    op.who = et_op.agent;
    tx.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, sam_private_key, 0 ), fc::exception );


    BOOST_TEST_MESSAGE("--- failure when 'agent' attempts to release non-disputed escrow to 'from' " );
    op.receiver = et_op.from;

    tx.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, sam_private_key, 0 ), fc::exception );


    BOOST_TEST_MESSAGE( "--- failure when 'agent' attempt to release non-disputed escrow to not 'to' or 'from'" );
    op.receiver = "dave";

    tx.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, sam_private_key, 0 ), fc::exception );


    BOOST_TEST_MESSAGE( "--- failure when other attempts to release non-disputed escrow to 'to'" );
    op.receiver = et_op.to;
    op.who = "dave";

    tx.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, dave_private_key, 0 ), fc::exception );


    BOOST_TEST_MESSAGE("--- failure when other attempts to release non-disputed escrow to 'from' " );
    op.receiver = et_op.from;

    tx.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, dave_private_key, 0 ), fc::exception );


    BOOST_TEST_MESSAGE( "--- failure when other attempt to release non-disputed escrow to not 'to' or 'from'" );
    op.receiver = "dave";

    tx.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, dave_private_key, 0 ), fc::exception );


    BOOST_TEST_MESSAGE( "--- failure when 'to' attemtps to release non-disputed escrow to 'to'" );
    op.receiver = et_op.to;
    op.who = et_op.to;

    tx.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, bob_private_key, 0 ), fc::exception );


    BOOST_TEST_MESSAGE("--- failure when 'to' attempts to release non-dispured escrow to 'agent' " );
    op.receiver = et_op.agent;

    tx.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, bob_private_key, 0 ), fc::exception );


    BOOST_TEST_MESSAGE( "--- failure when 'to' attempts to release non-disputed escrow to not 'from'" );
    op.receiver = "dave";

    tx.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, bob_private_key, 0 ), fc::exception );


    BOOST_TEST_MESSAGE( "--- success release non-disputed escrow to 'to' from 'from'" );
    op.receiver = et_op.from;

    tx.clear();
    tx.operations.push_back( op );
    push_transaction( tx, bob_private_key, 0 );

    BOOST_REQUIRE( db->get_escrow( op.from, op.escrow_id ).get_hive_balance() == ASSET( "0.900 TESTS" ) );
    BOOST_REQUIRE( get_balance( "alice" ) == ASSET( "9.000 TESTS" ) );


    BOOST_TEST_MESSAGE( "--- failure when 'from' attempts to release non-disputed escrow to 'from'" );
    op.receiver = et_op.from;
    op.who = et_op.from;

    tx.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );


    BOOST_TEST_MESSAGE("--- failure when 'from' attempts to release non-disputed escrow to 'agent'" );
    op.receiver = et_op.agent;

    tx.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );


    BOOST_TEST_MESSAGE( "--- failure when 'from' attempts to release non-disputed escrow to not 'from'" );
    op.receiver = "dave";

    tx.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );


    BOOST_TEST_MESSAGE( "--- success release non-disputed escrow to 'from' from 'to'" );
    op.receiver = et_op.to;

    tx.clear();
    tx.operations.push_back( op );
    push_transaction( tx, alice_private_key, 0 );

    BOOST_REQUIRE( db->get_escrow( op.from, op.escrow_id ).get_hive_balance() == ASSET( "0.800 TESTS" ) );
    BOOST_REQUIRE( get_balance( "bob" ) == ASSET( "0.100 TESTS" ) );


    BOOST_TEST_MESSAGE( "--- failure when releasing more hbd than available" );
    op.hive_amount = ASSET( "1.000 TESTS" );

    tx.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );


    BOOST_TEST_MESSAGE( "--- failure when releasing less hive than available" );
    op.hive_amount = ASSET( "0.000 TESTS" );
    op.hbd_amount = ASSET( "1.000 TBD" );

    tx.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );


    BOOST_TEST_MESSAGE( "--- failure when 'to' attempts to release disputed escrow" );
    escrow_dispute_operation ed_op;
    ed_op.from = "alice";
    ed_op.to = "bob";
    ed_op.agent = "sam";
    ed_op.who = "alice";

    tx.clear();
    tx.operations.push_back( ed_op );
    push_transaction( tx, alice_private_key, 0 );

    tx.clear();
    op.from = et_op.from;
    op.receiver = et_op.from;
    op.who = et_op.to;
    op.hive_amount = ASSET( "0.100 TESTS" );
    op.hbd_amount = ASSET( "0.000 TBD" );
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, bob_private_key, 0 ), fc::exception );


    BOOST_TEST_MESSAGE( "--- failure when 'from' attempts to release disputed escrow" );
    tx.clear();
    op.receiver = et_op.to;
    op.who = et_op.from;
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );


    BOOST_TEST_MESSAGE( "--- failure when releasing disputed escrow to an account not 'to' or 'from'" );
    tx.clear();
    op.who = et_op.agent;
    op.receiver = "dave";
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, sam_private_key, 0 ), fc::exception );


    BOOST_TEST_MESSAGE( "--- failure when agent does not match escrow" );
    tx.clear();
    op.who = "dave";
    op.receiver = et_op.from;
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, dave_private_key, 0 ), fc::exception );


    BOOST_TEST_MESSAGE( "--- success releasing disputed escrow with agent to 'to'" );
    tx.clear();
    op.receiver = et_op.to;
    op.who = et_op.agent;
    tx.operations.push_back( op );
    push_transaction( tx, sam_private_key, 0 );

    BOOST_REQUIRE( get_balance( "bob" ) == ASSET( "0.200 TESTS" ) );
    BOOST_REQUIRE( db->get_escrow( et_op.from, et_op.escrow_id ).get_hive_balance() == ASSET( "0.700 TESTS" ) );


    BOOST_TEST_MESSAGE( "--- success releasing disputed escrow with agent to 'from'" );
    tx.clear();
    op.receiver = et_op.from;
    op.who = et_op.agent;
    tx.operations.push_back( op );
    push_transaction( tx, sam_private_key, 0 );

    BOOST_REQUIRE( get_balance( "alice" ) == ASSET( "9.100 TESTS" ) );
    BOOST_REQUIRE( db->get_escrow( et_op.from, et_op.escrow_id ).get_hive_balance() == ASSET( "0.600 TESTS" ) );


    BOOST_TEST_MESSAGE( "--- failure when 'to' attempts to release disputed expired escrow" );
    generate_blocks( 2 );

    tx.clear();
    op.receiver = et_op.from;
    op.who = et_op.to;
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    HIVE_REQUIRE_THROW( push_transaction( tx, bob_private_key, 0 ), fc::exception );


    BOOST_TEST_MESSAGE( "--- failure when 'from' attempts to release disputed expired escrow" );
    tx.clear();
    op.receiver = et_op.to;
    op.who = et_op.from;
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );


    BOOST_TEST_MESSAGE( "--- success releasing disputed expired escrow with agent" );
    tx.clear();
    op.receiver = et_op.from;
    op.who = et_op.agent;
    tx.operations.push_back( op );
    push_transaction( tx, sam_private_key, 0 );

    BOOST_REQUIRE( get_balance( "alice" ) == ASSET( "9.200 TESTS" ) );
    BOOST_REQUIRE( db->get_escrow( et_op.from, et_op.escrow_id ).get_hive_balance() == ASSET( "0.500 TESTS" ) );


    BOOST_TEST_MESSAGE( "--- success deleting escrow when balances are both zero" );
    tx.clear();
    op.hive_amount = ASSET( "0.500 TESTS" );
    tx.operations.push_back( op );
    push_transaction( tx, sam_private_key, 0 );

    BOOST_REQUIRE( get_balance( "alice" ) == ASSET( "9.700 TESTS" ) );
    HIVE_REQUIRE_THROW( db->get_escrow( et_op.from, et_op.escrow_id ), fc::exception );


    tx.clear();
    et_op.ratification_deadline = db->head_block_time() + HIVE_BLOCK_INTERVAL;
    et_op.escrow_expiration = db->head_block_time() + 2 * HIVE_BLOCK_INTERVAL;
    tx.operations.push_back( et_op );
    tx.operations.push_back( ea_b_op );
    tx.operations.push_back( ea_s_op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    sign( tx, alice_private_key );
    sign( tx, bob_private_key );
    sign( tx, sam_private_key );
    push_transaction( tx, 0 );
    generate_blocks( 2 );


    BOOST_TEST_MESSAGE( "--- failure when 'agent' attempts to release non-disputed expired escrow to 'to'" );
    tx.clear();
    op.receiver = et_op.to;
    op.who = et_op.agent;
    op.hive_amount = ASSET( "0.100 TESTS" );
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, sam_private_key, 0 ), fc::exception );


    BOOST_TEST_MESSAGE( "--- failure when 'agent' attempts to release non-disputed expired escrow to 'from'" );
    tx.clear();
    op.receiver = et_op.from;
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, sam_private_key, 0 ), fc::exception );


    BOOST_TEST_MESSAGE( "--- failure when 'agent' attempt to release non-disputed expired escrow to not 'to' or 'from'" );
    tx.clear();
    op.receiver = "dave";
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, sam_private_key, 0 ), fc::exception );


    BOOST_TEST_MESSAGE( "--- failure when 'to' attempts to release non-dispured expired escrow to 'agent'" );
    tx.clear();
    op.who = et_op.to;
    op.receiver = et_op.agent;
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, bob_private_key, 0 ), fc::exception );


    BOOST_TEST_MESSAGE( "--- failure when 'to' attempts to release non-disputed expired escrow to not 'from' or 'to'" );
    tx.clear();
    op.receiver = "dave";
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, bob_private_key, 0 ), fc::exception );


    BOOST_TEST_MESSAGE( "--- success release non-disputed expired escrow to 'to' from 'to'" );
    tx.clear();
    op.receiver = et_op.to;
    tx.operations.push_back( op );
    push_transaction( tx, bob_private_key, 0 );

    BOOST_REQUIRE( get_balance( "bob" ) == ASSET( "0.300 TESTS" ) );
    BOOST_REQUIRE( db->get_escrow( et_op.from, et_op.escrow_id ).get_hive_balance() == ASSET( "0.900 TESTS" ) );


    BOOST_TEST_MESSAGE( "--- success release non-disputed expired escrow to 'from' from 'to'" );
    tx.clear();
    op.receiver = et_op.from;
    tx.operations.push_back( op );
    push_transaction( tx, bob_private_key, 0 );

    BOOST_REQUIRE( get_balance( "alice" ) == ASSET( "8.700 TESTS" ) );
    BOOST_REQUIRE( db->get_escrow( et_op.from, et_op.escrow_id ).get_hive_balance() == ASSET( "0.800 TESTS" ) );


    BOOST_TEST_MESSAGE( "--- failure when 'from' attempts to release non-disputed expired escrow to 'agent'" );
    tx.clear();
    op.who = et_op.from;
    op.receiver = et_op.agent;
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );


    BOOST_TEST_MESSAGE( "--- failure when 'from' attempts to release non-disputed expired escrow to not 'from' or 'to'" );
    tx.clear();
    op.receiver = "dave";
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );


    BOOST_TEST_MESSAGE( "--- success release non-disputed expired escrow to 'to' from 'from'" );
    tx.clear();
    op.receiver = et_op.to;
    tx.operations.push_back( op );
    push_transaction( tx, alice_private_key, 0 );

    BOOST_REQUIRE( get_balance( "bob" ) == ASSET( "0.400 TESTS" ) );
    BOOST_REQUIRE( db->get_escrow( et_op.from, et_op.escrow_id ).get_hive_balance() == ASSET( "0.700 TESTS" ) );


    BOOST_TEST_MESSAGE( "--- success release non-disputed expired escrow to 'from' from 'from'" );
    tx.clear();
    op.receiver = et_op.from;
    tx.operations.push_back( op );
    push_transaction( tx, alice_private_key, 0 );

    BOOST_REQUIRE( get_balance( "alice" ) == ASSET( "8.800 TESTS" ) );
    BOOST_REQUIRE( db->get_escrow( et_op.from, et_op.escrow_id ).get_hive_balance() == ASSET( "0.600 TESTS" ) );


    BOOST_TEST_MESSAGE( "--- success deleting escrow when balances are zero on non-disputed escrow" );
    tx.clear();
    op.hive_amount = ASSET( "0.600 TESTS" );
    tx.operations.push_back( op );
    push_transaction( tx, alice_private_key, 0 );

    BOOST_REQUIRE( get_balance( "alice" ) == ASSET( "9.400 TESTS" ) );
    HIVE_REQUIRE_THROW( db->get_escrow( et_op.from, et_op.escrow_id ), fc::exception );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( escrow_limit )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: escrow_limit" );

    ACTORS((alice)(bob)(sam))
    fund( "alice", 10000 );
    generate_block();

    escrow_transfer_operation op;
    op.from = "alice";
    op.to = "bob";
    op.agent = "sam";
    op.hive_amount = ASSET( "0.001 TESTS" );
    op.fee = ASSET( "0.000 TESTS" );
    op.ratification_deadline = db->head_block_time() + 2 * ( HIVE_MAX_PENDING_TRANSFERS + 1 ) * HIVE_BLOCK_INTERVAL;
    op.escrow_expiration = op.ratification_deadline + HIVE_MAX_PENDING_TRANSFERS * HIVE_BLOCK_INTERVAL;

    int i;
    //create fresh escrow transfers from "alice", one per block, up to the limit
    for( i = 0; i < HIVE_MAX_PENDING_TRANSFERS; ++i )
    {
      signed_transaction tx;
      op.escrow_id = i;
      tx.operations.push_back( op );
      tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
      push_transaction( tx, alice_private_key, 0 );

      generate_block();
      BOOST_REQUIRE_EQUAL( db->get_account( "alice" ).pending_transfers, i + 1 );
    }
    
    //another escrow transfer from "alice" should fail
    {
      signed_transaction tx;
      op.escrow_id = i;
      tx.operations.push_back( op );
      tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
      HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );

      generate_block();
      BOOST_REQUIRE_EQUAL( db->get_account( "alice" ).pending_transfers, i );
    }

    escrow_approve_operation bob_approve, sam_approve;
    bob_approve.from = sam_approve.from = "alice";
    bob_approve.to = sam_approve.to = "bob";
    bob_approve.agent = sam_approve.agent = "sam";
    bob_approve.who = "bob";
    sam_approve.who = "sam";

    int declined = 0;
    int expired = 0;
    //approve or decline transfers - out of every 4 transfers:
    //0. bob and sam approves
    //1. bob approves, sam declines
    //2. bob declines, sam has nothing to approve anymore
    //3. skipped - it is left to expire unratified
    for( i = 0; i < HIVE_MAX_PENDING_TRANSFERS; ++i )
    {
      if( ( i % 4 ) == 3 )
      {
        ++expired;
        generate_block();
        continue;
      }

      bob_approve.approve = ( i % 4 ) != 2;
      sam_approve.approve = ( i % 4 ) != 1;
      bob_approve.escrow_id = sam_approve.escrow_id = i;

      {
        signed_transaction tx;
        tx.operations.push_back( bob_approve );
        tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
        push_transaction( tx, bob_private_key, 0 );
      }

      if( !bob_approve.approve )
      {
        ++declined;
      }
      else
      {
        signed_transaction tx;
        tx.operations.push_back( sam_approve );
        tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
        push_transaction( tx, sam_private_key, 0 );
        if( !sam_approve.approve )
          ++declined;
      }

      generate_block();
      BOOST_REQUIRE_EQUAL( db->get_account( "alice" ).pending_transfers, HIVE_MAX_PENDING_TRANSFERS - declined );
    }

    generate_block();
    //unratified transfers should expire by now
    BOOST_REQUIRE_EQUAL( db->get_account( "alice" ).pending_transfers, HIVE_MAX_PENDING_TRANSFERS - declined - expired );

    //every 4 of remaining transfers (only those with id divisible by 4 remain):
    //0. alice disputes transfer
    //4. alice completes transfer
    //8. bob disputes transfer
    //12. bob completes transfer
    escrow_dispute_operation alice_dispute, bob_dispute;
    escrow_release_operation alice_release, bob_release;
    alice_dispute.from = bob_dispute.from = alice_release.from = bob_release.from = "alice";
    alice_dispute.to = bob_dispute.to = alice_release.to = bob_release.to = "bob";
    alice_dispute.agent = bob_dispute.agent = alice_release.agent = bob_release.agent = "sam";
    alice_dispute.who = alice_release.who = "alice";
    bob_dispute.who = bob_release.who = "bob";
    alice_release.receiver = "bob";
    bob_release.receiver = "alice";
    alice_release.hive_amount = bob_release.hive_amount = ASSET( "0.001 TESTS" );

    int released = 0;
    for( i = 0; i < HIVE_MAX_PENDING_TRANSFERS; i += 4 )
    {
      alice_dispute.escrow_id = bob_dispute.escrow_id = alice_release.escrow_id = bob_release.escrow_id = i;
      signed_transaction tx;
      tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );

      fc::ecc::private_key _key;

      switch( i % 16 )
      {
      case 0:
        tx.operations.push_back( alice_dispute );
        _key = alice_private_key;
        break;
      case 4:
        tx.operations.push_back( alice_release );
        _key = alice_private_key;
        ++released;
        break;
      case 8:
        tx.operations.push_back( bob_dispute );
        _key = bob_private_key;
        break;
      case 12:
        tx.operations.push_back( bob_release );
        _key = bob_private_key;
        ++released;
        break;
      }

      push_transaction( tx, _key, 0 );
      generate_block();
      BOOST_REQUIRE_EQUAL( db->get_account( "alice" ).pending_transfers, HIVE_MAX_PENDING_TRANSFERS - declined - expired - released );
    }

    //every 2 of remaining transfers (only those with id divisible by 8 remain):
    //0. sam completes transfer to alice
    //8. sam completes transfer to bob
    alice_release.who = bob_release.who = "sam";
    alice_release.receiver = "alice";
    bob_release.receiver = "bob";

    int remaining = HIVE_MAX_PENDING_TRANSFERS - declined - expired - released;
    for( i = 0; i < HIVE_MAX_PENDING_TRANSFERS; i += 8 )
    {
      alice_release.escrow_id = bob_release.escrow_id = i;
      signed_transaction tx;
      tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );

      if( i % 16 )
        tx.operations.push_back( bob_release );
      else
        tx.operations.push_back( alice_release );

      push_transaction( tx, sam_private_key, 0 );
      --remaining;

      generate_block();
      BOOST_REQUIRE_EQUAL( db->get_account( "alice" ).pending_transfers, remaining );
    }

    BOOST_REQUIRE_EQUAL( remaining, 0 );
    validate_database();

  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( transfer_to_savings_validate )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: transfer_to_savings_validate" );

    transfer_to_savings_operation op;
    op.from = "alice";
    op.to = "alice";
    op.amount = ASSET( "1.000 TESTS" );


    BOOST_TEST_MESSAGE( "failure when 'from' is empty" );
    op.from = "";
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );


    BOOST_TEST_MESSAGE( "failure when 'to' is empty" );
    op.from = "alice";
    op.to = "";
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );


    BOOST_TEST_MESSAGE( "sucess when 'to' is not empty" );
    op.to = "bob";
    op.validate();


    BOOST_TEST_MESSAGE( "failure when amount is VESTS" );
    op.to = "alice";
    op.amount = ASSET( "1.000000 VESTS" );
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );


    BOOST_TEST_MESSAGE( "success when amount is HBD" );
    op.amount = ASSET( "1.000 TBD" );
    op.validate();


    BOOST_TEST_MESSAGE( "success when amount is HIVE" );
    op.amount = ASSET( "1.000 TESTS" );
    op.validate();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( transfer_to_savings_authorities )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: transfer_to_savings_authorities" );

    transfer_to_savings_operation op;
    op.from = "alice";
    op.to = "alice";
    op.amount = ASSET( "1.000 TESTS" );

    flat_set< account_name_type > auths;
    flat_set< account_name_type > expected;

    op.get_required_owner_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    op.get_required_posting_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    op.get_required_active_authorities( auths );
    expected.insert( "alice" );
    BOOST_REQUIRE( auths == expected );

    auths.clear();
    expected.clear();
    op.from = "bob";
    op.get_required_active_authorities( auths );
    expected.insert( "bob" );
    BOOST_REQUIRE( auths == expected );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( transfer_to_savings_apply )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: transfer_to_savings_apply" );

    ACTORS( (alice)(bob) );
    generate_block();

    fund( "alice", ASSET( "10.000 TESTS" ) );
    fund( "alice", ASSET( "10.000 TBD" ) );

    BOOST_REQUIRE( get_balance( "alice" ) == ASSET( "10.000 TESTS" ) );
    BOOST_REQUIRE( get_hbd_balance( "alice" ) == ASSET( "10.000 TBD" ) );

    transfer_to_savings_operation op;
    signed_transaction tx;

    BOOST_TEST_MESSAGE( "--- failure with insufficient funds" );
    op.from = "alice";
    op.to = "alice";
    op.amount = ASSET( "20.000 TESTS" );

    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );
    validate_database();


    BOOST_TEST_MESSAGE( "--- failure when transferring to non-existent account" );
    op.to = "sam";
    op.amount = ASSET( "1.000 TESTS" );

    tx.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Failure when transferring to treasury" );

    op.to = db->get_treasury_name();
    tx.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );
    validate_database();

    op.amount = ASSET( "1.000 TBD" );
    tx.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );
    validate_database();


    BOOST_TEST_MESSAGE( "--- success transferring HIVE to self" );
    op.to = "alice";
    op.amount = ASSET( "1.000 TESTS" );

    tx.clear();
    tx.operations.push_back( op );
    push_transaction( tx, alice_private_key, 0 );

    BOOST_REQUIRE( get_balance( "alice" ) == ASSET( "9.000 TESTS" ) );
    BOOST_REQUIRE( get_savings( "alice" ) == ASSET( "1.000 TESTS" ) );
    validate_database();


    BOOST_TEST_MESSAGE( "--- success transferring HBD to self" );
    op.amount = ASSET( "1.000 TBD" );

    tx.clear();
    tx.operations.push_back( op );
    push_transaction( tx, alice_private_key, 0 );

    BOOST_REQUIRE( get_hbd_balance( "alice" ) == ASSET( "9.000 TBD" ) );
    BOOST_REQUIRE( get_hbd_savings( "alice" ) == ASSET( "1.000 TBD" ) );
    validate_database();


    BOOST_TEST_MESSAGE( "--- success transferring HIVE to other" );
    op.to = "bob";
    op.amount = ASSET( "1.000 TESTS" );

    tx.clear();
    tx.operations.push_back( op );
    push_transaction( tx, alice_private_key, 0 );

    BOOST_REQUIRE( get_balance( "alice" ) == ASSET( "8.000 TESTS" ) );
    BOOST_REQUIRE( get_savings( "bob" ) == ASSET( "1.000 TESTS" ) );
    validate_database();


    BOOST_TEST_MESSAGE( "--- success transferring HBD to other" );
    op.amount = ASSET( "1.000 TBD" );

    tx.clear();
    tx.operations.push_back( op );
    push_transaction( tx, alice_private_key, 0 );

    BOOST_REQUIRE( get_hbd_balance( "alice" ) == ASSET( "8.000 TBD" ) );
    BOOST_REQUIRE( get_hbd_savings( "bob" ) == ASSET( "1.000 TBD" ) );
    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( transfer_from_savings_validate )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: transfer_from_savings_validate" );

    transfer_from_savings_operation op;
    op.from = "alice";
    op.request_id = 0;
    op.to = "alice";
    op.amount = ASSET( "1.000 TESTS" );


    BOOST_TEST_MESSAGE( "failure when 'from' is empty" );
    op.from = "";
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );


    BOOST_TEST_MESSAGE( "failure when 'to' is empty" );
    op.from = "alice";
    op.to = "";
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );


    BOOST_TEST_MESSAGE( "sucess when 'to' is not empty" );
    op.to = "bob";
    op.validate();


    BOOST_TEST_MESSAGE( "failure when amount is VESTS" );
    op.to = "alice";
    op.amount = ASSET( "1.000000 VESTS" );
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );


    BOOST_TEST_MESSAGE( "success when amount is HBD" );
    op.amount = ASSET( "1.000 TBD" );
    op.validate();


    BOOST_TEST_MESSAGE( "success when amount is HIVE" );
    op.amount = ASSET( "1.000 TESTS" );
    op.validate();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( transfer_from_savings_authorities )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: transfer_from_savings_authorities" );

    transfer_from_savings_operation op;
    op.from = "alice";
    op.to = "alice";
    op.amount = ASSET( "1.000 TESTS" );

    flat_set< account_name_type > auths;
    flat_set< account_name_type > expected;

    op.get_required_owner_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    op.get_required_posting_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    op.get_required_active_authorities( auths );
    expected.insert( "alice" );
    BOOST_REQUIRE( auths == expected );

    auths.clear();
    expected.clear();
    op.from = "bob";
    op.get_required_active_authorities( auths );
    expected.insert( "bob" );
    BOOST_REQUIRE( auths == expected );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( transfer_from_savings_apply )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: transfer_from_savings_apply" );

    ACTORS( (alice)(bob) );
    generate_block();

    fund( "alice", ASSET( "10.000 TESTS" ) );
    fund( "alice", ASSET( "10.000 TBD" ) );

    transfer_to_savings_operation save;
    save.from = "alice";
    save.to = "alice";
    save.amount = ASSET( "10.000 TESTS" );

    signed_transaction tx;
    tx.operations.push_back( save );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    push_transaction( tx, alice_private_key, 0 );

    save.amount = ASSET( "10.000 TBD" );
    tx.clear();
    tx.operations.push_back( save );
    push_transaction( tx, alice_private_key, 0 );


    BOOST_TEST_MESSAGE( "--- failure when account has insufficient funds" );
    transfer_from_savings_operation op;
    op.from = "alice";
    op.to = "bob";
    op.amount = ASSET( "20.000 TESTS" );
    op.request_id = 0;

    tx.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );


    BOOST_TEST_MESSAGE( "--- failure withdrawing to non-existant account" );
    op.to = "sam";
    op.amount = ASSET( "1.000 TESTS" );

    tx.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );

    BOOST_TEST_MESSAGE( "--- Failure withdrawing TESTS to treasury" );

    op.to = db->get_treasury_name();

    tx.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Success withdrawing TBD to treasury" );

    op.amount = ASSET( "1.000 TBD" );

    tx.clear();
    tx.operations.push_back( op );
    push_transaction( tx, alice_private_key, 0 );
    BOOST_REQUIRE( get_hbd_savings( "alice" ) == ASSET( "9.000 TBD" ) );
    validate_database();

    BOOST_TEST_MESSAGE( "--- success withdrawing HIVE to self" );
    op.to = "alice";
    op.amount = ASSET( "1.000 TESTS" );
    op.request_id++;

    tx.clear();
    tx.operations.push_back( op );
    push_transaction( tx, alice_private_key, 0 );

    BOOST_REQUIRE( get_balance( "alice" ) == ASSET( "0.000 TESTS" ) );
    BOOST_REQUIRE( get_savings( "alice" ) == ASSET( "9.000 TESTS" ) );
    BOOST_REQUIRE( db->get_account( "alice" ).savings_withdraw_requests == op.request_id + 1 );
    BOOST_REQUIRE( db->get_savings_withdraw( "alice", op.request_id ).from == op.from );
    BOOST_REQUIRE( db->get_savings_withdraw( "alice", op.request_id ).to == op.to );
    BOOST_REQUIRE( to_string( db->get_savings_withdraw( "alice", op.request_id ).memo ) == op.memo );
    BOOST_REQUIRE( db->get_savings_withdraw( "alice", op.request_id ).request_id == op.request_id );
    BOOST_REQUIRE( db->get_savings_withdraw( "alice", op.request_id ).amount == op.amount );
    BOOST_REQUIRE( db->get_savings_withdraw( "alice", op.request_id ).complete == db->head_block_time() + HIVE_SAVINGS_WITHDRAW_TIME );
    validate_database();


    BOOST_TEST_MESSAGE( "--- success withdrawing HBD to self" );
    op.amount = ASSET( "1.000 TBD" );
    op.request_id++;

    tx.clear();
    tx.operations.push_back( op );
    push_transaction( tx, alice_private_key, 0 );

    BOOST_REQUIRE( get_hbd_balance( "alice" ) == ASSET( "0.000 TBD" ) );
    BOOST_REQUIRE( get_hbd_savings( "alice" ) == ASSET( "8.000 TBD" ) );
    BOOST_REQUIRE( db->get_account( "alice" ).savings_withdraw_requests == op.request_id + 1 );
    BOOST_REQUIRE( db->get_savings_withdraw( "alice", op.request_id ).from == op.from );
    BOOST_REQUIRE( db->get_savings_withdraw( "alice", op.request_id ).to == op.to );
    BOOST_REQUIRE( to_string( db->get_savings_withdraw( "alice", op.request_id ).memo ) == op.memo );
    BOOST_REQUIRE( db->get_savings_withdraw( "alice", op.request_id ).request_id == op.request_id );
    BOOST_REQUIRE( db->get_savings_withdraw( "alice", op.request_id ).amount == op.amount );
    BOOST_REQUIRE( db->get_savings_withdraw( "alice", op.request_id ).complete == db->head_block_time() + HIVE_SAVINGS_WITHDRAW_TIME );
    validate_database();


    BOOST_TEST_MESSAGE( "--- failure withdrawing with repeat request id" );
    op.amount = ASSET( "2.000 TESTS" );

    tx.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );


    BOOST_TEST_MESSAGE( "--- success withdrawing HIVE to other" );
    op.to = "bob";
    op.amount = ASSET( "1.000 TESTS" );
    op.request_id = 3;

    tx.clear();
    tx.operations.push_back( op );
    push_transaction( tx, alice_private_key, 0 );

    BOOST_REQUIRE( get_balance( "alice" ) == ASSET( "0.000 TESTS" ) );
    BOOST_REQUIRE( get_savings( "alice" ) == ASSET( "8.000 TESTS" ) );
    BOOST_REQUIRE( db->get_account( "alice" ).savings_withdraw_requests == op.request_id + 1 );
    BOOST_REQUIRE( db->get_savings_withdraw( "alice", op.request_id ).from == op.from );
    BOOST_REQUIRE( db->get_savings_withdraw( "alice", op.request_id ).to == op.to );
    BOOST_REQUIRE( to_string( db->get_savings_withdraw( "alice", op.request_id ).memo ) == op.memo );
    BOOST_REQUIRE( db->get_savings_withdraw( "alice", op.request_id ).request_id == op.request_id );
    BOOST_REQUIRE( db->get_savings_withdraw( "alice", op.request_id ).amount == op.amount );
    BOOST_REQUIRE( db->get_savings_withdraw( "alice", op.request_id ).complete == db->head_block_time() + HIVE_SAVINGS_WITHDRAW_TIME );
    validate_database();


    BOOST_TEST_MESSAGE( "--- success withdrawing HBD to other" );
    op.amount = ASSET( "1.000 TBD" );
    op.request_id = 4;

    tx.clear();
    tx.operations.push_back( op );
    push_transaction( tx, alice_private_key, 0 );

    BOOST_REQUIRE( get_hbd_balance( "alice" ) == ASSET( "0.000 TBD" ) );
    BOOST_REQUIRE( get_hbd_savings( "alice" ) == ASSET( "7.000 TBD" ) );
    BOOST_REQUIRE( db->get_account( "alice" ).savings_withdraw_requests == op.request_id + 1 );
    BOOST_REQUIRE( db->get_savings_withdraw( "alice", op.request_id ).from == op.from );
    BOOST_REQUIRE( db->get_savings_withdraw( "alice", op.request_id ).to == op.to );
    BOOST_REQUIRE( to_string( db->get_savings_withdraw( "alice", op.request_id ).memo ) == op.memo );
    BOOST_REQUIRE( db->get_savings_withdraw( "alice", op.request_id ).request_id == op.request_id );
    BOOST_REQUIRE( db->get_savings_withdraw( "alice", op.request_id ).amount == op.amount );
    BOOST_REQUIRE( db->get_savings_withdraw( "alice", op.request_id ).complete == db->head_block_time() + HIVE_SAVINGS_WITHDRAW_TIME );
    validate_database();


    BOOST_TEST_MESSAGE( "--- withdraw on timeout" );
    generate_blocks( db->head_block_time() + HIVE_SAVINGS_WITHDRAW_TIME - fc::seconds( HIVE_BLOCK_INTERVAL ), true );

    BOOST_REQUIRE( get_balance( "alice" ) == ASSET( "0.000 TESTS" ) );
    BOOST_REQUIRE( get_hbd_balance( "alice" ) == ASSET( "0.000 TBD" ) );
    BOOST_REQUIRE( get_balance( "bob" ) == ASSET( "0.000 TESTS" ) );
    BOOST_REQUIRE( get_hbd_balance( "bob" ) == ASSET( "0.000 TBD" ) );
    BOOST_REQUIRE( db->get_account( "alice" ).savings_withdraw_requests == op.request_id + 1 );
    validate_database();

    generate_block();

    BOOST_REQUIRE( get_balance( "alice" ) == ASSET( "1.000 TESTS" ) );
    BOOST_REQUIRE( get_hbd_balance( "alice" ) == ASSET( "1.000 TBD" ) );
    BOOST_REQUIRE( get_balance( "bob" ) == ASSET( "1.000 TESTS" ) );
    BOOST_REQUIRE( get_hbd_balance( "bob" ) == ASSET( "1.000 TBD" ) );
    BOOST_REQUIRE( db->get_account( "alice" ).savings_withdraw_requests == 0 );
    validate_database();


    BOOST_TEST_MESSAGE( "--- savings withdraw request limit" );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    op.to = "alice";
    op.amount = ASSET( "0.001 TESTS" );

    for( int i = 0; i < HIVE_SAVINGS_WITHDRAW_REQUEST_LIMIT; i++ )
    {
      op.request_id = i;
      tx.clear();
      tx.operations.push_back( op );
      push_transaction( tx, alice_private_key, 0 );
      BOOST_REQUIRE( db->get_account( "alice" ).savings_withdraw_requests == i + 1 );
    }

    op.request_id = HIVE_SAVINGS_WITHDRAW_REQUEST_LIMIT;
    tx.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );
    BOOST_REQUIRE( db->get_account( "alice" ).savings_withdraw_requests == HIVE_SAVINGS_WITHDRAW_REQUEST_LIMIT );
    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( cancel_transfer_from_savings_validate )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: cancel_transfer_from_savings_validate" );

    cancel_transfer_from_savings_operation op;
    op.from = "alice";
    op.request_id = 0;


    BOOST_TEST_MESSAGE( "--- failure when 'from' is empty" );
    op.from = "";
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );


    BOOST_TEST_MESSAGE( "--- sucess when 'from' is not empty" );
    op.from = "alice";
    op.validate();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( cancel_transfer_from_savings_authorities )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: cancel_transfer_from_savings_authorities" );

    cancel_transfer_from_savings_operation op;
    op.from = "alice";

    flat_set< account_name_type > auths;
    flat_set< account_name_type > expected;

    op.get_required_owner_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    op.get_required_posting_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    op.get_required_active_authorities( auths );
    expected.insert( "alice" );
    BOOST_REQUIRE( auths == expected );

    auths.clear();
    expected.clear();
    op.from = "bob";
    op.get_required_active_authorities( auths );
    expected.insert( "bob" );
    BOOST_REQUIRE( auths == expected );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( cancel_transfer_from_savings_apply )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: cancel_transfer_from_savings_apply" );

    ACTORS( (alice)(bob) )
    generate_block();

    fund( "alice", ASSET( "10.000 TESTS" ) );

    transfer_to_savings_operation save;
    save.from = "alice";
    save.to = "alice";
    save.amount = ASSET( "10.000 TESTS" );

    transfer_from_savings_operation withdraw;
    withdraw.from = "alice";
    withdraw.to = "bob";
    withdraw.request_id = 1;
    withdraw.amount = ASSET( "3.000 TESTS" );

    signed_transaction tx;
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.operations.push_back( save );
    tx.operations.push_back( withdraw );
    push_transaction( tx, alice_private_key, 0 );
    validate_database();

    BOOST_REQUIRE( db->get_account( "alice" ).savings_withdraw_requests == 1 );
    BOOST_REQUIRE( db->get_account( "bob" ).savings_withdraw_requests == 0 );


    BOOST_TEST_MESSAGE( "--- Failure when there is no pending request" );
    cancel_transfer_from_savings_operation op;
    op.from = "alice";
    op.request_id = 0;

    tx.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );
    validate_database();

    BOOST_REQUIRE( db->get_account( "alice" ).savings_withdraw_requests == 1 );
    BOOST_REQUIRE( db->get_account( "bob" ).savings_withdraw_requests == 0 );


    BOOST_TEST_MESSAGE( "--- Success" );
    op.request_id = 1;

    tx.clear();
    tx.operations.push_back( op );
    push_transaction( tx, alice_private_key, 0 );

    BOOST_REQUIRE( get_balance( "alice" ) == ASSET( "0.000 TESTS" ) );
    BOOST_REQUIRE( get_savings( "alice" ) == ASSET( "10.000 TESTS" ) );
    BOOST_REQUIRE( db->get_account( "alice" ).savings_withdraw_requests == 0 );
    BOOST_REQUIRE( get_balance( "bob" ) == ASSET( "0.000 TESTS" ) );
    BOOST_REQUIRE( get_savings( "bob" ) == ASSET( "0.000 TESTS" ) );
    BOOST_REQUIRE( db->get_account( "bob" ).savings_withdraw_requests == 0 );
    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( decline_voting_rights_authorities )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: decline_voting_rights_authorities" );

    decline_voting_rights_operation op;
    op.account = "alice";

    flat_set< account_name_type > auths;
    flat_set< account_name_type > expected;

    op.get_required_active_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    op.get_required_posting_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    expected.insert( "alice" );
    op.get_required_owner_authorities( auths );
    BOOST_REQUIRE( auths == expected );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( decline_voting_rights_apply )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: decline_voting_rights_apply" );

    ACTORS( (alice)(bob) );
    generate_block();
    vest( HIVE_INIT_MINER_NAME, "alice", ASSET( "10.000 TESTS" ) );
    vest( HIVE_INIT_MINER_NAME, "bob", ASSET( "10.000 TESTS" ) );
    generate_block();

    account_witness_proxy_operation proxy;
    proxy.account = "bob";
    proxy.proxy = "alice";

    signed_transaction tx;
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.operations.push_back( proxy );
    push_transaction( tx, bob_private_key, 0 );


    decline_voting_rights_operation op;
    op.account = "alice";


    BOOST_TEST_MESSAGE( "--- success" );
    tx.clear();
    tx.operations.push_back( op );
    push_transaction( tx, alice_private_key, 0 );

    const auto& request_idx = db->get_index< decline_voting_rights_request_index >().indices().get< by_account >();
    auto itr = request_idx.find( db->get_account( "alice" ).name );
    BOOST_REQUIRE( itr != request_idx.end() );
    BOOST_REQUIRE( itr->effective_date == db->head_block_time() + HIVE_OWNER_AUTH_RECOVERY_PERIOD );


    BOOST_TEST_MESSAGE( "--- failure revoking voting rights with existing request" );
    generate_block();
    tx.clear();
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );


    BOOST_TEST_MESSAGE( "--- successs cancelling a request" );
    op.decline = false;
    tx.clear();
    tx.operations.push_back( op );
    push_transaction( tx, alice_private_key, 0 );

    itr = request_idx.find( db->get_account( "alice" ).name );
    BOOST_REQUIRE( itr == request_idx.end() );


    BOOST_TEST_MESSAGE( "--- failure cancelling a request that doesn't exist" );
    generate_block();
    tx.clear();
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );


    BOOST_TEST_MESSAGE( "--- check account can vote during waiting period" );
    op.decline = true;
    tx.clear();
    tx.operations.push_back( op );
    push_transaction( tx, alice_private_key, 0 );

    generate_blocks( db->head_block_time() + HIVE_OWNER_AUTH_RECOVERY_PERIOD - fc::seconds( HIVE_BLOCK_INTERVAL ), true );
    BOOST_REQUIRE( db->get_account( "alice" ).can_vote );
    witness_create( "alice", alice_private_key, "foo.bar", alice_private_key.get_public_key(), 0 );

    account_witness_vote_operation witness_vote;
    witness_vote.account = "alice";
    witness_vote.witness = "alice";
    tx.clear();
    tx.operations.push_back( witness_vote );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    push_transaction( tx, alice_private_key, 0 );

    comment_operation comment;
    comment.author = "alice";
    comment.permlink = "test";
    comment.parent_permlink = "test";
    comment.title = "test";
    comment.body = "test";
    vote_operation vote;
    vote.voter = "alice";
    vote.author = "alice";
    vote.permlink = "test";
    vote.weight = HIVE_100_PERCENT;
    tx.clear();
    tx.operations.push_back( comment );
    tx.operations.push_back( vote );
    push_transaction( tx, alice_private_key, 0 );
    validate_database();


    BOOST_TEST_MESSAGE( "--- check account cannot vote after request is processed" );
    generate_block();
    BOOST_REQUIRE( !db->get_account( "alice" ).can_vote );
    validate_database();

    itr = request_idx.find( db->get_account( "alice" ).name );
    BOOST_REQUIRE( itr == request_idx.end() );

    const auto& witness_idx = db->get_index< witness_vote_index >().indices().get< by_account_witness >();
    auto witness_itr = witness_idx.find( boost::make_tuple( db->get_account( "alice" ).name, db->get_witness( "alice" ).owner ) );
    BOOST_REQUIRE( witness_itr == witness_idx.end() );

    tx.clear();
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.operations.push_back( witness_vote );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );

    db->get< comment_vote_object, by_comment_voter >( boost::make_tuple( db->get_comment( "alice", string( "test" ) ).get_id(), get_account_id( "alice" ) ) );

    vote.weight = 0;
    tx.clear();
    tx.operations.push_back( vote );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );

    vote.weight = HIVE_1_PERCENT * 50;
    tx.clear();
    tx.operations.push_back( vote );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );

    proxy.account = "alice";
    proxy.proxy = "bob";
    tx.clear();
    tx.operations.push_back( proxy );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( claim_reward_balance_validate )
{
  try
  {
    claim_reward_balance_operation op;
    op.account = "alice";
    op.reward_hive = ASSET( "0.000 TESTS" );
    op.reward_hbd = ASSET( "0.000 TBD" );
    op.reward_vests = ASSET( "0.000000 VESTS" );


    BOOST_TEST_MESSAGE( "Testing all 0 amounts" );
    HIVE_REQUIRE_THROW( op.validate(), fc::assert_exception );


    BOOST_TEST_MESSAGE( "Testing single reward claims" );
    op.reward_hive.amount = 1000;
    op.validate();

    op.reward_hive.amount = 0;
    op.reward_hbd.amount = 1000;
    op.validate();

    op.reward_hbd.amount = 0;
    op.reward_vests.amount = 1000;
    op.validate();

    op.reward_vests.amount = 0;


    BOOST_TEST_MESSAGE( "Testing wrong HIVE symbol" );
    op.reward_hive = ASSET( "1.000 TBD" );
    HIVE_REQUIRE_THROW( op.validate(), fc::assert_exception );


    BOOST_TEST_MESSAGE( "Testing wrong HBD symbol" );
    op.reward_hive = ASSET( "1.000 TESTS" );
    op.reward_hbd = ASSET( "1.000 TESTS" );
    HIVE_REQUIRE_THROW( op.validate(), fc::assert_exception );


    BOOST_TEST_MESSAGE( "Testing wrong VESTS symbol" );
    op.reward_hbd = ASSET( "1.000 TBD" );
    op.reward_vests = ASSET( "1.000 TESTS" );
    HIVE_REQUIRE_THROW( op.validate(), fc::assert_exception );


    BOOST_TEST_MESSAGE( "Testing a single negative amount" );
    op.reward_hive.amount = 1000;
    op.reward_hbd.amount = -1000;
    HIVE_REQUIRE_THROW( op.validate(), fc::assert_exception );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( claim_reward_balance_authorities )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: decline_voting_rights_authorities" );

    claim_reward_balance_operation op;
    op.account = "alice";

    flat_set< account_name_type > auths;
    flat_set< account_name_type > expected;

    op.get_required_owner_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    op.get_required_active_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    expected.insert( "alice" );
    op.get_required_posting_authorities( auths );
    BOOST_REQUIRE( auths == expected );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( account_create_with_delegation_authorities )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: account_create_with_delegation_authorities" );

    account_create_with_delegation_operation op;
    op.creator = "alice";

    flat_set< account_name_type > auths;
    flat_set< account_name_type > expected;

    op.get_required_owner_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    expected.insert( "alice" );
    op.get_required_active_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    expected.clear();
    auths.clear();
    op.get_required_posting_authorities( auths );
    BOOST_REQUIRE( auths == expected );
  }
  FC_LOG_AND_RETHROW()

}

BOOST_AUTO_TEST_CASE( account_create_with_delegation_apply )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: account_create_with_delegation_apply" );
    signed_transaction tx;
    ACTORS( (alice) );
    // 150 * fee = ( 5 * HIVE ) + SP
    //auto& gpo = db->get_dynamic_global_properties();
    generate_blocks(1);
    fund( "alice", ASSET("1510.000 TESTS") );
    vest( HIVE_INIT_MINER_NAME, "alice", ASSET("1000.000 TESTS") );

    private_key_type priv_key = generate_private_key( "temp_key" );

    generate_block();

    db_plugin->debug_update( [=]( database& db )
    {
      db.modify( db.get_witness_schedule_object(), [&]( witness_schedule_object& w )
      {
        w.median_props.account_creation_fee = ASSET( "1.000 TESTS" );
      });
    });

    generate_block();

    // This test passed pre HF20
    BOOST_TEST_MESSAGE( "--- Test deprecation. " );
    account_create_with_delegation_operation op;
    op.fee = ASSET( "10.000 TESTS" );
    op.delegation = ASSET( "100000000.000000 VESTS" );
    op.creator = "alice";
    op.new_account_name = "bob";
    op.owner = authority( 1, priv_key.get_public_key(), 1 );
    op.active = authority( 2, priv_key.get_public_key(), 2 );
    op.memo_key = priv_key.get_public_key();
    op.json_metadata = "{\"foo\":\"bar\"}";
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::assert_exception );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( claim_reward_balance_apply )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: claim_reward_balance_apply" );
    BOOST_TEST_MESSAGE( "--- Setting up test state" );

    ACTORS( (alice) )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );

    db_plugin->debug_update( []( database& db )
    {
      db.modify( db.get_account( "alice" ), []( account_object& a )
      {
        a.reward_hive_balance = ASSET( "10.000 TESTS" );
        a.reward_hbd_balance = ASSET( "10.000 TBD" );
        a.reward_vesting_balance = ASSET( "10.000000 VESTS" );
        a.reward_vesting_hive = ASSET( "10.000 TESTS" );
      });

      db.modify( db.get_dynamic_global_properties(), []( dynamic_global_property_object& gpo )
      {
        gpo.current_supply += ASSET( "20.000 TESTS" );
        gpo.current_hbd_supply += ASSET( "10.000 TBD" );
        gpo.virtual_supply += ASSET( "20.000 TESTS" );
        gpo.pending_rewarded_vesting_shares += ASSET( "10.000000 VESTS" );
        gpo.pending_rewarded_vesting_hive += ASSET( "10.000 TESTS" );
      });
    });

    generate_block();
    validate_database();

    auto alice_hive = get_balance( "alice" );
    auto alice_hbd = get_hbd_balance( "alice" );
    auto alice_vests = get_vesting( "alice" );


    BOOST_TEST_MESSAGE( "--- Attempting to claim more HIVE than exists in the reward balance." );

    claim_reward_balance_operation op;
    signed_transaction tx;

    op.account = "alice";
    op.reward_hive = ASSET( "20.000 TESTS" );
    op.reward_hbd = ASSET( "0.000 TBD" );
    op.reward_vests = ASSET( "0.000000 VESTS" );

    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::assert_exception );


    BOOST_TEST_MESSAGE( "--- Claiming a partial reward balance" );

    op.reward_hive = ASSET( "0.000 TESTS" );
    op.reward_vests = ASSET( "5.000000 VESTS" );
    tx.clear();
    tx.operations.push_back( op );
    push_transaction( tx, alice_private_key, 0 );

    BOOST_REQUIRE( get_balance( "alice" ) == alice_hive + op.reward_hive );
    BOOST_REQUIRE( get_rewards( "alice" ) == ASSET( "10.000 TESTS" ) );
    BOOST_REQUIRE( get_hbd_balance( "alice" ) == alice_hbd + op.reward_hbd );
    BOOST_REQUIRE( get_hbd_rewards( "alice" ) == ASSET( "10.000 TBD" ) );
    BOOST_REQUIRE( get_vesting( "alice" ) == alice_vests + op.reward_vests );
    BOOST_REQUIRE( get_vest_rewards( "alice" ) == ASSET( "5.000000 VESTS" ) );
    BOOST_REQUIRE( get_vest_rewards_as_hive( "alice" ) == ASSET( "5.000 TESTS" ) );
    validate_database();

    alice_vests += op.reward_vests;


    BOOST_TEST_MESSAGE( "--- Claiming the full reward balance" );

    op.reward_hive = ASSET( "10.000 TESTS" );
    op.reward_hbd = ASSET( "10.000 TBD" );
    tx.clear();
    tx.operations.push_back( op );
    push_transaction( tx, alice_private_key, 0 );

    BOOST_REQUIRE( get_balance( "alice" ) == alice_hive + op.reward_hive );
    BOOST_REQUIRE( get_rewards( "alice" ) == ASSET( "0.000 TESTS" ) );
    BOOST_REQUIRE( get_hbd_balance( "alice" ) == alice_hbd + op.reward_hbd );
    BOOST_REQUIRE( get_hbd_rewards( "alice" ) == ASSET( "0.000 TBD" ) );
    BOOST_REQUIRE( get_vesting( "alice" ) == alice_vests + op.reward_vests );
    BOOST_REQUIRE( get_vest_rewards( "alice" ) == ASSET( "0.000000 VESTS" ) );
    BOOST_REQUIRE( get_vest_rewards_as_hive( "alice" ) == ASSET( "0.000 TESTS" ) );
        validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( delegate_vesting_shares_validate )
{
  try
  {
    delegate_vesting_shares_operation op;

    op.delegator = "alice";
    op.delegatee = "bob";
    op.vesting_shares = asset( -1, VESTS_SYMBOL );
    HIVE_REQUIRE_THROW( op.validate(), fc::assert_exception );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( delegate_vesting_shares_authorities )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: delegate_vesting_shares_authorities" );
    signed_transaction tx;
    ACTORS( (alice)(bob) )
    vest( HIVE_INIT_MINER_NAME, "alice", ASSET( "10000.000 TESTS" ) );

    delegate_vesting_shares_operation op;
    op.vesting_shares = ASSET( "300.000000 VESTS");
    op.delegator = "alice";
    op.delegatee = "bob";

    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.operations.push_back( op );

    BOOST_TEST_MESSAGE( "--- Test failure when no signatures" );
    HIVE_REQUIRE_THROW( push_transaction( tx, 0 ), tx_missing_active_auth );

    BOOST_TEST_MESSAGE( "--- Test success with witness signature" );
    push_transaction( tx, alice_private_key, 0 );

    BOOST_TEST_MESSAGE( "--- Test failure when duplicate signatures" );
    tx.operations.clear();
    tx.signatures.clear();
    op.delegatee = "sam";
    tx.operations.push_back( op );
    sign( tx, alice_private_key );
    sign( tx, alice_private_key );
    HIVE_REQUIRE_THROW( push_transaction( tx, 0 ), tx_duplicate_sig );

    BOOST_TEST_MESSAGE( "--- Test failure when signed by an additional signature not in the creator's authority" );
    tx.signatures.clear();
    sign( tx, init_account_priv_key );
    sign( tx, alice_private_key );
    HIVE_REQUIRE_THROW( push_transaction( tx, 0 ), tx_irrelevant_sig );

    BOOST_TEST_MESSAGE( "--- Test failure when signed by a signature not in the creator's authority" );
    tx.signatures.clear();
    HIVE_REQUIRE_THROW( push_transaction( tx, init_account_priv_key, 0 ), tx_missing_active_auth );
    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( delegate_vesting_shares_apply )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: delegate_vesting_shares_apply" );
    signed_transaction tx;
    ACTORS( (alice)(bob)(charlie) )
    generate_block();

    vest( HIVE_INIT_MINER_NAME, "alice", ASSET( "1000.000 TESTS" ) );

    generate_block();

    db_plugin->debug_update( [=]( database& db )
    {
      db.modify( db.get_witness_schedule_object(), [&]( witness_schedule_object& w )
      {
        w.median_props.account_creation_fee = ASSET( "1.000 TESTS" );
      });
    });

    generate_block();

    delegate_vesting_shares_operation op;
    op.vesting_shares = ASSET( "10000000.000000 VESTS");
    op.delegator = "alice";
    op.delegatee = "bob";

    util::manabar old_manabar = VOTING_MANABAR( "alice" );
    util::manabar_params params( util::get_effective_vesting_shares( db->get_account( "alice" ) ), HIVE_VOTING_MANA_REGENERATION_SECONDS );
    old_manabar.regenerate_mana( params, db->head_block_time() );

    util::manabar old_downvote_manabar = DOWNVOTE_MANABAR( "alice" );
    params.max_mana = util::get_effective_vesting_shares( db->get_account( "alice" ) ) / 4;
    old_downvote_manabar.regenerate_mana( params, db->head_block_time() );

    util::manabar old_bob_manabar = VOTING_MANABAR( "bob" );
    params.max_mana = util::get_effective_vesting_shares( db->get_account( "bob" ) );
    old_bob_manabar.regenerate_mana( params, db->head_block_time() );

    util::manabar old_bob_downvote_manabar = DOWNVOTE_MANABAR( "bob" );
    params.max_mana = util::get_effective_vesting_shares( db->get_account( "bob" ) ) / 4;
    old_bob_downvote_manabar.regenerate_mana( params, db->head_block_time() );

    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.operations.push_back( op );
    push_transaction( tx, alice_private_key, 0 );
    generate_blocks( 1 );
    const account_object& alice_acc = db->get_account( "alice" );
    const account_object& bob_acc = db->get_account( "bob" );

    BOOST_REQUIRE( alice_acc.get_delegated_vesting() == ASSET( "10000000.000000 VESTS" ) );
    BOOST_REQUIRE( alice_acc.voting_manabar.current_mana == old_manabar.current_mana - op.vesting_shares.amount.value );
    BOOST_REQUIRE( alice_acc.downvote_manabar.current_mana == old_downvote_manabar.current_mana - op.vesting_shares.amount.value / 4 );
    BOOST_REQUIRE( bob_acc.get_received_vesting() == ASSET( "10000000.000000 VESTS" ) );
    BOOST_REQUIRE( bob_acc.voting_manabar.current_mana == old_bob_manabar.current_mana + op.vesting_shares.amount.value );
    BOOST_REQUIRE( bob_acc.downvote_manabar.current_mana == old_bob_downvote_manabar.current_mana + op.vesting_shares.amount.value / 4 );

    BOOST_TEST_MESSAGE( "--- Test that the delegation object is correct. " );
    auto delegation = db->find< vesting_delegation_object, by_delegation >( boost::make_tuple( alice_acc.get_id(), bob_acc.get_id() ) );


    BOOST_REQUIRE( delegation != nullptr );
    BOOST_REQUIRE( delegation->get_delegator() == alice_acc.get_id() );
    BOOST_REQUIRE( delegation->get_vesting() == ASSET( "10000000.000000 VESTS"));

    old_manabar = VOTING_MANABAR( "alice" );
    params.max_mana = util::get_effective_vesting_shares( db->get_account( "alice" ) );
    old_manabar.regenerate_mana( params, db->head_block_time() );

    old_downvote_manabar = DOWNVOTE_MANABAR( "alice" );
    params.max_mana = util::get_effective_vesting_shares( db->get_account( "alice" ) ) / 4;
    old_downvote_manabar.regenerate_mana( params, db->head_block_time() );

    old_bob_manabar = VOTING_MANABAR( "bob" );
    params.max_mana = util::get_effective_vesting_shares( db->get_account( "bob" ) );
    old_bob_manabar.regenerate_mana( params, db->head_block_time() );

    old_bob_downvote_manabar = DOWNVOTE_MANABAR( "bob" );
    params.max_mana = util::get_effective_vesting_shares( db->get_account( "bob" ) ) / 4;
    old_bob_downvote_manabar.regenerate_mana( params, db->head_block_time() );

    int64_t delta = 10000000000000;

    validate_database();
    tx.clear();
    op.vesting_shares.amount += delta;
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.operations.push_back( op );
    push_transaction( tx, alice_private_key, 0 );
    generate_blocks(1);

    idump( (alice_acc.voting_manabar)(old_manabar)(delta) );

    BOOST_REQUIRE( delegation != nullptr );
    BOOST_REQUIRE( delegation->get_delegator() == alice_acc.get_id() );
    BOOST_REQUIRE( delegation->get_vesting() == ASSET( "20000000.000000 VESTS"));
    BOOST_REQUIRE( alice_acc.get_delegated_vesting() == ASSET( "20000000.000000 VESTS"));
    BOOST_REQUIRE( alice_acc.voting_manabar.current_mana == old_manabar.current_mana - delta );
    BOOST_REQUIRE( alice_acc.downvote_manabar.current_mana == old_downvote_manabar.current_mana - delta / 4 );
    BOOST_REQUIRE( bob_acc.get_received_vesting() == ASSET( "20000000.000000 VESTS"));
    BOOST_REQUIRE( bob_acc.voting_manabar.current_mana == old_bob_manabar.current_mana + delta );
    BOOST_REQUIRE( bob_acc.downvote_manabar.current_mana == old_bob_downvote_manabar.current_mana + delta / 4 );

    BOOST_TEST_MESSAGE( "--- Test failure delegating delgated VESTS." );

    op.delegator = "bob";
    op.delegatee = "charlie";
    tx.clear();
    tx.operations.push_back( op );
    BOOST_REQUIRE_THROW( push_transaction( tx, bob_private_key, 0 ), fc::exception );


    BOOST_TEST_MESSAGE( "--- Test that effective vesting shares is accurate and being applied." );
    tx.operations.clear();
    tx.signatures.clear();

    old_manabar = VOTING_MANABAR( "bob" );
    params.max_mana = util::get_effective_vesting_shares( db->get_account( "bob" ) );
    old_manabar.regenerate_mana( params, db->head_block_time() );

    comment_operation comment_op;
    comment_op.author = "alice";
    comment_op.permlink = "foo";
    comment_op.parent_permlink = "test";
    comment_op.title = "bar";
    comment_op.body = "foo bar";
    tx.operations.push_back( comment_op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    push_transaction( tx, alice_private_key, 0 );
    tx.signatures.clear();
    tx.operations.clear();
    vote_operation vote_op;
    vote_op.voter = "bob";
    vote_op.author = "alice";
    vote_op.permlink = "foo";
    vote_op.weight = HIVE_100_PERCENT;
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.operations.push_back( vote_op );

    push_transaction( tx, bob_private_key, 0 );
    generate_blocks(1);

    const auto& vote_idx = db->get_index< comment_vote_index >().indices().get< by_comment_voter >();

    auto& alice_comment = db->get_comment( "alice", string( "foo" ) );
    const comment_cashout_object* alice_comment_cashout = db->find_comment_cashout( alice_comment );
    auto itr = vote_idx.find( boost::make_tuple( alice_comment.get_id(), bob_acc.get_id() ) );
    BOOST_REQUIRE( alice_comment_cashout->get_net_rshares() == old_manabar.current_mana - db->get_account( "bob" ).voting_manabar.current_mana - HIVE_VOTE_DUST_THRESHOLD );
    BOOST_REQUIRE( itr->get_rshares() == old_manabar.current_mana - db->get_account( "bob" ).voting_manabar.current_mana - HIVE_VOTE_DUST_THRESHOLD );

    generate_block();
    ACTORS( (sam)(dave) )
    generate_block();

    const account_object& sam_acc = db->get_account( "sam" );
    const account_object& dave_acc = db->get_account( "dave" );

    vest( HIVE_INIT_MINER_NAME, "sam", ASSET( "1000.000 TESTS" ) );

    generate_block();

    auto sam_vest = get_vesting( "sam" );

    BOOST_TEST_MESSAGE( "--- Test failure when delegating 0 VESTS" );
    tx.clear();
    op.delegator = "sam";
    op.delegatee = "dave";
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    HIVE_REQUIRE_THROW( push_transaction( tx, sam_private_key, 0 ), fc::assert_exception );


    BOOST_TEST_MESSAGE( "--- Testing failure delegating more vesting shares than account has." );
    tx.clear();
    op.vesting_shares = asset( sam_vest.amount + 1, VESTS_SYMBOL );
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, sam_private_key, 0 ), fc::assert_exception );

    BOOST_TEST_MESSAGE( "--- Testing failure delegating when there is not enough mana" );

    generate_block();
    db_plugin->debug_update( [=]( database& db )
    {
      db.modify( db.get_account( "sam" ), [&]( account_object& a )
      {
        a.voting_manabar.current_mana = a.downvote_manabar.current_mana * 3 / 4;
        a.voting_manabar.last_update_time = db.head_block_time().sec_since_epoch();
      });
    });

    op.vesting_shares = sam_vest;
    tx.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, sam_private_key, 0 ), fc::assert_exception );

    BOOST_TEST_MESSAGE( "--- Testing failure delegating when there is not enough downvote mana" );

    generate_block();
    db_plugin->debug_update( [=]( database& db )
    {
      db.modify( db.get_account( "sam" ), [&]( account_object& a )
      {
        a.voting_manabar.current_mana = util::get_effective_vesting_shares( a );
        a.voting_manabar.last_update_time = db.head_block_time().sec_since_epoch();
        a.downvote_manabar.current_mana = a.downvote_manabar.current_mana * 3 / 4;
        a.downvote_manabar.last_update_time = db.head_block_time().sec_since_epoch();
      });
    });

    tx.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, sam_private_key, 0 ), fc::assert_exception );

    BOOST_TEST_MESSAGE( "--- Test failure delegating vesting shares that are part of a power down" );
    generate_block();
    db_plugin->debug_update( [=]( database& db )
    {
      db.modify( db.get_account( "sam" ), [&]( account_object& a )
      {
        a.downvote_manabar.current_mana = util::get_effective_vesting_shares( a ) / 4;
        a.downvote_manabar.last_update_time = db.head_block_time().sec_since_epoch();
      });
    });

    tx.clear();
    sam_vest = asset( sam_vest.amount / 2, VESTS_SYMBOL );
    withdraw_vesting_operation withdraw;
    withdraw.account = "sam";
    withdraw.vesting_shares = sam_vest;
    tx.operations.push_back( withdraw );
    push_transaction( tx, sam_private_key, 0 );

    tx.clear();
    op.vesting_shares = asset( sam_vest.amount + 2, VESTS_SYMBOL );
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, sam_private_key, 0 ), fc::assert_exception );

    tx.clear();
    withdraw.vesting_shares = ASSET( "0.000000 VESTS" );
    tx.operations.push_back( withdraw );
    push_transaction( tx, sam_private_key, 0 );


    BOOST_TEST_MESSAGE( "--- Test failure powering down vesting shares that are delegated" );
    sam_vest.amount += 1000;
    op.vesting_shares = sam_vest;
    tx.clear();
    tx.operations.push_back( op );
    push_transaction( tx, sam_private_key, 0 );

    tx.clear();
    withdraw.vesting_shares = asset( sam_vest.amount, VESTS_SYMBOL );
    tx.operations.push_back( withdraw );
    HIVE_REQUIRE_THROW( push_transaction( tx, sam_private_key, 0 ), fc::assert_exception );


    BOOST_TEST_MESSAGE( "--- Remove a delegation and ensure it is returned after 1 week" );

    util::manabar old_sam_manabar = VOTING_MANABAR( "sam" );
    util::manabar old_sam_downvote_manabar = DOWNVOTE_MANABAR( "sam" );
    util::manabar old_dave_manabar = VOTING_MANABAR( "dave" );
    util::manabar old_dave_downvote_manabar = DOWNVOTE_MANABAR( "dave" );

    util::manabar_params sam_params( util::get_effective_vesting_shares( db->get_account( "sam" ) ), HIVE_VOTING_MANA_REGENERATION_SECONDS );
    util::manabar_params dave_params( util::get_effective_vesting_shares( db->get_account( "dave" ) ), HIVE_VOTING_MANA_REGENERATION_SECONDS );

    tx.clear();
    op.vesting_shares = ASSET( "0.000000 VESTS" );
    tx.operations.push_back( op );
    push_transaction( tx, sam_private_key, 0 );

    auto exp_obj = db->get_index< vesting_delegation_expiration_index, by_id >().begin();
    auto end = db->get_index< vesting_delegation_expiration_index, by_id >().end();
    auto& gpo = db->get_dynamic_global_properties();

    BOOST_REQUIRE( gpo.delegation_return_period == HIVE_DELEGATION_RETURN_PERIOD_HF20 );

    BOOST_REQUIRE( exp_obj != end );
    BOOST_REQUIRE( exp_obj->get_delegator() == sam_id );
    BOOST_REQUIRE( exp_obj->get_vesting() == sam_vest );
    BOOST_REQUIRE( exp_obj->get_expiration_time() == db->head_block_time() + gpo.delegation_return_period );
    BOOST_REQUIRE( db->get_account( "sam" ).get_delegated_vesting() == sam_vest );
    BOOST_REQUIRE( db->get_account( "dave" ).get_received_vesting() == ASSET( "0.000000 VESTS" ) );
    delegation = db->find< vesting_delegation_object, by_delegation >( boost::make_tuple( sam_acc.get_id(), dave_acc.get_id() ) );
    BOOST_REQUIRE( delegation == nullptr );

    old_sam_manabar.regenerate_mana( sam_params, db->head_block_time() );
    sam_params.max_mana /= 4;
    old_sam_downvote_manabar.regenerate_mana( sam_params, db->head_block_time() );

    old_dave_manabar.regenerate_mana( dave_params, db->head_block_time() );
    dave_params.max_mana /= 4;
    old_dave_downvote_manabar.regenerate_mana( dave_params, db->head_block_time() );

    BOOST_REQUIRE( VOTING_MANABAR( "sam" ).current_mana == old_sam_manabar.current_mana );
    BOOST_REQUIRE( DOWNVOTE_MANABAR( "sam" ).current_mana == old_sam_downvote_manabar.current_mana );
    BOOST_REQUIRE( VOTING_MANABAR( "dave" ).current_mana == old_dave_manabar.current_mana - sam_vest.amount.value );
    BOOST_REQUIRE( DOWNVOTE_MANABAR( "dave" ).current_mana == old_dave_downvote_manabar.current_mana - sam_vest.amount.value / 4 );

    old_sam_manabar = VOTING_MANABAR( "sam" );
    old_sam_downvote_manabar = DOWNVOTE_MANABAR( "sam" );
    old_dave_manabar = VOTING_MANABAR( "dave" );
    old_dave_downvote_manabar = DOWNVOTE_MANABAR( "dave" );

    sam_params.max_mana = util::get_effective_vesting_shares( db->get_account( "sam" ) );
    dave_params.max_mana = util::get_effective_vesting_shares( db->get_account( "dave" ) );

    generate_blocks( exp_obj->get_expiration_time() + HIVE_BLOCK_INTERVAL );

    old_sam_manabar.regenerate_mana( sam_params, db->head_block_time() );
    sam_params.max_mana /= 4;
    old_sam_downvote_manabar.regenerate_mana( sam_params, db->head_block_time() );

    old_dave_manabar.regenerate_mana( dave_params, db->head_block_time() );
    dave_params.max_mana /= 4;
    old_dave_downvote_manabar.regenerate_mana( dave_params, db->head_block_time() );

    exp_obj = db->get_index< vesting_delegation_expiration_index, by_id >().begin();
    end = db->get_index< vesting_delegation_expiration_index, by_id >().end();

    BOOST_REQUIRE( exp_obj == end );
    BOOST_REQUIRE( db->get_account( "sam" ).get_delegated_vesting() == ASSET( "0.000000 VESTS" ) );
    BOOST_REQUIRE( VOTING_MANABAR( "sam" ).current_mana == old_sam_manabar.current_mana + sam_vest.amount.value );
    BOOST_REQUIRE( DOWNVOTE_MANABAR( "sam" ).current_mana == old_sam_downvote_manabar.current_mana + sam_vest.amount.value / 4 );
    BOOST_REQUIRE( VOTING_MANABAR( "dave" ).current_mana == old_dave_manabar.current_mana );
    BOOST_REQUIRE( DOWNVOTE_MANABAR( "dave" ).current_mana == old_dave_downvote_manabar.current_mana );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( issue_971_vesting_removal )
{
  // This is a regression test specifically for issue #971
  try
  {
    BOOST_TEST_MESSAGE( "Test Issue 971 Vesting Removal" );
    ACTORS( (alice)(bob) )
    generate_block();

    vest( HIVE_INIT_MINER_NAME, "alice", ASSET( "1000.000 TESTS" ) );

    generate_block();

    db_plugin->debug_update( [=]( database& db )
    {
      db.modify( db.get_witness_schedule_object(), [&]( witness_schedule_object& w )
      {
        w.median_props.account_creation_fee = ASSET( "1.000 TESTS" );
      });
    });

    generate_block();

    signed_transaction tx;
    delegate_vesting_shares_operation op;
    op.vesting_shares = ASSET( "10000000.000000 VESTS");
    op.delegator = "alice";
    op.delegatee = "bob";

    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.operations.push_back( op );
    push_transaction( tx, alice_private_key, 0 );
    generate_block();
    const account_object& alice_acc = db->get_account( "alice" );
    const account_object& bob_acc = db->get_account( "bob" );

    BOOST_REQUIRE( alice_acc.get_delegated_vesting() == ASSET( "10000000.000000 VESTS"));
    BOOST_REQUIRE( bob_acc.get_received_vesting() == ASSET( "10000000.000000 VESTS"));

    generate_block();

    db_plugin->debug_update( [=]( database& db )
    {
      db.modify( db.get_witness_schedule_object(), [&]( witness_schedule_object& w )
      {
        w.median_props.account_creation_fee = ASSET( "100.000 TESTS" );
      });
    });

    generate_block();

    op.vesting_shares = ASSET( "0.000000 VESTS" );

    tx.clear();
    tx.operations.push_back( op );
    push_transaction( tx, alice_private_key, 0 );
    generate_block();

    BOOST_REQUIRE( alice_acc.get_delegated_vesting() == ASSET( "10000000.000000 VESTS"));
    BOOST_REQUIRE( bob_acc.get_received_vesting() == ASSET( "0.000000 VESTS"));
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( comment_beneficiaries_validate )
{
  try
  {
    BOOST_TEST_MESSAGE( "Test Comment Beneficiaries Validate" );
    comment_options_operation op;

    op.author = "alice";
    op.permlink = "test";

    BOOST_TEST_MESSAGE( "--- Testing more than 100% weight on a single route" );
    comment_payout_beneficiaries b;
    b.beneficiaries.push_back( beneficiary_route_type( account_name_type( "bob" ), HIVE_100_PERCENT + 1 ) );
    op.extensions.insert( b );
    HIVE_REQUIRE_THROW( op.validate(), fc::assert_exception );

    BOOST_TEST_MESSAGE( "--- Testing more than 100% total weight" );
    b.beneficiaries.clear();
    b.beneficiaries.push_back( beneficiary_route_type( account_name_type( "bob" ), HIVE_1_PERCENT * 75 ) );
    b.beneficiaries.push_back( beneficiary_route_type( account_name_type( "sam" ), HIVE_1_PERCENT * 75 ) );
    op.extensions.clear();
    op.extensions.insert( b );
    HIVE_REQUIRE_THROW( op.validate(), fc::assert_exception );

    BOOST_TEST_MESSAGE( "--- Testing maximum number of routes" );
    b.beneficiaries.clear();
    for( size_t i = 0; i < 127; i++ )
    {
      b.beneficiaries.push_back( beneficiary_route_type( account_name_type( "foo" + fc::to_string( i ) ), 1 ) );
    }

    op.extensions.clear();
    std::sort( b.beneficiaries.begin(), b.beneficiaries.end() );
    op.extensions.insert( b );
    op.validate();

    BOOST_TEST_MESSAGE( "--- Testing one too many routes" );
    b.beneficiaries.push_back( beneficiary_route_type( account_name_type( "bar" ), 1 ) );
    std::sort( b.beneficiaries.begin(), b.beneficiaries.end() );
    op.extensions.clear();
    op.extensions.insert( b );
    HIVE_REQUIRE_THROW( op.validate(), fc::assert_exception );


    BOOST_TEST_MESSAGE( "--- Testing duplicate accounts" );
    b.beneficiaries.clear();
    b.beneficiaries.push_back( beneficiary_route_type( "bob", HIVE_1_PERCENT * 2 ) );
    b.beneficiaries.push_back( beneficiary_route_type( "bob", HIVE_1_PERCENT ) );
    op.extensions.clear();
    op.extensions.insert( b );
    HIVE_REQUIRE_THROW( op.validate(), fc::assert_exception );

    BOOST_TEST_MESSAGE( "--- Testing incorrect account sort order" );
    b.beneficiaries.clear();
    b.beneficiaries.push_back( beneficiary_route_type( "bob", HIVE_1_PERCENT ) );
    b.beneficiaries.push_back( beneficiary_route_type( "alice", HIVE_1_PERCENT ) );
    op.extensions.clear();
    op.extensions.insert( b );
    HIVE_REQUIRE_THROW( op.validate(), fc::assert_exception );

    BOOST_TEST_MESSAGE( "--- Testing correct account sort order" );
    b.beneficiaries.clear();
    b.beneficiaries.push_back( beneficiary_route_type( "alice", HIVE_1_PERCENT ) );
    b.beneficiaries.push_back( beneficiary_route_type( "bob", HIVE_1_PERCENT ) );
    op.extensions.clear();
    op.extensions.insert( b );
    op.validate();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( comment_beneficiaries_apply )
{
  try
  {
    BOOST_TEST_MESSAGE( "Test Comment Beneficiaries" );
    ACTORS( (alice)(bob)(sam)(dave) )
    generate_block();

    db_plugin->debug_update( [=]( database& db )
    {
      db.modify( db.get_dynamic_global_properties(), [=]( dynamic_global_property_object& gpo )
      {
        gpo.proposal_fund_percent = 0;
      });

      db.modify( db.get_treasury(), [=]( account_object& a )
      {
        a.hbd_balance.amount.value = 0;
      });
    });

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );

    comment_operation comment;
    vote_operation vote;
    comment_options_operation op;
    comment_payout_beneficiaries b;
    signed_transaction tx;

    comment.author = "alice";
    comment.permlink = "test";
    comment.parent_permlink = "test";
    comment.title = "test";
    comment.body = "foobar";

    tx.operations.push_back( comment );
    tx.set_expiration( db->head_block_time() + HIVE_MIN_TRANSACTION_EXPIRATION_LIMIT );
    push_transaction( tx, alice_private_key, 0 );

    BOOST_TEST_MESSAGE( "--- Test failure on more than 8 benefactors" );
    b.beneficiaries.push_back( beneficiary_route_type( account_name_type( "bob" ), HIVE_1_PERCENT ) );

    for( size_t i = 0; i < 8; i++ )
    {
      b.beneficiaries.push_back( beneficiary_route_type( account_name_type( HIVE_INIT_MINER_NAME + fc::to_string( i ) ), HIVE_1_PERCENT ) );
    }

    op.author = "alice";
    op.permlink = "test";
    op.allow_curation_rewards = false;
    op.extensions.insert( b );
    tx.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), chain::plugin_exception );


    BOOST_TEST_MESSAGE( "--- Test specifying a non-existent benefactor" );
    b.beneficiaries.clear();
    b.beneficiaries.push_back( beneficiary_route_type( account_name_type( "doug" ), HIVE_1_PERCENT ) );
    op.extensions.clear();
    op.extensions.insert( b );
    tx.clear();
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::assert_exception );


    BOOST_TEST_MESSAGE( "--- Test setting when comment has been voted on" );
    vote.author = "alice";
    vote.permlink = "test";
    vote.voter = "bob";
    vote.weight = HIVE_100_PERCENT;

    b.beneficiaries.clear();
    b.beneficiaries.push_back( beneficiary_route_type( account_name_type( "bob" ), 25 * HIVE_1_PERCENT ) );
    b.beneficiaries.push_back( beneficiary_route_type( account_name_type( db->get_treasury_name() ), 10 * HIVE_1_PERCENT ) );
    b.beneficiaries.push_back( beneficiary_route_type( account_name_type( "sam" ), 50 * HIVE_1_PERCENT ) );
    op.extensions.clear();
    op.extensions.insert( b );

    tx.clear();
    tx.operations.push_back( vote );
    tx.operations.push_back( op );
    sign( tx, alice_private_key );
    sign( tx, bob_private_key );
    HIVE_REQUIRE_THROW( push_transaction( tx ), fc::assert_exception );


    BOOST_TEST_MESSAGE( "--- Test success" );
    tx.clear();
    tx.operations.push_back( op );
    push_transaction( tx, alice_private_key, 0 );


    BOOST_TEST_MESSAGE( "--- Test setting when there are already beneficiaries" );
    b.beneficiaries.clear();
    b.beneficiaries.push_back( beneficiary_route_type( account_name_type( "dave" ), 25 * HIVE_1_PERCENT ) );
    op.extensions.clear();
    op.extensions.insert( b );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::assert_exception );


    BOOST_TEST_MESSAGE( "--- Payout and verify rewards were split properly" );
    tx.clear();
    tx.operations.push_back( vote );
    push_transaction( tx, bob_private_key, 0 );

    generate_blocks( db->find_comment_cashout( db->get_comment( "alice", string( "test" ) ) )->get_cashout_time() - HIVE_BLOCK_INTERVAL );

    db_plugin->debug_update( [=]( database& db )
    {
      db.modify( db.get_dynamic_global_properties(), [=]( dynamic_global_property_object& gpo )
      {
        gpo.current_supply -= gpo.get_total_reward_fund_hive();
        gpo.total_reward_fund_hive = ASSET( "100.000 TESTS" );
        gpo.current_supply += gpo.get_total_reward_fund_hive();
      });
    });

    generate_block();

    //note: below we are mixing HIVE and HBD but it works due to use of amount instead of whole asset plus the exchange rate is 1-1
    const comment_object& _comment = db->get_comment( "alice", string( "test" ) );
    const comment_cashout_object* _comment_cashout = db->find_comment_cashout( _comment );
    BOOST_REQUIRE( _comment_cashout == nullptr );

    BOOST_REQUIRE( ( get_hbd_rewards( "alice" ).amount + get_vest_rewards_as_hive( "alice" ).amount + db->get_treasury().get_hbd_balance().amount ) == get_vest_rewards_as_hive( "bob" ).amount + get_hbd_rewards( "bob" ).amount + 1 );
    BOOST_REQUIRE( ( get_hbd_rewards( "alice" ).amount + get_vest_rewards_as_hive( "alice" ).amount + db->get_treasury().get_hbd_balance().amount ) == ( get_vest_rewards_as_hive( "sam" ).amount + get_hbd_rewards( "sam" ).amount ) / 2 + 1 );
    BOOST_REQUIRE( get_vest_rewards_as_hive( "bob" ).amount == get_hbd_rewards( "bob" ).amount + 1 );
    BOOST_REQUIRE( get_vest_rewards_as_hive( "sam" ).amount == get_hbd_rewards( "sam" ).amount + 1 );

  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( comment_options_apply )
{
  try
  {
    BOOST_TEST_MESSAGE( "Test Comment options" );
    ACTORS( (alice)(bob)(sam)(dave) )
    generate_block();

    db_plugin->debug_update( [=]( database& db )
    {
      db.modify( db.get_dynamic_global_properties(), [=]( dynamic_global_property_object& gpo )
      {
        gpo.proposal_fund_percent = 0;
      } );

      db.modify( db.get_treasury(), [=]( account_object& a )
      {
        a.hbd_balance.amount.value = 0;
      } );
    } );

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );

    comment_operation comment;
    vote_operation vote;
    comment_options_operation op;
    signed_transaction tx;

    comment.author = "alice";
    comment.permlink = "test1";
    comment.parent_permlink = "test";
    comment.title = "test";
    comment.body = "foobar";
    push_transaction( comment, alice_private_key );
    generate_block();
    comment.parent_author = "alice";
    comment.parent_permlink = "test1";
    comment.permlink = "test2";
    push_transaction( comment, alice_private_key );
    generate_block();
    comment.permlink = "test3";
    push_transaction( comment, alice_private_key );
    generate_block();

    BOOST_TEST_MESSAGE( "--- Test clearing allow_votes when comment has been voted on" );
    vote.author = "alice";
    vote.permlink = "test1";
    vote.voter = "bob";
    vote.weight = HIVE_100_PERCENT;

    op.author = "alice";
    op.permlink = "test1";
    op.allow_votes = false;

    tx.clear();
    tx.operations.push_back( vote );
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MIN_TRANSACTION_EXPIRATION_LIMIT );
    sign( tx, alice_private_key );
    sign( tx, bob_private_key );
    HIVE_REQUIRE_ASSERT( push_transaction( tx ), "!comment_cashout->has_votes()" );

    BOOST_TEST_MESSAGE( "--- Test success of clearing allow_votes" );
    push_transaction( op, alice_private_key );

    BOOST_TEST_MESSAGE( "--- Test voting no longer possible" );
    HIVE_REQUIRE_ASSERT( push_transaction( vote, bob_private_key ), "comment_cashout->allows_votes()" );

    BOOST_TEST_MESSAGE( "--- Test enabling voting back not possible" );
    op.allow_votes = true;
    HIVE_REQUIRE_ASSERT( push_transaction( op, alice_private_key ), "comment_cashout->allows_votes() >= o.allow_votes" );

    BOOST_TEST_MESSAGE( "--- Test success - voting actually possible but only downvoting" );
    //ABW: that is probably the source of confusion, however if all votes were disallowed author
    //of some negative content could not even be affected on reputation
    vote.weight = -HIVE_100_PERCENT;
    push_transaction( vote, bob_private_key );

    BOOST_TEST_MESSAGE( "--- Test clearing allow_curation_rewards when comment has been voted on" );
    vote.voter = "sam";
    vote.permlink = "test2";
    vote.weight = HIVE_100_PERCENT;
    op.permlink = "test2";
    op.allow_curation_rewards = false;

    tx.clear();
    tx.operations.push_back( vote );
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MIN_TRANSACTION_EXPIRATION_LIMIT );
    sign( tx, alice_private_key );
    sign( tx, sam_private_key );
    HIVE_REQUIRE_ASSERT( push_transaction( tx ), "!comment_cashout->has_votes()" );

    BOOST_TEST_MESSAGE( "--- Test success of clearing allow_curation_rewards" );
    push_transaction( op, alice_private_key );

    BOOST_TEST_MESSAGE( "--- Test enabling curations back not possible" );
    op.allow_curation_rewards = true;
    HIVE_REQUIRE_ASSERT( push_transaction( op, alice_private_key ), "comment_cashout->allows_curation_rewards() >= o.allow_curation_rewards" );

    BOOST_TEST_MESSAGE( "--- Test success of voting on comment with cleared allow_curation_rewards" );
    push_transaction( vote, sam_private_key );


    BOOST_TEST_MESSAGE( "--- Test setting lower max_accepted_payout when comment has been voted on" );
    vote.voter = "dave";
    vote.permlink = "test3";
    op.permlink = "test3";
    op.max_accepted_payout = ASSET( "0.010 TBD" );

    tx.clear();
    tx.operations.push_back( vote );
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MIN_TRANSACTION_EXPIRATION_LIMIT );
    sign( tx, alice_private_key );
    sign( tx, dave_private_key );
    HIVE_REQUIRE_ASSERT( push_transaction( tx ), "!comment_cashout->has_votes()" );

    BOOST_TEST_MESSAGE( "--- Test success of setting lower max_accepted_payout" );
    push_transaction( op, alice_private_key );

    BOOST_TEST_MESSAGE( "--- Test increasing max_accepted_payout not possible" );
    op.max_accepted_payout.amount *= 2;
    HIVE_REQUIRE_ASSERT( push_transaction( op, alice_private_key ), "comment_cashout->get_max_accepted_payout() >= o.max_accepted_payout" );

    BOOST_TEST_MESSAGE( "--- Test success of voting on comment with lowered max_accepted_payout" );
    push_transaction( vote, dave_private_key );

    BOOST_TEST_MESSAGE( "--- Payout and verify rewards on comment with disabled curation rewards" );
    generate_blocks( db->find_comment_cashout( db->get_comment( "alice", string( "test1" ) ) )->get_cashout_time() - HIVE_BLOCK_INTERVAL );

    auto set_reward_pool = [=]( database& db )
    {
      db.modify( db.get_dynamic_global_properties(), [=]( dynamic_global_property_object& gpo )
      {
        gpo.current_supply -= gpo.get_total_reward_fund_hive();
        gpo.total_reward_fund_hive = ASSET( "100.000 TESTS" );
        gpo.current_supply += gpo.get_total_reward_fund_hive();
      } );
    };

    db_plugin->debug_update( set_reward_pool );
    generate_block();

    BOOST_REQUIRE( db->find_comment_cashout( db->get_comment( "alice", string( "test1" ) ) ) == nullptr );
    //no reward for test1 comment
    BOOST_REQUIRE_EQUAL( get_rewards( "alice" ).amount.value, 0 );
    BOOST_REQUIRE_EQUAL( get_hbd_rewards( "alice" ).amount.value, 0 );
    BOOST_REQUIRE_EQUAL( get_vest_rewards_as_hive( "alice" ).amount.value, 0 );
    BOOST_REQUIRE_EQUAL( get_rewards( "bob" ).amount.value, 0 );
    BOOST_REQUIRE_EQUAL( get_hbd_rewards( "bob" ).amount.value, 0 );
    BOOST_REQUIRE_EQUAL( get_vest_rewards_as_hive( "bob" ).amount.value, 0 );

    db_plugin->debug_update( set_reward_pool );
    generate_block();

    BOOST_REQUIRE( db->find_comment_cashout( db->get_comment( "alice", string( "test2" ) ) ) == nullptr );
    //alice got reward for test2 comment as author...
    BOOST_REQUIRE_EQUAL( get_rewards( "alice" ).amount.value, 0 );
    BOOST_REQUIRE_EQUAL( get_hbd_rewards( "alice" ).amount.value, 18635 );
    BOOST_REQUIRE_EQUAL( get_vest_rewards_as_hive( "alice" ).amount.value, 18635 );
    //...but no curation for sam
    BOOST_REQUIRE_EQUAL( get_rewards( "sam" ).amount.value, 0 );
    BOOST_REQUIRE_EQUAL( get_hbd_rewards( "sam" ).amount.value, 0 );
    BOOST_REQUIRE_EQUAL( get_vest_rewards_as_hive( "sam" ).amount.value, 0 );

    db_plugin->debug_update( set_reward_pool );
    generate_block();

    BOOST_REQUIRE( db->find_comment_cashout( db->get_comment( "alice", string( "test3" ) ) ) == nullptr );
    //alice and dave got reward for test3 comment but they sum to 0.010 HBD
    BOOST_REQUIRE_EQUAL( get_rewards( "alice" ).amount.value, 0 );
    BOOST_REQUIRE_EQUAL( get_hbd_rewards( "alice" ).amount.value, 18635 + 2 );
    BOOST_REQUIRE_EQUAL( get_vest_rewards_as_hive( "alice" ).amount.value, 18635 + 3 );
    BOOST_REQUIRE_EQUAL( get_rewards( "dave" ).amount.value, 0 );
    BOOST_REQUIRE_EQUAL( get_hbd_rewards( "dave" ).amount.value, 0 );
    BOOST_REQUIRE_EQUAL( get_vest_rewards_as_hive( "dave" ).amount.value, 5 );

  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( comment_options_deleted_permlink_reuse )
{
  try
  {
    BOOST_TEST_MESSAGE( "Test if comment options persist through deleted comment reuse" );
    ACTORS( (alice)(bob) )
    generate_block();

    db_plugin->debug_update( [=]( database& db )
    {
      db.modify( db.get_dynamic_global_properties(), [=]( dynamic_global_property_object& gpo )
      {
        gpo.proposal_fund_percent = 0;
      } );

      db.modify( db.get_treasury(), [=]( account_object& a )
      {
        a.hbd_balance.amount.value = 0;
      } );
    } );

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );

    comment_operation comment;
    vote_operation vote;
    comment_options_operation op;

    comment.author = "alice";
    comment.permlink = "test";
    comment.parent_permlink = "test";
    comment.title = "test";
    comment.body = "foobar";
    push_transaction( comment, alice_private_key );
    op.author = "alice";
    op.permlink = "test";
    op.allow_curation_rewards = false;
    op.allow_votes = false;
    op.max_accepted_payout = ASSET( "0.010 TBD" );
    push_transaction( op, alice_private_key );
    generate_block();

    BOOST_TEST_MESSAGE( "--- Downvoting comment (possible even with voting disabled)" );
    vote.author = "alice";
    vote.permlink = "test";
    vote.voter = "bob";
    vote.weight = -HIVE_100_PERCENT;
    push_transaction( vote, bob_private_key );
    generate_block();

    BOOST_TEST_MESSAGE( "--- Comment has curations and voting blocked, small max payout; vote exists" );
    const auto& old_comment = db->get_comment( "alice", string( "test" ) );
    const auto* comment_cashout = db->find_comment_cashout( old_comment );
    auto old_comment_id = old_comment.get_id();
    BOOST_REQUIRE_EQUAL( comment_cashout->allows_curation_rewards(), false );
    BOOST_REQUIRE_EQUAL( comment_cashout->allows_votes(), false );
    BOOST_REQUIRE_EQUAL( comment_cashout->get_max_accepted_payout().amount.value, 10 );
    const auto& vote_idx = db->get_index< comment_vote_index, by_comment_voter >();
    auto voteI = vote_idx.find( boost::make_tuple( old_comment_id, bob_id ) );
    BOOST_REQUIRE( voteI != vote_idx.end() );

    BOOST_TEST_MESSAGE( "--- Deleting comment (right before cashout)" );
    generate_blocks( comment_cashout->get_cashout_time() - HIVE_BLOCK_INTERVAL );

    delete_comment_operation del_com;
    del_com.author = "alice";
    del_com.permlink = "test";
    push_transaction( del_com, alice_private_key );
    generate_block();

    BOOST_TEST_MESSAGE( "--- Comment no longer exists, vote was also deleted" );
    BOOST_REQUIRE( db->find_comment( "alice", string( "test" ) ) == nullptr );
    voteI = vote_idx.find( boost::make_tuple( old_comment_id, bob_id ) );
    BOOST_REQUIRE( voteI == vote_idx.end() );

    BOOST_TEST_MESSAGE( "--- Reusing old permlink for new comment" );
    comment.body = "bob, why the hate? :o(";
    push_transaction( comment, alice_private_key );
    generate_block();

    BOOST_TEST_MESSAGE( "--- New comment has default options, no vote on it exists" );
    const auto& new_comment = db->get_comment( "alice", string( "test" ) );
    comment_cashout = db->find_comment_cashout( new_comment );
    auto new_comment_id = new_comment.get_id();
    BOOST_REQUIRE_EQUAL( comment_cashout->allows_curation_rewards(), true );
    BOOST_REQUIRE_EQUAL( comment_cashout->allows_votes(), true );
    BOOST_REQUIRE_EQUAL( comment_cashout->get_max_accepted_payout().amount.value, 1000000000 );
    voteI = vote_idx.find( boost::make_tuple( new_comment_id, bob_id ) );
    BOOST_REQUIRE( voteI == vote_idx.end() );
    voteI = vote_idx.find( boost::make_tuple( old_comment_id, bob_id ) );
    BOOST_REQUIRE( voteI == vote_idx.end() );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( witness_set_properties_validate )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: witness_set_properties_validate" );

    ACTORS( (alice) )
    fund( "alice", 10000 );
    private_key_type signing_key = generate_private_key( "old_key" );

    witness_update_operation op;
    op.owner = "alice";
    op.url = "foo.bar";
    op.fee = ASSET( "1.000 TESTS" );
    op.block_signing_key = signing_key.get_public_key();
    op.props.account_creation_fee = legacy_hive_asset::from_asset( asset(HIVE_MIN_ACCOUNT_CREATION_FEE + 10, HIVE_SYMBOL) );
    op.props.maximum_block_size = HIVE_MIN_BLOCK_SIZE_LIMIT + 100;

    signed_transaction tx;
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.operations.push_back( op );
    push_transaction( tx, alice_private_key, 0 );
    generate_block();

    BOOST_TEST_MESSAGE( "--- failure when signing key is not present" );
    witness_set_properties_operation prop_op;
    prop_op.owner = "alice";
    HIVE_REQUIRE_THROW( prop_op.validate(), fc::assert_exception );

    BOOST_TEST_MESSAGE( "--- success when signing key is present" );
    prop_op.props[ "key" ] = fc::raw::pack_to_vector( signing_key.get_public_key() );
    prop_op.validate();

    BOOST_TEST_MESSAGE( "--- failure when setting account_creation_fee with incorrect symbol" );
    prop_op.props[ "account_creation_fee" ] = fc::raw::pack_to_vector( ASSET( "2.000 TBD" ) );
    HIVE_REQUIRE_THROW( prop_op.validate(), fc::assert_exception );

    BOOST_TEST_MESSAGE( "--- failure when setting maximum_block_size below HIVE_MIN_BLOCK_SIZE_LIMIT" );
    prop_op.props.erase( "account_creation_fee" );
    prop_op.props[ "maximum_block_size" ] = fc::raw::pack_to_vector( HIVE_MIN_BLOCK_SIZE_LIMIT - 1 );
    HIVE_REQUIRE_THROW( prop_op.validate(), fc::assert_exception );

    BOOST_TEST_MESSAGE( "--- failure when setting hbd_interest_rate with negative number" );
    prop_op.props.erase( "maximum_block_size" );
    prop_op.props[ "sbd_interest_rate" ] = fc::raw::pack_to_vector( -700 );
    //ABW: works also with outdated "sbd_interest_rate" instead of "hbd_interest_rate"
    HIVE_REQUIRE_THROW( prop_op.validate(), fc::assert_exception );

    BOOST_TEST_MESSAGE( "--- failure when setting hbd_interest_rate to HIVE_100_PERCENT + 1" );
    prop_op.props.erase( "sbd_interest_rate" );
    prop_op.props[ "hbd_interest_rate" ] = fc::raw::pack_to_vector( HIVE_100_PERCENT + 1 );
    HIVE_REQUIRE_THROW( prop_op.validate(), fc::assert_exception );

    BOOST_TEST_MESSAGE( "--- failure when setting new hbd_exchange_rate with HBD / HIVE" );
    prop_op.props.erase( "hbd_interest_rate" );
    prop_op.props[ "sbd_exchange_rate" ] = fc::raw::pack_to_vector( price( ASSET( "1.000 TESTS" ), ASSET( "10.000 TBD" ) ) );
    //ABW: works also with outdated "sbd_exchange_rate" instead of "hbd_exchange_rate"
    HIVE_REQUIRE_THROW( prop_op.validate(), fc::assert_exception );

    BOOST_TEST_MESSAGE( "--- failure when setting new url with length of zero" );
    prop_op.props.erase( "sbd_exchange_rate" );
    prop_op.props[ "url" ] = fc::raw::pack_to_vector( "" );
    HIVE_REQUIRE_THROW( prop_op.validate(), fc::assert_exception );

    BOOST_TEST_MESSAGE( "--- failure when setting new url with non UTF-8 character" );
    prop_op.props[ "url" ].clear();
    prop_op.props[ "url" ] = fc::raw::pack_to_vector( "\xE0\x80\x80" );
    HIVE_REQUIRE_THROW( prop_op.validate(), fc::assert_exception );

    BOOST_TEST_MESSAGE( "--- success when account subsidy rate is reasonable" );
    prop_op.props.clear();
    prop_op.props[ "key" ] = fc::raw::pack_to_vector( signing_key.get_public_key() );
    prop_op.props[ "account_subsidy_budget" ] = fc::raw::pack_to_vector( int32_t( 5000 ) );
    prop_op.validate();

    BOOST_TEST_MESSAGE( "--- failure when budget is zero" );
    prop_op.props[ "account_subsidy_budget" ] = fc::raw::pack_to_vector( int32_t( 0 ) );
    HIVE_REQUIRE_THROW( prop_op.validate(), fc::assert_exception );

    BOOST_TEST_MESSAGE( "--- failure when budget is negative" );
    prop_op.props[ "account_subsidy_budget" ] = fc::raw::pack_to_vector( int32_t( -5000 ) );
    HIVE_REQUIRE_THROW( prop_op.validate(), fc::assert_exception );

    BOOST_TEST_MESSAGE( "--- success when budget is just under too big" );
    prop_op.props[ "account_subsidy_budget" ] = fc::raw::pack_to_vector( int32_t( 268435455 ) );
    prop_op.validate();

    BOOST_TEST_MESSAGE( "--- failure when account subsidy budget is just a little too big" );
    prop_op.props[ "account_subsidy_budget" ] = fc::raw::pack_to_vector( int32_t( 268435456 ) );
    HIVE_REQUIRE_THROW( prop_op.validate(), fc::assert_exception );

    BOOST_TEST_MESSAGE( "--- failure when account subsidy budget is enormous" );
    prop_op.props[ "account_subsidy_budget" ] = fc::raw::pack_to_vector( int32_t( 0x50000000 ) );
    HIVE_REQUIRE_THROW( prop_op.validate(), fc::assert_exception );

    BOOST_TEST_MESSAGE( "--- success when account subsidy decay is reasonable" );
    prop_op.props.clear();
    prop_op.props[ "key" ] = fc::raw::pack_to_vector( signing_key.get_public_key() );
    prop_op.props[ "account_subsidy_decay" ] = fc::raw::pack_to_vector( uint32_t( 300000 ) );
    prop_op.validate();

    BOOST_TEST_MESSAGE( "--- failure when account subsidy decay is zero" );
    prop_op.props[ "account_subsidy_decay" ] = fc::raw::pack_to_vector( uint32_t( 0 ) );
    HIVE_REQUIRE_THROW( prop_op.validate(), fc::assert_exception );

    BOOST_TEST_MESSAGE( "--- failure when account subsidy decay is very small" );
    prop_op.props[ "account_subsidy_decay" ] = fc::raw::pack_to_vector( uint32_t( 40 ) );
    HIVE_REQUIRE_THROW( prop_op.validate(), fc::assert_exception );

    uint64_t unit = uint64_t(1) << HIVE_RD_DECAY_DENOM_SHIFT;

    BOOST_TEST_MESSAGE( "--- success when account subsidy decay is one year" );
    prop_op.props[ "account_subsidy_decay" ] = fc::raw::pack_to_vector( uint32_t( unit / HIVE_BLOCKS_PER_YEAR ) );
    prop_op.validate();

    BOOST_TEST_MESSAGE( "--- success when account subsidy decay is one day" );
    prop_op.props[ "account_subsidy_decay" ] = fc::raw::pack_to_vector( uint32_t( unit / HIVE_BLOCKS_PER_DAY ) );
    prop_op.validate();

    BOOST_TEST_MESSAGE( "--- success when account subsidy decay is one hour" );
    prop_op.props[ "account_subsidy_decay" ] = fc::raw::pack_to_vector( uint32_t( unit / ((60*60)/HIVE_BLOCK_INTERVAL) ) );
    prop_op.validate();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( witness_set_properties_authorities )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: witness_set_properties_authorities" );

    witness_set_properties_operation op;
    op.owner = "alice";
    op.props[ "key" ] = fc::raw::pack_to_vector( generate_private_key( "key" ).get_public_key() );

    flat_set< account_name_type > auths;
    flat_set< account_name_type > expected;

    op.get_required_owner_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    op.get_required_active_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    op.get_required_posting_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    vector< authority > key_auths;
    vector< authority > expected_keys;
    expected_keys.push_back( authority( 1, generate_private_key( "key" ).get_public_key(), 1 ) );
    op.get_required_authorities( key_auths );
    BOOST_REQUIRE( key_auths == expected_keys );

    op.props.erase( "key" );
    key_auths.clear();
    expected_keys.clear();

    op.get_required_owner_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    op.get_required_active_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    op.get_required_posting_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    expected_keys.push_back( authority( 1, HIVE_NULL_ACCOUNT, 1 ) );
    op.get_required_authorities( key_auths );
    BOOST_REQUIRE( key_auths == expected_keys );

  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( witness_set_properties_apply )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: witness_set_properties_apply" );

    ACTORS( (alice) )
    fund( "alice", 10000 );
    private_key_type signing_key = generate_private_key( "old_key" );

    witness_update_operation op;
    op.owner = "alice";
    op.url = "foo.bar";
    op.fee = ASSET( "1.000 TESTS" );
    op.block_signing_key = signing_key.get_public_key();
    op.props.account_creation_fee = legacy_hive_asset::from_asset( asset(HIVE_MIN_ACCOUNT_CREATION_FEE + 10, HIVE_SYMBOL) );
    op.props.maximum_block_size = HIVE_MIN_BLOCK_SIZE_LIMIT + 100;

    signed_transaction tx;
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.operations.push_back( op );
    push_transaction( tx, alice_private_key, 0 );

    BOOST_TEST_MESSAGE( "--- Test setting runtime parameters" );

    // Setting account_creation_fee
    const witness_object& alice_witness = db->get_witness( "alice" );
    witness_set_properties_operation prop_op;
    prop_op.owner = "alice";
    prop_op.props[ "key" ] = fc::raw::pack_to_vector( signing_key.get_public_key() );
    prop_op.props[ "account_creation_fee" ] = fc::raw::pack_to_vector( ASSET( "2.000 TESTS" ) );
    tx.clear();
    tx.operations.push_back( prop_op );
    push_transaction( tx, signing_key, 0 );
    BOOST_REQUIRE( alice_witness.props.account_creation_fee == ASSET( "2.000 TESTS" ) );

    // Setting maximum_block_size
    prop_op.props.erase( "account_creation_fee" );
    prop_op.props[ "maximum_block_size" ] = fc::raw::pack_to_vector( HIVE_MIN_BLOCK_SIZE_LIMIT + 1 );
    tx.clear();
    tx.operations.push_back( prop_op );
    push_transaction( tx, signing_key, 0 );
    BOOST_REQUIRE( alice_witness.props.maximum_block_size == HIVE_MIN_BLOCK_SIZE_LIMIT + 1 );

    // Setting hbd_interest_rate
    prop_op.props.erase( "maximum_block_size" );
    prop_op.props[ "hbd_interest_rate" ] = fc::raw::pack_to_vector( 700 );
    tx.clear();
    tx.operations.push_back( prop_op );
    push_transaction( tx, signing_key, 0 );
    BOOST_REQUIRE( alice_witness.props.hbd_interest_rate == 700 );

    // Setting new signing_key
    private_key_type old_signing_key = signing_key;
    signing_key = generate_private_key( "new_key" );
    public_key_type alice_pub = signing_key.get_public_key();
    prop_op.props.erase( "hbd_interest_rate" );
    prop_op.props[ "new_signing_key" ] = fc::raw::pack_to_vector( alice_pub );
    tx.clear();
    tx.operations.push_back( prop_op );
    push_transaction( tx, old_signing_key, 0 );
    BOOST_REQUIRE( alice_witness.signing_key == alice_pub );

    // Setting new hbd_exchange_rate
    prop_op.props.erase( "new_signing_key" );
    prop_op.props[ "key" ].clear();
    prop_op.props[ "key" ] = fc::raw::pack_to_vector( signing_key.get_public_key() );
    prop_op.props[ "sbd_exchange_rate" ] = fc::raw::pack_to_vector( price( ASSET(" 1.000 TBD" ), ASSET( "100.000 TESTS" ) ) );
    //ABW: works also with outdated "sbd_exchange_rate" instead of "hbd_exchange_rate"
    tx.clear();
    tx.operations.push_back( prop_op );
    push_transaction( tx, signing_key, 0 );
    BOOST_REQUIRE( alice_witness.get_hbd_exchange_rate() == price( ASSET( "1.000 TBD" ), ASSET( "100.000 TESTS" ) ) );
    BOOST_REQUIRE( alice_witness.get_last_hbd_exchange_update() == db->head_block_time() );

    // Setting new url
    prop_op.props.erase( "sbd_exchange_rate" );
    prop_op.props[ "url" ] = fc::raw::pack_to_vector( "foo.bar" );
    tx.clear();
    tx.operations.push_back( prop_op );
    push_transaction( tx, signing_key, 0 );
    BOOST_REQUIRE( alice_witness.url == "foo.bar" );

    // Setting new extranious_property
    prop_op.props[ "extraneous_property" ] = fc::raw::pack_to_vector( "foo" );
    tx.clear();
    tx.operations.push_back( prop_op );
    push_transaction( tx, signing_key, 0 );

    BOOST_TEST_MESSAGE( "--- Testing failure when 'key' does not match witness signing key" );
    prop_op.props.erase( "extranious_property" );
    prop_op.props[ "key" ].clear();
    prop_op.props[ "key" ] = fc::raw::pack_to_vector( old_signing_key.get_public_key() );
    tx.clear();
    tx.operations.push_back( prop_op );
    HIVE_REQUIRE_THROW( push_transaction( tx, old_signing_key, 0 ), fc::assert_exception );

    BOOST_TEST_MESSAGE( "--- Testing setting account subsidy rate" );
    prop_op.props[ "key" ].clear();
    prop_op.props[ "key" ] = fc::raw::pack_to_vector( signing_key.get_public_key() );
    prop_op.props[ "account_subsidy_budget" ] = fc::raw::pack_to_vector( HIVE_ACCOUNT_SUBSIDY_PRECISION );
    tx.clear();
    tx.operations.push_back( prop_op );
    push_transaction( tx, signing_key, 0 );
    BOOST_REQUIRE( alice_witness.props.account_subsidy_budget == HIVE_ACCOUNT_SUBSIDY_PRECISION );

    BOOST_TEST_MESSAGE( "--- Testing setting account subsidy pool cap" );
    uint64_t day_decay = ( uint64_t(1) << HIVE_RD_DECAY_DENOM_SHIFT ) / HIVE_BLOCKS_PER_DAY;
    prop_op.props.erase( "account_subsidy_decay" );
    prop_op.props[ "account_subsidy_decay" ] = fc::raw::pack_to_vector( day_decay );
    tx.clear();
    tx.operations.push_back( prop_op );
    push_transaction( tx, signing_key, 0 );
    BOOST_REQUIRE( alice_witness.props.account_subsidy_decay == day_decay );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( claim_account_validate )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: claim_account_validate" );

    claim_account_operation op;
    op.creator = "alice";
    op.fee = ASSET( "1.000 TESTS" );

    BOOST_TEST_MESSAGE( "--- Test failure with invalid account name" );
    op.creator = "aA0";
    BOOST_REQUIRE_THROW( op.validate(), fc::assert_exception );

    BOOST_TEST_MESSAGE( "--- Test failure with invalid fee symbol" );
    op.creator = "alice";
    op.fee = ASSET( "1.000 TBD" );
    BOOST_REQUIRE_THROW( op.validate(), fc::assert_exception );

    BOOST_TEST_MESSAGE( "--- Test failure with negative fee" );
    op.fee = ASSET( "-1.000 TESTS" );
    BOOST_REQUIRE_THROW( op.validate(), fc::assert_exception );

    BOOST_TEST_MESSAGE( "--- Test failure with non-zero extensions" );
    op.fee = ASSET( "1.000 TESTS" );
    op.extensions.insert( future_extensions( void_t() ) );
    BOOST_REQUIRE_THROW( op.validate(), fc::assert_exception );

    BOOST_TEST_MESSAGE( "--- Test success" );
    op.extensions.clear();
    op.validate();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( claim_account_authorities )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: claim_account_authorities" );

    claim_account_operation op;
    op.creator = "alice";

    flat_set< account_name_type > auths;
    flat_set< account_name_type > expected;

    op.get_required_owner_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    expected.insert( "alice" );
    op.get_required_active_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    expected.clear();
    auths.clear();
    op.get_required_posting_authorities( auths );
    BOOST_REQUIRE( auths == expected );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( claim_account_apply )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: claim_account_apply" );
    ACTORS( (alice) )
    generate_block();

    fund( "alice", ASSET( "20.000 TESTS" ) );
    generate_block();

    auto set_subsidy_budget = [&]( int32_t budget, uint32_t decay )
    {
      flat_map< string, vector<char> > props;
      props["account_subsidy_budget"] = fc::raw::pack_to_vector( budget );
      props["account_subsidy_decay"] = fc::raw::pack_to_vector( decay );
      set_witness_props( props );
    };

    auto get_subsidy_pools = [&]( int64_t& con_subs, int64_t &ncon_subs )
    {
      // get consensus and non-consensus subsidies
      con_subs = db->get_dynamic_global_properties().available_account_subsidies;
      ncon_subs = db->get< plugins::rc::rc_pool_object >().get_pool( plugins::rc::resource_new_accounts );
    };

    // set_subsidy_budget creates a lot of blocks, so there should be enough for a few accounts
    // half-life of 10 minutes
    set_subsidy_budget( 5000, 249617279 );

    // generate a half hour worth of blocks to warm up the per-witness pools, etc.
    generate_blocks( HIVE_BLOCKS_PER_HOUR / 2 );

    signed_transaction tx;
    claim_account_operation op;

    BOOST_TEST_MESSAGE( "--- Test failure when creator cannot cover fee" );
    op.creator = "alice";
    op.fee = ASSET( "30.000 TESTS" );
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    BOOST_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::assert_exception );
    validate_database();


    BOOST_TEST_MESSAGE( "--- Test success claiming an account" );
    generate_block();
    db_plugin->debug_update( [=]( database& db )
    {
      db.modify( db.get_witness_schedule_object(), [&]( witness_schedule_object& wso )
      {
        wso.median_props.account_creation_fee = ASSET( "5.000 TESTS" );
      });
    });
    generate_block();

    int64_t prev_c_subs = 0, prev_nc_subs = 0,
          c_subs = 0, nc_subs = 0;

    get_subsidy_pools( prev_c_subs, prev_nc_subs );
    op.fee = ASSET( "5.000 TESTS" );
    tx.clear();
    tx.operations.push_back( op );
    push_transaction( tx, alice_private_key, 0 );
    get_subsidy_pools( c_subs, nc_subs );

    BOOST_REQUIRE( db->get_account( "alice" ).pending_claimed_accounts == 1 );
    BOOST_REQUIRE( get_balance( "alice" ) == ASSET( "15.000 TESTS" ) );
    BOOST_REQUIRE( get_balance( HIVE_NULL_ACCOUNT ) == ASSET( "5.000 TESTS" ) );
    BOOST_CHECK_EQUAL( c_subs, prev_c_subs );
    BOOST_CHECK_EQUAL( nc_subs, prev_nc_subs );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test claiming from a non-existent account" );
    op.creator = "bob";
    tx.clear();
    tx.operations.push_back( op );
    BOOST_REQUIRE_THROW( push_transaction( tx, 0 ), fc::exception );
    validate_database();


    BOOST_TEST_MESSAGE( "--- Test failure claiming account with an excess fee" );
    generate_block();
    op.creator = "alice";
    op.fee = ASSET( "10.000 TESTS" );
    tx.clear();
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::assert_exception );
    validate_database();


    BOOST_TEST_MESSAGE( "--- Test success claiming a second account" );
    generate_block();
    get_subsidy_pools( prev_c_subs, prev_nc_subs );
    op.fee = ASSET( "5.000 TESTS" );
    tx.clear();
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    push_transaction( tx, alice_private_key, 0 );
    get_subsidy_pools( c_subs, nc_subs );

    BOOST_REQUIRE( db->get_account( "alice" ).pending_claimed_accounts == 2 );
    BOOST_REQUIRE( get_balance( "alice" ) == ASSET( "10.000 TESTS" ) );
    BOOST_REQUIRE( c_subs == prev_c_subs );
    BOOST_REQUIRE( nc_subs == prev_nc_subs );
    validate_database();


    BOOST_TEST_MESSAGE( "--- Test success claiming a subsidized account" );
    generate_block();    // Put all previous test transactions into a block

    while( true )
    {
      account_name_type next_witness = db->get_scheduled_witness( 1 );
      if( db->get_witness( next_witness ).schedule == witness_object::elected )
        break;
      generate_block();
    }

    get_subsidy_pools( prev_c_subs, prev_nc_subs );
    op.creator = "alice";
    op.fee = ASSET( "0.000 TESTS" );
    tx.clear();
    tx.operations.push_back( op );
    ilog( "Pushing transaction: ${t}", ("t", tx) );
    push_transaction( tx, alice_private_key, 0 );
    get_subsidy_pools( c_subs, nc_subs );
    BOOST_CHECK( db->get_account( "alice" ).pending_claimed_accounts == 3 );
    BOOST_CHECK( get_balance( "alice" ) == ASSET( "10.000 TESTS" ) );
    // Non-consensus isn't updated until end of block
    BOOST_CHECK_EQUAL( c_subs, prev_c_subs - HIVE_ACCOUNT_SUBSIDY_PRECISION );
    BOOST_CHECK_EQUAL( nc_subs, prev_nc_subs );

    generate_block();

    // RC update at the end of the block
    get_subsidy_pools( c_subs, nc_subs );
    block_id_type hbid = db->head_block_id();
    std::shared_ptr<full_block_type> block = db->fetch_block_by_id(hbid);
    BOOST_REQUIRE( block );
    BOOST_CHECK_EQUAL( block->get_block().transactions.size(), 1u );
    BOOST_CHECK( db->get_account( "alice" ).pending_claimed_accounts == 3 );

    int64_t new_value = prev_c_subs - HIVE_ACCOUNT_SUBSIDY_PRECISION;     // Usage applied before decay
    new_value = new_value
        + 5000                                                             // Budget
        - ((new_value*249617279) >> HIVE_RD_DECAY_DENOM_SHIFT);           // Decay

    BOOST_CHECK_EQUAL( c_subs, new_value );
    BOOST_CHECK_EQUAL( nc_subs, new_value );

    BOOST_TEST_MESSAGE( "--- Test failure claiming a partial subsidized account" );
    op.fee = ASSET( "2.500 TESTS" );
    tx.clear();
    tx.operations.push_back( op );
    BOOST_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::assert_exception );


    BOOST_TEST_MESSAGE( "--- Test failure with no available subsidized accounts" );
    generate_block();
    db_plugin->debug_update( [=]( database& db )
    {
      db.modify( db.get_dynamic_global_properties(), [&]( dynamic_global_property_object& gpo )
      {
        gpo.available_account_subsidies = 0;
      });
    });
    generate_block();
    op.fee = ASSET( "0.000 TESTS" );
    tx.clear();
    tx.operations.push_back( op );
    BOOST_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::assert_exception );


    BOOST_TEST_MESSAGE( "--- Test failure on claim overflow" );
    generate_block();
    db_plugin->debug_update( [=]( database& db )
    {
      db.modify( db.get_account( "alice" ), [&]( account_object& a )
      {
        a.pending_claimed_accounts = std::numeric_limits< int64_t >::max();
      });
    });
    generate_block();

    op.fee = ASSET( "5.000 TESTS" );
    tx.clear();
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    BOOST_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );
    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( create_claimed_account_validate )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: create_claimed_account_validate" );

    private_key_type priv_key = generate_private_key( "alice" );

    create_claimed_account_operation op;
    op.creator = "alice";
    op.new_account_name = "bob";
    op.owner = authority( 1, priv_key.get_public_key(), 1 );
    op.active = authority( 1, priv_key.get_public_key(), 1 );
    op.posting = authority( 1, priv_key.get_public_key(), 1 );
    op.memo_key = priv_key.get_public_key();

    BOOST_TEST_MESSAGE( "--- Test invalid creator name" );
    op.creator = "aA0";
    BOOST_REQUIRE_THROW( op.validate(), fc::assert_exception );

    BOOST_TEST_MESSAGE( "--- Test invalid new account name" );
    op.creator = "alice";
    op.new_account_name = "aA0";
    BOOST_REQUIRE_THROW( op.validate(), fc::assert_exception );

    BOOST_TEST_MESSAGE( "--- Test invalid account name in owner authority" );
    op.new_account_name = "bob";
    op.owner = authority( 1, "aA0", 1 );
    BOOST_REQUIRE_THROW( op.validate(), fc::assert_exception );

    BOOST_TEST_MESSAGE( "--- Test invalid account name in active authority" );
    op.owner = authority( 1, priv_key.get_public_key(), 1 );
    op.active = authority( 1, "aA0", 1 );
    BOOST_REQUIRE_THROW( op.validate(), fc::assert_exception );

    BOOST_TEST_MESSAGE( "--- Test invalid account name in posting authority" );
    op.active = authority( 1, priv_key.get_public_key(), 1 );
    op.posting = authority( 1, "aA0", 1 );
    BOOST_REQUIRE_THROW( op.validate(), fc::assert_exception );

    BOOST_TEST_MESSAGE( "--- Test invalid JSON metadata" );
    op.posting = authority( 1, priv_key.get_public_key(), 1 );
    op.json_metadata = "{\"foo\",\"bar\"}";
    BOOST_REQUIRE_THROW( op.validate(), fc::exception );

    BOOST_TEST_MESSAGE( "--- Test non UTF-8 JSON metadata" );
    op.json_metadata = "{\"foo\":\"\xa0\xa1\"}";
    BOOST_REQUIRE_THROW( op.validate(), fc::assert_exception );

    BOOST_TEST_MESSAGE( "--- Test failure with non-zero extensions" );
    op.json_metadata = "";
    op.extensions.insert( future_extensions( void_t() ) );
    BOOST_REQUIRE_THROW( op.validate(), fc::assert_exception );

    BOOST_TEST_MESSAGE( "--- Test success" );
    op.extensions.clear();
    op.validate();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( create_claimed_account_authorities )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: create_claimed_account_authorities" );

    create_claimed_account_operation op;
    op.creator = "alice";

    flat_set< account_name_type > auths;
    flat_set< account_name_type > expected;

    op.get_required_owner_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    expected.insert( "alice" );
    op.get_required_active_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    expected.clear();
    auths.clear();
    op.get_required_posting_authorities( auths );
    BOOST_REQUIRE( auths == expected );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( create_claimed_account_apply )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: create_claimed_account_apply" );

    ACTORS( (alice) )
    vest( HIVE_INIT_MINER_NAME, HIVE_TEMP_ACCOUNT, ASSET( "10.000 TESTS" ) );
    generate_block();

    signed_transaction tx;
    create_claimed_account_operation op;
    private_key_type priv_key = generate_private_key( "bob" );

    BOOST_TEST_MESSAGE( "--- Test failure when creator has not claimed an account" );
    op.creator = "alice";
    op.new_account_name = "bob";
    op.owner = authority( 1, priv_key.get_public_key(), 1 );
    op.active = authority( 2, priv_key.get_public_key(), 2 );
    op.posting = authority( 3, priv_key.get_public_key(), 3 );
    op.memo_key = priv_key.get_public_key();
    op.json_metadata = "{\"foo\":\"bar\"}";
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    BOOST_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::assert_exception );
    validate_database();

    BOOST_TEST_MESSAGE( "--- Test failure creating account with non-existent account auth" );
    generate_block();
    db_plugin->debug_update( [=]( database& db )
    {
      db.modify( db.get_account( "alice" ), [&]( account_object& a )
      {
        a.pending_claimed_accounts = 2;
      });
    });
    generate_block();
    op.owner = authority( 1, "bob", 1 );
    tx.clear();
    BOOST_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );
    validate_database();


    BOOST_TEST_MESSAGE( "--- Test success creating claimed account" );
    op.owner = authority( 1, priv_key.get_public_key(), 1 );
    tx.clear();
    tx.operations.push_back( op );
    push_transaction( tx, alice_private_key, 0 );

    const auto& bob = db->get_account( "bob" );
    const auto& bob_auth = db->get< account_authority_object, by_account >( "bob" );

    BOOST_REQUIRE( bob.name == "bob" );
    BOOST_REQUIRE( bob_auth.owner == authority( 1, priv_key.get_public_key(), 1 ) );
    BOOST_REQUIRE( bob_auth.active == authority( 2, priv_key.get_public_key(), 2 ) );
    BOOST_REQUIRE( bob_auth.posting == authority( 3, priv_key.get_public_key(), 3 ) );
    BOOST_REQUIRE( bob.memo_key == priv_key.get_public_key() );
#ifdef COLLECT_ACCOUNT_METADATA // json_metadata is stored conditionally
    const auto& bob_meta = db->get< account_metadata_object, by_account >( bob.get_id() );
    BOOST_REQUIRE( bob_meta.json_metadata == "{\"foo\":\"bar\"}" );
#endif
    CHECK_NO_PROXY( bob );
    BOOST_REQUIRE( bob.get_recovery_account() == "alice" );
    BOOST_REQUIRE( bob.created == db->head_block_time() );
    BOOST_REQUIRE( bob.get_balance().amount.value == ASSET( "0.000 TESTS" ).amount.value );
    BOOST_REQUIRE( bob.get_hbd_balance().amount.value == ASSET( "0.000 TBD" ).amount.value );
    BOOST_REQUIRE( bob.get_vesting().amount.value == ASSET( "0.000000 VESTS" ).amount.value );
    BOOST_REQUIRE( bob.get_id().get_value() == bob_auth.get_id().get_value() );

    BOOST_REQUIRE( db->get_account( "alice" ).pending_claimed_accounts == 1 );
    validate_database();


    BOOST_TEST_MESSAGE( "--- Test failure creating duplicate account name" );
    tx.signatures.clear();
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    BOOST_REQUIRE_THROW( push_transaction( tx, alice_private_key, 0 ), fc::exception );
    validate_database();


    BOOST_TEST_MESSAGE( "--- Test account creation with temp account does not set recovery account" );
    generate_block();
    db_plugin->debug_update( [=]( database& db )
    {
      db.modify( db.get_account( HIVE_TEMP_ACCOUNT ), [&]( account_object& a )
      {
        a.pending_claimed_accounts = 1;
      });
    });
    generate_block();
    op.creator = HIVE_TEMP_ACCOUNT;
    op.new_account_name = "charlie";
    tx.clear();
    tx.operations.push_back( op );
    push_transaction( tx, 0 );

    BOOST_REQUIRE( !db->get_account( "charlie" ).has_recovery_account() );
    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( account_auth_tests )
{
  try
  {
    ACTORS( (alice)(bob)(charlie) )
    generate_block();

    fund( "alice", ASSET( "20.000 TESTS" ) );
    fund( "bob", ASSET( "20.000 TESTS" ) );
    fund( "charlie", ASSET( "20.000 TESTS" ) );
    vest( HIVE_INIT_MINER_NAME, "alice" , ASSET( "10.000 TESTS" ) );
    vest( HIVE_INIT_MINER_NAME, "bob" , ASSET( "10.000 TESTS" ) );
    vest( HIVE_INIT_MINER_NAME, "charlie" , ASSET( "10.000 TESTS" ) );
    generate_block();

    private_key_type bob_active_private_key = bob_private_key;
    private_key_type bob_posting_private_key = generate_private_key( "bob_posting" );
    private_key_type charlie_active_private_key = charlie_private_key;
    private_key_type charlie_posting_private_key = generate_private_key( "charlie_posting" );

    db_plugin->debug_update( [=]( database& db )
    {
      db.modify( db.get< account_authority_object, by_account >( "alice"), [&]( account_authority_object& auth )
      {
        auth.active.add_authority( "bob", 1 );
        auth.posting.add_authority( "charlie", 1 );
      });

      db.modify( db.get< account_authority_object, by_account >( "bob" ), [&]( account_authority_object& auth )
      {
        auth.posting = authority( 1, bob_posting_private_key.get_public_key(), 1 );
      });

      db.modify( db.get< account_authority_object, by_account >( "charlie" ), [&]( account_authority_object& auth )
      {
        auth.posting = authority( 1, charlie_posting_private_key.get_public_key(), 1 );
      });
    });

    generate_block();

    signed_transaction tx;
    transfer_operation transfer;

    transfer.from = "alice";
    transfer.to = "bob";
    transfer.amount = ASSET( "1.000 TESTS" );
    tx.operations.push_back( transfer );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.signatures.clear();
    push_transaction( tx, bob_active_private_key, 0 );

    generate_block();
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.signatures.clear();
    HIVE_REQUIRE_THROW( push_transaction( tx, bob_posting_private_key, 0 ), tx_missing_active_auth );

    generate_block();
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.signatures.clear();
    HIVE_REQUIRE_THROW( push_transaction( tx, charlie_active_private_key, 0 ), tx_missing_active_auth );

    generate_block();
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.signatures.clear();
    HIVE_REQUIRE_THROW( push_transaction( tx, charlie_posting_private_key, 0 ), tx_missing_active_auth );

    custom_json_operation json;
    json.required_posting_auths.insert( "alice" );
    json.json = "{\"foo\":\"bar\"}";
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( json );
    HIVE_REQUIRE_THROW( push_transaction( tx, bob_active_private_key, 0 ), tx_missing_posting_auth );

    generate_block();
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.signatures.clear();
    push_transaction( tx, bob_posting_private_key, 0 );

    generate_block();
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.signatures.clear();
    HIVE_REQUIRE_THROW( push_transaction( tx, charlie_active_private_key, 0 ), tx_missing_posting_auth );

    generate_block();
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.signatures.clear();
    push_transaction( tx, charlie_posting_private_key, 0 );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( account_update2_validate )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: account_update2_validate" );

    BOOST_TEST_MESSAGE( " -- Testing failure when account name is not valid" );
    account_update2_operation op;
    op.account = "invalid_account";

    HIVE_REQUIRE_THROW( op.validate(), fc::assert_exception );

    BOOST_TEST_MESSAGE( " -- Testing failure when json_metadata is not json" );
    op.account = "alice";
    op.json_metadata = "not json";

    HIVE_REQUIRE_THROW( op.validate(), fc::assert_exception );

    BOOST_TEST_MESSAGE( " -- Testing failure when posting_json_metadata is not json" );
    op.json_metadata.clear();
    op.posting_json_metadata = "not json";

    HIVE_REQUIRE_THROW( op.validate(), fc::assert_exception );
    op.posting_json_metadata.clear();

    op.json_metadata = "{\"success\":true}";
    op.posting_json_metadata = "{\"success\":true}";
    op.validate();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( account_update2_authorities )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: account_update2_authorities" );

    BOOST_TEST_MESSAGE( " -- Testing account only case" );
    account_update2_operation op;
    op.account = "alice";

    flat_set< account_name_type > auths;
    flat_set< account_name_type > expected;

    op.get_required_owner_authorities( auths );
    BOOST_REQUIRE( auths == expected );


    op.get_required_active_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    expected.insert( "alice" );
    op.get_required_posting_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    expected.clear();
    auths.clear();

    BOOST_TEST_MESSAGE( " -- Testing owner case" );
    op.account = "alice";
    op.owner = authority();

    expected.insert( "alice" );
    op.get_required_owner_authorities( auths );
    BOOST_REQUIRE( auths == expected );
    auths.clear();
    expected.clear();

    op.get_required_active_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    op.get_required_posting_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    expected.clear();
    auths.clear();

    BOOST_TEST_MESSAGE( " -- Testing active case" );
    op = account_update2_operation();
    op.account = "alice";
    op.active = authority();
    op.active->weight_threshold = 1;
    op.active->add_authorities( "alice", 1 );

    op.get_required_owner_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    expected.insert( "alice" );
    op.get_required_active_authorities( auths );
    BOOST_REQUIRE( auths == expected );
    auths.clear();
    expected.clear();

    op.get_required_posting_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    expected.clear();
    auths.clear();

    BOOST_TEST_MESSAGE( " -- Testing posting case" );
    op = account_update2_operation();
    op.account = "alice";
    op.posting = authority();
    op.posting->weight_threshold = 1;
    op.posting->add_authorities( "alice", 1 );

    op.get_required_owner_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    expected.insert( "alice" );
    op.get_required_active_authorities( auths );
    BOOST_REQUIRE( auths == expected );
    auths.clear();
    expected.clear();

    op.get_required_posting_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    expected.clear();
    auths.clear();

    BOOST_TEST_MESSAGE( " -- Testing memo_key case" );
    op = account_update2_operation();
    op.account = "alice";
    op.memo_key = public_key_type();

    op.get_required_owner_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    expected.insert( "alice" );
    op.get_required_active_authorities( auths );
    BOOST_REQUIRE( auths == expected );
    auths.clear();
    expected.clear();

    op.get_required_posting_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    expected.clear();
    auths.clear();

    BOOST_TEST_MESSAGE( " -- Testing json_metadata case" );
    op = account_update2_operation();
    op.account = "alice";
    op.json_metadata = "{\"success\":true}";

    op.get_required_owner_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    expected.insert( "alice" );
    op.get_required_active_authorities( auths );
    BOOST_REQUIRE( auths == expected );
    auths.clear();
    expected.clear();

    op.get_required_posting_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    expected.clear();
    auths.clear();

    BOOST_TEST_MESSAGE( " -- Testing posting_json_metadata case" );
    op = account_update2_operation();
    op.account = "alice";
    op.posting_json_metadata = "{\"success\":true}";

    op.get_required_owner_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    op.get_required_active_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    expected.insert( "alice" );
    op.get_required_posting_authorities( auths );
    BOOST_REQUIRE( auths == expected );
    auths.clear();
    expected.clear();

    expected.clear();
    auths.clear();

    BOOST_TEST_MESSAGE( " -- Testing full operation cases" );

    op = account_update2_operation();
    op.account = "alice";

    op.get_required_owner_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    op.get_required_active_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    expected.insert( "alice" );
    op.get_required_posting_authorities( auths );
    BOOST_REQUIRE( auths == expected );
    auths.clear();
    expected.clear();

    op.posting_json_metadata = "{\"success\":true}";

    op.get_required_owner_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    op.get_required_active_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    expected.insert( "alice" );
    op.get_required_posting_authorities( auths );
    BOOST_REQUIRE( auths == expected );
    auths.clear();
    expected.clear();

    op.json_metadata = "{\"success\":true}";

    op.get_required_owner_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    expected.insert( "alice" );
    op.get_required_active_authorities( auths );
    BOOST_REQUIRE( auths == expected );
    auths.clear();
    expected.clear();

    op.get_required_posting_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    op.memo_key = public_key_type();

    op.get_required_owner_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    expected.insert( "alice" );
    op.get_required_active_authorities( auths );
    BOOST_REQUIRE( auths == expected );
    auths.clear();
    expected.clear();

    op.get_required_posting_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    op.posting = authority();

    op.get_required_owner_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    expected.insert( "alice" );
    op.get_required_active_authorities( auths );
    BOOST_REQUIRE( auths == expected );
    auths.clear();
    expected.clear();

    op.get_required_posting_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    op.active = authority();

    op.get_required_owner_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    expected.insert( "alice" );
    op.get_required_active_authorities( auths );
    BOOST_REQUIRE( auths == expected );
    auths.clear();
    expected.clear();

    op.get_required_posting_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    op.owner = authority();

    expected.insert( "alice" );
    op.get_required_owner_authorities( auths );
    BOOST_REQUIRE( auths == expected );
    auths.clear();
    expected.clear();

    op.get_required_active_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    op.get_required_posting_authorities( auths );
    BOOST_REQUIRE( auths == expected );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( account_update2_apply )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: account_update2_apply" );

    ACTORS( (alice)(sam) )
    private_key_type new_private_key = generate_private_key( "new_key" );

    BOOST_TEST_MESSAGE( "--- Test normal update" );

    account_update2_operation op;
    op.account = "alice";
    op.owner = authority( 1, new_private_key.get_public_key(), 1 );
    op.active = authority( 2, new_private_key.get_public_key(), 2 );
    op.memo_key = new_private_key.get_public_key();
    op.json_metadata = "{\"bar\":\"foo\"}";
    op.posting_json_metadata = "{\"success\":true}";

    signed_transaction tx;
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    push_transaction( tx, alice_private_key, 0 );

    const account_object& acct = db->get_account( "alice" );
    const account_authority_object& acct_auth = db->get< account_authority_object, by_account >( "alice" );

    BOOST_REQUIRE( acct.name == "alice" );
    BOOST_REQUIRE( acct_auth.owner == authority( 1, new_private_key.get_public_key(), 1 ) );
    BOOST_REQUIRE( acct_auth.active == authority( 2, new_private_key.get_public_key(), 2 ) );
    BOOST_REQUIRE( acct.memo_key == new_private_key.get_public_key() );

#ifdef COLLECT_ACCOUNT_METADATA
    const account_metadata_object& acct_metadata = db->get< account_metadata_object, by_account >( acct.get_id() );
    BOOST_REQUIRE( acct_metadata.json_metadata == "{\"bar\":\"foo\"}" );
    BOOST_REQUIRE( acct_metadata.posting_json_metadata == "{\"success\":true}" );
#endif

    validate_database();

    BOOST_TEST_MESSAGE( "--- Test failure when updating a non-existent account" );
    tx.operations.clear();
    tx.signatures.clear();
    op.account = "bob";
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, new_private_key, 0 ), fc::exception )
    validate_database();


    BOOST_TEST_MESSAGE( "--- Test failure when account authority does not exist" );
    tx.clear();
    op = account_update2_operation();
    op.account = "alice";
    op.posting = authority();
    op.posting->weight_threshold = 1;
    op.posting->add_authorities( "dave", 1 );
    tx.operations.push_back( op );
    HIVE_REQUIRE_THROW( push_transaction( tx, new_private_key, 0 ), fc::exception );
    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( recurrent_transfer_validate )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: recurrent_transfer_validate" );

    recurrent_transfer_operation op;
    op.from = "alice";
    op.to = "bob";
    op.memo = "Memo";
    op.recurrence = 24;
    op.executions = 10;
    op.amount = asset( 100, HIVE_SYMBOL );
    op.validate();

    BOOST_TEST_MESSAGE( " --- Invalid from account" );
    op.from = "alice-";
    HIVE_REQUIRE_THROW( op.validate(), fc::assert_exception );
    op.from = "alice";

    BOOST_TEST_MESSAGE( " --- Invalid to account" );
    op.to = "bob-";
    HIVE_REQUIRE_THROW( op.validate(), fc::assert_exception );
    op.to = "bob";

    BOOST_TEST_MESSAGE( " --- Memo too long" );
    std::string memo;
    for ( int i = 0; i < HIVE_MAX_MEMO_SIZE + 1; i++ )
      memo += "x";
    op.memo = memo;
    HIVE_REQUIRE_THROW( op.validate(), fc::assert_exception );
    op.memo = "Memo";

    BOOST_TEST_MESSAGE( " --- Negative amount" );
    op.amount = -op.amount;
    HIVE_REQUIRE_THROW( op.validate(), fc::assert_exception );
    op.amount = -op.amount;

    BOOST_TEST_MESSAGE( " --- Transferring vests" );
    op.amount = asset( 100, VESTS_SYMBOL );
    HIVE_REQUIRE_THROW( op.validate(), fc::assert_exception );
    op.amount = asset( 100, HIVE_SYMBOL );

    BOOST_TEST_MESSAGE( " --- Recurrence too low" );
    op.recurrence = 3;
    HIVE_REQUIRE_THROW( op.validate(), fc::assert_exception );
    op.recurrence = 24;

    BOOST_TEST_MESSAGE( " --- Transfer to yourself" );
    op.from = "bob";
    HIVE_REQUIRE_THROW( op.validate(), fc::assert_exception );
    op.from = "alice";

    BOOST_TEST_MESSAGE( " --- recurrence * executions is too high with recurrence being every day" );
    op.executions = HIVE_MAX_RECURRENT_TRANSFER_END_DATE + 1; // one day too many
    HIVE_REQUIRE_THROW( op.validate(), fc::assert_exception );

    BOOST_TEST_MESSAGE( " --- recurrence * executions is too high with recurrence being every two days" );
    op.recurrence = 48;
    op.executions = HIVE_MAX_RECURRENT_TRANSFER_END_DATE / 2 + 1; // one day too many
    HIVE_REQUIRE_THROW( op.validate(), fc::assert_exception );
    op.executions = 10;

    BOOST_TEST_MESSAGE( " --- executions is less than 2" );
    op.executions = 1;
    HIVE_REQUIRE_THROW( op.validate(), fc::assert_exception );
    op.executions = 0;
    HIVE_REQUIRE_THROW( op.validate(), fc::assert_exception );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( recurrent_transfer_apply )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: recurrent_transfer_apply" );

    ACTORS( (alice)(bob) )
    generate_block();

    BOOST_REQUIRE( db->get_account( "alice" ).open_recurrent_transfers == 0 );

    fund( "alice", 10000 );
    fund( "alice", ASSET("100.000 TBD") );

    BOOST_REQUIRE( get_balance( "alice" ).amount.value == ASSET( "10.000 TESTS" ).amount.value );

    recurrent_transfer_operation op;
    op.from = "alice";
    op.to = "bob";
    op.memo = "test";
    op.amount = ASSET( "5.000 TESTS" );
    op.recurrence = 72;
    op.executions = 10;

    BOOST_TEST_MESSAGE( "--- Test normal transaction" );
    push_transaction(op, alice_private_key);

    const auto execution_block_time = db->head_block_time();
    const auto& recurrent_transfer_pre_execution = db->find< recurrent_transfer_object, by_from_to_id >(boost::make_tuple( alice_id, bob_id) );

    BOOST_REQUIRE( get_balance( "alice" ).amount.value == ASSET( "10.000 TESTS" ).amount.value );
    BOOST_REQUIRE( get_balance( "bob" ).amount.value == ASSET( "0.000 TESTS" ).amount.value );
    BOOST_REQUIRE( db->get_account( "alice" ).open_recurrent_transfers == 1 );
    BOOST_REQUIRE( recurrent_transfer_pre_execution->get_trigger_date() == execution_block_time);
    BOOST_REQUIRE( recurrent_transfer_pre_execution->recurrence == 72 );
    BOOST_REQUIRE( recurrent_transfer_pre_execution->amount == ASSET( "5.000 TESTS" ) );
    BOOST_REQUIRE( recurrent_transfer_pre_execution->remaining_executions == 10 );
    BOOST_REQUIRE( recurrent_transfer_pre_execution->memo == "test" );
    validate_database();

    generate_block();
    BOOST_TEST_MESSAGE( "--- test initial recurrent transfer execution" );
    BOOST_REQUIRE( get_balance( "alice" ).amount.value == ASSET( "5.000 TESTS" ).amount.value );
    BOOST_REQUIRE( get_balance( "bob" ).amount.value == ASSET( "5.000 TESTS" ).amount.value );
    validate_database();

    BOOST_TEST_MESSAGE( "--- test recurrent transfer trigger date and remaining executions post genesis execution" );
    const auto& recurrent_transfer = db->find< recurrent_transfer_object, by_from_to_id >(boost::make_tuple( alice_id, bob_id) );

    BOOST_REQUIRE( recurrent_transfer->get_trigger_date() == execution_block_time + fc::hours(op.recurrence) );
    BOOST_REQUIRE( recurrent_transfer->remaining_executions == 9 );

    BOOST_TEST_MESSAGE( "--- test updating the recurrent transfer parameters" );
    op.memo = "test_updated";
    op.amount = ASSET( "2.000 TESTS" );
    op.executions = 20;
    push_transaction(op, alice_private_key);

    const auto recurrent_transfer_new = db->find< recurrent_transfer_object, by_from_to_id >(boost::make_tuple( alice_id, bob_id) )->copy_chain_object();

    BOOST_REQUIRE( recurrent_transfer_new.get_trigger_date() == recurrent_transfer->get_trigger_date());
    BOOST_REQUIRE( recurrent_transfer_new.recurrence == 72 );
    BOOST_REQUIRE( recurrent_transfer_new.amount == ASSET( "2.000 TESTS" ) );
    BOOST_REQUIRE( recurrent_transfer_new.remaining_executions == 20 );
    BOOST_REQUIRE( recurrent_transfer_new.memo == "test_updated" );
    validate_database();

    generate_block();

    BOOST_TEST_MESSAGE( "--- test setting the amount to HBD" );

    op.amount = ASSET("10.000 TBD");
    push_transaction(op, alice_private_key);

    BOOST_REQUIRE( recurrent_transfer->amount == ASSET("10.000 TBD") );
    validate_database();

    generate_block();
    BOOST_TEST_MESSAGE( "--- test updating the recurrence" );

    op.recurrence = 96;
    push_transaction(op, alice_private_key);

    BOOST_REQUIRE( recurrent_transfer->get_trigger_date() == db->head_block_time() + fc::hours(op.recurrence) );
    BOOST_REQUIRE( recurrent_transfer->recurrence == 96 );
    BOOST_REQUIRE( recurrent_transfer->get_trigger_date() != recurrent_transfer_new.get_trigger_date() );
    BOOST_REQUIRE( recurrent_transfer->remaining_executions == 20 );
    validate_database();

    generate_block();
    BOOST_TEST_MESSAGE( "--- test deleting the recurrent transfer" );

    op.amount = ASSET("0.000 TESTS");
    push_transaction(op, alice_private_key);

    const auto* deleted_recurrent_transfer = db->find< recurrent_transfer_object, by_from_to_id >(boost::make_tuple( alice_id, bob_id) );
    BOOST_REQUIRE( deleted_recurrent_transfer == nullptr );

    generate_block();
    op.amount = ASSET("50.000 TESTS");

    validate_database();

    BOOST_TEST_MESSAGE( "--- test not enough tokens for the first recurrent transfer" );

    op.amount = ASSET("500000.000 TBD");

    HIVE_REQUIRE_THROW( push_transaction(op, alice_private_key), fc::exception );
    validate_database();

    BOOST_TEST_MESSAGE( "--- test trying to delete a non existing transfer" );

    op.amount = ASSET( "0.000 TBD" );
    HIVE_REQUIRE_THROW( push_transaction(op, alice_private_key), fc::exception );
    validate_database();
 }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( recurrent_transfer_hbd )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: recurrent_transfer with HBD" );

    ACTORS( (alice)(bob) )
    generate_block();

    BOOST_REQUIRE( db->get_account( "alice" ).open_recurrent_transfers == 0 );

    fund( "alice", ASSET("100.000 TBD") );

    recurrent_transfer_operation op;
    op.from = "alice";
    op.to = "bob";
    op.memo = "test";
    op.amount = ASSET( "10.000 TBD" );
    op.recurrence = 72;
    op.executions = 5;
    push_transaction(op, alice_private_key);

    const auto execution_block_time = db->head_block_time();
    BOOST_REQUIRE( get_hbd_balance( "alice" ).amount.value == ASSET( "100.000 TBD" ).amount.value );
    BOOST_REQUIRE( get_hbd_balance( "bob" ).amount.value == ASSET( "0.000 TBD" ).amount.value );
    BOOST_REQUIRE( db->get_account( "alice" ).open_recurrent_transfers == 1 );
    validate_database();
    generate_block();

    BOOST_TEST_MESSAGE( "--- test initial recurrent transfer execution" );
    BOOST_REQUIRE( get_hbd_balance( "alice" ).amount.value == ASSET( "90.000 TBD" ).amount.value );
    BOOST_REQUIRE( get_hbd_balance( "bob" ).amount.value == ASSET( "10.000 TBD" ).amount.value );
    validate_database();

    BOOST_TEST_MESSAGE( "--- test recurrent transfer trigger date post genesis execution" );
    const auto& recurrent_transfer = db->find< recurrent_transfer_object, by_from_to_id >(boost::make_tuple( alice_id, bob_id) );

    BOOST_REQUIRE( recurrent_transfer->get_trigger_date() == execution_block_time + fc::hours(op.recurrence) );
 }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( recurrent_transfer_max_open_transfers )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: too many open recurrent transfers" );
    ACTORS( (alice)(bob) )

    generate_block();

    #define CREATE_ACTORS(z, n, text) ACTORS( (actor ## n) );
    BOOST_PP_REPEAT(HIVE_MAX_OPEN_RECURRENT_TRANSFERS, CREATE_ACTORS, )
    generate_block();

    BOOST_REQUIRE( db->get_account( "alice" ).open_recurrent_transfers == 0 );

    fund( "alice", ASSET("10000.000 TESTS") );

    recurrent_transfer_operation op;
    op.from = "alice";
    op.memo = "test";
    op.amount = ASSET( "5.000 TESTS" );
    op.recurrence = 72;
    op.executions = 13;

    for (int i = 0; i < HIVE_MAX_OPEN_RECURRENT_TRANSFERS; i++) {
      op.to = "actor" + std::to_string(i);
      push_transaction(op, alice_private_key);
    }

    BOOST_TEST_MESSAGE( "Testing: executing all the recurrent transfers");
    generate_block();

    BOOST_REQUIRE( get_balance( "alice" ).amount.value == ASSET( "8725.000 TESTS" ).amount.value );
    BOOST_REQUIRE( get_balance( "actor0" ).amount.value == ASSET( "5.000 TESTS" ).amount.value );
    BOOST_REQUIRE( get_balance( "actor123" ).amount.value == ASSET( "5.000 TESTS" ).amount.value );
    BOOST_REQUIRE( get_balance( "actor254" ).amount.value == ASSET( "5.000 TESTS" ).amount.value );
    BOOST_REQUIRE( db->get_account( "alice" ).open_recurrent_transfers == HIVE_MAX_OPEN_RECURRENT_TRANSFERS);

    BOOST_TEST_MESSAGE( "Testing: Cannot create more than HIVE_MAX_OPEN_RECURRENT_TRANSFERS transfers");
    op.to = "bob";
    HIVE_REQUIRE_ASSERT( push_transaction(op, alice_private_key), "from_account.open_recurrent_transfers < HIVE_MAX_OPEN_RECURRENT_TRANSFERS" );

    BOOST_TEST_MESSAGE( "Testing: Can still edit existing transfer even when at HIVE_MAX_OPEN_RECURRENT_TRANSFERS transfers" );
    op.to = "actor123";
    op.memo = "edit";
    op.amount = ASSET( "15.000 TESTS" );
    op.recurrence = 24;
    op.executions = 3;
    auto edit_time = db->head_block_time();
    push_transaction( op, alice_private_key );
    generate_block();

    // since edit does not trigger transfer right away (unless it was scheduled for that block)
    // there should be no immediate change in balance
    BOOST_REQUIRE( get_balance( "alice" ).amount.value == ASSET( "8725.000 TESTS" ).amount.value );
    BOOST_REQUIRE( get_balance( "actor123" ).amount.value == ASSET( "5.000 TESTS" ).amount.value );
    BOOST_REQUIRE( db->get_account( "alice" ).open_recurrent_transfers == HIVE_MAX_OPEN_RECURRENT_TRANSFERS );

    generate_blocks( edit_time + fc::hours( 24 ), false );

    BOOST_REQUIRE( get_balance( "alice" ).amount.value == ASSET( "8710.000 TESTS" ).amount.value );
    BOOST_REQUIRE( get_balance( "actor123" ).amount.value == ASSET( "20.000 TESTS" ).amount.value );
    BOOST_REQUIRE( db->get_account( "alice" ).open_recurrent_transfers == HIVE_MAX_OPEN_RECURRENT_TRANSFERS );

    BOOST_TEST_MESSAGE( "Testing: Can still remove existing transfer even when at HIVE_MAX_OPEN_RECURRENT_TRANSFERS transfers" );
    op.to = "actor123";
    op.memo = "erase";
    op.amount = ASSET( "0.000 TESTS" );
    push_transaction( op, alice_private_key );
    generate_block();

    BOOST_REQUIRE( get_balance( "alice" ).amount.value == ASSET( "8710.000 TESTS" ).amount.value );
    BOOST_REQUIRE( get_balance( "actor123" ).amount.value == ASSET( "20.000 TESTS" ).amount.value );
    BOOST_REQUIRE( db->get_account( "alice" ).open_recurrent_transfers == HIVE_MAX_OPEN_RECURRENT_TRANSFERS - 1 );

    validate_database();
 }
  FC_LOG_AND_RETHROW()
}


BOOST_AUTO_TEST_CASE( recurrent_transfer_max_transfer_processed_per_block )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: too many open recurrent transfers" );
    ACTORS( (alice)(bob)(eve)(martin) )
    generate_block();

    #define CREATE_ACTORS(z, n, text) ACTORS( (actor ## n) );
    BOOST_PP_REPEAT(251, CREATE_ACTORS, )

    generate_block();
    const char *senders[4] = { "alice", "bob", "eve", "martin" };
    const fc::ecc::private_key senders_keys[4] = { alice_private_key, bob_private_key, eve_private_key, martin_private_key };

    fund( "alice", ASSET("1000.000 TESTS") );
    fund( "bob", ASSET("1000.000 TESTS") );
    fund( "eve", ASSET("1000.000 TESTS") );
    fund( "martin", ASSET("1000.000 TESTS") );

    recurrent_transfer_operation op;
    op.memo = "test";
    op.amount = ASSET( "1.000 TESTS" );
    op.recurrence = 24;
    op.executions = 25;

    // 1000 recurrent transfers
    for (int k = 0; k < 4; k++) {
      op.from = senders[k];
      for (int i = 0; i < 250; i++) {
        op.to = "actor" + std::to_string(i);
        push_transaction(op, senders_keys[k]);
      }
    }

    // Those transfers won't be executed on the first block but on the second
    for (int k = 0; k < 4; k++) {
      op.from = senders[k];
      op.to = "actor250";
      op.amount = ASSET( "3.000 TESTS" );
      push_transaction(op, senders_keys[k]);
    }

    BOOST_TEST_MESSAGE( "Testing: executing the first 1000 recurrent transfers");
    const auto creation_block_time = db->head_block_time();
    generate_block();

    const auto recurrent_transfer_no_drift_before = db->find< recurrent_transfer_object, by_from_to_id >(boost::make_tuple( alice_id, actor0_id) )->copy_chain_object();

    BOOST_REQUIRE( get_balance( "alice" ).amount.value == ASSET( "750.000 TESTS" ).amount.value );
    BOOST_REQUIRE( get_balance( "actor0" ).amount.value == ASSET( "4.000 TESTS" ).amount.value );
    BOOST_REQUIRE( get_balance( "actor123" ).amount.value == ASSET( "4.000 TESTS" ).amount.value );
    BOOST_REQUIRE( get_balance( "actor250" ).amount.value == ASSET( "0.000 TESTS" ).amount.value );
    BOOST_REQUIRE( db->get_account( "alice" ).open_recurrent_transfers == 251);
    BOOST_REQUIRE( recurrent_transfer_no_drift_before.get_trigger_date() == creation_block_time + fc::days(1));

    BOOST_TEST_MESSAGE( "Executing the remaining 4 recurrent payments");

    generate_block();

    const auto recurrent_transfer_drift_before = db->find< recurrent_transfer_object, by_from_to_id >(boost::make_tuple( alice_id, actor250_id) )->copy_chain_object();

    BOOST_REQUIRE( get_balance( "alice" ).amount.value == ASSET( "747.000 TESTS" ).amount.value );
    BOOST_REQUIRE( get_balance( "actor0" ).amount.value == ASSET( "4.000 TESTS" ).amount.value );
    BOOST_REQUIRE( get_balance( "actor123" ).amount.value == ASSET( "4.000 TESTS" ).amount.value );
    BOOST_REQUIRE( get_balance( "actor250" ).amount.value == ASSET( "12.000 TESTS" ).amount.value );
    BOOST_REQUIRE( db->get_account( "alice" ).open_recurrent_transfers == 251);
    BOOST_REQUIRE( recurrent_transfer_drift_before.get_trigger_date() == creation_block_time + fc::days(1));
    validate_database();

    auto blocks_until_next_execution = fc::days(1).to_seconds() / HIVE_BLOCK_INTERVAL - 2;
    ilog("generating ${blocks} blocks", ("blocks", blocks_until_next_execution));
    generate_blocks( blocks_until_next_execution );

    BOOST_TEST_MESSAGE( "Testing: executing the first 1000 recurrent transfers for the second time");

    const auto recurrent_transfer_no_drift_after = db->find< recurrent_transfer_object, by_from_to_id >(boost::make_tuple( alice_id, actor0_id) )->copy_chain_object();

    BOOST_REQUIRE( get_balance( "alice" ).amount.value == ASSET( "497.000 TESTS" ).amount.value );
    BOOST_REQUIRE( get_balance( "actor0" ).amount.value == ASSET( "8.000 TESTS" ).amount.value );
    BOOST_REQUIRE( get_balance( "actor123" ).amount.value == ASSET( "8.000 TESTS" ).amount.value );
    BOOST_REQUIRE( get_balance( "actor250" ).amount.value == ASSET( "12.000 TESTS" ).amount.value );
    BOOST_REQUIRE( db->get_account( "alice" ).open_recurrent_transfers == 251);
    BOOST_REQUIRE( recurrent_transfer_no_drift_after.get_trigger_date() ==  recurrent_transfer_no_drift_before.get_trigger_date() + fc::hours(recurrent_transfer_no_drift_before.recurrence));

    BOOST_TEST_MESSAGE( "Executing the remaining 4 recurrent payments for the second time");
    generate_block();

    const auto recurrent_transfer_drift_after = db->find< recurrent_transfer_object, by_from_to_id >(boost::make_tuple( alice_id, actor250_id) )->copy_chain_object();
    BOOST_REQUIRE( get_balance( "alice" ).amount.value == ASSET( "494.000 TESTS" ).amount.value );
    BOOST_REQUIRE( get_balance( "actor0" ).amount.value == ASSET( "8.000 TESTS" ).amount.value );
    BOOST_REQUIRE( get_balance( "actor123" ).amount.value == ASSET( "8.000 TESTS" ).amount.value );
    BOOST_REQUIRE( get_balance( "actor250" ).amount.value == ASSET( "24.000 TESTS" ).amount.value );
    BOOST_REQUIRE( db->get_account( "alice" ).open_recurrent_transfers == 251);
    BOOST_REQUIRE( recurrent_transfer_drift_after.get_trigger_date() ==  recurrent_transfer_drift_before.get_trigger_date() + fc::hours(recurrent_transfer_drift_before.recurrence));
    validate_database();

 }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( account_witness_block_approve_authorities )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: witness_block_approve_operation" );

    ACTORS( (alice)(bob) )

    private_key_type alice_witness_key = generate_private_key( "alice_witness" );
    witness_create( "alice", alice_private_key, "foo.bar", alice_witness_key.get_public_key(), 0 );

    generate_block();

    const witness_object& alice_witness = db->get_witness( "alice" );
    wdump((alice_witness));

    //const account_object& _alice = db->get_account("alice");
    {
      witness_block_approve_operation op;
      op.witness = "mallory";
      op.block_id = db->head_block_id();

      signed_transaction tx;
      tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
      tx.operations.push_back( op );

      BOOST_TEST_MESSAGE( "--- Test failure when no account is not a witness" );
      HIVE_REQUIRE_THROW( push_transaction( tx, 0 ), tx_missing_witness_auth );
    }

    witness_block_approve_operation op;
    op.witness = "alice";
    op.block_id = db->head_block_id();

    signed_transaction tx;
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.operations.push_back( op );

    BOOST_TEST_MESSAGE( "--- Test failure when no signatures" );
    HIVE_REQUIRE_THROW( push_transaction( tx, 0 ), tx_missing_witness_auth );

    BOOST_TEST_MESSAGE( "--- Test failure when signed by a signature not in the account's authority" );
    sign( tx, bob_post_key );
    HIVE_REQUIRE_THROW( push_transaction( tx, 0 ), tx_missing_witness_auth );

    BOOST_TEST_MESSAGE( "--- Test failure when duplicate signatures" );
    tx.signatures.clear();
    sign( tx, alice_witness_key );
    sign( tx, alice_witness_key );
    HIVE_REQUIRE_THROW( push_transaction( tx, 0 ), tx_duplicate_sig );

    BOOST_TEST_MESSAGE( "--- Test failure when signed by an additional signature not in the creator's authority" );
    tx.signatures.clear();
    sign( tx, alice_witness_key );
    sign( tx, alice_private_key );
    HIVE_REQUIRE_THROW( push_transaction( tx, 0 ), tx_irrelevant_sig );

    BOOST_TEST_MESSAGE( "--- Test success with witness signature" );
    tx.signatures.clear();
    sign( tx, alice_witness_key );
    push_transaction( tx, 0 );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()
#endif
