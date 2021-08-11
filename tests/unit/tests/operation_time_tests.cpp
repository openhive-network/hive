#ifdef IS_TEST_NET
#include <boost/test/unit_test.hpp>

#include <hive/chain/hive_fwd.hpp>

#include <hive/protocol/exceptions.hpp>
#include <hive/protocol/hardfork.hpp>

#include <hive/chain/block_summary_object.hpp>
#include <hive/chain/database.hpp>
#include <hive/chain/history_object.hpp>
#include <hive/chain/hive_objects.hpp>

#include <hive/chain/util/reward.hpp>

#include <hive/plugins/debug_node/debug_node_plugin.hpp>
#include <hive/plugins/rc/rc_objects.hpp>
#include <hive/plugins/rc/resource_count.hpp>

#include <fc/crypto/digest.hpp>

#include "../db_fixture/database_fixture.hpp"

#include <cmath>

using namespace hive;
using namespace hive::chain;
using namespace hive::chain::util;
using namespace hive::protocol;

BOOST_FIXTURE_TEST_SUITE( operation_time_tests, clean_database_fixture )

BOOST_AUTO_TEST_CASE( comment_payout_equalize )
{
  try
  {
    ACTORS( (alice)(bob)(dave)
          (ulysses)(vivian)(wendy) )

    struct author_actor
    {
      author_actor(
        const std::string& n,
        fc::ecc::private_key pk,
        fc::optional<asset> mpay = fc::optional<asset>() )
        : name(n), private_key(pk), max_accepted_payout(mpay) {}
      std::string             name;
      fc::ecc::private_key    private_key;
      fc::optional< asset >   max_accepted_payout;
    };

    struct voter_actor
    {
      voter_actor( const std::string& n, fc::ecc::private_key pk, std::string fa )
        : name(n), private_key(pk), favorite_author(fa) {}
      std::string             name;
      fc::ecc::private_key    private_key;
      std::string             favorite_author;
    };


    std::vector< author_actor > authors;
    std::vector< voter_actor > voters;

    authors.emplace_back( "alice", alice_private_key );
    authors.emplace_back( "bob"  , bob_private_key, ASSET( "0.000 TBD" ) );
    authors.emplace_back( "dave" , dave_private_key );
    voters.emplace_back( "ulysses", ulysses_private_key, "alice");
    voters.emplace_back( "vivian" , vivian_private_key , "bob"  );
    voters.emplace_back( "wendy"  , wendy_private_key  , "dave" );

    // A,B,D : posters
    // U,V,W : voters

    // set a ridiculously high HIVE price ($1 / satoshi) to disable dust threshold
    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "0.001 TESTS" ) ) );

    for( const auto& voter : voters )
    {
      fund( voter.name, 10000 );
      vest( voter.name, 10000 );
    }

    // authors all write in the same block, but Bob declines payout
    for( const auto& author : authors )
    {
      signed_transaction tx;
      comment_operation com;
      com.author = author.name;
      com.permlink = "mypost";
      com.parent_author = HIVE_ROOT_POST_PARENT;
      com.parent_permlink = "test";
      com.title = "Hello from "+author.name;
      com.body = "Hello, my name is "+author.name;
      tx.operations.push_back( com );

      if( author.max_accepted_payout.valid() )
      {
        comment_options_operation copt;
        copt.author = com.author;
        copt.permlink = com.permlink;
        copt.max_accepted_payout = *(author.max_accepted_payout);
        tx.operations.push_back( copt );
      }

      tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
      sign( tx, author.private_key );
      db->push_transaction( tx, 0 );
    }

    generate_blocks(1);

    // voters all vote in the same block with the same stake
    for( const auto& voter : voters )
    {
      signed_transaction tx;
      vote_operation vote;
      vote.voter = voter.name;
      vote.author = voter.favorite_author;
      vote.permlink = "mypost";
      vote.weight = HIVE_100_PERCENT;
      tx.operations.push_back( vote );
      tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
      sign( tx, voter.private_key );
      db->push_transaction( tx, 0 );
    }

    //auto& reward_hive = db->get_dynamic_global_properties().get_total_reward_fund_hive();

    // generate a few blocks to seed the reward fund
    generate_blocks(10);
    //const auto& rf = db->get< reward_fund_object, by_name >( HIVE_POST_REWARD_FUND_NAME );
    //idump( (rf) );

    generate_blocks( db->find_comment_cashout( db->get_comment( "alice", string( "mypost" ) ) )->cashout_time, true );
    /*
    for( const auto& author : authors )
    {
      const account_object& a = db->get_account(author.name);
      ilog( "${n} : ${hive} ${hbd}", ("n", author.name)("hive", a.get_rewards())("hbd", a.get_hbd_rewards()) );
    }
    for( const auto& voter : voters )
    {
      const account_object& a = db->get_account(voter.name);
      ilog( "${n} : ${hive} ${hbd}", ("n", voter.name)("hive", a.get_rewards())("hbd", a.get_hbd_rewards()) );
    }
    */

    const account_object& alice_account = db->get_account("alice");
    const account_object& bob_account   = db->get_account("bob");
    const account_object& dave_account  = db->get_account("dave");

    BOOST_CHECK( alice_account.get_hbd_rewards() == ASSET( "6236.000 TBD" ) );
    BOOST_CHECK( bob_account.get_hbd_rewards() == ASSET( "0.000 TBD" ) );
    BOOST_CHECK( dave_account.get_hbd_rewards() == alice_account.get_hbd_rewards() );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( comment_payout_dust )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: comment_payout_dust" );

    ACTORS( (alice)(bob) )
    generate_block();

    vest( HIVE_INIT_MINER_NAME, "alice", ASSET( "10.000 TESTS" ) );
    vest( HIVE_INIT_MINER_NAME, "bob", ASSET( "10.000 TESTS" ) );

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );

    generate_block();
    validate_database();

    comment_operation comment;
    comment.author = "alice";
    comment.permlink = "test";
    comment.parent_permlink = "test";
    comment.title = "test";
    comment.body = "test";

    signed_transaction tx;
    tx.operations.push_back( comment );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    sign( tx, alice_private_key );
    db->push_transaction( tx, 0 );
    validate_database();

    comment.author = "bob";

    tx.clear();
    tx.operations.push_back( comment );
    sign( tx, bob_private_key );
    db->push_transaction( tx, 0 );
    validate_database();

    generate_blocks( db->head_block_time() + db->get_dynamic_global_properties().reverse_auction_seconds );

    vote_operation vote;
    vote.voter = "alice";
    vote.author = "alice";
    vote.permlink = "test";
    vote.weight = 81 * HIVE_1_PERCENT;

    tx.clear();
    tx.operations.push_back( vote );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    sign( tx, alice_private_key );
    db->push_transaction( tx, 0 );
    validate_database();

    vote.voter = "bob";
    vote.author = "bob";
    vote.weight = 59 * HIVE_1_PERCENT;

    tx.clear();
    tx.operations.push_back( vote );
    sign( tx, bob_private_key );
    db->push_transaction( tx, 0 );
    validate_database();

    generate_blocks( db->find_comment_cashout( db->get_comment( "alice", string( "test" ) ) )->cashout_time );

    // If comments are paid out independent of order, then the last satoshi of HIVE cannot be divided among them
    const auto& rf = db->get< reward_fund_object, by_name >( HIVE_POST_REWARD_FUND_NAME );
    BOOST_REQUIRE( rf.reward_balance == ASSET( "0.001 TESTS" ) );

    validate_database();

    BOOST_TEST_MESSAGE( "Done" );
  }
  FC_LOG_AND_RETHROW()
}

/*
BOOST_AUTO_TEST_CASE( reward_funds )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: reward_funds" );

    ACTORS( (alice)(bob) )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TESTS" ), ASSET( "1.000 TBD" ) ) );
    generate_block();

    comment_operation comment;
    vote_operation vote;
    signed_transaction tx;

    comment.author = "alice";
    comment.permlink = "test";
    comment.parent_permlink = "test";
    comment.title = "foo";
    comment.body = "bar";
    vote.voter = "alice";
    vote.author = "alice";
    vote.permlink = "test";
    vote.weight = HIVE_100_PERCENT;
    tx.operations.push_back( comment );
    tx.operations.push_back( vote );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    sign( tx, alice_private_key );
    db->push_transaction( tx, 0 );

    generate_blocks( 5 );

    comment.author = "bob";
    comment.parent_author = "alice";
    vote.voter = "bob";
    vote.author = "bob";
    tx.clear();
    tx.operations.push_back( comment );
    tx.operations.push_back( vote );
    sign( tx, bob_private_key );
    db->push_transaction( tx, 0 );

    generate_blocks( db->get_comment( "alice", string( "test" ) ).cashout_time );

    {
      const auto& post_rf = db->get< reward_fund_object, by_name >( HIVE_POST_REWARD_FUND_NAME );
      const auto& comment_rf = db->get< reward_fund_object, by_name >( HIVE_COMMENT_REWARD_FUND_NAME );

      BOOST_REQUIRE( post_rf.reward_balance.amount == 0 );
      BOOST_REQUIRE( comment_rf.reward_balance.amount > 0 );
      BOOST_REQUIRE( get_hbd_rewards( "alice" ).amount > 0 );
      BOOST_REQUIRE( get_hbd_rewards( "bob" ).amount == 0 );
      validate_database();
    }

    generate_blocks( db->get_comment( "bob", string( "test" ) ).cashout_time );

    {
      const auto& post_rf = db->get< reward_fund_object, by_name >( HIVE_POST_REWARD_FUND_NAME );
      const auto& comment_rf = db->get< reward_fund_object, by_name >( HIVE_COMMENT_REWARD_FUND_NAME );

      BOOST_REQUIRE( post_rf.reward_balance.amount > 0 );
      BOOST_REQUIRE( comment_rf.reward_balance.amount == 0 );
      BOOST_REQUIRE( get_hbd_rewards( "alice" ).amount > 0 );
      BOOST_REQUIRE( get_hbd_rewards( "bob" ).amount > 0 );
      validate_database();
    }
  }
  FC_LOG_AND_RETHROW()
}
*/

BOOST_AUTO_TEST_CASE( recent_claims_decay )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: recent_rshares_2decay" );
    ACTORS( (alice)(bob) )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    comment_operation comment;
    vote_operation vote;
    signed_transaction tx;

    comment.author = "alice";
    comment.permlink = "test";
    comment.parent_permlink = "test";
    comment.title = "foo";
    comment.body = "bar";
    vote.voter = "alice";
    vote.author = "alice";
    vote.permlink = "test";
    vote.weight = HIVE_100_PERCENT;
    tx.operations.push_back( comment );
    tx.operations.push_back( vote );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    sign( tx, alice_private_key );
    db->push_transaction( tx, 0 );

    auto alice_vshares = util::evaluate_reward_curve( db->find_comment_cashout( db->get_comment( "alice", string( "test" ) ) )->net_rshares.value,
      db->get< reward_fund_object, by_name >( HIVE_POST_REWARD_FUND_NAME ).author_reward_curve,
      db->get< reward_fund_object, by_name >( HIVE_POST_REWARD_FUND_NAME ).content_constant );

    generate_blocks( 5 );

    comment.author = "bob";
    vote.voter = "bob";
    vote.author = "bob";
    tx.clear();
    tx.operations.push_back( comment );
    tx.operations.push_back( vote );
    sign( tx, bob_private_key );
    db->push_transaction( tx, 0 );

    generate_blocks( db->find_comment_cashout( db->get_comment( "alice", string( "test" ) ) )->cashout_time );

    {
      const auto& post_rf = db->get< reward_fund_object, by_name >( HIVE_POST_REWARD_FUND_NAME );

      BOOST_REQUIRE( post_rf.recent_claims == alice_vshares );
      validate_database();
    }

    auto bob_cashout_time = db->find_comment_cashout( db->get_comment( "bob", string( "test" ) ) )->cashout_time;
    auto bob_vshares = util::evaluate_reward_curve( db->find_comment_cashout( db->get_comment( "bob", string( "test" ) ) )->net_rshares.value,
      db->get< reward_fund_object, by_name >( HIVE_POST_REWARD_FUND_NAME ).author_reward_curve,
      db->get< reward_fund_object, by_name >( HIVE_POST_REWARD_FUND_NAME ).content_constant );

    generate_block();

    while( db->head_block_time() < bob_cashout_time )
    {
      alice_vshares -= ( alice_vshares * HIVE_BLOCK_INTERVAL ) / HIVE_RECENT_RSHARES_DECAY_TIME_HF19.to_seconds();
      const auto& post_rf = db->get< reward_fund_object, by_name >( HIVE_POST_REWARD_FUND_NAME );

      BOOST_REQUIRE( post_rf.recent_claims == alice_vshares );

      generate_block();

    }

    {
      alice_vshares -= ( alice_vshares * HIVE_BLOCK_INTERVAL ) / HIVE_RECENT_RSHARES_DECAY_TIME_HF19.to_seconds();
      const auto& post_rf = db->get< reward_fund_object, by_name >( HIVE_POST_REWARD_FUND_NAME );

      BOOST_REQUIRE( post_rf.recent_claims == alice_vshares + bob_vshares );
      validate_database();
    }
  }
  FC_LOG_AND_RETHROW()
}

/*
BOOST_AUTO_TEST_CASE( comment_payout )
{
  try
  {
    ACTORS( (alice)(bob)(sam)(dave) )
    fund( "alice", 10000 );
    vest( "alice", 10000 );
    fund( "bob", 7500 );
    vest( "bob", 7500 );
    fund( "sam", 8000 );
    vest( "sam", 8000 );
    fund( "dave", 5000 );
    vest( "dave", 5000 );

    price exchange_rate( ASSET( "1.000 TESTS" ), ASSET( "1.000 TBD" ) );
    set_price_feed( exchange_rate );

    signed_transaction tx;

    BOOST_TEST_MESSAGE( "Creating comments." );

    comment_operation com;
    com.author = "alice";
    com.permlink = "test";
    com.parent_author = "";
    com.parent_permlink = "test";
    com.title = "foo";
    com.body = "bar";
    tx.operations.push_back( com );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    sign( tx, alice_private_key );
    db->push_transaction( tx, 0 );

    tx.operations.clear();
    tx.signatures.clear();

    com.author = "bob";
    tx.operations.push_back( com );
    sign( tx, bob_private_key );
    db->push_transaction( tx, 0 );

    tx.operations.clear();
    tx.signatures.clear();

    BOOST_TEST_MESSAGE( "Voting for comments." );

    vote_operation vote;
    vote.voter = "alice";
    vote.author = "alice";
    vote.permlink = "test";
    vote.weight = HIVE_100_PERCENT;
    tx.operations.push_back( vote );
    sign( tx, alice_private_key );
    db->push_transaction( tx, 0 );

    tx.operations.clear();
    tx.signatures.clear();

    vote.voter = "sam";
    vote.author = "alice";
    tx.operations.push_back( vote );
    sign( tx, sam_private_key );
    db->push_transaction( tx, 0 );

    tx.operations.clear();
    tx.signatures.clear();

    vote.voter = "bob";
    vote.author = "bob";
    tx.operations.push_back( vote );
    sign( tx, bob_private_key );
    db->push_transaction( tx, 0 );

    tx.operations.clear();
    tx.signatures.clear();

    vote.voter = "dave";
    vote.author = "bob";
    tx.operations.push_back( vote );
    sign( tx, dave_private_key );
    db->push_transaction( tx, 0 );

    BOOST_TEST_MESSAGE( "Generate blocks up until first payout" );

    //generate_blocks( db->get_comment( "bob", string( "test" ) ).cashout_time - HIVE_BLOCK_INTERVAL, true );

    auto reward_hive = db->get_dynamic_global_properties().get_total_reward_fund_hive() + ASSET( "1.667 TESTS" );
    auto total_rshares2 = db->get_dynamic_global_properties().total_reward_shares2;
    auto bob_comment_rshares = db->get_comment( "bob", string( "test" ) ).net_rshares;
    auto bob_vest_shares = get_vesting( "bob" );
    auto bob_hbd_balance = get_hbd_balance( "bob" );

    auto bob_comment_payout = asset( ( ( uint128_t( bob_comment_rshares.value ) * bob_comment_rshares.value * reward_hive.amount.value ) / total_rshares2 ).to_uint64(), HIVE_SYMBOL );
    auto bob_comment_discussion_rewards = asset( bob_comment_payout.amount / 4, HIVE_SYMBOL );
    bob_comment_payout -= bob_comment_discussion_rewards;
    auto bob_comment_hbd_reward = db->to_hbd( asset( bob_comment_payout.amount / 2, HIVE_SYMBOL ) );
    auto bob_comment_vesting_reward = ( bob_comment_payout - asset( bob_comment_payout.amount / 2, HIVE_SYMBOL) ) * db->get_dynamic_global_properties().get_vesting_share_price();

    BOOST_TEST_MESSAGE( "Cause first payout" );

    generate_block();

    BOOST_REQUIRE( db->get_dynamic_global_properties().get_total_reward_fund_hive() == reward_hive - bob_comment_payout );
    BOOST_REQUIRE( db->get_comment( "bob", string( "test" ) ).total_payout_value == bob_comment_vesting_reward * db->get_dynamic_global_properties().get_vesting_share_price() + bob_comment_hbd_reward * exchange_rate );
    BOOST_REQUIRE( get_vesting( "bob" ) == bob_vest_shares + bob_comment_vesting_reward );
    BOOST_REQUIRE( get_hbd_balance( "bob" ) == bob_hbd_balance + bob_comment_hbd_reward );

    BOOST_TEST_MESSAGE( "Testing no payout when less than $0.02" );

    tx.operations.clear();
    tx.signatures.clear();
    vote.voter = "alice";
    vote.author = "alice";
    tx.operations.push_back( vote );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    sign( tx, alice_private_key );
    db->push_transaction( tx, 0 );

    tx.operations.clear();
    tx.signatures.clear();
    vote.voter = "dave";
    vote.author = "bob";
    vote.weight = HIVE_1_PERCENT;
    tx.operations.push_back( vote );
    sign( tx, dave_private_key );
    db->push_transaction( tx, 0 );

    generate_blocks( db->get_comment( "bob", string( "test" ) ).cashout_time - HIVE_BLOCK_INTERVAL, true );

    tx.operations.clear();
    tx.signatures.clear();
    vote.voter = "bob";
    vote.author = "alice";
    vote.weight = HIVE_100_PERCENT;
    tx.operations.push_back( vote );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    sign( tx, bob_private_key );
    db->push_transaction( tx, 0 );

    tx.operations.clear();
    tx.signatures.clear();
    vote.voter = "sam";
    tx.operations.push_back( vote );
    sign( tx, sam_private_key );
    db->push_transaction( tx, 0 );

    tx.operations.clear();
    tx.signatures.clear();
    vote.voter = "dave";
    tx.operations.push_back( vote );
    sign( tx, dave_private_key );
    db->push_transaction( tx, 0 );

    bob_vest_shares = get_vesting( "bob" );
    bob_hbd_balance = get_hbd_balance( "bob" );

    validate_database();

    generate_block();

    BOOST_REQUIRE( bob_vest_shares.amount.value == get_vesting( "bob" ).amount.value );
    BOOST_REQUIRE( bob_hbd_balance.amount.value == get_hbd_balance( "bob" ).amount.value );
    validate_database();
  }
  FC_LOG_AND_RETHROW()
}*/

/*
BOOST_AUTO_TEST_CASE( comment_payout )
{
  try
  {
    ACTORS( (alice)(bob)(sam)(dave) )
    fund( "alice", 10000 );
    vest( "alice", 10000 );
    fund( "bob", 7500 );
    vest( "bob", 7500 );
    fund( "sam", 8000 );
    vest( "sam", 8000 );
    fund( "dave", 5000 );
    vest( "dave", 5000 );

    price exchange_rate( ASSET( "1.000 TESTS" ), ASSET( "1.000 TBD" ) );
    set_price_feed( exchange_rate );

    auto& gpo = db->get_dynamic_global_properties();

    signed_transaction tx;

    BOOST_TEST_MESSAGE( "Creating comments. " );

    comment_operation com;
    com.author = "alice";
    com.permlink = "test";
    com.parent_permlink = "test";
    com.title = "foo";
    com.body = "bar";
    tx.operations.push_back( com );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    sign( tx, alice_private_key );
    db->push_transaction( tx, 0 );

    tx.operations.clear();
    tx.signatures.clear();
    com.author = "bob";
    tx.operations.push_back( com );
    sign( tx, bob_private_key );
    db->push_transaction( tx, 0 );

    BOOST_TEST_MESSAGE( "First round of votes." );

    tx.operations.clear();
    tx.signatures.clear();
    vote_operation vote;
    vote.voter = "alice";
    vote.author = "alice";
    vote.permlink = "test";
    vote.weight = HIVE_100_PERCENT;
    tx.operations.push_back( vote );
    sign( tx, alice_private_key );
    db->push_transaction( tx, 0 );

    tx.operations.clear();
    tx.signatures.clear();
    vote.voter = "dave";
    tx.operations.push_back( vote );
    sign( tx, dave_private_key );
    db->push_transaction( tx, 0 );

    tx.operations.clear();
    tx.signatures.clear();
    vote.voter = "bob";
    vote.author = "bob";
    tx.operations.push_back( vote );
    sign( tx, bob_private_key );
    db->push_transaction( tx, 0 );

    tx.operations.clear();
    tx.signatures.clear();
    vote.voter = "sam";
    tx.operations.push_back( vote );
    sign( tx, sam_private_key );
    db->push_transaction( tx, 0 );

    BOOST_TEST_MESSAGE( "Generating blocks..." );

    generate_blocks( fc::time_point_sec( db->head_block_time().sec_since_epoch() + HIVE_CASHOUT_WINDOW_SECONDS / 2 ), true );

    BOOST_TEST_MESSAGE( "Second round of votes." );

    tx.operations.clear();
    tx.signatures.clear();
    vote.voter = "alice";
    vote.author = "bob";
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.operations.push_back( vote );
    sign( tx, alice_private_key );
    db->push_transaction( tx, 0 );

    tx.operations.clear();
    tx.signatures.clear();
    vote.voter = "bob";
    vote.author = "alice";
    tx.operations.push_back( vote );
    sign( tx, bob_private_key );
    db->push_transaction( tx, 0 );

    tx.operations.clear();
    tx.signatures.clear();
    vote.voter = "sam";
    tx.operations.push_back( vote );
    sign( tx, sam_private_key );
    db->push_transaction( tx, 0 );

    BOOST_TEST_MESSAGE( "Generating more blocks..." );

    generate_blocks( db->get_comment( "bob", string( "test" ) ).cashout_time - ( HIVE_BLOCK_INTERVAL / 2 ), true );

    BOOST_TEST_MESSAGE( "Check comments have not been paid out." );

    const auto& vote_idx = db->get_index< comment_vote_index >().indices().get< by_comment_voter >();

    BOOST_REQUIRE( vote_idx.find( boost::make_tuple( db->get_comment( "alice", string( "test" ).id, db->get_account( "alice" ) ).id ) ) != vote_idx.end() );
    BOOST_REQUIRE( vote_idx.find( boost::make_tuple( db->get_comment( "alice", string( "test" ).id, db->get_account( "bob" ) ).id   ) ) != vote_idx.end() );
    BOOST_REQUIRE( vote_idx.find( boost::make_tuple( db->get_comment( "alice", string( "test" ).id, db->get_account( "sam" ) ).id   ) ) != vote_idx.end() );
    BOOST_REQUIRE( vote_idx.find( boost::make_tuple( db->get_comment( "alice", string( "test" ).id, db->get_account( "dave" ) ).id  ) ) != vote_idx.end() );
    BOOST_REQUIRE( vote_idx.find( boost::make_tuple( db->get_comment( "bob", string( "test" ).id,   db->get_account( "alice" ) ).id ) ) != vote_idx.end() );
    BOOST_REQUIRE( vote_idx.find( boost::make_tuple( db->get_comment( "bob", string( "test" ).id,   db->get_account( "bob" ) ).id   ) ) != vote_idx.end() );
    BOOST_REQUIRE( vote_idx.find( boost::make_tuple( db->get_comment( "bob", string( "test" ).id,   db->get_account( "sam" ) ).id   ) ) != vote_idx.end() );
    BOOST_REQUIRE( db->get_comment( "alice", string( "test" ) ).net_rshares.value > 0 );
    BOOST_REQUIRE( db->get_comment( "bob", string( "test" ) ).net_rshares.value > 0 );
    validate_database();

    auto reward_hive = db->get_dynamic_global_properties().get_total_reward_fund_hive() + ASSET( "2.000 TESTS" );
    auto total_rshares2 = db->get_dynamic_global_properties().total_reward_shares2;
    auto bob_comment_vote_total = db->get_comment( "bob", string( "test" ) ).total_vote_weight;
    auto bob_comment_rshares = db->get_comment( "bob", string( "test" ) ).net_rshares;
    auto bob_hbd_balance = get_hbd_balance( "bob" );
    auto alice_vest_shares = get_vesting( "alice" );
    auto bob_vest_shares = get_vesting( "bob" );
    auto sam_vest_shares = get_vesting( "sam" );
    auto dave_vest_shares = get_vesting( "dave" );

    auto bob_comment_payout = asset( ( ( uint128_t( bob_comment_rshares.value ) * bob_comment_rshares.value * reward_hive.amount.value ) / total_rshares2 ).to_uint64(), HIVE_SYMBOL );
    auto bob_comment_vote_rewards = asset( bob_comment_payout.amount / 2, HIVE_SYMBOL );
    bob_comment_payout -= bob_comment_vote_rewards;
    auto bob_comment_hbd_reward = asset( bob_comment_payout.amount / 2, HIVE_SYMBOL ) * exchange_rate;
    auto bob_comment_vesting_reward = ( bob_comment_payout - asset( bob_comment_payout.amount / 2, HIVE_SYMBOL ) ) * db->get_dynamic_global_properties().get_vesting_share_price();
    auto unclaimed_payments = bob_comment_vote_rewards;
    auto alice_vote_reward = asset( static_cast< uint64_t >( ( u256( vote_idx.find( boost::make_tuple( db->get_comment( "bob", string( "test" ).id, db->get_account( "alice" ) ).get_id() ) )->get_weight() ) * bob_comment_vote_rewards.amount.value ) / bob_comment_vote_total ), HIVE_SYMBOL );
    auto alice_vote_vesting = alice_vote_reward * db->get_dynamic_global_properties().get_vesting_share_price();
    auto bob_vote_reward = asset( static_cast< uint64_t >( ( u256( vote_idx.find( boost::make_tuple( db->get_comment( "bob", string( "test" ).id, db->get_account( "bob" ) ).get_id() ) )->get_weight() ) * bob_comment_vote_rewards.amount.value ) / bob_comment_vote_total ), HIVE_SYMBOL );
    auto bob_vote_vesting = bob_vote_reward * db->get_dynamic_global_properties().get_vesting_share_price();
    auto sam_vote_reward = asset( static_cast< uint64_t >( ( u256( vote_idx.find( boost::make_tuple( db->get_comment( "bob", string( "test" ).id, db->get_account( "sam" ) ).get_id() ) )->get_weight() ) * bob_comment_vote_rewards.amount.value ) / bob_comment_vote_total ), HIVE_SYMBOL );
    auto sam_vote_vesting = sam_vote_reward * db->get_dynamic_global_properties().get_vesting_share_price();
    unclaimed_payments -= ( alice_vote_reward + bob_vote_reward + sam_vote_reward );

    BOOST_TEST_MESSAGE( "Generate one block to cause a payout" );

    generate_block();

    auto bob_comment_reward = get_last_operations( 1 )[0].get< comment_reward_operation >();

    BOOST_REQUIRE( db->get_dynamic_global_properties().get_total_reward_fund_hive().amount.value == reward_hive.amount.value - ( bob_comment_payout + bob_comment_vote_rewards - unclaimed_payments ).amount.value );
    BOOST_REQUIRE( db->get_comment( "bob", string( "test" ) ).total_payout_value.amount.value == ( ( bob_comment_vesting_reward * db->get_dynamic_global_properties().get_vesting_share_price() ) + ( bob_comment_hbd_reward * exchange_rate ) ).amount.value );
    BOOST_REQUIRE( get_hbd_balance( "bob" ).amount.value == ( bob_hbd_balance + bob_comment_hbd_reward ).amount.value );
    BOOST_REQUIRE( db->get_comment( "alice", string( "test" ) ).net_rshares.value > 0 );
    BOOST_REQUIRE( db->get_comment( "bob", string( "test" ) ).net_rshares.value == 0 );
    BOOST_REQUIRE( get_vesting( "alice" ).amount.value == ( alice_vest_shares + alice_vote_vesting ).amount.value );
    BOOST_REQUIRE( get_vesting( "bob" ).amount.value == ( bob_vest_shares + bob_vote_vesting + bob_comment_vesting_reward ).amount.value );
    BOOST_REQUIRE( get_vesting( "sam" ).amount.value == ( sam_vest_shares + sam_vote_vesting ).amount.value );
    BOOST_REQUIRE( get_vesting( "dave" ).amount.value == dave_vest_shares.amount.value );
    BOOST_REQUIRE( bob_comment_reward.author == "bob" );
    BOOST_REQUIRE( bob_comment_reward.permlink == "test" );
    BOOST_REQUIRE( bob_comment_reward.payout.amount.value == bob_comment_hbd_reward.amount.value );
    BOOST_REQUIRE( bob_comment_reward.vesting_payout.amount.value == bob_comment_vesting_reward.amount.value );
    BOOST_REQUIRE( vote_idx.find( boost::make_tuple( db->get_comment( "alice", string( "test" ).id, db->get_account( "alice" ) ).id ) ) != vote_idx.end() );
    BOOST_REQUIRE( vote_idx.find( boost::make_tuple( db->get_comment( "alice", string( "test" ).id, db->get_account( "bob" ) ).id   ) ) != vote_idx.end() );
    BOOST_REQUIRE( vote_idx.find( boost::make_tuple( db->get_comment( "alice", string( "test" ).id, db->get_account( "sam" ) ).id   ) ) != vote_idx.end() );
    BOOST_REQUIRE( vote_idx.find( boost::make_tuple( db->get_comment( "alice", string( "test" ).id, db->get_account( "dave" ) ).id  ) ) != vote_idx.end() );
    BOOST_REQUIRE( vote_idx.find( boost::make_tuple( db->get_comment( "bob", string( "test" ).id,   db->get_account( "alice" ) ).id ) ) == vote_idx.end() );
    BOOST_REQUIRE( vote_idx.find( boost::make_tuple( db->get_comment( "bob", string( "test" ).id,   db->get_account( "bob" ) ).id   ) ) == vote_idx.end() );
    BOOST_REQUIRE( vote_idx.find( boost::make_tuple( db->get_comment( "bob", string( "test" ).id,   db->get_account( "sam" ) ).id   ) ) == vote_idx.end() );
    validate_database();

    BOOST_TEST_MESSAGE( "Generating blocks up to next comment payout" );

    generate_blocks( db->get_comment( "alice", string( "test" ) ).cashout_time - ( HIVE_BLOCK_INTERVAL / 2 ), true );

    BOOST_REQUIRE( vote_idx.find( boost::make_tuple( db->get_comment( "alice", string( "test" ).id, db->get_account( "alice" ) ).id ) ) != vote_idx.end() );
    BOOST_REQUIRE( vote_idx.find( boost::make_tuple( db->get_comment( "alice", string( "test" ).id, db->get_account( "bob" ) ).id   ) ) != vote_idx.end() );
    BOOST_REQUIRE( vote_idx.find( boost::make_tuple( db->get_comment( "alice", string( "test" ).id, db->get_account( "sam" ) ).id   ) ) != vote_idx.end() );
    BOOST_REQUIRE( vote_idx.find( boost::make_tuple( db->get_comment( "alice", string( "test" ).id, db->get_account( "dave" ) ).id  ) ) != vote_idx.end() );
    BOOST_REQUIRE( vote_idx.find( boost::make_tuple( db->get_comment( "bob", string( "test" ).id,   db->get_account( "alice" ) ).id ) ) == vote_idx.end() );
    BOOST_REQUIRE( vote_idx.find( boost::make_tuple( db->get_comment( "bob", string( "test" ).id,   db->get_account( "bob" ) ).id   ) ) == vote_idx.end() );
    BOOST_REQUIRE( vote_idx.find( boost::make_tuple( db->get_comment( "bob", string( "test" ).id,   db->get_account( "sam" ) ).id   ) ) == vote_idx.end() );
    BOOST_REQUIRE( db->get_comment( "alice", string( "test" ) ).net_rshares.value > 0 );
    BOOST_REQUIRE( db->get_comment( "bob", string( "test" ) ).net_rshares.value == 0 );
    validate_database();

    BOOST_TEST_MESSAGE( "Generate block to cause payout" );

    reward_hive = db->get_dynamic_global_properties().get_total_reward_fund_hive() + ASSET( "2.000 TESTS" );
    total_rshares2 = db->get_dynamic_global_properties().total_reward_shares2;
    auto alice_comment_vote_total = db->get_comment( "alice", string( "test" ) ).total_vote_weight;
    auto alice_comment_rshares = db->get_comment( "alice", string( "test" ) ).net_rshares;
    auto alice_hbd_balance = get_hbd_balance( "alice" );
    alice_vest_shares = get_vesting( "alice" );
    bob_vest_shares = get_vesting( "bob" );
    sam_vest_shares = get_vesting( "sam" );
    dave_vest_shares = get_vesting( "dave" );

    u256 rs( alice_comment_rshares.value );
    u256 rf( reward_hive.amount.value );
    u256 trs2 = total_rshares2.hi;
    trs2 = ( trs2 << 64 ) + total_rshares2.lo;
    auto rs2 = rs*rs;

    auto alice_comment_payout = asset( static_cast< uint64_t >( ( rf * rs2 ) / trs2 ), HIVE_SYMBOL );
    auto alice_comment_vote_rewards = asset( alice_comment_payout.amount / 2, HIVE_SYMBOL );
    alice_comment_payout -= alice_comment_vote_rewards;
    auto alice_comment_hbd_reward = asset( alice_comment_payout.amount / 2, HIVE_SYMBOL ) * exchange_rate;
    auto alice_comment_vesting_reward = ( alice_comment_payout - asset( alice_comment_payout.amount / 2, HIVE_SYMBOL ) ) * db->get_dynamic_global_properties().get_vesting_share_price();
    unclaimed_payments = alice_comment_vote_rewards;
    alice_vote_reward = asset( static_cast< uint64_t >( ( u256( vote_idx.find( boost::make_tuple( db->get_comment( "alice", string( "test" ).id, db->get_account( "alice" ) ).get_id() ) )->get_weight() ) * alice_comment_vote_rewards.amount.value ) / alice_comment_vote_total ), HIVE_SYMBOL );
    alice_vote_vesting = alice_vote_reward * db->get_dynamic_global_properties().get_vesting_share_price();
    bob_vote_reward = asset( static_cast< uint64_t >( ( u256( vote_idx.find( boost::make_tuple( db->get_comment( "alice", string( "test" ).id, db->get_account( "bob" ) ).get_id() ) )->get_weight() ) * alice_comment_vote_rewards.amount.value ) / alice_comment_vote_total ), HIVE_SYMBOL );
    bob_vote_vesting = bob_vote_reward * db->get_dynamic_global_properties().get_vesting_share_price();
    sam_vote_reward = asset( static_cast< uint64_t >( ( u256( vote_idx.find( boost::make_tuple( db->get_comment( "alice", string( "test" ).id, db->get_account( "sam" ) ).get_id() ) )->get_weight() ) * alice_comment_vote_rewards.amount.value ) / alice_comment_vote_total ), HIVE_SYMBOL );
    sam_vote_vesting = sam_vote_reward * db->get_dynamic_global_properties().get_vesting_share_price();
    auto dave_vote_reward = asset( static_cast< uint64_t >( ( u256( vote_idx.find( boost::make_tuple( db->get_comment( "alice", string( "test" ).id, db->get_account( "dave" ) ).get_id() ) )->get_weight() ) * alice_comment_vote_rewards.amount.value ) / alice_comment_vote_total ), HIVE_SYMBOL );
    auto dave_vote_vesting = dave_vote_reward * db->get_dynamic_global_properties().get_vesting_share_price();
    unclaimed_payments -= ( alice_vote_reward + bob_vote_reward + sam_vote_reward + dave_vote_reward );

    generate_block();
    auto alice_comment_reward = get_last_operations( 1 )[0].get< comment_reward_operation >();

    BOOST_REQUIRE( ( db->get_dynamic_global_properties().get_total_reward_fund_hive() + alice_comment_payout + alice_comment_vote_rewards - unclaimed_payments ).amount.value == reward_hive.amount.value );
    BOOST_REQUIRE( db->get_comment( "alice", string( "test" ) ).total_payout_value.amount.value == ( ( alice_comment_vesting_reward * db->get_dynamic_global_properties().get_vesting_share_price() ) + ( alice_comment_hbd_reward * exchange_rate ) ).amount.value );
    BOOST_REQUIRE( get_hbd_balance( "alice" ).amount.value == ( alice_hbd_balance + alice_comment_hbd_reward ).amount.value );
    BOOST_REQUIRE( db->get_comment( "alice", string( "test" ) ).net_rshares.value == 0 );
    BOOST_REQUIRE( db->get_comment( "alice", string( "test" ) ).net_rshares.value == 0 );
    BOOST_REQUIRE( get_vesting( "alice" ).amount.value == ( alice_vest_shares + alice_vote_vesting + alice_comment_vesting_reward ).amount.value );
    BOOST_REQUIRE( get_vesting( "bob" ).amount.value == ( bob_vest_shares + bob_vote_vesting ).amount.value );
    BOOST_REQUIRE( get_vesting( "sam" ).amount.value == ( sam_vest_shares + sam_vote_vesting ).amount.value );
    BOOST_REQUIRE( get_vesting( "dave" ).amount.value == ( dave_vest_shares + dave_vote_vesting ).amount.value );
    BOOST_REQUIRE( alice_comment_reward.author == "alice" );
    BOOST_REQUIRE( alice_comment_reward.permlink == "test" );
    BOOST_REQUIRE( alice_comment_reward.payout.amount.value == alice_comment_hbd_reward.amount.value );
    BOOST_REQUIRE( alice_comment_reward.vesting_payout.amount.value == alice_comment_vesting_reward.amount.value );
    BOOST_REQUIRE( vote_idx.find( boost::make_tuple( db->get_comment( "alice", string( "test" ).id, db->get_account( "alice" ) ).id ) ) == vote_idx.end() );
    BOOST_REQUIRE( vote_idx.find( boost::make_tuple( db->get_comment( "alice", string( "test" ).id, db->get_account( "bob" ) ).id   ) ) == vote_idx.end() );
    BOOST_REQUIRE( vote_idx.find( boost::make_tuple( db->get_comment( "alice", string( "test" ).id, db->get_account( "sam" ) ).id   ) ) == vote_idx.end() );
    BOOST_REQUIRE( vote_idx.find( boost::make_tuple( db->get_comment( "alice", string( "test" ).id, db->get_account( "dave" ) ).id  ) ) == vote_idx.end() );
    BOOST_REQUIRE( vote_idx.find( boost::make_tuple( db->get_comment( "bob", string( "test" ).id,   db->get_account( "alice" ) ).id ) ) == vote_idx.end() );
    BOOST_REQUIRE( vote_idx.find( boost::make_tuple( db->get_comment( "bob", string( "test" ).id,   db->get_account( "bob" ) ).id   ) ) == vote_idx.end() );
    BOOST_REQUIRE( vote_idx.find( boost::make_tuple( db->get_comment( "bob", string( "test" ).id,   db->get_account( "sam" ) ).id   ) ) == vote_idx.end() );
    validate_database();

    BOOST_TEST_MESSAGE( "Testing no payout when less than $0.02" );

    tx.operations.clear();
    tx.signatures.clear();
    vote.voter = "alice";
    vote.author = "alice";
    tx.operations.push_back( vote );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    sign( tx, alice_private_key );
    db->push_transaction( tx, 0 );

    tx.operations.clear();
    tx.signatures.clear();
    vote.voter = "dave";
    vote.author = "bob";
    vote.weight = HIVE_1_PERCENT;
    tx.operations.push_back( vote );
    sign( tx, dave_private_key );
    db->push_transaction( tx, 0 );

    generate_blocks( db->get_comment( "bob", string( "test" ) ).cashout_time - HIVE_BLOCK_INTERVAL, true );

    tx.operations.clear();
    tx.signatures.clear();
    vote.voter = "bob";
    vote.author = "alice";
    vote.weight = HIVE_100_PERCENT;
    tx.operations.push_back( vote );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    sign( tx, bob_private_key );
    db->push_transaction( tx, 0 );

    tx.operations.clear();
    tx.signatures.clear();
    vote.voter = "sam";
    tx.operations.push_back( vote );
    sign( tx, sam_private_key );
    db->push_transaction( tx, 0 );

    tx.operations.clear();
    tx.signatures.clear();
    vote.voter = "dave";
    tx.operations.push_back( vote );
    sign( tx, dave_private_key );
    db->push_transaction( tx, 0 );

    bob_vest_shares = get_vesting( "bob" );
    auto bob_hbd = get_hbd_balance( "bob" );

    BOOST_REQUIRE( vote_idx.find( boost::make_tuple( db->get_comment( "bob", string( "test" ).id, db->get_account( "dave" ) ).id ) ) != vote_idx.end() );
    validate_database();

    generate_block();

    BOOST_REQUIRE( vote_idx.find( boost::make_tuple( db->get_comment( "bob", string( "test" ).id, db->get_account( "dave" ) ).id ) ) == vote_idx.end() );
    BOOST_REQUIRE( bob_vest_shares.amount.value == get_vesting( "bob" ).amount.value );
    BOOST_REQUIRE( bob_hbd.amount.value == get_hbd_balance( "bob" ).amount.value );
    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

OOST_AUTO_TEST_CASE( nested_comments )
{
  try
  {
    ACTORS( (alice)(bob)(sam)(dave) )
    fund( "alice", 10000 );
    vest( "alice", 10000 );
    fund( "bob", 10000 );
    vest( "bob", 10000 );
    fund( "sam", 10000 );
    vest( "sam", 10000 );
    fund( "dave", 10000 );
    vest( "dave", 10000 );

    price exchange_rate( ASSET( "1.000 TESTS" ), ASSET( "1.000 TBD" ) );
    set_price_feed( exchange_rate );

    signed_transaction tx;
    comment_operation comment_op;
    comment_op.author = "alice";
    comment_op.permlink = "test";
    comment_op.parent_permlink = "test";
    comment_op.title = "foo";
    comment_op.body = "bar";
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.operations.push_back( comment_op );
    sign( tx, alice_private_key );
    db->push_transaction( tx, 0 );

    comment_op.author = "bob";
    comment_op.parent_author = "alice";
    comment_op.parent_permlink = "test";
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( comment_op );
    sign( tx, bob_private_key );
    db->push_transaction( tx, 0 );

    comment_op.author = "sam";
    comment_op.parent_author = "bob";
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( comment_op );
    sign( tx, sam_private_key );
    db->push_transaction( tx, 0 );

    comment_op.author = "dave";
    comment_op.parent_author = "sam";
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( comment_op );
    sign( tx, dave_private_key );
    db->push_transaction( tx, 0 );

    tx.operations.clear();
    tx.signatures.clear();

    vote_operation vote_op;
    vote_op.voter = "alice";
    vote_op.author = "alice";
    vote_op.permlink = "test";
    vote_op.weight = HIVE_100_PERCENT;
    tx.operations.push_back( vote_op );
    vote_op.author = "bob";
    tx.operations.push_back( vote_op );
    sign( tx, alice_private_key );
    db->push_transaction( tx, 0 );

    tx.operations.clear();
    tx.signatures.clear();
    vote_op.voter = "bob";
    vote_op.author = "alice";
    tx.operations.push_back( vote_op );
    vote_op.author = "bob";
    tx.operations.push_back( vote_op );
    vote_op.author = "dave";
    vote_op.weight = HIVE_1_PERCENT * 20;
    tx.operations.push_back( vote_op );
    sign( tx, bob_private_key );
    db->push_transaction( tx, 0 );

    tx.operations.clear();
    tx.signatures.clear();
    vote_op.voter = "sam";
    vote_op.author = "bob";
    vote_op.weight = HIVE_100_PERCENT;
    tx.operations.push_back( vote_op );
    sign( tx, sam_private_key );
    db->push_transaction( tx, 0 );

    generate_blocks( db->get_comment( "alice", string( "test" ) ).cashout_time - fc::seconds( HIVE_BLOCK_INTERVAL ), true );

    auto& gpo = db->get_dynamic_global_properties();
    uint128_t reward_hive = gpo.get_total_reward_fund_hive().amount.value + ASSET( "2.000 TESTS" ).amount.value;
    uint128_t total_rshares2 = gpo.total_reward_shares2;

    auto alice_comment = db->get_comment( "alice", string( "test" ) );
    auto bob_comment = db->get_comment( "bob", string( "test" ) );
    auto sam_comment = db->get_comment( "sam", string( "test" ) );
    auto dave_comment = db->get_comment( "dave", string( "test" ) );

    const auto& vote_idx = db->get_index< comment_vote_index >().indices().get< by_comment_voter >();

    // Calculate total comment rewards and voting rewards.
    auto alice_comment_reward = ( ( reward_hive * alice_comment.net_rshares.value * alice_comment.net_rshares.value ) / total_rshares2 ).to_uint64();
    total_rshares2 -= uint128_t( alice_comment.net_rshares.value ) * ( alice_comment.net_rshares.value );
    reward_hive -= alice_comment_reward;
    auto alice_comment_vote_rewards = alice_comment_reward / 2;
    alice_comment_reward -= alice_comment_vote_rewards;

    auto alice_vote_alice_reward = asset( static_cast< uint64_t >( ( u256( vote_idx.find( boost::make_tuple( db->get_comment( "alice", string( "test" ).id, db->get_account( "alice" ) ).get_id() ) )->get_weight() ) * alice_comment_vote_rewards ) / alice_comment.total_vote_weight ), HIVE_SYMBOL );
    auto bob_vote_alice_reward = asset( static_cast< uint64_t >( ( u256( vote_idx.find( boost::make_tuple( db->get_comment( "alice", string( "test" ).id, db->get_account( "bob" ) ).get_id() ) )->get_weight() ) * alice_comment_vote_rewards ) / alice_comment.total_vote_weight ), HIVE_SYMBOL );
    reward_hive += alice_comment_vote_rewards - ( alice_vote_alice_reward + bob_vote_alice_reward ).amount.value;

    auto bob_comment_reward = ( ( reward_hive * bob_comment.net_rshares.value * bob_comment.net_rshares.value ) / total_rshares2 ).to_uint64();
    total_rshares2 -= uint128_t( bob_comment.net_rshares.value ) * bob_comment.net_rshares.value;
    reward_hive -= bob_comment_reward;
    auto bob_comment_vote_rewards = bob_comment_reward / 2;
    bob_comment_reward -= bob_comment_vote_rewards;

    auto alice_vote_bob_reward = asset( static_cast< uint64_t >( ( u256( vote_idx.find( boost::make_tuple( db->get_comment( "bob", string( "test" ).id, db->get_account( "alice" ) ).get_id() ) )->get_weight() ) * bob_comment_vote_rewards ) / bob_comment.total_vote_weight ), HIVE_SYMBOL );
    auto bob_vote_bob_reward = asset( static_cast< uint64_t >( ( u256( vote_idx.find( boost::make_tuple( db->get_comment( "bob", string( "test" ).id, db->get_account( "bob" ) ).get_id() ) )->get_weight() ) * bob_comment_vote_rewards ) / bob_comment.total_vote_weight ), HIVE_SYMBOL );
    auto sam_vote_bob_reward = asset( static_cast< uint64_t >( ( u256( vote_idx.find( boost::make_tuple( db->get_comment( "bob", string( "test" ).id, db->get_account( "sam" ) ).get_id() ) )->get_weight() ) * bob_comment_vote_rewards ) / bob_comment.total_vote_weight ), HIVE_SYMBOL );
    reward_hive += bob_comment_vote_rewards - ( alice_vote_bob_reward + bob_vote_bob_reward + sam_vote_bob_reward ).amount.value;

    auto dave_comment_reward = ( ( reward_hive * dave_comment.net_rshares.value * dave_comment.net_rshares.value ) / total_rshares2 ).to_uint64();
    total_rshares2 -= uint128_t( dave_comment.net_rshares.value ) * dave_comment.net_rshares.value;
    reward_hive -= dave_comment_reward;
    auto dave_comment_vote_rewards = dave_comment_reward / 2;
    dave_comment_reward -= dave_comment_vote_rewards;

    auto bob_vote_dave_reward = asset( static_cast< uint64_t >( ( u256( vote_idx.find( boost::make_tuple( db->get_comment( "dave", string( "test" ).id, db->get_account( "bob" ) ).get_id() ) )->get_weight() ) * dave_comment_vote_rewards ) / dave_comment.total_vote_weight ), HIVE_SYMBOL );
    reward_hive += dave_comment_vote_rewards - bob_vote_dave_reward.amount.value;

    // Calculate rewards paid to parent posts
    auto alice_pays_alice_hbd = alice_comment_reward / 2;
    auto alice_pays_alice_vest = alice_comment_reward - alice_pays_alice_hbd;
    auto bob_pays_bob_hbd = bob_comment_reward / 2;
    auto bob_pays_bob_vest = bob_comment_reward - bob_pays_bob_hbd;
    auto dave_pays_dave_hbd = dave_comment_reward / 2;
    auto dave_pays_dave_vest = dave_comment_reward - dave_pays_dave_hbd;

    auto bob_pays_alice_hbd = bob_pays_bob_hbd / 2;
    auto bob_pays_alice_vest = bob_pays_bob_vest / 2;
    bob_pays_bob_hbd -= bob_pays_alice_hbd;
    bob_pays_bob_vest -= bob_pays_alice_vest;

    auto dave_pays_sam_hbd = dave_pays_dave_hbd / 2;
    auto dave_pays_sam_vest = dave_pays_dave_vest / 2;
    dave_pays_dave_hbd -= dave_pays_sam_hbd;
    dave_pays_dave_vest -= dave_pays_sam_vest;
    auto dave_pays_bob_hbd = dave_pays_sam_hbd / 2;
    auto dave_pays_bob_vest = dave_pays_sam_vest / 2;
    dave_pays_sam_hbd -= dave_pays_bob_hbd;
    dave_pays_sam_vest -= dave_pays_bob_vest;
    auto dave_pays_alice_hbd = dave_pays_bob_hbd / 2;
    auto dave_pays_alice_vest = dave_pays_bob_vest / 2;
    dave_pays_bob_hbd -= dave_pays_alice_hbd;
    dave_pays_bob_vest -= dave_pays_alice_vest;

    // Calculate total comment payouts
    auto alice_comment_total_payout = db->to_hbd( asset( alice_pays_alice_hbd + alice_pays_alice_vest, HIVE_SYMBOL ) );
    alice_comment_total_payout += db->to_hbd( asset( bob_pays_alice_hbd + bob_pays_alice_vest, HIVE_SYMBOL ) );
    alice_comment_total_payout += db->to_hbd( asset( dave_pays_alice_hbd + dave_pays_alice_vest, HIVE_SYMBOL ) );
    auto bob_comment_total_payout = db->to_hbd( asset( bob_pays_bob_hbd + bob_pays_bob_vest, HIVE_SYMBOL ) );
    bob_comment_total_payout += db->to_hbd( asset( dave_pays_bob_hbd + dave_pays_bob_vest, HIVE_SYMBOL ) );
    auto sam_comment_total_payout = db->to_hbd( asset( dave_pays_sam_hbd + dave_pays_sam_vest, HIVE_SYMBOL ) );
    auto dave_comment_total_payout = db->to_hbd( asset( dave_pays_dave_hbd + dave_pays_dave_vest, HIVE_SYMBOL ) );

    auto alice_starting_vesting = get_vesting( "alice" );
    auto alice_starting_hbd = get_hbd_balance( "alice" );
    auto bob_starting_vesting = get_vesting( "bob" );
    auto bob_starting_hbd = get_hbd_balance( "bob" );
    auto sam_starting_vesting = get_vesting( "sam" );
    auto sam_starting_hbd = get_hbd_balance( "sam" );
    auto dave_starting_vesting = get_vesting( "dave" );
    auto dave_starting_hbd = get_hbd_balance( "dave" );

    generate_block();

    // Calculate vesting share rewards from voting.
    auto alice_vote_alice_vesting = alice_vote_alice_reward * gpo.get_vesting_share_price();
    auto bob_vote_alice_vesting = bob_vote_alice_reward * gpo.get_vesting_share_price();
    auto alice_vote_bob_vesting = alice_vote_bob_reward * gpo.get_vesting_share_price();
    auto bob_vote_bob_vesting = bob_vote_bob_reward * gpo.get_vesting_share_price();
    auto sam_vote_bob_vesting = sam_vote_bob_reward * gpo.get_vesting_share_price();
    auto bob_vote_dave_vesting = bob_vote_dave_reward * gpo.get_vesting_share_price();

    BOOST_REQUIRE( db->get_comment( "alice", string( "test" ) ).total_payout_value.amount.value == alice_comment_total_payout.amount.value );
    BOOST_REQUIRE( db->get_comment( "bob", string( "test" ) ).total_payout_value.amount.value == bob_comment_total_payout.amount.value );
    BOOST_REQUIRE( db->get_comment( "sam", string( "test" ) ).total_payout_value.amount.value == sam_comment_total_payout.amount.value );
    BOOST_REQUIRE( db->get_comment( "dave", string( "test" ) ).total_payout_value.amount.value == dave_comment_total_payout.amount.value );

    // ops 0-3, 5-6, and 10 are comment reward ops
    auto ops = get_last_operations( 13 );

    BOOST_TEST_MESSAGE( "Checking Virtual Operation Correctness" );

    curate_reward_operation cur_vop;
    comment_reward_operation com_vop = ops[0].get< comment_reward_operation >();

    BOOST_REQUIRE( com_vop.author == "alice" );
    BOOST_REQUIRE( com_vop.permlink == "test" );
    BOOST_REQUIRE( com_vop.originating_author == "dave" );
    BOOST_REQUIRE( com_vop.originating_permlink == "test" );
    BOOST_REQUIRE( com_vop.payout.amount.value == dave_pays_alice_hbd );
    BOOST_REQUIRE( ( com_vop.vesting_payout * gpo.get_vesting_share_price() ).amount.value == dave_pays_alice_vest );

    com_vop = ops[1].get< comment_reward_operation >();
    BOOST_REQUIRE( com_vop.author == "bob" );
    BOOST_REQUIRE( com_vop.permlink == "test" );
    BOOST_REQUIRE( com_vop.originating_author == "dave" );
    BOOST_REQUIRE( com_vop.originating_permlink == "test" );
    BOOST_REQUIRE( com_vop.payout.amount.value == dave_pays_bob_hbd );
    BOOST_REQUIRE( ( com_vop.vesting_payout * gpo.get_vesting_share_price() ).amount.value == dave_pays_bob_vest );

    com_vop = ops[2].get< comment_reward_operation >();
    BOOST_REQUIRE( com_vop.author == "sam" );
    BOOST_REQUIRE( com_vop.permlink == "test" );
    BOOST_REQUIRE( com_vop.originating_author == "dave" );
    BOOST_REQUIRE( com_vop.originating_permlink == "test" );
    BOOST_REQUIRE( com_vop.payout.amount.value == dave_pays_sam_hbd );
    BOOST_REQUIRE( ( com_vop.vesting_payout * gpo.get_vesting_share_price() ).amount.value == dave_pays_sam_vest );

    com_vop = ops[3].get< comment_reward_operation >();
    BOOST_REQUIRE( com_vop.author == "dave" );
    BOOST_REQUIRE( com_vop.permlink == "test" );
    BOOST_REQUIRE( com_vop.originating_author == "dave" );
    BOOST_REQUIRE( com_vop.originating_permlink == "test" );
    BOOST_REQUIRE( com_vop.payout.amount.value == dave_pays_dave_hbd );
    BOOST_REQUIRE( ( com_vop.vesting_payout * gpo.get_vesting_share_price() ).amount.value == dave_pays_dave_vest );

    cur_vop = ops[4].get< curate_reward_operation >();
    BOOST_REQUIRE( cur_vop.curator == "bob" );
    BOOST_REQUIRE( cur_vop.reward.amount.value == bob_vote_dave_vesting.amount.value );
    BOOST_REQUIRE( cur_vop.comment_author == "dave" );
    BOOST_REQUIRE( cur_vop.comment_permlink == "test" );

    com_vop = ops[5].get< comment_reward_operation >();
    BOOST_REQUIRE( com_vop.author == "alice" );
    BOOST_REQUIRE( com_vop.permlink == "test" );
    BOOST_REQUIRE( com_vop.originating_author == "bob" );
    BOOST_REQUIRE( com_vop.originating_permlink == "test" );
    BOOST_REQUIRE( com_vop.payout.amount.value == bob_pays_alice_hbd );
    BOOST_REQUIRE( ( com_vop.vesting_payout * gpo.get_vesting_share_price() ).amount.value == bob_pays_alice_vest );

    com_vop = ops[6].get< comment_reward_operation >();
    BOOST_REQUIRE( com_vop.author == "bob" );
    BOOST_REQUIRE( com_vop.permlink == "test" );
    BOOST_REQUIRE( com_vop.originating_author == "bob" );
    BOOST_REQUIRE( com_vop.originating_permlink == "test" );
    BOOST_REQUIRE( com_vop.payout.amount.value == bob_pays_bob_hbd );
    BOOST_REQUIRE( ( com_vop.vesting_payout * gpo.get_vesting_share_price() ).amount.value == bob_pays_bob_vest );

    cur_vop = ops[7].get< curate_reward_operation >();
    BOOST_REQUIRE( cur_vop.curator == "sam" );
    BOOST_REQUIRE( cur_vop.reward.amount.value == sam_vote_bob_vesting.amount.value );
    BOOST_REQUIRE( cur_vop.comment_author == "bob" );
    BOOST_REQUIRE( cur_vop.comment_permlink == "test" );

    cur_vop = ops[8].get< curate_reward_operation >();
    BOOST_REQUIRE( cur_vop.curator == "bob" );
    BOOST_REQUIRE( cur_vop.reward.amount.value == bob_vote_bob_vesting.amount.value );
    BOOST_REQUIRE( cur_vop.comment_author == "bob" );
    BOOST_REQUIRE( cur_vop.comment_permlink == "test" );

    cur_vop = ops[9].get< curate_reward_operation >();
    BOOST_REQUIRE( cur_vop.curator == "alice" );
    BOOST_REQUIRE( cur_vop.reward.amount.value == alice_vote_bob_vesting.amount.value );
    BOOST_REQUIRE( cur_vop.comment_author == "bob" );
    BOOST_REQUIRE( cur_vop.comment_permlink == "test" );

    com_vop = ops[10].get< comment_reward_operation >();
    BOOST_REQUIRE( com_vop.author == "alice" );
    BOOST_REQUIRE( com_vop.permlink == "test" );
    BOOST_REQUIRE( com_vop.originating_author == "alice" );
    BOOST_REQUIRE( com_vop.originating_permlink == "test" );
    BOOST_REQUIRE( com_vop.payout.amount.value == alice_pays_alice_hbd );
    BOOST_REQUIRE( ( com_vop.vesting_payout * gpo.get_vesting_share_price() ).amount.value == alice_pays_alice_vest );

    cur_vop = ops[11].get< curate_reward_operation >();
    BOOST_REQUIRE( cur_vop.curator == "bob" );
    BOOST_REQUIRE( cur_vop.reward.amount.value == bob_vote_alice_vesting.amount.value );
    BOOST_REQUIRE( cur_vop.comment_author == "alice" );
    BOOST_REQUIRE( cur_vop.comment_permlink == "test" );

    cur_vop = ops[12].get< curate_reward_operation >();
    BOOST_REQUIRE( cur_vop.curator == "alice" );
    BOOST_REQUIRE( cur_vop.reward.amount.value == alice_vote_alice_vesting.amount.value );
    BOOST_REQUIRE( cur_vop.comment_author == "alice" );
    BOOST_REQUIRE( cur_vop.comment_permlink == "test" );

    BOOST_TEST_MESSAGE( "Checking account balances" );

    auto alice_total_hbd = alice_starting_hbd + asset( alice_pays_alice_hbd + bob_pays_alice_hbd + dave_pays_alice_hbd, HIVE_SYMBOL ) * exchange_rate;
    auto alice_total_vesting = alice_starting_vesting + asset( alice_pays_alice_vest + bob_pays_alice_vest + dave_pays_alice_vest + alice_vote_alice_reward.amount + alice_vote_bob_reward.amount, HIVE_SYMBOL ) * gpo.get_vesting_share_price();
    BOOST_REQUIRE( get_hbd_balance( "alice" ).amount.value == alice_total_hbd.amount.value );
    BOOST_REQUIRE( get_vesting( "alice" ).amount.value == alice_total_vesting.amount.value );

    auto bob_total_hbd = bob_starting_hbd + asset( bob_pays_bob_hbd + dave_pays_bob_hbd, HIVE_SYMBOL ) * exchange_rate;
    auto bob_total_vesting = bob_starting_vesting + asset( bob_pays_bob_vest + dave_pays_bob_vest + bob_vote_alice_reward.amount + bob_vote_bob_reward.amount + bob_vote_dave_reward.amount, HIVE_SYMBOL ) * gpo.get_vesting_share_price();
    BOOST_REQUIRE( get_hbd_balance( "bob" ).amount.value == bob_total_hbd.amount.value );
    BOOST_REQUIRE( get_vesting( "bob" ).amount.value == bob_total_vesting.amount.value );

    auto sam_total_hbd = sam_starting_hbd + asset( dave_pays_sam_hbd, HIVE_SYMBOL ) * exchange_rate;
    auto sam_total_vesting = bob_starting_vesting + asset( dave_pays_sam_vest + sam_vote_bob_reward.amount, HIVE_SYMBOL ) * gpo.get_vesting_share_price();
    BOOST_REQUIRE( get_hbd_balance( "sam" ).amount.value == sam_total_hbd.amount.value );
    BOOST_REQUIRE( get_vesting( "sam" ).amount.value == sam_total_vesting.amount.value );

    auto dave_total_hbd = dave_starting_hbd + asset( dave_pays_dave_hbd, HIVE_SYMBOL ) * exchange_rate;
    auto dave_total_vesting = dave_starting_vesting + asset( dave_pays_dave_vest, HIVE_SYMBOL ) * gpo.get_vesting_share_price();
    BOOST_REQUIRE( get_hbd_balance( "dave" ).amount.value == dave_total_hbd.amount.value );
    BOOST_REQUIRE( get_vesting( "dave" ).amount.value == dave_total_vesting.amount.value );
  }
  FC_LOG_AND_RETHROW()
}
*/


BOOST_AUTO_TEST_CASE( vesting_withdrawals )
{
  try
  {
    ACTORS( (alice) )
    fund( "alice", 100000 );
    vest( "alice", 100000 );

    const auto& new_alice = db->get_account( "alice" );

    BOOST_TEST_MESSAGE( "Setting up withdrawal" );

    signed_transaction tx;
    withdraw_vesting_operation op;
    op.account = "alice";
    op.vesting_shares = asset( new_alice.get_vesting().amount / 2, VESTS_SYMBOL );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.operations.push_back( op );
    sign( tx, alice_private_key );
    db->push_transaction( tx, 0 );

    auto next_withdrawal = db->head_block_time() + HIVE_VESTING_WITHDRAW_INTERVAL_SECONDS;
    asset vesting_shares = new_alice.get_vesting();
    asset original_vesting = vesting_shares;
    asset withdraw_rate = new_alice.vesting_withdraw_rate;

    BOOST_TEST_MESSAGE( "Generating block up to first withdrawal" );
    generate_blocks( next_withdrawal - HIVE_BLOCK_INTERVAL );

    BOOST_REQUIRE( get_vesting( "alice" ).amount.value == vesting_shares.amount.value );

    BOOST_TEST_MESSAGE( "Generating block to cause withdrawal" );
    generate_block();

    auto fill_op = get_last_operations( 1 )[0].get< fill_vesting_withdraw_operation >();
    auto& gpo = db->get_dynamic_global_properties();

    BOOST_REQUIRE( get_vesting( "alice" ).amount.value == ( vesting_shares - withdraw_rate ).amount.value );
    BOOST_REQUIRE( ( withdraw_rate * gpo.get_vesting_share_price() ).amount.value - get_balance( "alice" ).amount.value <= 1 ); // Check a range due to differences in the share price
    BOOST_REQUIRE( fill_op.from_account == "alice" );
    BOOST_REQUIRE( fill_op.to_account == "alice" );
    BOOST_REQUIRE( fill_op.withdrawn.amount.value == withdraw_rate.amount.value );
    BOOST_REQUIRE( std::abs( ( fill_op.deposited - fill_op.withdrawn * gpo.get_vesting_share_price() ).amount.value ) <= 1 );
    validate_database();

    BOOST_TEST_MESSAGE( "Generating the rest of the blocks in the withdrawal" );

    vesting_shares = get_vesting( "alice" );
    auto balance = get_balance( "alice" );
    auto old_next_vesting = db->get_account( "alice" ).next_vesting_withdrawal;

    for( int i = 1; i < HIVE_VESTING_WITHDRAW_INTERVALS - 1; i++ )
    {
      generate_blocks( db->head_block_time() + HIVE_VESTING_WITHDRAW_INTERVAL_SECONDS );

      const auto& alice = db->get_account( "alice" );

      fill_op = get_last_operations( 2 )[ 1 ].get< fill_vesting_withdraw_operation >();

      BOOST_REQUIRE( alice.get_vesting().amount.value == ( vesting_shares - withdraw_rate ).amount.value );
      BOOST_REQUIRE( balance.amount.value + ( withdraw_rate * gpo.get_vesting_share_price() ).amount.value - alice.get_balance().amount.value <= 1 );
      BOOST_REQUIRE( fill_op.from_account == "alice" );
      BOOST_REQUIRE( fill_op.to_account == "alice" );
      BOOST_REQUIRE( fill_op.withdrawn.amount.value == withdraw_rate.amount.value );
      BOOST_REQUIRE( std::abs( ( fill_op.deposited - fill_op.withdrawn * gpo.get_vesting_share_price() ).amount.value ) <= 1 );

      if ( i == HIVE_VESTING_WITHDRAW_INTERVALS - 1 )
        BOOST_REQUIRE( alice.next_vesting_withdrawal == fc::time_point_sec::maximum() );
      else
        BOOST_REQUIRE( alice.next_vesting_withdrawal.sec_since_epoch() == ( old_next_vesting + HIVE_VESTING_WITHDRAW_INTERVAL_SECONDS ).sec_since_epoch() );

      validate_database();

      vesting_shares = alice.get_vesting();
      balance = alice.get_balance();
      old_next_vesting = alice.next_vesting_withdrawal;
    }

    BOOST_TEST_MESSAGE( "Generating one more block to finish vesting withdrawal" );
    generate_blocks( db->head_block_time() + HIVE_VESTING_WITHDRAW_INTERVAL_SECONDS, true );

    BOOST_REQUIRE( db->get_account( "alice" ).next_vesting_withdrawal.sec_since_epoch() == fc::time_point_sec::maximum().sec_since_epoch() );
    BOOST_REQUIRE( get_vesting( "alice" ).amount.value == ( original_vesting - op.vesting_shares ).amount.value );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( vesting_withdraw_route )
{
  try
  {
    ACTORS( (alice)(bob)(sam) )

    auto original_vesting = alice.get_vesting();

    fund( "alice", 1040000 );
    vest( "alice", 1040000 );

    auto withdraw_amount = alice.get_vesting() - original_vesting;

    BOOST_TEST_MESSAGE( "Setup vesting withdraw" );
    withdraw_vesting_operation wv;
    wv.account = "alice";
    wv.vesting_shares = withdraw_amount;

    signed_transaction tx;
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.operations.push_back( wv );
    sign( tx, alice_private_key );
    db->push_transaction( tx, 0 );

    tx.operations.clear();
    tx.signatures.clear();

    BOOST_TEST_MESSAGE( "Setting up bob destination" );
    set_withdraw_vesting_route_operation op;
    op.from_account = "alice";
    op.to_account = "bob";
    op.percent = HIVE_1_PERCENT * 50;
    op.auto_vest = true;
    tx.operations.push_back( op );

    BOOST_TEST_MESSAGE( "Setting up sam destination" );
    op.to_account = "sam";
    op.percent = HIVE_1_PERCENT * 30;
    op.auto_vest = false;
    tx.operations.push_back( op );
    sign( tx, alice_private_key );
    db->push_transaction( tx, 0 );

    BOOST_TEST_MESSAGE( "Setting up first withdraw" );

    auto vesting_withdraw_rate = alice.vesting_withdraw_rate;
    auto old_alice_balance = alice.get_balance();
    auto old_alice_vesting = alice.get_vesting();
    auto old_bob_balance = bob.get_balance();
    auto old_bob_vesting = bob.get_vesting();
    auto old_sam_balance = sam.get_balance();
    auto old_sam_vesting = sam.get_vesting();
    generate_blocks( alice.next_vesting_withdrawal, true );

    {
      const auto& alice = db->get_account( "alice" );
      const auto& bob = db->get_account( "bob" );
      const auto& sam = db->get_account( "sam" );

      BOOST_REQUIRE( alice.get_vesting() == old_alice_vesting - vesting_withdraw_rate );
      BOOST_REQUIRE( alice.get_balance() == old_alice_balance + asset( ( vesting_withdraw_rate.amount * HIVE_1_PERCENT * 20 ) / HIVE_100_PERCENT, VESTS_SYMBOL ) * db->get_dynamic_global_properties().get_vesting_share_price() );
      BOOST_REQUIRE( bob.get_vesting() == old_bob_vesting + asset( ( vesting_withdraw_rate.amount * HIVE_1_PERCENT * 50 ) / HIVE_100_PERCENT, VESTS_SYMBOL ) );
      BOOST_REQUIRE( bob.get_balance() == old_bob_balance );
      BOOST_REQUIRE( sam.get_vesting() == old_sam_vesting );
      BOOST_REQUIRE( sam.get_balance() ==  old_sam_balance + asset( ( vesting_withdraw_rate.amount * HIVE_1_PERCENT * 30 ) / HIVE_100_PERCENT, VESTS_SYMBOL ) * db->get_dynamic_global_properties().get_vesting_share_price() );

      old_alice_balance = alice.get_balance();
      old_alice_vesting = alice.get_vesting();
      old_bob_balance = bob.get_balance();
      old_bob_vesting = bob.get_vesting();
      old_sam_balance = sam.get_balance();
      old_sam_vesting = sam.get_vesting();
    }

    BOOST_TEST_MESSAGE( "Test failure with greater than 100% destination assignment" );

    tx.operations.clear();
    tx.signatures.clear();

    op.to_account = "sam";
    op.percent = HIVE_1_PERCENT * 50 + 1;
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    sign( tx, alice_private_key );
    HIVE_REQUIRE_THROW( db->push_transaction( tx, 0 ), fc::exception );

    BOOST_TEST_MESSAGE( "Test from_account receiving no withdraw" );

    tx.operations.clear();
    tx.signatures.clear();

    op.to_account = "sam";
    op.percent = HIVE_1_PERCENT * 50;
    tx.operations.push_back( op );
    sign( tx, alice_private_key );
    db->push_transaction( tx, 0 );

    generate_blocks( db->get_account( "alice" ).next_vesting_withdrawal, true );
    {
      const auto& alice = db->get_account( "alice" );
      const auto& bob = db->get_account( "bob" );
      const auto& sam = db->get_account( "sam" );

      BOOST_REQUIRE( alice.get_vesting() == old_alice_vesting - vesting_withdraw_rate );
      BOOST_REQUIRE( alice.get_balance() == old_alice_balance );
      BOOST_REQUIRE( bob.get_vesting() == old_bob_vesting + asset( ( vesting_withdraw_rate.amount * HIVE_1_PERCENT * 50 ) / HIVE_100_PERCENT, VESTS_SYMBOL ) );
      BOOST_REQUIRE( bob.get_balance() == old_bob_balance );
      BOOST_REQUIRE( sam.get_vesting() == old_sam_vesting );
      BOOST_REQUIRE( sam.get_balance() ==  old_sam_balance + asset( ( vesting_withdraw_rate.amount * HIVE_1_PERCENT * 50 ) / HIVE_100_PERCENT, VESTS_SYMBOL ) * db->get_dynamic_global_properties().get_vesting_share_price() );
    }
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( feed_publish_mean )
{
  try
  {
    resize_shared_mem( 1024 * 1024 * 32 );

    ACTORS( (alice0)(alice1)(alice2)(alice3)(alice4)(alice5)(alice6) )

    BOOST_TEST_MESSAGE( "Setup" );

    generate_blocks( 30 / HIVE_BLOCK_INTERVAL );

    vector< string > accounts;
    accounts.push_back( "alice0" );
    accounts.push_back( "alice1" );
    accounts.push_back( "alice2" );
    accounts.push_back( "alice3" );
    accounts.push_back( "alice4" );
    accounts.push_back( "alice5" );
    accounts.push_back( "alice6" );

    vector< private_key_type > keys;
    keys.push_back( alice0_private_key );
    keys.push_back( alice1_private_key );
    keys.push_back( alice2_private_key );
    keys.push_back( alice3_private_key );
    keys.push_back( alice4_private_key );
    keys.push_back( alice5_private_key );
    keys.push_back( alice6_private_key );

    vector< feed_publish_operation > ops;
    vector< signed_transaction > txs;

    // Upgrade accounts to witnesses
    for( int i = 0; i < 7; i++ )
    {
      transfer( HIVE_INIT_MINER_NAME, accounts[i], asset( 10000, HIVE_SYMBOL ) );
      witness_create( accounts[i], keys[i], "foo.bar", keys[i].get_public_key(), 1000 );

      ops.push_back( feed_publish_operation() );
      ops[i].publisher = accounts[i];

      txs.push_back( signed_transaction() );
    }

    ops[0].exchange_rate = price( asset( 1000, HBD_SYMBOL ), asset( 100000, HIVE_SYMBOL ) );
    ops[1].exchange_rate = price( asset( 1000, HBD_SYMBOL ), asset( 105000, HIVE_SYMBOL ) );
    ops[2].exchange_rate = price( asset( 1000, HBD_SYMBOL ), asset(  98000, HIVE_SYMBOL ) );
    ops[3].exchange_rate = price( asset( 1000, HBD_SYMBOL ), asset(  97000, HIVE_SYMBOL ) );
    ops[4].exchange_rate = price( asset( 1000, HBD_SYMBOL ), asset(  99000, HIVE_SYMBOL ) );
    ops[5].exchange_rate = price( asset( 1000, HBD_SYMBOL ), asset(  97500, HIVE_SYMBOL ) );
    ops[6].exchange_rate = price( asset( 1000, HBD_SYMBOL ), asset( 102000, HIVE_SYMBOL ) );

    for( int i = 0; i < 7; i++ )
    {
      txs[i].set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
      txs[i].operations.push_back( ops[i] );
      sign( txs[i], keys[i] );
      db->push_transaction( txs[i], 0 );
    }

    BOOST_TEST_MESSAGE( "Jump forward an hour" );

    generate_blocks( HIVE_BLOCKS_PER_HOUR ); // Jump forward 1 hour
    BOOST_TEST_MESSAGE( "Get feed history object" );
    auto& feed_history = db->get_feed_history();
    BOOST_TEST_MESSAGE( "Check state" );
    {
      auto expected_price = price( asset( 1000, HBD_SYMBOL ), asset( 99000, HIVE_SYMBOL ) );
      BOOST_REQUIRE( feed_history.current_median_history == expected_price );
      BOOST_REQUIRE( feed_history.price_history[ 0 ] == expected_price );
    }
    validate_database();

    for ( int i = 0; i < 23; i++ )
    {
      BOOST_TEST_MESSAGE( "Updating ops" );

      for( int j = 0; j < 7; j++ )
      {
        txs[j].operations.clear();
        txs[j].signatures.clear();
        ops[j].exchange_rate = price( ops[j].exchange_rate.base, asset( ops[j].exchange_rate.quote.amount + 10, HIVE_SYMBOL ) );
        txs[j].set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
        txs[j].operations.push_back( ops[j] );
        sign( txs[j], keys[j] );
        db->push_transaction( txs[j], 0 );
      }

      BOOST_TEST_MESSAGE( "Generate Blocks" );

      generate_blocks( HIVE_BLOCKS_PER_HOUR ); // Jump forward 1 hour

      BOOST_TEST_MESSAGE( "Check feed_history" );

      auto& feed_history = db->get(feed_history_id_type());
      BOOST_REQUIRE( feed_history.current_median_history == feed_history.price_history[ ( i + 1 ) / 2 ] );
      BOOST_REQUIRE( feed_history.price_history[ i + 1 ] == ops[4].exchange_rate );
      validate_database();
    }
  }
  FC_LOG_AND_RETHROW();
}

BOOST_AUTO_TEST_CASE( convert_delay )
{
  try
  {
    ACTORS( (alice) )
    generate_block();
    vest( HIVE_INIT_MINER_NAME, "alice", ASSET( "10.000 TESTS" ) );
    fund( "alice", ASSET( "25.000 TBD" ) );

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.250 TESTS" ) ) );

    convert_operation op;
    signed_transaction tx;

    auto start_balance = ASSET( "25.000 TBD" );

    BOOST_TEST_MESSAGE( "Setup conversion to TESTS" );
    tx.operations.clear();
    tx.signatures.clear();
    op.owner = "alice";
    op.amount = asset( 2000, HBD_SYMBOL );
    op.requestid = 2;
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    sign( tx, alice_private_key );
    db->push_transaction( tx, 0 );

    BOOST_TEST_MESSAGE( "Generating Blocks up to conversion block" );
    generate_blocks( db->head_block_time() + HIVE_CONVERSION_DELAY - fc::seconds( HIVE_BLOCK_INTERVAL / 2 ), true );

    BOOST_TEST_MESSAGE( "Verify conversion is not applied" );
    const auto& alice_2 = db->get_account( "alice" );
    const auto& convert_request_idx = db->get_index< convert_request_index, by_owner >();
    auto convert_request = convert_request_idx.find( boost::make_tuple( alice_2.get_id(), 2 ) );

    BOOST_REQUIRE( convert_request != convert_request_idx.end() );
    BOOST_REQUIRE( alice_2.get_balance().amount.value == 0 );
    BOOST_REQUIRE( alice_2.get_hbd_balance().amount.value == ( start_balance - op.amount ).amount.value );
    validate_database();

    BOOST_TEST_MESSAGE( "Generate one more block" );
    generate_block();

    BOOST_TEST_MESSAGE( "Verify conversion applied" );
    const auto& alice_3 = db->get_account( "alice" );
    auto vop = get_last_operations( 1 )[0].get< fill_convert_request_operation >();

    convert_request = convert_request_idx.find( boost::make_tuple( alice_3.get_id(), 2 ) );
    BOOST_REQUIRE( convert_request == convert_request_idx.end() );
    BOOST_REQUIRE( alice_3.get_balance().amount.value == 2500 );
    BOOST_REQUIRE( alice_3.get_hbd_balance().amount.value == ( start_balance - op.amount ).amount.value );
    BOOST_REQUIRE( vop.owner == "alice" );
    BOOST_REQUIRE( vop.requestid == 2 );
    BOOST_REQUIRE( vop.amount_in.amount.value == ASSET( "2.000 TBD" ).amount.value );
    BOOST_REQUIRE( vop.amount_out.amount.value == ASSET( "2.500 TESTS" ).amount.value );
    validate_database();
  }
  FC_LOG_AND_RETHROW();
}

BOOST_AUTO_TEST_CASE( hive_inflation )
{
  try
  {
  /*
    BOOST_TEST_MESSAGE( "Testing HIVE Inflation until the vesting start block" );

    auto& gpo = db->get_dynamic_global_properties();
    auto virtual_supply = gpo.virtual_supply;
    auto witness_name = db->get_scheduled_witness(1);
    auto old_witness_balance = get_balance( witness_name );
    auto old_witness_shares = get_vesting( witness_name );

    auto new_rewards = std::max( HIVE_MIN_CONTENT_REWARD, asset( ( HIVE_CONTENT_APR * gpo.virtual_supply.amount ) / ( HIVE_BLOCKS_PER_YEAR * 100 ), HIVE_SYMBOL ) )
      + std::max( HIVE_MIN_CURATE_REWARD, asset( ( HIVE_CURATE_APR * gpo.virtual_supply.amount ) / ( HIVE_BLOCKS_PER_YEAR * 100 ), HIVE_SYMBOL ) );
    auto witness_pay = std::max( HIVE_MIN_PRODUCER_REWARD, asset( ( HIVE_PRODUCER_APR * gpo.virtual_supply.amount ) / ( HIVE_BLOCKS_PER_YEAR * 100 ), HIVE_SYMBOL ) );
    auto witness_pay_shares = asset( 0, VESTS_SYMBOL );
    auto new_vesting_hive = asset( 0, HIVE_SYMBOL );
    auto new_vesting_shares = gpo.get_total_vesting_shares();

    if ( get_vesting( witness_name ).amount.value == 0 )
    {
      new_vesting_hive += witness_pay;
      new_vesting_shares += witness_pay * ( gpo.get_total_vesting_shares() / gpo.get_total_vesting_fund_hive() );
    }

    auto new_supply = gpo.get_current_supply() + new_rewards + witness_pay + new_vesting_hive;
    new_rewards += gpo.get_total_reward_fund_hive();
    new_vesting_hive += gpo.get_total_vesting_fund_hive();

    generate_block();

    BOOST_REQUIRE( gpo.get_current_supply().amount.value == new_supply.amount.value );
    BOOST_REQUIRE( gpo.virtual_supply.amount.value == new_supply.amount.value );
    BOOST_REQUIRE( gpo.get_total_reward_fund_hive().amount.value == new_rewards.amount.value );
    BOOST_REQUIRE( gpo.get_total_vesting_fund_hive().amount.value == new_vesting_hive.amount.value );
    BOOST_REQUIRE( gpo.get_total_vesting_shares().amount.value == new_vesting_shares.amount.value );
    BOOST_REQUIRE( get_balance( witness_name ).amount.value == ( old_witness_balance + witness_pay ).amount.value );

    validate_database();

    while( db->head_block_num() < HIVE_START_VESTING_BLOCK - 1)
    {
      virtual_supply = gpo.virtual_supply;
      witness_name = db->get_scheduled_witness(1);
      old_witness_balance = get_balance( witness_name );
      old_witness_shares = get_vesting( witness_name );


      new_rewards = std::max( HIVE_MIN_CONTENT_REWARD, asset( ( HIVE_CONTENT_APR * gpo.virtual_supply.amount ) / ( HIVE_BLOCKS_PER_YEAR * 100 ), HIVE_SYMBOL ) )
        + std::max( HIVE_MIN_CURATE_REWARD, asset( ( HIVE_CURATE_APR * gpo.virtual_supply.amount ) / ( HIVE_BLOCKS_PER_YEAR * 100 ), HIVE_SYMBOL ) );
      witness_pay = std::max( HIVE_MIN_PRODUCER_REWARD, asset( ( HIVE_PRODUCER_APR * gpo.virtual_supply.amount ) / ( HIVE_BLOCKS_PER_YEAR * 100 ), HIVE_SYMBOL ) );
      new_vesting_hive = asset( 0, HIVE_SYMBOL );
      new_vesting_shares = gpo.get_total_vesting_shares();

      if ( get_vesting( witness_name ).amount.value == 0 )
      {
        new_vesting_hive += witness_pay;
        witness_pay_shares = witness_pay * gpo.get_vesting_share_price();
        new_vesting_shares += witness_pay_shares;
        new_supply += witness_pay;
        witness_pay = asset( 0, HIVE_SYMBOL );
      }

      new_supply = gpo.get_current_supply() + new_rewards + witness_pay + new_vesting_hive;
      new_rewards += gpo.get_total_reward_fund_hive();
      new_vesting_hive += gpo.get_total_vesting_fund_hive();

      generate_block();

      BOOST_REQUIRE( gpo.get_current_supply().amount.value == new_supply.amount.value );
      BOOST_REQUIRE( gpo.virtual_supply.amount.value == new_supply.amount.value );
      BOOST_REQUIRE( gpo.get_total_reward_fund_hive().amount.value == new_rewards.amount.value );
      BOOST_REQUIRE( gpo.get_total_vesting_fund_hive().amount.value == new_vesting_hive.amount.value );
      BOOST_REQUIRE( gpo.get_total_vesting_shares().amount.value == new_vesting_shares.amount.value );
      BOOST_REQUIRE( get_balance( witness_name ).amount.value == ( old_witness_balance + witness_pay ).amount.value );
      BOOST_REQUIRE( get_vesting( witness_name ).amount.value == ( old_witness_shares + witness_pay_shares ).amount.value );

      validate_database();
    }

    BOOST_TEST_MESSAGE( "Testing up to the start block for miner voting" );

    while( db->head_block_num() < HIVE_START_MINER_VOTING_BLOCK - 1 )
    {
      virtual_supply = gpo.virtual_supply;
      witness_name = db->get_scheduled_witness(1);
      old_witness_balance = get_balance( witness_name );

      new_rewards = std::max( HIVE_MIN_CONTENT_REWARD, asset( ( HIVE_CONTENT_APR * gpo.virtual_supply.amount ) / ( HIVE_BLOCKS_PER_YEAR * 100 ), HIVE_SYMBOL ) )
        + std::max( HIVE_MIN_CURATE_REWARD, asset( ( HIVE_CURATE_APR * gpo.virtual_supply.amount ) / ( HIVE_BLOCKS_PER_YEAR * 100 ), HIVE_SYMBOL ) );
      witness_pay = std::max( HIVE_MIN_PRODUCER_REWARD, asset( ( HIVE_PRODUCER_APR * gpo.virtual_supply.amount ) / ( HIVE_BLOCKS_PER_YEAR * 100 ), HIVE_SYMBOL ) );
      auto witness_pay_shares = asset( 0, VESTS_SYMBOL );
      new_vesting_hive = asset( ( witness_pay + new_rewards ).amount * 9, HIVE_SYMBOL );
      new_vesting_shares = gpo.get_total_vesting_shares();

      if ( get_vesting( witness_name ).amount.value == 0 )
      {
        new_vesting_hive += witness_pay;
        witness_pay_shares = witness_pay * gpo.get_vesting_share_price();
        new_vesting_shares += witness_pay_shares;
        new_supply += witness_pay;
        witness_pay = asset( 0, HIVE_SYMBOL );
      }

      new_supply = gpo.get_current_supply() + new_rewards + witness_pay + new_vesting_hive;
      new_rewards += gpo.get_total_reward_fund_hive();
      new_vesting_hive += gpo.get_total_vesting_fund_hive();

      generate_block();

      BOOST_REQUIRE( gpo.get_current_supply().amount.value == new_supply.amount.value );
      BOOST_REQUIRE( gpo.virtual_supply.amount.value == new_supply.amount.value );
      BOOST_REQUIRE( gpo.get_total_reward_fund_hive().amount.value == new_rewards.amount.value );
      BOOST_REQUIRE( gpo.get_total_vesting_fund_hive().amount.value == new_vesting_hive.amount.value );
      BOOST_REQUIRE( gpo.get_total_vesting_shares().amount.value == new_vesting_shares.amount.value );
      BOOST_REQUIRE( get_balance( witness_name ).amount.value == ( old_witness_balance + witness_pay ).amount.value );
      BOOST_REQUIRE( get_vesting( witness_name ).amount.value == ( old_witness_shares + witness_pay_shares ).amount.value );

      validate_database();
    }

    for( int i = 0; i < HIVE_BLOCKS_PER_DAY; i++ )
    {
      virtual_supply = gpo.virtual_supply;
      witness_name = db->get_scheduled_witness(1);
      old_witness_balance = get_balance( witness_name );

      new_rewards = std::max( HIVE_MIN_CONTENT_REWARD, asset( ( HIVE_CONTENT_APR * gpo.virtual_supply.amount ) / ( HIVE_BLOCKS_PER_YEAR * 100 ), HIVE_SYMBOL ) )
        + std::max( HIVE_MIN_CURATE_REWARD, asset( ( HIVE_CURATE_APR * gpo.virtual_supply.amount ) / ( HIVE_BLOCKS_PER_YEAR * 100 ), HIVE_SYMBOL ) );
      witness_pay = std::max( HIVE_MIN_PRODUCER_REWARD, asset( ( HIVE_PRODUCER_APR * gpo.virtual_supply.amount ) / ( HIVE_BLOCKS_PER_YEAR * 100 ), HIVE_SYMBOL ) );
      witness_pay_shares = witness_pay * gpo.get_vesting_share_price();
      new_vesting_hive = asset( ( witness_pay + new_rewards ).amount * 9, HIVE_SYMBOL ) + witness_pay;
      new_vesting_shares = gpo.get_total_vesting_shares() + witness_pay_shares;
      new_supply = gpo.get_current_supply() + new_rewards + new_vesting_hive;
      new_rewards += gpo.get_total_reward_fund_hive();
      new_vesting_hive += gpo.get_total_vesting_fund_hive();

      generate_block();

      BOOST_REQUIRE( gpo.get_current_supply().amount.value == new_supply.amount.value );
      BOOST_REQUIRE( gpo.virtual_supply.amount.value == new_supply.amount.value );
      BOOST_REQUIRE( gpo.get_total_reward_fund_hive().amount.value == new_rewards.amount.value );
      BOOST_REQUIRE( gpo.get_total_vesting_fund_hive().amount.value == new_vesting_hive.amount.value );
      BOOST_REQUIRE( gpo.get_total_vesting_shares().amount.value == new_vesting_shares.amount.value );
      BOOST_REQUIRE( get_vesting( witness_name ).amount.value == ( old_witness_shares + witness_pay_shares ).amount.value );

      validate_database();
    }

    virtual_supply = gpo.virtual_supply;
    vesting_shares = gpo.get_total_vesting_shares();
    vesting_hive = gpo.get_total_vesting_fund_hive();
    reward_hive = gpo.get_total_reward_fund_hive();

    witness_name = db->get_scheduled_witness(1);
    old_witness_shares = get_vesting( witness_name );

    generate_block();

    gpo = db->get_dynamic_global_properties();

    BOOST_REQUIRE_EQUAL( gpo.get_total_vesting_fund_hive().amount.value,
      ( vesting_hive.amount.value
        + ( ( ( uint128_t( virtual_supply.amount.value ) / 10 ) / HIVE_BLOCKS_PER_YEAR ) * 9 )
        + ( uint128_t( virtual_supply.amount.value ) / 100 / HIVE_BLOCKS_PER_YEAR ) ).to_uint64() );
    BOOST_REQUIRE_EQUAL( gpo.get_total_reward_fund_hive().amount.value,
      reward_hive.amount.value + virtual_supply.amount.value / 10 / HIVE_BLOCKS_PER_YEAR + virtual_supply.amount.value / 10 / HIVE_BLOCKS_PER_DAY );
    BOOST_REQUIRE_EQUAL( get_vesting( witness_name ).amount.value,
      old_witness_shares.amount.value + ( asset( ( ( virtual_supply.amount.value / HIVE_BLOCKS_PER_YEAR ) * HIVE_1_PERCENT ) / HIVE_100_PERCENT, HIVE_SYMBOL ) * ( vesting_shares / vesting_hive ) ).amount.value );
    validate_database();
    */
  }
  FC_LOG_AND_RETHROW();
}

BOOST_AUTO_TEST_CASE( hbd_interest )
{
  try
  {
    ACTORS( (alice)(bob) )
    generate_block();
    vest( HIVE_INIT_MINER_NAME, "alice", ASSET( "10.000 TESTS" ) );
    vest( HIVE_INIT_MINER_NAME, "bob", ASSET( "10.000 TESTS" ) );

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );

    BOOST_TEST_MESSAGE( "Testing interest over smallest interest period" );

    convert_operation op;
    signed_transaction tx;

    fund( "alice", ASSET( "31.903 TBD" ) );

    auto start_time = db->get_account( "alice" ).hbd_seconds_last_update;
    auto alice_hbd = get_hbd_balance( "alice" );

    generate_blocks( db->head_block_time() + fc::seconds( HIVE_HBD_INTEREST_COMPOUND_INTERVAL_SEC ), true );

    transfer_operation transfer;
    transfer.to = "bob";
    transfer.from = "alice";
    transfer.amount = ASSET( "1.000 TBD" );
    tx.operations.clear();
    tx.signatures.clear();
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.operations.push_back( transfer );
    sign( tx, alice_private_key );
    db->push_transaction( tx, 0 );

    auto& gpo = db->get_dynamic_global_properties();
    BOOST_REQUIRE(gpo.get_hbd_interest_rate() > 0);

    if(db->has_hardfork(HIVE_HARDFORK_1_25))
    {
      /// After HF 25 only HBD held on savings should get interest
      BOOST_REQUIRE(get_hbd_balance("alice").amount.value == alice_hbd.amount.value - ASSET("1.000 TBD").amount.value);
    }
    else
    {
      auto interest_op = get_last_operations( 1 )[0].get< interest_operation >();
      BOOST_REQUIRE( static_cast<uint64_t>(get_hbd_balance( "alice" ).amount.value) == alice_hbd.amount.value - ASSET( "1.000 TBD" ).amount.value + ( ( ( ( uint128_t( alice_hbd.amount.value ) * ( db->head_block_time() - start_time ).to_seconds() ) / HIVE_SECONDS_PER_YEAR ) * gpo.get_hbd_interest_rate() ) / HIVE_100_PERCENT ).to_uint64() );
      BOOST_REQUIRE( interest_op.owner == "alice" );
      BOOST_REQUIRE( interest_op.interest.amount.value == get_hbd_balance( "alice" ).amount.value - ( alice_hbd.amount.value - ASSET( "1.000 TBD" ).amount.value ) );
    }

    validate_database();

    BOOST_TEST_MESSAGE( "Testing interest under interest period" );

    start_time = db->get_account( "alice" ).hbd_seconds_last_update;
    alice_hbd = get_hbd_balance( "alice" );

    generate_blocks( db->head_block_time() + fc::seconds( HIVE_HBD_INTEREST_COMPOUND_INTERVAL_SEC / 2 ), true );

    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( transfer );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    sign( tx, alice_private_key );
    db->push_transaction( tx, 0 );

    BOOST_REQUIRE( get_hbd_balance( "alice" ).amount.value == alice_hbd.amount.value - ASSET( "1.000 TBD" ).amount.value );
    validate_database();

    auto alice_coindays = uint128_t( alice_hbd.amount.value ) * ( db->head_block_time() - start_time ).to_seconds();
    alice_hbd = get_hbd_balance( "alice" );
    start_time = db->get_account( "alice" ).hbd_seconds_last_update;

    BOOST_TEST_MESSAGE( "Testing longer interest period" );

    generate_blocks( db->head_block_time() + fc::seconds( ( HIVE_HBD_INTEREST_COMPOUND_INTERVAL_SEC * 7 ) / 3 ), true );

    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( transfer );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    sign( tx, alice_private_key );
    db->push_transaction( tx, 0 );

    if(db->has_hardfork(HIVE_HARDFORK_1_25))
    {
      /// After HF 25 only HBD held on savings should get interest
      BOOST_REQUIRE(get_hbd_balance("alice").amount.value == alice_hbd.amount.value - ASSET("1.000 TBD").amount.value);
    }
    else
    {
      BOOST_REQUIRE( static_cast<uint64_t>(get_hbd_balance( "alice" ).amount.value) == alice_hbd.amount.value - ASSET( "1.000 TBD" ).amount.value + ( ( ( ( uint128_t( alice_hbd.amount.value ) * ( db->head_block_time() - start_time ).to_seconds() + alice_coindays ) / HIVE_SECONDS_PER_YEAR ) * gpo.get_hbd_interest_rate() ) / HIVE_100_PERCENT ).to_uint64() );
    }

    validate_database();
  }
  FC_LOG_AND_RETHROW();
}

BOOST_AUTO_TEST_CASE(hbd_savings_interest)
{
  using hive::plugins::condenser_api::legacy_asset;

  try
  {
    ACTORS((alice))
      generate_block();
    vest(HIVE_INIT_MINER_NAME, "alice", ASSET("10.000 TESTS"));

    set_price_feed(price(ASSET("1.000 TBD"), ASSET("1.000 TESTS")));

    BOOST_TEST_MESSAGE("Testing savings interest over smallest interest period");

    auto alice_funds = ASSET("31.903 TBD");
    fund("alice", alice_funds);

    transfer_to_savings_operation savings_supply;
    savings_supply.from = "alice";
    savings_supply.to = "alice";
    savings_supply.memo = "New Tesla fund";
    savings_supply.amount = alice_funds;
    push_transaction(savings_supply, alice_private_key);

    fund("alice", ASSET("3.000 TBD"));

    auto start_time = db->get_account("alice").savings_hbd_seconds_last_update;
    auto alice_hbd = get_hbd_balance("alice");

    BOOST_REQUIRE(alice_hbd == ASSET("3.000 TBD"));

    auto alice_hbd_savings = get_hbd_savings("alice");
    BOOST_REQUIRE(alice_hbd_savings == alice_funds);

    generate_blocks(db->head_block_time() + fc::seconds(HIVE_HBD_INTEREST_COMPOUND_INTERVAL_SEC), true);

    BOOST_REQUIRE(get_hbd_balance("alice") == ASSET("3.000 TBD"));

    transfer_to_savings_operation transfer;
    transfer.to = "alice";
    transfer.from = "alice";
    transfer.memo = "Interest trigger";
    transfer.amount = ASSET("1.000 TBD");

    /// This op is needed to trigger interest payment...
    push_transaction(transfer, alice_private_key);

    auto& gpo = db->get_dynamic_global_properties();
    BOOST_REQUIRE(gpo.get_hbd_interest_rate() > 0);

    BOOST_TEST_MESSAGE("Alice HDB saving balance: " + legacy_asset::from_asset(get_hbd_savings("alice")).to_string());

    auto interest_op = get_last_operations(1)[0].get< interest_operation >();
    BOOST_REQUIRE(static_cast<uint64_t>(get_hbd_savings("alice").amount.value) == alice_hbd_savings.amount.value + ASSET("1.000 TBD").amount.value + ((((uint128_t(alice_hbd_savings.amount.value) * (db->head_block_time() - start_time).to_seconds()) / HIVE_SECONDS_PER_YEAR) * gpo.get_hbd_interest_rate()) / HIVE_100_PERCENT).to_uint64());
    BOOST_REQUIRE(interest_op.owner == "alice");
    BOOST_REQUIRE(interest_op.interest == get_hbd_savings("alice") - (alice_hbd_savings + ASSET("1.000 TBD")));

    BOOST_TEST_MESSAGE("Alice got HDB saving interests: " + legacy_asset::from_asset(interest_op.interest).to_string());

    validate_database();

    BOOST_TEST_MESSAGE("Testing savings interest under interest period");

    start_time = db->get_account("alice").savings_hbd_seconds_last_update;
    alice_hbd_savings = get_hbd_savings("alice");

    generate_blocks(db->head_block_time() + fc::seconds(HIVE_HBD_INTEREST_COMPOUND_INTERVAL_SEC / 2), true);
    
    /// This op is needed to trigger interest payment...
    push_transaction(transfer, alice_private_key);

    BOOST_REQUIRE(get_hbd_savings("alice") == alice_hbd_savings + ASSET("1.000 TBD"));

    validate_database();

    auto alice_coindays = uint128_t(alice_hbd_savings.amount.value) * (db->head_block_time() - start_time).to_seconds();
    alice_hbd_savings = get_hbd_savings("alice");
    start_time = db->get_account("alice").savings_hbd_seconds_last_update;

    BOOST_TEST_MESSAGE("Testing savings interest for longer period");

    generate_blocks(db->head_block_time() + fc::seconds((HIVE_HBD_INTEREST_COMPOUND_INTERVAL_SEC * 7) / 3), true);
    
    /// This op is needed to trigger interest payment...
    push_transaction(transfer, alice_private_key);

    BOOST_REQUIRE(static_cast<uint64_t>(get_hbd_savings("alice").amount.value) == alice_hbd_savings.amount.value + ASSET("1.000 TBD").amount.value + ((((uint128_t(alice_hbd_savings.amount.value) * (db->head_block_time() - start_time).to_seconds() + alice_coindays) / HIVE_SECONDS_PER_YEAR) * gpo.get_hbd_interest_rate()) / HIVE_100_PERCENT).to_uint64());

    validate_database();
  }
  FC_LOG_AND_RETHROW();
}

BOOST_AUTO_TEST_CASE( liquidity_rewards )
{
  using std::abs;

  try
  {
    db->liquidity_rewards_enabled = false;

    ACTORS( (alice)(bob)(sam)(dave) )
    generate_block();
    vest( HIVE_INIT_MINER_NAME, "alice", ASSET( "10.000 TESTS" ) );
    vest( HIVE_INIT_MINER_NAME, "bob", ASSET( "10.000 TESTS" ) );
    vest( HIVE_INIT_MINER_NAME, "sam", ASSET( "10.000 TESTS" ) );
    vest( HIVE_INIT_MINER_NAME, "dave", ASSET( "10.000 TESTS" ) );

    BOOST_TEST_MESSAGE( "Rewarding Bob with TESTS" );

    auto exchange_rate = price( ASSET( "1.000 TBD" ), ASSET( "1.250 TESTS" ) );
    set_price_feed( exchange_rate );

    signed_transaction tx;

    fund( "alice", ASSET( "25.522 TBD" ) );
    asset alice_hbd = get_hbd_balance( "alice" );

    generate_block();

    fund( "alice", alice_hbd.amount );
    fund( "bob", alice_hbd.amount );
    fund( "sam", alice_hbd.amount );
    fund( "dave", alice_hbd.amount );

    int64_t alice_hbd_volume = 0;
    int64_t alice_hive_volume = 0;
    time_point_sec alice_reward_last_update = fc::time_point_sec::min();
    int64_t bob_hbd_volume = 0;
    int64_t bob_hive_volume = 0;
    time_point_sec bob_reward_last_update = fc::time_point_sec::min();
    int64_t sam_hbd_volume = 0;
    int64_t sam_hive_volume = 0;
    time_point_sec sam_reward_last_update = fc::time_point_sec::min();
    int64_t dave_hbd_volume = 0;
    int64_t dave_hive_volume = 0;
    time_point_sec dave_reward_last_update = fc::time_point_sec::min();

    BOOST_TEST_MESSAGE( "Creating Limit Order for HIVE that will stay on the books for 30 minutes exactly." );

    limit_order_create_operation op;
    op.owner = "alice";
    op.amount_to_sell = asset( alice_hbd.amount.value / 20, HBD_SYMBOL ) ;
    op.min_to_receive = op.amount_to_sell * exchange_rate;
    op.expiration = db->head_block_time() + fc::seconds( HIVE_MAX_LIMIT_ORDER_EXPIRATION );
    op.orderid = 1;

    tx.signatures.clear();
    tx.operations.clear();
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.operations.push_back( op );
    sign( tx, alice_private_key );
    db->push_transaction( tx, 0 );

    BOOST_TEST_MESSAGE( "Waiting 10 minutes" );

    generate_blocks( db->head_block_time() + HIVE_MIN_LIQUIDITY_REWARD_PERIOD_SEC_HF10, true );

    BOOST_TEST_MESSAGE( "Creating Limit Order for HBD that will be filled immediately." );

    op.owner = "bob";
    op.min_to_receive = op.amount_to_sell;
    op.amount_to_sell = op.min_to_receive * exchange_rate;
    op.fill_or_kill = false;
    op.orderid = 2;

    tx.signatures.clear();
    tx.operations.clear();
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.operations.push_back( op );
    sign( tx, bob_private_key );
    db->push_transaction( tx, 0 );

    alice_hive_volume += ( asset( alice_hbd.amount / 20, HBD_SYMBOL ) * exchange_rate ).amount.value;
    alice_reward_last_update = db->head_block_time();
    bob_hive_volume -= ( asset( alice_hbd.amount / 20, HBD_SYMBOL ) * exchange_rate ).amount.value;
    bob_reward_last_update = db->head_block_time();

    auto ops = get_last_operations( 1 );
    const auto& liquidity_idx = db->get_index< liquidity_reward_balance_index >().indices().get< by_owner >();
    const auto& limit_order_idx = db->get_index< limit_order_index >().indices().get< by_account >();

    auto reward = liquidity_idx.find( get_account_id( "alice" ) );
    BOOST_REQUIRE( reward == liquidity_idx.end() );
    /*BOOST_REQUIRE( reward->owner == get_account_id( "alice" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == alice_hbd_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == alice_hive_volume );
    BOOST_CHECK( reward->last_update == alice_reward_last_update );*/

    reward = liquidity_idx.find( get_account_id( "bob" ) );
    BOOST_REQUIRE( reward == liquidity_idx.end() );
    /*BOOST_REQUIRE( reward->owner == get_account_id( "bob" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == bob_hbd_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == bob_hive_volume );
    BOOST_CHECK( reward->last_update == bob_reward_last_update );*/

    auto fill_order_op = ops[0].get< fill_order_operation >();

    BOOST_REQUIRE( fill_order_op.open_owner == "alice" );
    BOOST_REQUIRE( fill_order_op.open_orderid == 1 );
    BOOST_REQUIRE( fill_order_op.open_pays.amount.value == asset( alice_hbd.amount.value / 20, HBD_SYMBOL ).amount.value );
    BOOST_REQUIRE( fill_order_op.current_owner == "bob" );
    BOOST_REQUIRE( fill_order_op.current_orderid == 2 );
    BOOST_REQUIRE( fill_order_op.current_pays.amount.value == ( asset( alice_hbd.amount.value / 20, HBD_SYMBOL ) * exchange_rate ).amount.value );

    BOOST_CHECK( limit_order_idx.find( boost::make_tuple( "alice", 1 ) ) == limit_order_idx.end() );
    BOOST_CHECK( limit_order_idx.find( boost::make_tuple( "bob", 2 ) ) == limit_order_idx.end() );

    BOOST_TEST_MESSAGE( "Creating Limit Order for HBD that will stay on the books for 60 minutes." );

    op.owner = "sam";
    op.amount_to_sell = asset( ( alice_hbd.amount.value / 20 ), HIVE_SYMBOL );
    op.min_to_receive = asset( ( alice_hbd.amount.value / 20 ), HBD_SYMBOL );
    op.orderid = 3;

    tx.signatures.clear();
    tx.operations.clear();
    tx.operations.push_back( op );
    sign( tx, sam_private_key );
    db->push_transaction( tx, 0 );

    BOOST_TEST_MESSAGE( "Waiting 10 minutes" );

    generate_blocks( db->head_block_time() + HIVE_MIN_LIQUIDITY_REWARD_PERIOD_SEC_HF10, true );

    BOOST_TEST_MESSAGE( "Creating Limit Order for HBD that will stay on the books for 30 minutes." );

    op.owner = "bob";
    op.orderid = 4;
    op.amount_to_sell = asset( ( alice_hbd.amount.value / 10 ) * 3 - alice_hbd.amount.value / 20, HIVE_SYMBOL );
    op.min_to_receive = asset( ( alice_hbd.amount.value / 10 ) * 3 - alice_hbd.amount.value / 20, HBD_SYMBOL );

    tx.signatures.clear();
    tx.operations.clear();
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.operations.push_back( op );
    sign( tx, bob_private_key );
    db->push_transaction( tx, 0 );

    BOOST_TEST_MESSAGE( "Waiting 30 minutes" );

    generate_blocks( db->head_block_time() + HIVE_MIN_LIQUIDITY_REWARD_PERIOD_SEC_HF10, true );

    BOOST_TEST_MESSAGE( "Filling both limit orders." );

    op.owner = "alice";
    op.orderid = 5;
    op.amount_to_sell = asset( ( alice_hbd.amount.value / 10 ) * 3, HBD_SYMBOL );
    op.min_to_receive = asset( ( alice_hbd.amount.value / 10 ) * 3, HIVE_SYMBOL );

    tx.signatures.clear();
    tx.operations.clear();
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.operations.push_back( op );
    sign( tx, alice_private_key );
    db->push_transaction( tx, 0 );

    alice_hbd_volume -= ( alice_hbd.amount.value / 10 ) * 3;
    alice_reward_last_update = db->head_block_time();
    sam_hbd_volume += alice_hbd.amount.value / 20;
    sam_reward_last_update = db->head_block_time();
    bob_hbd_volume += ( alice_hbd.amount.value / 10 ) * 3 - ( alice_hbd.amount.value / 20 );
    bob_reward_last_update = db->head_block_time();
    ops = get_last_operations( 4 );

    fill_order_op = ops[1].get< fill_order_operation >();
    BOOST_REQUIRE( fill_order_op.open_owner == "bob" );
    BOOST_REQUIRE( fill_order_op.open_orderid == 4 );
    BOOST_REQUIRE( fill_order_op.open_pays.amount.value == asset( ( alice_hbd.amount.value / 10 ) * 3 - alice_hbd.amount.value / 20, HIVE_SYMBOL ).amount.value );
    BOOST_REQUIRE( fill_order_op.current_owner == "alice" );
    BOOST_REQUIRE( fill_order_op.current_orderid == 5 );
    BOOST_REQUIRE( fill_order_op.current_pays.amount.value == asset( ( alice_hbd.amount.value / 10 ) * 3 - alice_hbd.amount.value / 20, HBD_SYMBOL ).amount.value );

    fill_order_op = ops[3].get< fill_order_operation >();
    BOOST_REQUIRE( fill_order_op.open_owner == "sam" );
    BOOST_REQUIRE( fill_order_op.open_orderid == 3 );
    BOOST_REQUIRE( fill_order_op.open_pays.amount.value == asset( alice_hbd.amount.value / 20, HIVE_SYMBOL ).amount.value );
    BOOST_REQUIRE( fill_order_op.current_owner == "alice" );
    BOOST_REQUIRE( fill_order_op.current_orderid == 5 );
    BOOST_REQUIRE( fill_order_op.current_pays.amount.value == asset( alice_hbd.amount.value / 20, HBD_SYMBOL ).amount.value );

    reward = liquidity_idx.find( get_account_id( "alice" ) );
    BOOST_REQUIRE( reward == liquidity_idx.end() );
    /*BOOST_REQUIRE( reward->owner == get_account_id( "alice" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == alice_hbd_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == alice_hive_volume );
    BOOST_CHECK( reward->last_update == alice_reward_last_update );*/

    reward = liquidity_idx.find( get_account_id( "bob" ) );
    BOOST_REQUIRE( reward == liquidity_idx.end() );
    /*BOOST_REQUIRE( reward->owner == get_account_id( "bob" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == bob_hbd_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == bob_hive_volume );
    BOOST_CHECK( reward->last_update == bob_reward_last_update );*/

    reward = liquidity_idx.find( get_account_id( "sam" ) );
    BOOST_REQUIRE( reward == liquidity_idx.end() );
    /*BOOST_REQUIRE( reward->owner == get_account_id( "sam" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == sam_hbd_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == sam_hive_volume );
    BOOST_CHECK( reward->last_update == sam_reward_last_update );*/

    BOOST_TEST_MESSAGE( "Testing a partial fill before minimum time and full fill after minimum time" );

    op.orderid = 6;
    op.amount_to_sell = asset( alice_hbd.amount.value / 20 * 2, HBD_SYMBOL );
    op.min_to_receive = asset( alice_hbd.amount.value / 20 * 2, HIVE_SYMBOL );

    tx.signatures.clear();
    tx.operations.clear();
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.operations.push_back( op );
    sign( tx, alice_private_key );
    db->push_transaction( tx, 0 );

    generate_blocks( db->head_block_time() + fc::seconds( HIVE_MIN_LIQUIDITY_REWARD_PERIOD_SEC_HF10.to_seconds() / 2 ), true );

    op.owner = "bob";
    op.orderid = 7;
    op.amount_to_sell = asset( alice_hbd.amount.value / 20, HIVE_SYMBOL );
    op.min_to_receive = asset( alice_hbd.amount.value / 20, HBD_SYMBOL );

    tx.signatures.clear();
    tx.operations.clear();
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.operations.push_back( op );
    sign( tx, bob_private_key );
    db->push_transaction( tx, 0 );

    generate_blocks( db->head_block_time() + fc::seconds( HIVE_MIN_LIQUIDITY_REWARD_PERIOD_SEC_HF10.to_seconds() / 2 ), true );

    ops = get_last_operations( 4 );
    fill_order_op = ops[3].get< fill_order_operation >();

    BOOST_REQUIRE( fill_order_op.open_owner == "alice" );
    BOOST_REQUIRE( fill_order_op.open_orderid == 6 );
    BOOST_REQUIRE( fill_order_op.open_pays.amount.value == asset( alice_hbd.amount.value / 20, HBD_SYMBOL ).amount.value );
    BOOST_REQUIRE( fill_order_op.current_owner == "bob" );
    BOOST_REQUIRE( fill_order_op.current_orderid == 7 );
    BOOST_REQUIRE( fill_order_op.current_pays.amount.value == asset( alice_hbd.amount.value / 20, HIVE_SYMBOL ).amount.value );

    reward = liquidity_idx.find( get_account_id( "alice" ) );
    BOOST_REQUIRE( reward == liquidity_idx.end() );
    /*BOOST_REQUIRE( reward->owner == get_account_id( "alice" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == alice_hbd_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == alice_hive_volume );
    BOOST_CHECK( reward->last_update == alice_reward_last_update );*/

    reward = liquidity_idx.find( get_account_id( "bob" ) );
    BOOST_REQUIRE( reward == liquidity_idx.end() );
    /*BOOST_REQUIRE( reward->owner == get_account_id( "bob" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == bob_hbd_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == bob_hive_volume );
    BOOST_CHECK( reward->last_update == bob_reward_last_update );*/

    reward = liquidity_idx.find( get_account_id( "sam" ) );
    BOOST_REQUIRE( reward == liquidity_idx.end() );
    /*BOOST_REQUIRE( reward->owner == get_account_id( "sam" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == sam_hbd_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == sam_hive_volume );
    BOOST_CHECK( reward->last_update == sam_reward_last_update );*/

    generate_blocks( db->head_block_time() + HIVE_MIN_LIQUIDITY_REWARD_PERIOD_SEC_HF10, true );

    op.owner = "sam";
    op.orderid = 8;

    tx.signatures.clear();
    tx.operations.clear();
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.operations.push_back( op );
    sign( tx, sam_private_key );
    db->push_transaction( tx, 0 );

    alice_hive_volume += alice_hbd.amount.value / 20;
    alice_reward_last_update = db->head_block_time();
    sam_hive_volume -= alice_hbd.amount.value / 20;
    sam_reward_last_update = db->head_block_time();

    ops = get_last_operations( 2 );
    fill_order_op = ops[1].get< fill_order_operation >();

    BOOST_REQUIRE( fill_order_op.open_owner == "alice" );
    BOOST_REQUIRE( fill_order_op.open_orderid == 6 );
    BOOST_REQUIRE( fill_order_op.open_pays.amount.value == asset( alice_hbd.amount.value / 20, HBD_SYMBOL ).amount.value );
    BOOST_REQUIRE( fill_order_op.current_owner == "sam" );
    BOOST_REQUIRE( fill_order_op.current_orderid == 8 );
    BOOST_REQUIRE( fill_order_op.current_pays.amount.value == asset( alice_hbd.amount.value / 20, HIVE_SYMBOL ).amount.value );

    reward = liquidity_idx.find( get_account_id( "alice" ) );
    BOOST_REQUIRE( reward == liquidity_idx.end() );
    /*BOOST_REQUIRE( reward->owner == get_account_id( "alice" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == alice_hbd_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == alice_hive_volume );
    BOOST_CHECK( reward->last_update == alice_reward_last_update );*/

    reward = liquidity_idx.find( get_account_id( "bob" ) );
    BOOST_REQUIRE( reward == liquidity_idx.end() );
    /*BOOST_REQUIRE( reward->owner == get_account_id( "bob" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == bob_hbd_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == bob_hive_volume );
    BOOST_CHECK( reward->last_update == bob_reward_last_update );*/

    reward = liquidity_idx.find( get_account_id( "sam" ) );
    BOOST_REQUIRE( reward == liquidity_idx.end() );
    /*BOOST_REQUIRE( reward->owner == get_account_id( "sam" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == sam_hbd_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == sam_hive_volume );
    BOOST_CHECK( reward->last_update == sam_reward_last_update );*/

    BOOST_TEST_MESSAGE( "Trading to give Alice and Bob positive volumes to receive rewards" );

    transfer_operation transfer;
    transfer.to = "dave";
    transfer.from = "alice";
    transfer.amount = asset( alice_hbd.amount / 2, HBD_SYMBOL );

    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( transfer );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    sign( tx, alice_private_key );
    db->push_transaction( tx, 0 );

    op.owner = "alice";
    op.amount_to_sell = asset( 8 * ( alice_hbd.amount.value / 20 ), HIVE_SYMBOL );
    op.min_to_receive = asset( op.amount_to_sell.amount, HBD_SYMBOL );
    op.orderid = 9;
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    sign( tx, alice_private_key );
    db->push_transaction( tx, 0 );

    generate_blocks( db->head_block_time() + HIVE_MIN_LIQUIDITY_REWARD_PERIOD_SEC_HF10, true );

    op.owner = "dave";
    op.amount_to_sell = asset( 7 * ( alice_hbd.amount.value / 20 ), HBD_SYMBOL );;
    op.min_to_receive = asset( op.amount_to_sell.amount, HIVE_SYMBOL );
    op.orderid = 10;
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    sign( tx, dave_private_key );
    db->push_transaction( tx, 0 );

    alice_hbd_volume += op.amount_to_sell.amount.value;
    alice_reward_last_update = db->head_block_time();
    dave_hbd_volume -= op.amount_to_sell.amount.value;
    dave_reward_last_update = db->head_block_time();

    ops = get_last_operations( 1 );
    fill_order_op = ops[0].get< fill_order_operation >();

    BOOST_REQUIRE( fill_order_op.open_owner == "alice" );
    BOOST_REQUIRE( fill_order_op.open_orderid == 9 );
    BOOST_REQUIRE( fill_order_op.open_pays.amount.value == 7 * ( alice_hbd.amount.value / 20 ) );
    BOOST_REQUIRE( fill_order_op.current_owner == "dave" );
    BOOST_REQUIRE( fill_order_op.current_orderid == 10 );
    BOOST_REQUIRE( fill_order_op.current_pays.amount.value == 7 * ( alice_hbd.amount.value / 20 ) );

    reward = liquidity_idx.find( get_account_id( "alice" ) );
    BOOST_REQUIRE( reward == liquidity_idx.end() );
    /*BOOST_REQUIRE( reward->owner == get_account_id( "alice" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == alice_hbd_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == alice_hive_volume );
    BOOST_CHECK( reward->last_update == alice_reward_last_update );*/

    reward = liquidity_idx.find( get_account_id( "bob" ) );
    BOOST_REQUIRE( reward == liquidity_idx.end() );
    /*BOOST_REQUIRE( reward->owner == get_account_id( "bob" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == bob_hbd_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == bob_hive_volume );
    BOOST_CHECK( reward->last_update == bob_reward_last_update );*/

    reward = liquidity_idx.find( get_account_id( "sam" ) );
    BOOST_REQUIRE( reward == liquidity_idx.end() );
    /*BOOST_REQUIRE( reward->owner == get_account_id( "sam" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == sam_hbd_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == sam_hive_volume );
    BOOST_CHECK( reward->last_update == sam_reward_last_update );*/

    reward = liquidity_idx.find( get_account_id( "dave" ) );
    BOOST_REQUIRE( reward == liquidity_idx.end() );
    /*BOOST_REQUIRE( reward->owner == get_account_id( "dave" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == dave_hbd_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == dave_hive_volume );
    BOOST_CHECK( reward->last_update == dave_reward_last_update );*/

    op.owner = "bob";
    op.amount_to_sell.amount = alice_hbd.amount / 20;
    op.min_to_receive.amount = op.amount_to_sell.amount;
    op.orderid = 11;
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    sign( tx, bob_private_key );
    db->push_transaction( tx, 0 );

    alice_hbd_volume += op.amount_to_sell.amount.value;
    alice_reward_last_update = db->head_block_time();
    bob_hbd_volume -= op.amount_to_sell.amount.value;
    bob_reward_last_update = db->head_block_time();

    ops = get_last_operations( 1 );
    fill_order_op = ops[0].get< fill_order_operation >();

    BOOST_REQUIRE( fill_order_op.open_owner == "alice" );
    BOOST_REQUIRE( fill_order_op.open_orderid == 9 );
    BOOST_REQUIRE( fill_order_op.open_pays.amount.value == alice_hbd.amount.value / 20 );
    BOOST_REQUIRE( fill_order_op.current_owner == "bob" );
    BOOST_REQUIRE( fill_order_op.current_orderid == 11 );
    BOOST_REQUIRE( fill_order_op.current_pays.amount.value == alice_hbd.amount.value / 20 );

    reward = liquidity_idx.find( get_account_id( "alice" ) );
    BOOST_REQUIRE( reward == liquidity_idx.end() );
    /*BOOST_REQUIRE( reward->owner == get_account_id( "alice" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == alice_hbd_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == alice_hive_volume );
    BOOST_CHECK( reward->last_update == alice_reward_last_update );*/

    reward = liquidity_idx.find( get_account_id( "bob" ) );
    BOOST_REQUIRE( reward == liquidity_idx.end() );
    /*BOOST_REQUIRE( reward->owner == get_account_id( "bob" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == bob_hbd_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == bob_hive_volume );
    BOOST_CHECK( reward->last_update == bob_reward_last_update );*/

    reward = liquidity_idx.find( get_account_id( "sam" ) );
    BOOST_REQUIRE( reward == liquidity_idx.end() );
    /*BOOST_REQUIRE( reward->owner == get_account_id( "sam" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == sam_hbd_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == sam_hive_volume );
    BOOST_CHECK( reward->last_update == sam_reward_last_update );*/

    reward = liquidity_idx.find( get_account_id( "dave" ) );
    BOOST_REQUIRE( reward == liquidity_idx.end() );
    /*BOOST_REQUIRE( reward->owner == get_account_id( "dave" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == dave_hbd_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == dave_hive_volume );
    BOOST_CHECK( reward->last_update == dave_reward_last_update );*/

    transfer.to = "bob";
    transfer.from = "alice";
    transfer.amount = asset( alice_hbd.amount / 5, HBD_SYMBOL );
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( transfer );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    sign( tx, alice_private_key );
    db->push_transaction( tx, 0 );

    op.owner = "bob";
    op.orderid = 12;
    op.amount_to_sell = asset( 3 * ( alice_hbd.amount / 40 ), HBD_SYMBOL );
    op.min_to_receive = asset( op.amount_to_sell.amount, HIVE_SYMBOL );
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    sign( tx, bob_private_key );
    db->push_transaction( tx, 0 );

    generate_blocks( db->head_block_time() + HIVE_MIN_LIQUIDITY_REWARD_PERIOD_SEC_HF10, true );

    op.owner = "dave";
    op.orderid = 13;
    op.amount_to_sell = op.min_to_receive;
    op.min_to_receive.symbol = HBD_SYMBOL;
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    sign( tx, dave_private_key );
    db->push_transaction( tx, 0 );

    bob_hive_volume += op.amount_to_sell.amount.value;
    bob_reward_last_update = db->head_block_time();
    dave_hive_volume -= op.amount_to_sell.amount.value;
    dave_reward_last_update = db->head_block_time();

    ops = get_last_operations( 1 );
    fill_order_op = ops[0].get< fill_order_operation >();

    BOOST_REQUIRE( fill_order_op.open_owner == "bob" );
    BOOST_REQUIRE( fill_order_op.open_orderid == 12 );
    BOOST_REQUIRE( fill_order_op.open_pays.amount.value == 3 * ( alice_hbd.amount.value / 40 ) );
    BOOST_REQUIRE( fill_order_op.current_owner == "dave" );
    BOOST_REQUIRE( fill_order_op.current_orderid == 13 );
    BOOST_REQUIRE( fill_order_op.current_pays.amount.value == 3 * ( alice_hbd.amount.value / 40 ) );

    reward = liquidity_idx.find( get_account_id( "alice" ) );
    BOOST_REQUIRE( reward == liquidity_idx.end() );
    /*BOOST_REQUIRE( reward->owner == get_account_id( "alice" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == alice_hbd_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == alice_hive_volume );
    BOOST_CHECK( reward->last_update == alice_reward_last_update );*/

    reward = liquidity_idx.find( get_account_id( "bob" ) );
    BOOST_REQUIRE( reward == liquidity_idx.end() );
    /*BOOST_REQUIRE( reward->owner == get_account_id( "bob" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == bob_hbd_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == bob_hive_volume );
    BOOST_CHECK( reward->last_update == bob_reward_last_update );*/

    reward = liquidity_idx.find( get_account_id( "sam" ) );
    BOOST_REQUIRE( reward == liquidity_idx.end() );
    /*BOOST_REQUIRE( reward->owner == get_account_id( "sam" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == sam_hbd_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == sam_hive_volume );
    BOOST_CHECK( reward->last_update == sam_reward_last_update );*/

    reward = liquidity_idx.find( get_account_id( "dave" ) );
    BOOST_REQUIRE( reward == liquidity_idx.end() );
    /*BOOST_REQUIRE( reward->owner == get_account_id( "dave" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == dave_hbd_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == dave_hive_volume );
    BOOST_CHECK( reward->last_update == dave_reward_last_update );*/

    auto alice_balance = get_balance( "alice" );
    auto bob_balance = get_balance( "bob" );
    auto sam_balance = get_balance( "sam" );
    auto dave_balance = get_balance( "dave" );

    BOOST_TEST_MESSAGE( "Generating Blocks to trigger liquidity rewards" );

    db->liquidity_rewards_enabled = true;
    generate_blocks( HIVE_LIQUIDITY_REWARD_BLOCKS - ( db->head_block_num() % HIVE_LIQUIDITY_REWARD_BLOCKS ) - 1 );

    BOOST_REQUIRE( db->head_block_num() % HIVE_LIQUIDITY_REWARD_BLOCKS == HIVE_LIQUIDITY_REWARD_BLOCKS - 1 );
    BOOST_REQUIRE( get_balance( "alice" ).amount.value == alice_balance.amount.value );
    BOOST_REQUIRE( get_balance( "bob" ).amount.value == bob_balance.amount.value );
    BOOST_REQUIRE( get_balance( "sam" ).amount.value == sam_balance.amount.value );
    BOOST_REQUIRE( get_balance( "dave" ).amount.value == dave_balance.amount.value );

    generate_block();

    //alice_balance += HIVE_MIN_LIQUIDITY_REWARD;

    BOOST_REQUIRE( get_balance( "alice" ).amount.value == alice_balance.amount.value );
    BOOST_REQUIRE( get_balance( "bob" ).amount.value == bob_balance.amount.value );
    BOOST_REQUIRE( get_balance( "sam" ).amount.value == sam_balance.amount.value );
    BOOST_REQUIRE( get_balance( "dave" ).amount.value == dave_balance.amount.value );

    ops = get_last_operations( 1 );

    HIVE_REQUIRE_THROW( ops[0].get< liquidity_reward_operation>(), fc::exception );
    //BOOST_REQUIRE( ops[0].get< liquidity_reward_operation>().payout.amount.value == HIVE_MIN_LIQUIDITY_REWARD.amount.value );

    generate_blocks( HIVE_LIQUIDITY_REWARD_BLOCKS );

    //bob_balance += HIVE_MIN_LIQUIDITY_REWARD;

    BOOST_REQUIRE( get_balance( "alice" ).amount.value == alice_balance.amount.value );
    BOOST_REQUIRE( get_balance( "bob" ).amount.value == bob_balance.amount.value );
    BOOST_REQUIRE( get_balance( "sam" ).amount.value == sam_balance.amount.value );
    BOOST_REQUIRE( get_balance( "dave" ).amount.value == dave_balance.amount.value );

    ops = get_last_operations( 1 );

    HIVE_REQUIRE_THROW( ops[0].get< liquidity_reward_operation>(), fc::exception );
    //BOOST_REQUIRE( ops[0].get< liquidity_reward_operation>().payout.amount.value == HIVE_MIN_LIQUIDITY_REWARD.amount.value );

    alice_hive_volume = 0;
    alice_hbd_volume = 0;
    bob_hive_volume = 0;
    bob_hbd_volume = 0;

    BOOST_TEST_MESSAGE( "Testing liquidity timeout" );

    generate_blocks( sam_reward_last_update + HIVE_LIQUIDITY_TIMEOUT_SEC - fc::seconds( HIVE_BLOCK_INTERVAL / 2 ) - HIVE_MIN_LIQUIDITY_REWARD_PERIOD_SEC, true );

    op.owner = "sam";
    op.orderid = 14;
    op.amount_to_sell = ASSET( "1.000 TESTS" );
    op.min_to_receive = ASSET( "1.000 TBD" );
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    sign( tx, sam_private_key );
    db->push_transaction( tx, 0 );

    generate_blocks( db->head_block_time() + ( HIVE_BLOCK_INTERVAL / 2 ) + HIVE_LIQUIDITY_TIMEOUT_SEC, true );

    reward = liquidity_idx.find( get_account_id( "sam" ) );
    /*BOOST_REQUIRE( reward == liquidity_idx.end() );
    BOOST_REQUIRE( reward->owner == get_account_id( "sam" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == sam_hbd_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == sam_hive_volume );
    BOOST_CHECK( reward->last_update == sam_reward_last_update );*/

    generate_block();

    op.owner = "alice";
    op.orderid = 15;
    op.amount_to_sell.symbol = HBD_SYMBOL;
    op.min_to_receive.symbol = HIVE_SYMBOL;
    tx.operations.clear();
    tx.signatures.clear();
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    sign( tx, alice_private_key );
    db->push_transaction( tx, 0 );

    sam_hbd_volume = ASSET( "1.000 TBD" ).amount.value;
    sam_hive_volume = 0;
    sam_reward_last_update = db->head_block_time();

    reward = liquidity_idx.find( get_account_id( "sam" ) );
    /*BOOST_REQUIRE( reward == liquidity_idx.end() );
    BOOST_REQUIRE( reward->owner == get_account_id( "sam" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == sam_hbd_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == sam_hive_volume );
    BOOST_CHECK( reward->last_update == sam_reward_last_update );*/
  }
  FC_LOG_AND_RETHROW();
}

BOOST_AUTO_TEST_CASE( post_rate_limit )
{
  try
  {
    ACTORS( (alice) )

    fund( "alice", 10000 );
    vest( "alice", 10000 );

    comment_operation op;
    op.author = "alice";
    op.permlink = "test1";
    op.parent_author = "";
    op.parent_permlink = "test";
    op.body = "test";

    signed_transaction tx;

    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    sign( tx, alice_private_key );
    db->push_transaction( tx, 0 );


    BOOST_REQUIRE( db->find_comment_cashout( db->get_comment( "alice", string( "test1" ) ) )->reward_weight == HIVE_100_PERCENT );

    tx.operations.clear();
    tx.signatures.clear();

    generate_blocks( db->head_block_time() + HIVE_MIN_ROOT_COMMENT_INTERVAL + fc::seconds( HIVE_BLOCK_INTERVAL ), true );

    op.permlink = "test2";

    tx.operations.push_back( op );
    sign( tx, alice_private_key );
    db->push_transaction( tx, 0 );

    BOOST_REQUIRE( db->find_comment_cashout( db->get_comment( "alice", string( "test2" ) ) )->reward_weight == HIVE_100_PERCENT );

    generate_blocks( db->head_block_time() + HIVE_MIN_ROOT_COMMENT_INTERVAL + fc::seconds( HIVE_BLOCK_INTERVAL ), true );

    tx.operations.clear();
    tx.signatures.clear();

    op.permlink = "test3";

    tx.operations.push_back( op );
    sign( tx, alice_private_key );
    db->push_transaction( tx, 0 );

    BOOST_REQUIRE( db->find_comment_cashout( db->get_comment( "alice", string( "test3" ) ) )->reward_weight == HIVE_100_PERCENT );

    generate_blocks( db->head_block_time() + HIVE_MIN_ROOT_COMMENT_INTERVAL + fc::seconds( HIVE_BLOCK_INTERVAL ), true );

    tx.operations.clear();
    tx.signatures.clear();

    op.permlink = "test4";

    tx.operations.push_back( op );
    sign( tx, alice_private_key );
    db->push_transaction( tx, 0 );

    BOOST_REQUIRE( db->find_comment_cashout( db->get_comment( "alice", string( "test4" ) ) )->reward_weight == HIVE_100_PERCENT );

    generate_blocks( db->head_block_time() + HIVE_MIN_ROOT_COMMENT_INTERVAL + fc::seconds( HIVE_BLOCK_INTERVAL ), true );

    tx.operations.clear();
    tx.signatures.clear();

    op.permlink = "test5";

    tx.operations.push_back( op );
    sign( tx, alice_private_key );
    db->push_transaction( tx, 0 );

    BOOST_REQUIRE( db->find_comment_cashout( db->get_comment( "alice", string( "test5" ) ) )->reward_weight == HIVE_100_PERCENT );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( comment_freeze )
{
  try
  {
    ACTORS( (alice)(bob)(sam)(dave) )
    fund( "alice", 10000 );
    fund( "bob", 10000 );
    fund( "sam", 10000 );
    fund( "dave", 10000 );

    vest( "alice", 10000 );
    vest( "bob", 10000 );
    vest( "sam", 10000 );
    vest( "dave", 10000 );

    auto exchange_rate = price( ASSET( "1.000 TBD" ), ASSET( "1.250 TESTS" ) );
    set_price_feed( exchange_rate );

    comment_operation comment;
    comment.author = "alice";
    comment.parent_author = "";
    comment.permlink = "test";
    comment.parent_permlink = "test";
    comment.body = "test";
    push_transaction( comment, alice_private_key );

    generate_block();

    comment.body = "test2";
    push_transaction( comment, alice_private_key );

    vote_operation vote;
    vote.weight = HIVE_100_PERCENT;
    vote.voter = "bob";
    vote.author = "alice";
    vote.permlink = "test";
    push_transaction( vote, bob_private_key );

    BOOST_REQUIRE( db->find_comment_cashout( db->get_comment( "alice", string( "test" ) ) )->cashout_time != fc::time_point_sec::min() );
    BOOST_REQUIRE( db->find_comment_cashout( db->get_comment( "alice", string( "test" ) ) )->cashout_time != fc::time_point_sec::maximum() );

    generate_blocks( db->find_comment_cashout( db->get_comment( "alice", string( "test" ) ) )->cashout_time, true );

    {
      const comment_object& _comment = db->get_comment( "alice", string( "test" ) );
      const comment_cashout_object* _comment_cashout = db->find_comment_cashout( _comment );
      BOOST_REQUIRE( _comment_cashout == nullptr );
    }

    vote.voter = "sam";
    /// Starting from HF25 voting for already paid posts is allowed again.
    push_transaction( vote, sam_private_key );

    {
      const comment_object& _comment = db->get_comment( "alice", string( "test" ) );
      const comment_cashout_object* _comment_cashout = db->find_comment_cashout( _comment );
      BOOST_REQUIRE( _comment_cashout == nullptr );
    }

    vote.voter = "bob";
    vote.weight = HIVE_100_PERCENT * -1;
    /// Starting from HF25 voting for already paid posts is allowed again.
    push_transaction( vote, bob_private_key );

    {
      const comment_object& _comment = db->get_comment( "alice", string( "test" ) );
      const comment_cashout_object* _comment_cashout = db->find_comment_cashout( _comment );
      BOOST_REQUIRE( _comment_cashout == nullptr );
    }

    vote.voter = "dave";
    vote.weight = 0;
    /// Starting from HF25 voting for already paid posts is allowed again.
    push_transaction( vote, dave_private_key );

    {
      const comment_object& _comment = db->get_comment( "alice", string( "test" ) );
      const comment_cashout_object* _comment_cashout = db->find_comment_cashout( _comment );
      BOOST_REQUIRE( _comment_cashout == nullptr );
    }

    comment.body = "test4";
    push_transaction( comment, alice_private_key ); // Works now in #1714

    comment.author = "alice";
    comment.parent_author = "";
    comment.permlink = "test-fast-vote";
    comment.parent_permlink = "test";
    comment.body = "test";
    push_transaction( comment, alice_private_key );

    comment.author = "bob";
    push_transaction( comment, bob_private_key );

    generate_block();

    // vote multiple times in the same block - no longer 3s cooldown - works since HF26
    vote.author = "alice";
    vote.permlink = "test-fast-vote";
    vote.voter = "dave";
    vote.weight = HIVE_100_PERCENT;
    push_transaction( vote, dave_private_key );
    vote.author = "bob";
    push_transaction( vote, dave_private_key );

    generate_block();
  }
  FC_LOG_AND_RETHROW()
}

// This test is too intensive without optimizations. Disable it when we build in debug
BOOST_AUTO_TEST_CASE( hbd_stability )
{
  #ifndef DEBUG
  try
  {
    db_plugin->debug_update( [=]( database& db )
    {
      db.modify( db.get_dynamic_global_properties(), [&]( dynamic_global_property_object& gpo )
      {
        gpo.sps_fund_percent = 0;
        gpo.content_reward_percent = 75 * HIVE_1_PERCENT;
      });
    }, database::skip_witness_signature );

    resize_shared_mem( 1024 * 1024 * 512 ); // Due to number of blocks in the test, it requires a large file. (64 MB)

    auto debug_key = "5JdouSvkK75TKWrJixYufQgePT21V7BAVWbNUWt3ktqhPmy8Z78"; //get_dev_key debug node

    ACTORS( (alice)(bob)(sam)(dave)(greg) );

    fund( "alice", 10000 );
    fund( "bob", 10000 );

    vest( "alice", 10000 );
    vest( "bob", 10000 );

    auto exchange_rate = price( ASSET( "1.000 TBD" ), ASSET( "10.000 TESTS" ) );
    set_price_feed( exchange_rate );

    BOOST_REQUIRE( db->get_dynamic_global_properties().get_hbd_print_rate() == HIVE_100_PERCENT );

    comment_operation comment;
    comment.author = "alice";
    comment.permlink = "test";
    comment.parent_permlink = "test";
    comment.title = "test";
    comment.body = "test";

    signed_transaction tx;
    tx.operations.push_back( comment );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    sign( tx, alice_private_key );
    db->push_transaction( tx, 0 );

    vote_operation vote;
    vote.voter = "bob";
    vote.author = "alice";
    vote.permlink = "test";
    vote.weight = HIVE_100_PERCENT;

    tx.operations.clear();
    tx.signatures.clear();

    tx.operations.push_back( vote );
    sign( tx, bob_private_key );
    db->push_transaction( tx, 0 );

    BOOST_TEST_MESSAGE( "Generating blocks up to comment payout" );

    db_plugin->debug_generate_blocks_until( debug_key, fc::time_point_sec( db->find_comment_cashout( db->get_comment( comment.author, comment.permlink ) )->cashout_time.sec_since_epoch() - 2 * HIVE_BLOCK_INTERVAL ), true, database::skip_witness_signature );

    auto& gpo = db->get_dynamic_global_properties();

    BOOST_TEST_MESSAGE( "Changing sam and gpo to set up market cap conditions" );

    asset hbd_balance = asset( ( gpo.virtual_supply.amount * ( gpo.hbd_stop_percent + 112 ) ) / HIVE_100_PERCENT, HIVE_SYMBOL ) * exchange_rate;
    db_plugin->debug_update( [=]( database& db )
    {
      db.modify( db.get_account( "sam" ), [&]( account_object& a )
      {
        a.hbd_balance = hbd_balance;
      });
    }, database::skip_witness_signature );

    db_plugin->debug_update( [=]( database& db )
    {
      db.modify( db.get_dynamic_global_properties(), [&]( dynamic_global_property_object& gpo )
      {
        gpo.current_hbd_supply = hbd_balance + db.get_treasury().get_hbd_balance();
        gpo.virtual_supply = gpo.virtual_supply + hbd_balance * exchange_rate;
      });
    }, database::skip_witness_signature );

    validate_database();

    db_plugin->debug_generate_blocks( debug_key, 1, database::skip_witness_signature );

    auto comment_reward = ( gpo.get_total_reward_fund_hive().amount + 2000 ) - ( ( gpo.get_total_reward_fund_hive().amount + 2000 ) * 25 * HIVE_1_PERCENT ) / HIVE_100_PERCENT ;
    comment_reward /= 2;
    auto hbd_reward = ( comment_reward * gpo.get_hbd_print_rate() ) / HIVE_100_PERCENT;
    auto alice_hbd = get_hbd_balance( "alice" ) + get_hbd_rewards( "alice" ) + asset( hbd_reward, HIVE_SYMBOL ) * exchange_rate;
    auto alice_hive = get_balance( "alice" ) + get_rewards( "alice" ) ;

    BOOST_TEST_MESSAGE( "Checking printing HBD has slowed" );
    BOOST_REQUIRE( db->get_dynamic_global_properties().get_hbd_print_rate() < HIVE_100_PERCENT );

    BOOST_TEST_MESSAGE( "Pay out comment and check rewards are paid as HIVE" );
    db_plugin->debug_generate_blocks( debug_key, 1, database::skip_witness_signature );

    validate_database();

    BOOST_REQUIRE( get_hbd_balance( "alice" ) + get_hbd_rewards( "alice" ) == alice_hbd );
    BOOST_REQUIRE( get_balance( "alice" ) + get_rewards( "alice" ) > alice_hive );

    BOOST_TEST_MESSAGE( "Letting percent market cap fall to hbd_start_percent to verify printing of HBD turns back on" );

    // Get close to hbd_start_percent for printing HBD to start again, but not all the way
    db_plugin->debug_update( [&]( database& db )
    {
      db.modify( db.get_account( "sam" ), [&]( account_object& a )
      {
        a.hbd_balance = asset( ( ( gpo.hbd_start_percent - 9 ) * hbd_balance.amount ) / gpo.hbd_stop_percent, HBD_SYMBOL );
      });
    }, database::skip_witness_signature );

    auto current_hbd_supply = alice_hbd + asset( ( ( gpo.hbd_start_percent - 9 ) * hbd_balance.amount ) / gpo.hbd_stop_percent, HBD_SYMBOL ) + db->get_treasury().get_hbd_balance();

    db_plugin->debug_update( [=]( database& db )
    {
      db.modify( db.get_dynamic_global_properties(), [&]( dynamic_global_property_object& gpo )
      {
        gpo.current_hbd_supply = current_hbd_supply;
      });
    }, database::skip_witness_signature );

    db_plugin->debug_generate_blocks( debug_key, 1, database::skip_witness_signature );
    validate_database();

    BOOST_REQUIRE( db->get_dynamic_global_properties().get_hbd_print_rate() < HIVE_100_PERCENT );

    auto last_print_rate = db->get_dynamic_global_properties().get_hbd_print_rate();

    // Keep producing blocks until printing HBD is back
    while( ( db->get_dynamic_global_properties().get_current_hbd_supply() * exchange_rate ).amount >= ( db->get_dynamic_global_properties().virtual_supply.amount * db->get_dynamic_global_properties().hbd_start_percent ) / HIVE_100_PERCENT )
    {
      auto& gpo = db->get_dynamic_global_properties();
      BOOST_REQUIRE( gpo.get_hbd_print_rate() >= last_print_rate );
      last_print_rate = gpo.get_hbd_print_rate();
      db_plugin->debug_generate_blocks( debug_key, 1, database::skip_witness_signature );
      if( db->head_block_num() % 1000 == 0 )
        validate_database();
    }

    validate_database();

    BOOST_REQUIRE( db->get_dynamic_global_properties().get_hbd_print_rate() == HIVE_100_PERCENT );
  }
  FC_LOG_AND_RETHROW()
  #endif
}

BOOST_AUTO_TEST_CASE( hbd_price_feed_limit )
{
  try
  {
    ACTORS( (alice) );
    generate_block();
    vest( HIVE_INIT_MINER_NAME, "alice", ASSET( "10.000 TESTS" ) );

    price exchange_rate( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) );
    set_price_feed( exchange_rate );

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

    signed_transaction tx;
    tx.operations.push_back( comment );
    tx.operations.push_back( vote );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    sign( tx, alice_private_key );
    db->push_transaction( tx, 0 );

    generate_blocks( db->find_comment_cashout( db->get_comment( "alice", string( "test" ) ) )->cashout_time, true );

    BOOST_TEST_MESSAGE( "Setting HBD percent to greater than 10% market cap." );

    db->skip_price_feed_limit_check = false;
    const auto& gpo = db->get_dynamic_global_properties();
    auto new_exchange_rate = price( gpo.get_current_hbd_supply(), asset( ( HIVE_100_PERCENT ) * gpo.get_current_supply().amount, HIVE_SYMBOL ) );
    set_price_feed( new_exchange_rate );
    set_price_feed( new_exchange_rate, true );

    auto recent_ops = get_last_operations( 2 );
    auto sys_warn_op = recent_ops.back().get< system_warning_operation >();
    BOOST_REQUIRE( sys_warn_op.message.compare( 0, 27, "HIVE price corrected upward" ) == 0 );

    const auto& feed = db->get_feed_history();
    BOOST_REQUIRE( feed.current_median_history > new_exchange_rate && feed.current_median_history < exchange_rate );
    BOOST_REQUIRE( feed.market_median_history == new_exchange_rate );
    BOOST_REQUIRE( feed.current_min_history == new_exchange_rate );
    BOOST_REQUIRE( feed.current_max_history == exchange_rate );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( clear_null_account )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing clearing the null account's balances on block" );

    ACTORS( (alice) );
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );

    fund( "alice", ASSET( "10.000 TESTS" ) );
    fund( "alice", ASSET( "10.000 TBD" ) );

    transfer_operation transfer1;
    transfer1.from = "alice";
    transfer1.to = HIVE_NULL_ACCOUNT;
    transfer1.amount = ASSET( "1.000 TESTS" );

    transfer_operation transfer2;
    transfer2.from = "alice";
    transfer2.to = HIVE_NULL_ACCOUNT;
    transfer2.amount = ASSET( "2.000 TBD" );

    transfer_to_vesting_operation vest;
    vest.from = "alice";
    vest.to = HIVE_NULL_ACCOUNT;
    vest.amount = ASSET( "3.000 TESTS" );

    transfer_to_savings_operation save1;
    save1.from = "alice";
    save1.to = HIVE_NULL_ACCOUNT;
    save1.amount = ASSET( "4.000 TESTS" );

    transfer_to_savings_operation save2;
    save2.from = "alice";
    save2.to = HIVE_NULL_ACCOUNT;
    save2.amount = ASSET( "5.000 TBD" );

    BOOST_TEST_MESSAGE( "--- Transferring to NULL Account" );

    signed_transaction tx;
    tx.operations.push_back( transfer1 );
    tx.operations.push_back( transfer2 );
    tx.operations.push_back( vest );
    tx.operations.push_back( save1);
    tx.operations.push_back( save2 );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    sign( tx, alice_private_key );
    db->push_transaction( tx, 0 );
    validate_database();

    db_plugin->debug_update( [=]( database& db )
    {
      db.modify( db.get_account( HIVE_NULL_ACCOUNT ), [&]( account_object& a )
      {
        a.reward_hive_balance = ASSET( "1.000 TESTS" );
        a.reward_hbd_balance = ASSET( "1.000 TBD" );
        a.reward_vesting_balance = ASSET( "1.000000 VESTS" );
        a.reward_vesting_hive = ASSET( "1.000 TESTS" );
      });

      db.modify( db.get_dynamic_global_properties(), [&]( dynamic_global_property_object& gpo )
      {
        gpo.current_supply += ASSET( "2.000 TESTS" );
        gpo.virtual_supply += ASSET( "3.000 TESTS" );
        gpo.current_hbd_supply += ASSET( "1.000 TBD" );
        gpo.pending_rewarded_vesting_shares += ASSET( "1.000000 VESTS" );
        gpo.pending_rewarded_vesting_hive += ASSET( "1.000 TESTS" );
      });
    });

    validate_database();

    BOOST_REQUIRE( get_balance( HIVE_NULL_ACCOUNT ) == ASSET( "1.000 TESTS" ) );
    BOOST_REQUIRE( get_hbd_balance( HIVE_NULL_ACCOUNT ) == ASSET( "2.000 TBD" ) );
    BOOST_REQUIRE( get_vesting( HIVE_NULL_ACCOUNT ) > ASSET( "0.000000 VESTS" ) );
    BOOST_REQUIRE( get_savings( HIVE_NULL_ACCOUNT ) == ASSET( "4.000 TESTS" ) );
    BOOST_REQUIRE( get_hbd_savings( HIVE_NULL_ACCOUNT ) == ASSET( "5.000 TBD" ) );
    BOOST_REQUIRE( get_hbd_rewards( HIVE_NULL_ACCOUNT ) == ASSET( "1.000 TBD" ) );
    BOOST_REQUIRE( get_rewards( HIVE_NULL_ACCOUNT ) == ASSET( "1.000 TESTS" ) );
    BOOST_REQUIRE( get_vest_rewards( HIVE_NULL_ACCOUNT ) == ASSET( "1.000000 VESTS" ) );
    BOOST_REQUIRE( get_vest_rewards_as_hive( HIVE_NULL_ACCOUNT ) == ASSET( "1.000 TESTS" ) );
    BOOST_REQUIRE( get_balance( "alice" ) == ASSET( "2.000 TESTS" ) );
    BOOST_REQUIRE( get_hbd_balance( "alice" ) == ASSET( "3.000 TBD" ) );

    BOOST_TEST_MESSAGE( "--- Generating block to clear balances" );
    generate_block();
    validate_database();

    BOOST_REQUIRE( get_balance( HIVE_NULL_ACCOUNT ) == ASSET( "0.000 TESTS" ) );
    BOOST_REQUIRE( get_hbd_balance( HIVE_NULL_ACCOUNT ) == ASSET( "0.000 TBD" ) );
    BOOST_REQUIRE( get_vesting( HIVE_NULL_ACCOUNT ) == ASSET( "0.000000 VESTS" ) );
    BOOST_REQUIRE( get_savings( HIVE_NULL_ACCOUNT ) == ASSET( "0.000 TESTS" ) );
    BOOST_REQUIRE( get_hbd_savings( HIVE_NULL_ACCOUNT ) == ASSET( "0.000 TBD" ) );
    BOOST_REQUIRE( get_hbd_rewards( HIVE_NULL_ACCOUNT ) == ASSET( "0.000 TBD" ) );
    BOOST_REQUIRE( get_rewards( HIVE_NULL_ACCOUNT ) == ASSET( "0.000 TESTS" ) );
    BOOST_REQUIRE( get_vest_rewards( HIVE_NULL_ACCOUNT ) == ASSET( "0.000000 VESTS" ) );
    BOOST_REQUIRE( get_vest_rewards_as_hive( HIVE_NULL_ACCOUNT ) == ASSET( "0.000 TESTS" ) );
    BOOST_REQUIRE( get_balance( "alice" ) == ASSET( "2.000 TESTS" ) );
    BOOST_REQUIRE( get_hbd_balance( "alice" ) == ASSET( "3.000 TBD" ) );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( generate_account_subsidies )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: generate_account_subsidies" );

    // The purpose of this test is to validate the dynamics of available_account_subsidies.

    // set_subsidy_budget creates a lot of blocks, so there should be enough for a few accounts
    // half-life of 10 minutes
    flat_map< string, vector<char> > props;
    props["account_subsidy_budget"] = fc::raw::pack_to_vector( int32_t( 5123 ) );
    props["account_subsidy_decay"] = fc::raw::pack_to_vector( uint32_t( 249617279 ) );
    set_witness_props( props );

    while( true )
    {
      const witness_schedule_object& wso = db->get_witness_schedule_object();
      if(    (wso.account_subsidy_rd.budget_per_time_unit == 5123)
          && (wso.account_subsidy_rd.decay_params.decay_per_time_unit == 249617279) )
        break;
      generate_block();
    }

    auto is_pool_in_equilibrium = []( int64_t pool, int32_t budget, const rd_decay_params& decay_params ) -> bool
    {
      int64_t decay = rd_compute_pool_decay( decay_params, pool, 1 );
      return (decay == budget);
    };

    const witness_schedule_object& wso = db->get_witness_schedule_object();
    BOOST_CHECK_EQUAL( wso.account_subsidy_rd.resource_unit, static_cast<uint64_t>(HIVE_ACCOUNT_SUBSIDY_PRECISION) );
    BOOST_CHECK_EQUAL( wso.account_subsidy_rd.budget_per_time_unit, 5123 );
    BOOST_CHECK(  is_pool_in_equilibrium( int64_t( wso.account_subsidy_rd.pool_eq )  , wso.account_subsidy_rd.budget_per_time_unit, wso.account_subsidy_rd.decay_params ) );
    BOOST_CHECK( !is_pool_in_equilibrium( int64_t( wso.account_subsidy_rd.pool_eq )-1, wso.account_subsidy_rd.budget_per_time_unit, wso.account_subsidy_rd.decay_params ) );

    int64_t pool = db->get_dynamic_global_properties().available_account_subsidies;

    while( true )
    {
      const dynamic_global_property_object& gpo = db->get_dynamic_global_properties();
      BOOST_CHECK_EQUAL( pool, gpo.available_account_subsidies );
      if( gpo.available_account_subsidies >= 100 * HIVE_ACCOUNT_SUBSIDY_PRECISION )
        break;
      generate_block();
      pool = pool + 5123 - ((249617279 * pool) >> HIVE_RD_DECAY_DENOM_SHIFT);
    }

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( account_subsidy_witness_limits )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: account_subsidy_witness_limits" );

    ACTORS( (alice) )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );

    claim_account_operation op;
    signed_transaction tx;

    BOOST_TEST_MESSAGE( "--- Test rejecting claim account when block is generated by a timeshare witness" );

    // set_subsidy_budget creates a lot of blocks, so there should be enough for a few accounts
    // half-life of 10 minutes
    flat_map< string, vector<char> > props;
    props["account_subsidy_budget"] = fc::raw::pack_to_vector( int32_t( 5000 ) );
    props["account_subsidy_decay"] = fc::raw::pack_to_vector( uint32_t( 249617279 ) );
    set_witness_props( props );

    while( true )
    {
      const dynamic_global_property_object& gpo = db->get_dynamic_global_properties();
      if( gpo.available_account_subsidies >= 100 * HIVE_ACCOUNT_SUBSIDY_PRECISION )
        break;
      generate_block();
    }

    // Verify the timeshare witness can't create subsidized accounts
    while( db->get< witness_object, by_name >( db->get_scheduled_witness( 1 ) ).schedule != witness_object::timeshare )
    {
      generate_block();
    }

    op.creator = "alice";
    op.fee = ASSET( "0.000 TESTS" );
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    sign( tx, alice_private_key );

    BOOST_CHECK( db->get_account( "alice" ).pending_claimed_accounts == 0 );
    BOOST_CHECK( db->_pending_tx.size() == 0 );

    // Pushes successfully
    db->push_transaction( tx, 0 );
    BOOST_CHECK( db->get_account( "alice" ).pending_claimed_accounts == 1 );
    BOOST_CHECK( db->_pending_tx.size() == 1 );

    do
    {
      generate_block();

      // The transaction fails in generate_block(), meaning it is removed from the local node's transaction list
      BOOST_CHECK_EQUAL( db->fetch_block_by_number( db->head_block_num() )->transactions.size(), 0u );
      BOOST_CHECK( db->get_account( "alice" ).pending_claimed_accounts == 0 );
      BOOST_CHECK_EQUAL( db->_pending_tx.size(), 0u );
    } while( db->get< witness_object, by_name >( db->get_scheduled_witness( 1 ) ).schedule == witness_object::timeshare );

    db->push_transaction( tx, 0 );
    BOOST_CHECK( db->get_account( "alice" ).pending_claimed_accounts == 1 );
    BOOST_CHECK( db->_pending_tx.size() == 1 );
    // But generate another block, as a non-time-share witness, and it works
    generate_block();
    BOOST_CHECK_EQUAL( db->fetch_block_by_number( db->head_block_num() )->transactions.size(), 1u );
    BOOST_CHECK( db->get_account( "alice" ).pending_claimed_accounts == 1 );
    BOOST_CHECK_EQUAL( db->_pending_tx.size(), 0u );

    while( db->get< witness_object, by_name >( db->get_scheduled_witness( 1 ) ).schedule == witness_object::timeshare )
    {
      generate_block();
    }

    fc::time_point_sec expiration = db->head_block_time() + fc::seconds(60);
    size_t n = size_t( db->get< witness_object, by_name >( db->get_scheduled_witness( 1 ) ).available_witness_account_subsidies / HIVE_ACCOUNT_SUBSIDY_PRECISION );

    ilog( "Creating ${np1} transactions", ("np1", n+1) );
    // Create n+1 transactions
    for( size_t i=0; i<=n; i++ )
    {
      tx.signatures.clear();
      tx.set_expiration( expiration );
      sign( tx, alice_private_key );
      db->push_transaction( tx, 0 );
      expiration += fc::seconds(3);
    }
    BOOST_CHECK( db->get_account( "alice" ).pending_claimed_accounts == n+2 );
    BOOST_CHECK_EQUAL( db->_pending_tx.size(), n+1 );
    generate_block();
    BOOST_CHECK_EQUAL( db->fetch_block_by_number( db->head_block_num() )->transactions.size(), n );
    BOOST_CHECK_EQUAL( db->_pending_tx.size(), 1u );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( recurrent_transfer_expiration )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: recurrent transfer expiration" );

    ACTORS( (alice)(bob) )
    generate_block();

    BOOST_REQUIRE( db->get_account( "alice" ).open_recurrent_transfers == 0 );

    fund( "alice", ASSET("100.000 TESTS") );
    fund( "alice", ASSET("100.000 TBD") );

    BOOST_REQUIRE( get_balance( "alice" ).amount.value == ASSET( "100.000 TESTS" ).amount.value );

    recurrent_transfer_operation op;
    op.from = "alice";
    op.to = "bob";
    op.memo = "test";
    op.amount = ASSET( "5.000 TESTS" );
    op.recurrence = 24;
    op.executions = 2;
    push_transaction(op, alice_private_key);

    BOOST_REQUIRE( get_balance( "alice" ).amount.value == ASSET( "100.000 TESTS" ).amount.value );
    BOOST_REQUIRE( get_balance( "bob" ).amount.value == ASSET( "0.000 TESTS" ).amount.value );
    BOOST_REQUIRE( db->get_account( "alice" ).open_recurrent_transfers == 1 );
    validate_database();

    generate_block();
    BOOST_TEST_MESSAGE( "--- test initial recurrent transfer execution" );
    BOOST_REQUIRE( get_balance( "alice" ).amount.value == ASSET( "95.000 TESTS" ).amount.value );
    BOOST_REQUIRE( get_balance( "bob" ).amount.value == ASSET( "5.000 TESTS" ).amount.value );
    validate_database();

    auto blocks_until_expiration = (fc::hours(op.recurrence * op.executions) + fc::seconds(30)).to_seconds() / HIVE_BLOCK_INTERVAL;
    ilog("generating ${blocks} blocks", ("blocks", blocks_until_expiration));
    generate_blocks( blocks_until_expiration );

    const auto* recurrent_transfer = db->find< recurrent_transfer_object, by_from_to_id >(boost::make_tuple( alice_id, bob_id) );
    BOOST_TEST_MESSAGE( "--- test recurrent transfer fully executed" );
    BOOST_REQUIRE( get_balance( "alice" ).amount.value == ASSET( "90.000 TESTS" ).amount.value );
    BOOST_REQUIRE( get_balance( "bob" ).amount.value == ASSET( "10.000 TESTS" ).amount.value );
    BOOST_REQUIRE( recurrent_transfer == nullptr );
    BOOST_REQUIRE( db->get_account( "alice" ).open_recurrent_transfers == 0 );

    BOOST_TEST_MESSAGE( "Waiting another 24 hours to see if the recurrent transfer does not execute again past its expiration" );

    auto blocks_until_next_execution = fc::days(1).to_seconds() / HIVE_BLOCK_INTERVAL;
    ilog("generating ${blocks} blocks", ("blocks", blocks_until_next_execution));
    generate_blocks( blocks_until_next_execution );

    const auto* recurrent_transfer_post_expiration = db->find< recurrent_transfer_object, by_from_to_id >(boost::make_tuple( alice_id, bob_id) );
    BOOST_REQUIRE( get_balance( "alice" ).amount.value == ASSET( "90.000 TESTS" ).amount.value );
    BOOST_REQUIRE( get_balance( "bob" ).amount.value == ASSET( "10.000 TESTS" ).amount.value );
    BOOST_REQUIRE( recurrent_transfer_post_expiration == nullptr );
    BOOST_REQUIRE( db->get_account( "alice" ).open_recurrent_transfers == 0 );

    validate_database();
 }
 FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( recurrent_transfer_consecutive_failure_deletion )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: recurrent transfer failure deletion" );

    ACTORS( (alice)(bob) )
    generate_block();

    BOOST_REQUIRE( db->get_account( "alice" ).open_recurrent_transfers == 0 );

    fund( "alice", ASSET("5.000 TESTS") );
    fund( "alice", ASSET("100.000 TBD") );

    BOOST_REQUIRE( get_balance( "alice" ).amount.value == ASSET( "5.000 TESTS" ).amount.value );

    recurrent_transfer_operation op;
    op.from = "alice";
    op.to = "bob";
    op.memo = "test";
    op.amount = ASSET( "5.000 TESTS" );
    op.recurrence = 24;
    op.executions = 100;
    push_transaction(op, alice_private_key);

    BOOST_REQUIRE( get_balance( "alice" ).amount.value == ASSET( "5.000 TESTS" ).amount.value );
    BOOST_REQUIRE( get_balance( "bob" ).amount.value == ASSET( "0.000 TESTS" ).amount.value );
    BOOST_REQUIRE( db->get_account( "alice" ).open_recurrent_transfers == 1 );
    validate_database();

    generate_block();
    BOOST_TEST_MESSAGE( "--- test initial recurrent transfer execution" );
    BOOST_REQUIRE( get_balance( "alice" ).amount.value == ASSET( "0.000 TESTS" ).amount.value );
    BOOST_REQUIRE( get_balance( "bob" ).amount.value == ASSET( "5.000 TESTS" ).amount.value );
    validate_database();

    auto blocks_until_failure = (fc::days(10) + fc::seconds(60) ).to_seconds() / HIVE_BLOCK_INTERVAL;
    ilog("generating ${blocks} blocks", ("blocks", blocks_until_failure));
    generate_blocks( blocks_until_failure );

    const auto* recurrent_transfer = db->find< recurrent_transfer_object, by_from_to_id >(boost::make_tuple( alice_id, bob_id) );
    BOOST_TEST_MESSAGE( "--- test recurrent transfer got deleted" );
    BOOST_REQUIRE( get_balance( "alice" ).amount.value == ASSET( "0.000 TESTS" ).amount.value );
    BOOST_REQUIRE( get_balance( "bob" ).amount.value == ASSET( "5.000 TESTS" ).amount.value );
    BOOST_REQUIRE( recurrent_transfer == nullptr );
    BOOST_REQUIRE( db->get_account( "alice" ).open_recurrent_transfers == 0 );

    BOOST_TEST_MESSAGE( "Waiting another 24 hours to see if the recurrent transfer does not execute again past its deletion" );

    fund( "alice", ASSET("5.000 TESTS") );
    auto blocks_until_next_execution = fc::days(1).to_seconds() / HIVE_BLOCK_INTERVAL;
    ilog("generating ${blocks} blocks", ("blocks", blocks_until_next_execution));
    generate_blocks( blocks_until_next_execution );

    const auto* recurrent_transfer_post_deletion = db->find< recurrent_transfer_object, by_from_to_id >(boost::make_tuple( alice_id, bob_id) );
    BOOST_REQUIRE( get_balance( "alice" ).amount.value == ASSET( "5.000 TESTS" ).amount.value );
    BOOST_REQUIRE( get_balance( "bob" ).amount.value == ASSET( "5.000 TESTS" ).amount.value );
    BOOST_REQUIRE( recurrent_transfer_post_deletion == nullptr );
    BOOST_REQUIRE( db->get_account( "alice" ).open_recurrent_transfers == 0 );
    validate_database();
 }
 FC_LOG_AND_RETHROW()
}


BOOST_AUTO_TEST_SUITE_END()
#endif
