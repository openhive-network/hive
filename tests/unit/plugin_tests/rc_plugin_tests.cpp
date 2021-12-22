#ifdef IS_TEST_NET
#include <boost/test/unit_test.hpp>

#include <hive/plugins/rc/rc_objects.hpp>

#include "../db_fixture/database_fixture.hpp"

using namespace hive::chain;
using namespace hive::protocol;
using namespace hive::plugins;
using namespace hive::plugins::rc;

BOOST_FIXTURE_TEST_SUITE( rc_plugin_tests, genesis_database_fixture )

BOOST_AUTO_TEST_CASE( account_creation )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing existence of rc_accounts after regular account creation" );

    generate_block();

    PREP_ACTOR( steem )
    //ABW: there was this idea to create "steem" account in genesis block but with fake data (so it has
    //the same as today creation time etc.) so it is available when it is used as recovery account;
    //this test guards creation of rc_account_object for that account in case we actually implement
    //such "hack"
    {
      pow_operation op;
      op.worker_account = "steem";
      op.block_id = db->head_block_id();
      op.work.worker = steem_public_key;
      //note: we cannot compute nonce once and hardcode it in the test because HIVE_BLOCKCHAIN_VERSION
      //becomes part of first block as extension, which means when version changes (because we have new
      //hardfork coming or just because of HIVE_ENABLE_SMT) the nonces will have to change as well
      op.nonce = -1;
      do
      {
        ++op.nonce;
        op.work.create( steem_private_key, op.work_input() );
      }
      while( op.work.work >= db->get_pow_target() );
      //default props

      //pow_operation does not need signature - signing it anyway leads to superfluous signature error
      //on the other hand current RC mechanism needs to decide who pays for the transaction and when there is
      //no signature no one is paying which is also an error; therefore we need some other operation that
      //actually needs a signature; moreover we cannot sign transaction with "steem" account because signature
      //is verified before operations are executed, so there is no "steem" authority object yet
      comment_operation comment;
      comment.author = "initminer"; //cannot be "steem"
      comment.permlink = "test";
      comment.body = "Hello world!";

      signed_transaction tx;
      tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
      tx.operations.push_back( op );
      tx.operations.push_back( comment );
      sign( tx, init_account_priv_key ); //cannot be steem_private_key
      db->push_transaction( tx, 0 );
    }
    generate_block();

    auto* rc_steem = db->find< rc_account_object, by_name >( "steem" );
    BOOST_REQUIRE( rc_steem != nullptr );

    //now the same with "normal" account
    PREP_ACTOR( alice )
    {
      pow_operation op;
      op.worker_account = "alice";
      op.block_id = db->head_block_id();
      op.work.worker = alice_public_key;
      op.nonce = -1;
      do
      {
        ++op.nonce;
        op.work.create( alice_private_key, op.work_input() );
      }
      while( op.work.work >= db->get_pow_target() );
      //default props

      //same story as with "steem" above
      comment_operation comment;
      comment.author = "initminer"; //cannot be "alice"
      comment.permlink = "test";
      comment.body = "Hello world!!";

      signed_transaction tx;
      tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
      tx.operations.push_back( op );
      tx.operations.push_back( comment );
      sign( tx, init_account_priv_key ); //cannot be alice_private_key
      db->push_transaction( tx, 0 );
    }
    generate_block();
    
    auto* rc_alice = db->find< rc_account_object, by_name >( "alice" );
    BOOST_REQUIRE( rc_alice != nullptr );

    inject_hardfork( 13 );

    PREP_ACTOR( bob )
    {
      pow2_operation op;
      pow2 pow;
      int nonce = -1;
      do
      {
        ++nonce;
        pow.create( db->head_block_id(), "bob", nonce );
      }
      while( pow.pow_summary >= db->get_pow_summary_target() );
      op.work = pow;
      op.new_owner_key = bob_public_key;
      //default props

      //and once again the same as above with "steem" and "alice", however this time signature is not superfluous;
      //when we set new_owner_key (which we have to for new account) pow2_operation declares needed authority as "other",
      //which prevents us from mixing it with comment_operation which requires posting key - using transfer instead;
      //why can't we just have only bob key then? because it is not effective yet (no such authority) and also because
      //while bob signature is required, it does not count when RC chooses who to charge for transaction
      transfer_operation transfer;
      transfer.from = "initminer";
      transfer.to = "bob";
      transfer.amount = ASSET( "0.001 TESTS ");
      transfer.memo = "test";

      signed_transaction tx;
      tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
      tx.operations.push_back( op );
      tx.operations.push_back( transfer );
      sign( tx, init_account_priv_key );
      sign( tx, bob_private_key ); //still needed as "other"
      db->push_transaction( tx, 0 );
    }
    generate_block();

    auto* rc_bob = db->find< rc_account_object, by_name >( "bob" );
    BOOST_REQUIRE( rc_bob != nullptr );

    inject_hardfork( 17 );

    PREP_ACTOR( sam )
    {
      vest( HIVE_INIT_MINER_NAME, HIVE_INIT_MINER_NAME, ASSET( "1000.000 TESTS" ) );

      account_create_with_delegation_operation op;
      op.fee = db->get_witness_schedule_object().median_props.account_creation_fee;
      op.delegation = ASSET( "100000000.000000 VESTS" );
      op.creator = HIVE_INIT_MINER_NAME;
      op.new_account_name = "sam";
      op.owner = authority( 1, sam_public_key, 1 );
      op.active = authority( 1, sam_public_key, 1 );
      op.posting = authority( 1, sam_post_key.get_public_key(), 1 );
      op.memo_key = sam_post_key.get_public_key();
      op.json_metadata = "";

      push_transaction( op, init_account_priv_key );
    }
    generate_block();

    auto* rc_sam = db->find< rc_account_object, by_name >( "sam" );
    BOOST_REQUIRE( rc_sam != nullptr );

    inject_hardfork( 20 );

    PREP_ACTOR( dave )
    {
      account_create_operation op;
      op.fee = db->get_witness_schedule_object().median_props.account_creation_fee;
      op.creator = HIVE_INIT_MINER_NAME;
      op.new_account_name = "dave";
      op.owner = authority( 1, dave_public_key, 1 );
      op.active = authority( 1, dave_public_key, 1 );
      op.posting = authority( 1, dave_post_key.get_public_key(), 1 );
      op.memo_key = dave_post_key.get_public_key();
      op.json_metadata = "";

      push_transaction( op, init_account_priv_key );
    }
    generate_block();

    auto* rc_dave = db->find< rc_account_object, by_name >( "dave" );
    BOOST_REQUIRE( rc_dave != nullptr );

    inject_hardfork( HIVE_BLOCKCHAIN_VERSION.minor_v() );
    
    PREP_ACTOR( greg )
    {
      db_plugin->debug_update( [=]( database& db )
      {
        db.modify( db.get_account( HIVE_INIT_MINER_NAME ), [&]( account_object& a )
        {
          a.pending_claimed_accounts += 1;
        } );
      } );

      create_claimed_account_operation op;
      op.creator = HIVE_INIT_MINER_NAME;
      op.new_account_name = "greg";
      op.owner = authority( 1, greg_public_key, 1 );
      op.active = authority( 1, greg_public_key, 1 );
      op.posting = authority( 1, greg_post_key.get_public_key(), 1 );
      op.memo_key = greg_post_key.get_public_key();
      op.json_metadata = "";
      
      push_transaction( op, init_account_priv_key );
    }
    generate_block();

    auto* rc_greg = db->find< rc_account_object, by_name >( "greg" );
    BOOST_REQUIRE( rc_greg != nullptr );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( rc_usage_buckets )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing rc usage bucket tracking" );

    fc::int_array< std::string, HIVE_RC_NUM_RESOURCE_TYPES > rc_names;
    for( int i = 0; i < HIVE_RC_NUM_RESOURCE_TYPES; ++i )
      rc_names[i] = fc::reflector< rc_resource_types >::to_string(i);

    const auto& pools = db->get< rc_pool_object, by_id >( rc_pool_id_type() );
    const auto& bucketIdx = db->get_index< rc_usage_bucket_index >().indices().get< by_timestamp >();

    static_assert( HIVE_RC_NUM_RESOURCE_TYPES == 5, "If it fails update logging code below accordingly" );

    auto get_active_bucket = [&]() -> const rc_usage_bucket_object&
    {
      return *( bucketIdx.rbegin() );
    };
    auto check_eq = []( const resource_count_type& rc1, const resource_count_type& rc2 )
    {
      for( int i = 0; i < HIVE_RC_NUM_RESOURCE_TYPES; ++i )
        BOOST_CHECK_EQUAL( rc1[i], rc2[i] );
    };
    auto print = [&]( bool nonempty_buckets = false ) -> int //index of first nonempty bucket or -1 when only active
    {
      ilog( "Global usage ${b} @${t}: [${u0},${u1},${u2},${u3},${u4}]", ( "b", db->head_block_num() )( "t", db->head_block_time() )
        ( "u0", pools.get_usage(0) )( "u1", pools.get_usage(1) )( "u2", pools.get_usage(2) )
        ( "u3", pools.get_usage(3) )( "u4", pools.get_usage(4) )
      );
      ilog( "Resource weights: [${w0},${w1},${w2},${w3},${w4}] / ${sw}", ( "sw", pools.get_weight_divisor() )
        ( "w0", pools.get_weight(0) )( "w1", pools.get_weight(1) )( "w2", pools.get_weight(2) )
        ( "w3", pools.get_weight(3) )( "w4", pools.get_weight(4) )
      );
      ilog( "Relative resource weights: [${w0}bp,${w1}bp,${w2}bp,${w3}bp,${w4}bp]",
        ( "w0", pools.count_share(0) )( "w1", pools.count_share(1) )( "w2", pools.count_share(2) )
        ( "w3", pools.count_share(3) )( "w4", pools.count_share(4) )
      );
      int result = -1;
      if( nonempty_buckets )
      {
        ilog( "Nonempty buckets:" );
        int i = 0;
        for( const auto& bucket : bucketIdx )
        {
          if( bucket.get_usage(0) || bucket.get_usage(1) || bucket.get_usage(2) ||
              bucket.get_usage(3) || bucket.get_usage(4) )
          {
            if( result < 0 )
              result = i;
            ilog( "${i} @${t}: [${u0},${u1},${u2},${u3},${u4}]", ( "i", i )( "t", bucket.get_timestamp() )
              ( "u0", bucket.get_usage(0) )( "u1", bucket.get_usage(1) )( "u2", bucket.get_usage(2) )
              ( "u3", bucket.get_usage(3) )( "u4", bucket.get_usage(4) )
            );
          }
          ++i;
        }
      }
      else
      {
        const auto& bucket = *bucketIdx.rbegin();
        ilog( "Active bucket @${t}: [${u0},${u1},${u2},${u3},${u4}]", ( "t", bucket.get_timestamp() )
          ( "u0", bucket.get_usage(0) )( "u1", bucket.get_usage(1) )( "u2", bucket.get_usage(2) )
          ( "u3", bucket.get_usage(3) )( "u4", bucket.get_usage(4) )
        );
      }
      return result;
    };

    BOOST_TEST_MESSAGE( "All buckets empty" );

    BOOST_CHECK_EQUAL( print( true ), -1 );
    for( const auto& bucket : bucketIdx )
      check_eq( bucket.get_usage(), {} );

    inject_hardfork( HIVE_BLOCKCHAIN_VERSION.minor_v() );

    BOOST_TEST_MESSAGE( "Some resources consumed" );

    BOOST_CHECK_EQUAL( print( true ), HIVE_RC_WINDOW_BUCKET_COUNT - 1 );
    check_eq( get_active_bucket().get_usage(), pools.get_usage() );

    ACTORS( (alice)(bob)(sam) )
    fund( "alice", 1000 );
    fund( "bob", 1000 );
    fund( "sam", 1000 );
    generate_block(); //more resources consumed
    generate_blocks( get_active_bucket().get_timestamp() + fc::seconds( HIVE_RC_BUCKET_TIME_LENGTH - HIVE_BLOCK_INTERVAL ) );

    BOOST_TEST_MESSAGE( "First bucket is about to switch" );

    BOOST_CHECK_EQUAL( print( true ), HIVE_RC_WINDOW_BUCKET_COUNT - 1 );
    check_eq( get_active_bucket().get_usage(), pools.get_usage() );
    generate_block(); //switch bucket
    BOOST_CHECK_EQUAL( print( true ), HIVE_RC_WINDOW_BUCKET_COUNT - 2 );
    check_eq( get_active_bucket().get_usage(), {} );

    auto circular_transfer = [&]( int amount )
    {
      for( int i = 0; i < amount; ++i )
      {
        resource_count_type usage = pools.get_usage();
        transfer( "alice", "bob", ASSET( "0.100 TESTS" ) );
        transfer( "bob", "sam", ASSET( "0.100 TESTS" ) );
        transfer( "sam", "alice", ASSET( "0.100 TESTS" ) );
        generate_block();
        print(); //transfers use market and execution, but transaction itself uses history and state
        BOOST_CHECK_LT( usage[ resource_history_bytes ], pools.get_usage( resource_history_bytes ) );
        BOOST_CHECK_EQUAL( usage[ resource_new_accounts ], pools.get_usage( resource_new_accounts ) );
        BOOST_CHECK_LT( usage[ resource_market_bytes ], pools.get_usage( resource_market_bytes ) );
        BOOST_CHECK_LT( usage[ resource_state_bytes ], pools.get_usage( resource_state_bytes ) );
        BOOST_CHECK_LT( usage[ resource_execution_time ], pools.get_usage( resource_execution_time ) );
      }
    };
    BOOST_TEST_MESSAGE( "Spamming transfers" );
    int market_bytes_share = pools.count_share( resource_market_bytes );
    circular_transfer( ( HIVE_RC_BUCKET_TIME_LENGTH / HIVE_BLOCK_INTERVAL ) - 1 );
    int new_market_bytes_share = pools.count_share( resource_market_bytes );
    BOOST_CHECK_LT( market_bytes_share, new_market_bytes_share ); //share of market bytes in RC inflation should've increased

    BOOST_CHECK_EQUAL( print( true ), HIVE_RC_WINDOW_BUCKET_COUNT - 2 );
    generate_block(); //switch bucket
    BOOST_CHECK_EQUAL( print( true ), HIVE_RC_WINDOW_BUCKET_COUNT - 3 );
    check_eq( get_active_bucket().get_usage(), {} );

    BOOST_TEST_MESSAGE( "More transfer spam within new bucket subwindow" );
    market_bytes_share = new_market_bytes_share;
    circular_transfer( 100 ); //do some more to show switching bucket did not influence resource weights in any way
    new_market_bytes_share = pools.count_share( resource_market_bytes );
    BOOST_CHECK_LT( market_bytes_share, new_market_bytes_share ); //share of market bytes in RC inflation should've increased again

    custom_operation custom;
    custom.id = 1;
    custom.data.resize( HIVE_CUSTOM_OP_DATA_MAX_LENGTH );
    auto custom_spam = [&]( int amount )
    {
      for( int i = 0; i < amount; ++i )
      {
        resource_count_type usage = pools.get_usage();
        custom.required_auths = { "alice" };
        push_transaction( custom, alice_private_key );
        custom.required_auths = { "bob" };
        push_transaction( custom, bob_private_key );
        custom.required_auths = { "sam" };
        push_transaction( custom, sam_private_key );
        generate_block();
        print(); //custom_op uses only execution, but transaction itself uses history (a lot in this case) and state
        BOOST_CHECK_LT( usage[ resource_history_bytes ], pools.get_usage( resource_history_bytes ) );
        BOOST_CHECK_EQUAL( usage[ resource_new_accounts ], pools.get_usage( resource_new_accounts ) );
        BOOST_CHECK_EQUAL( usage[ resource_market_bytes ], pools.get_usage( resource_market_bytes ) );
        BOOST_CHECK_LT( usage[ resource_state_bytes ], pools.get_usage( resource_state_bytes ) );
        BOOST_CHECK_LT( usage[ resource_execution_time ], pools.get_usage( resource_execution_time ) );
      }
    };
    BOOST_TEST_MESSAGE( "Lengthy custom op spam within the same bucket subwindow as previous transfers" );
    market_bytes_share = new_market_bytes_share;
    int history_bytes_share = pools.count_share( resource_history_bytes );
    custom_spam( ( HIVE_RC_BUCKET_TIME_LENGTH / HIVE_BLOCK_INTERVAL ) - 101 );
    new_market_bytes_share = pools.count_share( resource_market_bytes );
    int new_history_bytes_share = pools.count_share( resource_history_bytes );
    BOOST_CHECK_GT( market_bytes_share, new_market_bytes_share ); //share of market bytes in RC inflation must fall...
    BOOST_CHECK_LT( history_bytes_share, new_history_bytes_share ); //...while share of history bytes increases

    BOOST_CHECK_EQUAL( print( true ), HIVE_RC_WINDOW_BUCKET_COUNT - 3 );
    generate_block(); //switch bucket
    BOOST_CHECK_EQUAL( print( true ), HIVE_RC_WINDOW_BUCKET_COUNT - 4 );
    check_eq( get_active_bucket().get_usage(), {} );

    BOOST_TEST_MESSAGE( "Bucket switches with small amount of custom ops in each to have all buckets nonempty" );
    for( int i = 5; i <= HIVE_RC_WINDOW_BUCKET_COUNT; ++i )
    {
      custom_spam( 1 );
      generate_blocks( get_active_bucket().get_timestamp() + fc::seconds( HIVE_RC_BUCKET_TIME_LENGTH - HIVE_BLOCK_INTERVAL ) );
      generate_block(); //switch bucket
      BOOST_CHECK_EQUAL( print( true ), HIVE_RC_WINDOW_BUCKET_COUNT - i );
      check_eq( get_active_bucket().get_usage(), {} );
    }
    BOOST_TEST_MESSAGE( "Some more custom op spam" );
    history_bytes_share = pools.count_share( resource_history_bytes );
    custom_spam( 200 );
    new_history_bytes_share = pools.count_share( resource_history_bytes );
    BOOST_CHECK_LT( history_bytes_share, new_history_bytes_share ); //despite being pretty high previously, share of history bytes should still increase

    custom_json_operation custom_json;
    custom_json.json = "{}";
    auto custom_json_spam = [&]( int amount )
    {
      for( int i = 0; i < amount; ++i )
      {
        resource_count_type usage = pools.get_usage();
        custom_json.id = i&1 ? "follow" : "reblog";
        custom_json.required_auths = {};
        custom_json.required_posting_auths = { "alice" };
        push_transaction( custom_json, alice_post_key );
        custom_json.id = i&1 ? "notify" : "community";
        custom_json.required_posting_auths = { "bob" };
        push_transaction( custom_json, bob_post_key );
        custom_json.id = i&1 ? "splinterlands" : "dex";
        custom_json.required_auths = { "sam" };
        custom_json.required_posting_auths = {};
        push_transaction( custom_json, sam_private_key );
        generate_block();
        print(); //custom_json uses only execution, transaction itself uses history (not much in this case) and state
        BOOST_CHECK_LT( usage[ resource_history_bytes ], pools.get_usage( resource_history_bytes ) );
        BOOST_CHECK_EQUAL( usage[ resource_new_accounts ], pools.get_usage( resource_new_accounts ) );
        BOOST_CHECK_EQUAL( usage[ resource_market_bytes ], pools.get_usage( resource_market_bytes ) );
        BOOST_CHECK_LT( usage[ resource_state_bytes ], pools.get_usage( resource_state_bytes ) );
        BOOST_CHECK_LT( usage[ resource_execution_time ], pools.get_usage( resource_execution_time ) );
      }
    };
    BOOST_TEST_MESSAGE( "Custom json spam (including HM related) within the same bucket subwindow as previous custom ops" );
    history_bytes_share = new_history_bytes_share;
    custom_json_spam( ( HIVE_RC_BUCKET_TIME_LENGTH / HIVE_BLOCK_INTERVAL ) - 201 );
    new_history_bytes_share = pools.count_share( resource_history_bytes );
    BOOST_CHECK_GT( history_bytes_share, new_history_bytes_share ); //share of history bytes in RC inflation must fall

    BOOST_TEST_MESSAGE( "First nonempty bucket is about to expire" );
    auto bucket_usage = bucketIdx.begin()->get_usage();
    auto global_usage = pools.get_usage();

    BOOST_CHECK_EQUAL( print( true ), 0 );
    generate_block(); //switch bucket (nonempty bucket 0 expired)
    BOOST_CHECK_EQUAL( print( true ), 0 );
    check_eq( get_active_bucket().get_usage(), {} );

    BOOST_TEST_MESSAGE( "Checking if bucket was expired correctly" );
    //content of first bucket (which was nonempty for the first time in this test) should be
    //subtracted from global usage
    for( int i = 0; i < HIVE_RC_NUM_RESOURCE_TYPES; ++i )
      global_usage[i] -= bucket_usage[i];
    check_eq( global_usage, pools.get_usage() );
    
    generate_blocks( get_active_bucket().get_timestamp() + fc::seconds( HIVE_RC_BUCKET_TIME_LENGTH - HIVE_BLOCK_INTERVAL ) );

    market_bytes_share = pools.count_share( resource_market_bytes );
    BOOST_CHECK_EQUAL( print( true ), 0 );
    generate_block(); //switch bucket (another nonempty bucket 0 expired)
    BOOST_CHECK_EQUAL( print( true ), 0 );
    check_eq( get_active_bucket().get_usage(), {} );

    BOOST_TEST_MESSAGE( "Checking how bucket expiration can influence resource shares" );
    new_market_bytes_share = pools.count_share( resource_market_bytes );
    //most transfers using market bytes were produced within second bucket subwindow
    //but some were done in next bucket, so we should still be above zero
    BOOST_CHECK_GT( market_bytes_share, new_market_bytes_share );
    BOOST_CHECK_GT( new_market_bytes_share, 0 );

    generate_blocks( get_active_bucket().get_timestamp() + fc::seconds( HIVE_RC_BUCKET_TIME_LENGTH - HIVE_BLOCK_INTERVAL ) );

    BOOST_TEST_MESSAGE( "Checking bucket expiration while there is pending operation" );
    market_bytes_share = new_market_bytes_share;
    BOOST_CHECK_EQUAL( pools.get_usage( resource_new_accounts ), 0 );

    claim_account_operation claim_account;
    claim_account.creator = "alice";
    claim_account.fee = ASSET( "0.000 TESTS" );
    push_transaction( claim_account, alice_private_key );
    generate_block(); //switch bucket
    BOOST_CHECK_EQUAL( print( true ), 0 );
    //last transaction should be registered within new bucket - it uses new account and execution, tx itself also state and history
    bucket_usage = get_active_bucket().get_usage();
    BOOST_CHECK_GT( bucket_usage[ resource_history_bytes ], 0 );
    BOOST_CHECK_GT( bucket_usage[ resource_new_accounts ], 0 );
    BOOST_CHECK_EQUAL( bucket_usage[ resource_market_bytes ], 0 );
    BOOST_CHECK_GT( bucket_usage[ resource_state_bytes ], 0 );
    BOOST_CHECK_GT( bucket_usage[ resource_execution_time ], 0 );

    //all remaining market bytes were used within expired bucket subwindow
    BOOST_CHECK_EQUAL( pools.get_usage( resource_market_bytes ), 0 );
    new_market_bytes_share = pools.count_share( resource_market_bytes );
    BOOST_CHECK_EQUAL( new_market_bytes_share, 0 );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()
#endif
