#ifdef IS_TEST_NET
#include <boost/test/unit_test.hpp>

#include <hive/plugins/rc/rc_objects.hpp>
#include <hive/plugins/rc/rc_operations.hpp>
#include <hive/chain/database_exceptions.hpp>

#include "../db_fixture/database_fixture.hpp"

#include <fc/log/appender.hpp>

#include <chrono>

using namespace hive::chain;
using namespace hive::protocol;
using namespace hive::plugins;
using namespace hive::plugins::rc;

int64_t regenerate_mana( debug_node::debug_node_plugin* db_plugin, const rc_account_object& acc )
{
  db_plugin->debug_update( [&]( database& db )
  {
    db.modify( acc, [&]( rc_account_object& rc_account )
    {
      auto max_rc = get_maximum_rc( db.get_account( rc_account.account ), rc_account );
      hive::chain::util::manabar_params manabar_params( max_rc, HIVE_RC_REGEN_TIME );
      rc_account.rc_manabar.regenerate_mana( manabar_params, db.head_block_time() );
    } );
  } );
  return acc.rc_manabar.current_mana;
}

void clear_mana( debug_node::debug_node_plugin* db_plugin, const rc_account_object& acc )
{
  db_plugin->debug_update( [&]( database& db )
  {
    db.modify( acc, [&]( rc_account_object& rc_account )
    {
      rc_account.rc_manabar.current_mana = 0;
      rc_account.rc_manabar.last_update_time = db.head_block_time().sec_since_epoch();
    } );
  } );
}

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
      push_transaction( tx, init_account_priv_key ); //cannot be steem_private_key
    }
    generate_block();

    auto* rc_steem = db->find< rc_account_object, by_name >( "steem" );
    BOOST_REQUIRE( rc_steem != nullptr );

    //now the same with "normal" account
    PREP_ACTOR( alice )
    create_with_pow( "alice", alice_public_key, alice_private_key );
    generate_block();
    
    auto* rc_alice = db->find< rc_account_object, by_name >( "alice" );
    BOOST_REQUIRE( rc_alice != nullptr );

    inject_hardfork( 13 );

    PREP_ACTOR( bob )
    create_with_pow2( "bob", bob_public_key, bob_private_key );
    generate_block();

    auto* rc_bob = db->find< rc_account_object, by_name >( "bob" );
    BOOST_REQUIRE( rc_bob != nullptr );

    inject_hardfork( 17 );

    PREP_ACTOR( sam )
    vest( HIVE_INIT_MINER_NAME, HIVE_INIT_MINER_NAME, ASSET( "1000.000 TESTS" ) );
    create_with_delegation( HIVE_INIT_MINER_NAME, "sam", sam_public_key, sam_post_key, ASSET( "100000000.000000 VESTS" ), init_account_priv_key );
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

      create_claimed_account( HIVE_INIT_MINER_NAME, "greg", greg_public_key, greg_post_key.get_public_key(), "", init_account_priv_key );
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

    claim_account( "alice", ASSET( "0.000 TESTS" ), alice_private_key );
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

BOOST_AUTO_TEST_CASE( rc_single_recover_account )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing rc resource cost of single recover_account_operation" );

    inject_hardfork( HIVE_BLOCKCHAIN_VERSION.minor_v() );
    auto skip_flags = rc_plugin->get_rc_plugin_skip_flags();
    skip_flags.skip_reject_not_enough_rc = 0;
    skip_flags.skip_reject_unknown_delta_vests = 0;
    rc_plugin->set_rc_plugin_skip_flags( skip_flags );

    ACTORS( (agent)(victim)(thief) )
    generate_block(); 
    vest( "initminer", "agent", ASSET( "1000.000 TESTS" ) );
    fund( "victim", ASSET( "1.000 TESTS" ) );

    const auto& agent_rc = db->get< rc_account_object, by_name >( "agent" );
    const auto& victim_rc = db->get< rc_account_object, by_name >( "victim" );
    const auto& thief_rc = db->get< rc_account_object, by_name >( "thief" );

    BOOST_TEST_MESSAGE( "agent becomes recovery account for victim" );
    change_recovery_account( "victim", "agent", victim_private_key );
    generate_blocks( db->head_block_time() + HIVE_OWNER_AUTH_RECOVERY_PERIOD );

    generate_block();

    BOOST_TEST_MESSAGE( "thief steals private key of victim and sets authority to himself" );
    account_update_operation account_update;
    account_update.account = "victim";
    account_update.owner = authority( 1, "thief", 1 );
    push_transaction( account_update, victim_private_key );
    generate_blocks( db->head_block_time() + ( HIVE_OWNER_AUTH_RECOVERY_PERIOD.to_seconds() / 2 ) );

    BOOST_TEST_MESSAGE( "victim notices a problem and asks agent for recovery" );
    auto victim_new_private_key = generate_private_key( "victim2" );
    request_account_recovery( "agent", "victim", authority( 1, victim_new_private_key.get_public_key(), 1 ), agent_private_key );
    generate_block();

    BOOST_TEST_MESSAGE( "thief keeps RC of victim at zero - recovery still possible" );
    auto pre_tx_agent_mana = regenerate_mana( db_plugin, agent_rc );
    clear_mana( db_plugin, victim_rc );
    auto pre_tx_thief_mana = regenerate_mana( db_plugin, thief_rc );
    signed_transaction tx;
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    recover_account_operation recover;
    recover.account_to_recover = "victim";
    recover.new_owner_authority = authority( 1, victim_new_private_key.get_public_key(), 1 );
    recover.recent_owner_authority = authority( 1, victim_private_key.get_public_key(), 1 );
    tx.operations.push_back( recover );
    push_transaction( tx, {victim_private_key, victim_new_private_key} );
    tx.clear();
    //RC cost covered by the network - no RC spent on any account (it would fail if victim was charged like it used to be)
    BOOST_REQUIRE_EQUAL( pre_tx_agent_mana, agent_rc.rc_manabar.current_mana );
    BOOST_REQUIRE_EQUAL( 0, victim_rc.rc_manabar.current_mana );
    BOOST_REQUIRE_EQUAL( pre_tx_thief_mana, thief_rc.rc_manabar.current_mana );
    generate_blocks( db->head_block_time() + HIVE_OWNER_AUTH_RECOVERY_PERIOD );

    BOOST_TEST_MESSAGE( "victim wants to try to rob rc from recovery agent or network" );
    //this part tests against former and similar solutions that would be exploitable
    //first change authority to contain agent but without actual control (weight below threshold) and finalize it
    account_update.account = "victim";
    account_update.owner = authority( 3, "agent", 1, victim_new_private_key.get_public_key(), 3 );
    push_transaction( account_update, victim_new_private_key );
    generate_blocks( db->head_block_time() + HIVE_OWNER_AUTH_RECOVERY_PERIOD );
    //"steal" account also including agent as above
    auto victim_testA_private_key = generate_private_key( "victimA" );
    account_update.owner = authority( 3, "agent", 1, victim_testA_private_key.get_public_key(), 3 );
    push_transaction( account_update, victim_new_private_key );
    generate_blocks( db->head_block_time() + ( HIVE_OWNER_AUTH_RECOVERY_PERIOD.to_seconds() / 2 ) );
    //ask agent for help to recover "stolen" account - again add agent
    auto victim_testB_private_key = generate_private_key( "victimB" );
    request_account_recovery( "agent", "victim", authority( 3, "agent", 1, victim_testB_private_key.get_public_key(), 3 ), agent_private_key );
    generate_block();
    //finally recover adding expensive operation as extra - test 1: before actual recovery
    pre_tx_agent_mana = regenerate_mana( db_plugin, agent_rc );
    auto pre_tx_victim_mana = regenerate_mana( db_plugin, victim_rc );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    claim_account_operation expensive;
    expensive.creator = "victim";
    expensive.fee = ASSET( "0.000 TESTS" );
    recover.account_to_recover = "victim";
    recover.new_owner_authority = authority( 3, "agent", 1, victim_testB_private_key.get_public_key(), 3 );
    recover.recent_owner_authority = authority( 3, "agent", 1, victim_new_private_key.get_public_key(), 3 );
    tx.operations.push_back( expensive );
    tx.operations.push_back( recover );
    HIVE_REQUIRE_EXCEPTION( push_transaction( tx, {victim_new_private_key, victim_testA_private_key/*that key is needed for claim account*/, victim_testB_private_key} ), "has_mana", plugin_exception );
    tx.clear();
    BOOST_REQUIRE_EQUAL( pre_tx_agent_mana, agent_rc.rc_manabar.current_mana );
    BOOST_REQUIRE_EQUAL( pre_tx_victim_mana, victim_rc.rc_manabar.current_mana );
    //test 2: after recovery
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.operations.push_back( recover );
    tx.operations.push_back( expensive );
    HIVE_REQUIRE_EXCEPTION( push_transaction( tx, {victim_new_private_key, victim_testA_private_key, victim_testB_private_key} ), "has_mana", plugin_exception );
    tx.clear();
    BOOST_REQUIRE_EQUAL( pre_tx_agent_mana, agent_rc.rc_manabar.current_mana );
    BOOST_REQUIRE_EQUAL( pre_tx_victim_mana, victim_rc.rc_manabar.current_mana );
    //test 3: add something less expensive so the tx will go through
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    transfer_operation cheap;
    cheap.from = "victim";
    cheap.to = "agent";
    cheap.amount = ASSET( "0.001 TESTS" );
    cheap.memo = "Thanks for help!";
    tx.operations.push_back( recover );
    tx.operations.push_back( cheap );
    push_transaction( tx, {victim_new_private_key, victim_testA_private_key, victim_testB_private_key} );
    tx.clear();
    //RC consumed from victim - recovery is not free if mixed with other operations
    BOOST_REQUIRE_EQUAL( pre_tx_agent_mana, agent_rc.rc_manabar.current_mana );
    BOOST_REQUIRE_GT( pre_tx_victim_mana, victim_rc.rc_manabar.current_mana );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( rc_many_recover_accounts )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing rc resource cost of many recover_account_operations" );

    inject_hardfork( HIVE_BLOCKCHAIN_VERSION.minor_v() );
    auto skip_flags = rc_plugin->get_rc_plugin_skip_flags();
    skip_flags.skip_reject_not_enough_rc = 0;
    skip_flags.skip_reject_unknown_delta_vests = 0;
    rc_plugin->set_rc_plugin_skip_flags( skip_flags );

    ACTORS( (agent)(victim1)(victim2)(victim3)(thief1)(thief2)(thief3) )
    generate_block();
    vest( "initminer", "agent", ASSET( "1000.000 TESTS" ) );
    fund( "victim1", ASSET( "1.000 TESTS" ) );

    const auto& agent_rc = db->get< rc_account_object, by_name >( "agent" );
    const auto& victim1_rc = db->get< rc_account_object, by_name >( "victim1" );
    const auto& victim2_rc = db->get< rc_account_object, by_name >( "victim2" );
    const auto& victim3_rc = db->get< rc_account_object, by_name >( "victim3" );
    const auto& thief1_rc = db->get< rc_account_object, by_name >( "thief1" );
    const auto& thief2_rc = db->get< rc_account_object, by_name >( "thief2" );
    const auto& thief3_rc = db->get< rc_account_object, by_name >( "thief3" );

    BOOST_TEST_MESSAGE( "agent becomes recovery account for all victims" );
    change_recovery_account( "victim1", "agent", victim1_private_key );
    generate_blocks( db->head_block_time() + HIVE_OWNER_AUTH_RECOVERY_PERIOD );
    change_recovery_account( "victim2", "agent", victim2_private_key );
    generate_blocks( db->head_block_time() + HIVE_OWNER_AUTH_RECOVERY_PERIOD );
    change_recovery_account( "victim3", "agent", victim3_private_key );
    generate_blocks( db->head_block_time() + HIVE_OWNER_AUTH_RECOVERY_PERIOD );

    generate_block();

    BOOST_TEST_MESSAGE( "thiefs steal private keys of victims and set authority to their keys" );
    account_update_operation account_update;
    account_update.account = "victim1";
    account_update.owner = authority( 1, thief1_private_key.get_public_key(), 1 );
    push_transaction( account_update, victim1_private_key );
    generate_blocks( db->head_block_time() + ( HIVE_OWNER_AUTH_RECOVERY_PERIOD.to_seconds() / 5 ) );
    account_update.account = "victim2";
    account_update.owner = authority( 1, thief2_private_key.get_public_key(), 1 );
    push_transaction( account_update, victim2_private_key );
    generate_blocks( db->head_block_time() + ( HIVE_OWNER_AUTH_RECOVERY_PERIOD.to_seconds() / 5 ) );
    account_update.account = "victim3";
    account_update.owner = authority( 1, thief3_private_key.get_public_key(), 1 );
    push_transaction( account_update, victim3_private_key );
    generate_blocks( db->head_block_time() + ( HIVE_OWNER_AUTH_RECOVERY_PERIOD.to_seconds() / 5 ) );

    BOOST_TEST_MESSAGE( "victims notice a problem and ask agent for recovery" );
    request_account_recovery_operation request;
    request.account_to_recover = "victim1";
    request.recovery_account = "agent";
    auto victim1_new_private_key = generate_private_key( "victim1n" );
    request.new_owner_authority = authority( 1, victim1_new_private_key.get_public_key(), 1 );
    push_transaction( request, agent_private_key );
    request.account_to_recover = "victim2";
    auto victim2_new_private_key = generate_private_key( "victim2n" );
    request.new_owner_authority = authority( 1, victim2_new_private_key.get_public_key(), 1 );
    push_transaction( request, agent_private_key );
    request.account_to_recover = "victim3";
    auto victim3_new_private_key = generate_private_key( "victim3n" );
    request.new_owner_authority = authority( 1, victim3_new_private_key.get_public_key(), 1 );
    push_transaction( request, agent_private_key );
    generate_block();

    BOOST_TEST_MESSAGE( "thiefs keep RC of victims at zero - recovery not possible for multiple accounts in one tx..." );
    auto pre_tx_agent_mana = regenerate_mana( db_plugin, agent_rc );
    clear_mana( db_plugin, victim1_rc );
    clear_mana( db_plugin, victim2_rc );
    clear_mana( db_plugin, victim3_rc );
    auto pre_tx_thief1_mana = regenerate_mana( db_plugin, thief1_rc );
    auto pre_tx_thief2_mana = regenerate_mana( db_plugin, thief2_rc );
    auto pre_tx_thief3_mana = regenerate_mana( db_plugin, thief3_rc );
    signed_transaction tx;
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    recover_account_operation recover;
    recover.account_to_recover = "victim1";
    recover.new_owner_authority = authority( 1, victim1_new_private_key.get_public_key(), 1 );
    recover.recent_owner_authority = authority( 1, victim1_private_key.get_public_key(), 1 );
    tx.operations.push_back( recover );
    recover.account_to_recover = "victim2";
    recover.new_owner_authority = authority( 1, victim2_new_private_key.get_public_key(), 1 );
    recover.recent_owner_authority = authority( 1, victim2_private_key.get_public_key(), 1 );
    tx.operations.push_back( recover );
    recover.account_to_recover = "victim3";
    recover.new_owner_authority = authority( 1, victim3_new_private_key.get_public_key(), 1 );
    recover.recent_owner_authority = authority( 1, victim3_private_key.get_public_key(), 1 );
    tx.operations.push_back( recover );
    transfer_operation transfer;
    transfer.from = "victim1";
    transfer.to = "agent";
    transfer.amount = ASSET( "0.001 TESTS" );
    transfer.memo = "All those accounts are actually mine";
    tx.operations.push_back( transfer );
    //oops! recovery failed when combined with transfer because it's not free then
    HIVE_REQUIRE_EXCEPTION( push_transaction( tx, {victim1_private_key, victim1_new_private_key, victim2_private_key, victim2_new_private_key, victim3_private_key, victim3_new_private_key} ), "has_mana", plugin_exception );
    BOOST_REQUIRE_EQUAL( pre_tx_agent_mana, agent_rc.rc_manabar.current_mana );
    BOOST_REQUIRE_EQUAL( 0, victim1_rc.rc_manabar.current_mana );
    BOOST_REQUIRE_EQUAL( 0, victim2_rc.rc_manabar.current_mana );
    BOOST_REQUIRE_EQUAL( 0, victim3_rc.rc_manabar.current_mana );
    BOOST_REQUIRE_EQUAL( pre_tx_thief1_mana, thief1_rc.rc_manabar.current_mana );
    BOOST_REQUIRE_EQUAL( pre_tx_thief2_mana, thief2_rc.rc_manabar.current_mana );
    BOOST_REQUIRE_EQUAL( pre_tx_thief3_mana, thief3_rc.rc_manabar.current_mana );
    //remove transfer from tx
    tx.operations.pop_back();
    
    //now that transfer was removed it used to work ok despite total lack of RC mana, however
    //rc_multisig_recover_account test showed the dangers of such approach, therefore it was blocked
    //now there can be only one subsidized operation in tx and with no more than allowed limit of
    //signatures (2 in this case) for the tx to be free
    HIVE_REQUIRE_EXCEPTION( push_transaction( tx, {victim1_private_key, victim1_new_private_key, victim2_private_key, victim2_new_private_key, victim3_private_key, victim3_new_private_key} ), "has_mana", plugin_exception );
    tx.clear();
    BOOST_REQUIRE_EQUAL( pre_tx_agent_mana, agent_rc.rc_manabar.current_mana );
    BOOST_REQUIRE_EQUAL( 0, victim1_rc.rc_manabar.current_mana );
    BOOST_REQUIRE_EQUAL( 0, victim2_rc.rc_manabar.current_mana );
    BOOST_REQUIRE_EQUAL( 0, victim3_rc.rc_manabar.current_mana );
    BOOST_REQUIRE_EQUAL( pre_tx_thief1_mana, thief1_rc.rc_manabar.current_mana );
    BOOST_REQUIRE_EQUAL( pre_tx_thief2_mana, thief2_rc.rc_manabar.current_mana );
    BOOST_REQUIRE_EQUAL( pre_tx_thief3_mana, thief3_rc.rc_manabar.current_mana );
    //try separate transactions
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    recover.account_to_recover = "victim1";
    recover.new_owner_authority = authority( 1, victim1_new_private_key.get_public_key(), 1 );
    recover.recent_owner_authority = authority( 1, victim1_private_key.get_public_key(), 1 );
    tx.operations.push_back( recover );
    push_transaction( tx, {victim1_private_key, victim1_new_private_key} );
    tx.clear();
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    recover.account_to_recover = "victim2";
    recover.new_owner_authority = authority( 1, victim2_new_private_key.get_public_key(), 1 );
    recover.recent_owner_authority = authority( 1, victim2_private_key.get_public_key(), 1 );
    tx.operations.push_back( recover );
    push_transaction( tx, {victim2_private_key, victim2_new_private_key} );
    tx.clear();
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    recover.account_to_recover = "victim3";
    recover.new_owner_authority = authority( 1, victim3_new_private_key.get_public_key(), 1 );
    recover.recent_owner_authority = authority( 1, victim3_private_key.get_public_key(), 1 );
    tx.operations.push_back( recover );
    push_transaction( tx, {victim3_private_key, victim3_new_private_key} );
    tx.clear();
    BOOST_REQUIRE_EQUAL( pre_tx_agent_mana, agent_rc.rc_manabar.current_mana );
    BOOST_REQUIRE_EQUAL( 0, victim1_rc.rc_manabar.current_mana );
    BOOST_REQUIRE_EQUAL( 0, victim2_rc.rc_manabar.current_mana );
    BOOST_REQUIRE_EQUAL( 0, victim3_rc.rc_manabar.current_mana );
    BOOST_REQUIRE_EQUAL( pre_tx_thief1_mana, thief1_rc.rc_manabar.current_mana );
    BOOST_REQUIRE_EQUAL( pre_tx_thief2_mana, thief2_rc.rc_manabar.current_mana );
    BOOST_REQUIRE_EQUAL( pre_tx_thief3_mana, thief3_rc.rc_manabar.current_mana );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( rc_multisig_recover_account )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing rc resource cost of recover_account_operation with complex authority" );

    inject_hardfork( HIVE_BLOCKCHAIN_VERSION.minor_v() );
    auto skip_flags = rc_plugin->get_rc_plugin_skip_flags();
    skip_flags.skip_reject_not_enough_rc = 0;
    skip_flags.skip_reject_unknown_delta_vests = 0;
    rc_plugin->set_rc_plugin_skip_flags( skip_flags );

    static_assert( HIVE_MAX_SIG_CHECK_DEPTH >= 2 );
    static_assert( HIVE_MAX_SIG_CHECK_ACCOUNTS >= 3 * HIVE_MAX_AUTHORITY_MEMBERSHIP );

    fc::flat_map< string, vector<char> > props;
    props[ "maximum_block_size" ] = fc::raw::pack_to_vector( HIVE_MAX_BLOCK_SIZE );
    set_witness_props( props ); //simple tx with maxed authority uses over 300kB
    const auto fee = db->get_witness_schedule_object().median_props.account_creation_fee;

    ACTORS( (agent)(thief) )
    generate_block();
    vest( "initminer", "agent", ASSET( "1000.000 TESTS" ) );
    vest( "initminer", "thief", ASSET( "1000.000 TESTS" ) );
    fund( "agent", ASSET( "1000.000 TESTS" ) );

    BOOST_TEST_MESSAGE( "create signer accounts and victim account (with agent as recovery)" );

    struct signer //key signer has 40 keys, mixed signer has 2 accounts + 38 keys
    {
      account_name_type name;
      std::vector< private_key_type > keys;
      authority auth;
      bool isMixed;

      const char* build_name( char* out, int n, bool _isMixed ) const
      {
        if( _isMixed ) { out[0] = 'm'; out[1] = 'i'; out[2] = 'x'; }
        else { out[0] = 'k'; out[1] = 'e'; out[2] = 'y'; }
        out[3] = char( '0' + n / 1000 );
        out[4] = char( '0' + n % 1000 / 100 );
        out[5] = char( '0' + n % 100 / 10 );
        out[6] = char( '0' + n % 10 );
        for( int i = 7; i < 16; ++i )
          out[i] = 0;
        return out;
      }

      void create( int n, bool _isMixed, database_fixture* _this, const asset& fee )
      {
        isMixed = _isMixed;

        account_create_operation create;
        create.fee = fee;
        create.creator = "initminer";

        char _name[16];
        name = build_name( _name, n, isMixed );
        create.new_account_name = name;

        auth = authority();
        auth.weight_threshold = HIVE_MAX_AUTHORITY_MEMBERSHIP;
        for( int k = isMixed ? 2 : 0; k < HIVE_MAX_AUTHORITY_MEMBERSHIP; ++k )
        {
          _name[7] = char( '0' + k / 10 );
          _name[8] = char( '0' + k % 10 );
          keys.emplace_back( generate_private_key( _name ) );
          auth.add_authority( keys.back().get_public_key(), 1 );
        }
        if( isMixed )
        {
          auth.add_authority( build_name( _name, 2*n, false ), 1 );
          auth.add_authority( build_name( _name, 2*n+1, false ), 1 );
        }
        create.owner = auth;
        create.active = create.owner;
        create.posting = create.owner;

        _this->push_transaction( create, _this->init_account_priv_key );
        _this->generate_block();
      }

    } key_signers[ 2 * HIVE_MAX_AUTHORITY_MEMBERSHIP ],
      mixed_signers[ HIVE_MAX_AUTHORITY_MEMBERSHIP ];

    //both victim_auth and alternative are built in layers:
    //victim needs signatures of mixed signers (40)
    //mixed signers need their own signatures (38) as well as those of key signers (2)
    //each key signer has just keys (40)
    //alternative replaces last mixed signer with another account (it is needed because during
    //recovery old and new authority have to differ)
    authority victim_auth;
    authority alternative_auth;
    victim_auth.weight_threshold = HIVE_MAX_AUTHORITY_MEMBERSHIP;
    alternative_auth.weight_threshold = HIVE_MAX_AUTHORITY_MEMBERSHIP;

    int key_count = 0;
    account_create_operation create;
    create.fee = fee;
    create.creator = "agent";
    create.new_account_name = "victim";
    for( int i = 0; i < HIVE_MAX_AUTHORITY_MEMBERSHIP; ++i )
    {
      key_signers[2*i].create( 2*i, false, this, fee );
      key_count += key_signers[2*i].keys.size();
      key_signers[2*i+1].create( 2*i+1, false, this, fee );
      key_count += key_signers[2*i+1].keys.size();
      mixed_signers[i].create( i, true, this, fee );
      key_count += mixed_signers[i].keys.size();
    }
    //create one alternative account with copy of authority from last mixed signer
    {
      account_create_operation create;
      create.fee = fee;
      create.creator = "initminer";
      create.new_account_name = "alternative";
      create.owner = mixed_signers[ HIVE_MAX_AUTHORITY_MEMBERSHIP - 1 ].auth;
      create.active = create.owner;
      create.posting = create.owner;
      push_transaction( create, init_account_priv_key );
      generate_block();
    }
    for( int i = 0; i < HIVE_MAX_AUTHORITY_MEMBERSHIP-1; ++i )
    {
      victim_auth.add_authority( mixed_signers[i].name, 1 );
      alternative_auth.add_authority( mixed_signers[i].name, 1 );
    }
    victim_auth.add_authority( mixed_signers[ HIVE_MAX_AUTHORITY_MEMBERSHIP-1 ].name, 1 );
    alternative_auth.add_authority( "alternative", 1 );
    create.owner = victim_auth;
    create.active = create.owner;
    create.posting = create.owner;
    push_transaction( create, agent_private_key );

    vest( "initminer", "victim", ASSET( "1000.000 TESTS" ) );
    generate_block();
    const auto& victim_rc = db->get< rc_account_object, by_name >( "victim" );

    //many repeats to gather average CPU consumption of recovery process
    uint64_t time = 0;
    const int ITERATIONS = 20; //slooow - note that there is 4720 keys and each signature is validated around 75us
    ilog( "Measuring time of account recovery operation with ${k} signatures - ${c} iterations",
      ( "k", key_count )( "c", ITERATIONS ) );
    for( int i = 0; i < ITERATIONS; ++i )
    {
      ilog( "iteration ${i}, RC = ${r}", ( "i", i )( "r", victim_rc.rc_manabar.current_mana ) );
      BOOST_TEST_MESSAGE( "thief steals private keys of signers and sets authority to himself" );
      signed_transaction tx;
      tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
      account_update_operation account_update;
      account_update.account = "victim";
      account_update.owner = authority( 1, "thief", 1 );
      tx.operations.push_back( account_update );
      std::vector< private_key_type >_keys;
      for( int k = 0; k < 2 * HIVE_MAX_AUTHORITY_MEMBERSHIP; ++k )
        std::copy( key_signers[k].keys.begin(), key_signers[k].keys.end(), std::back_inserter( _keys ) );
      for( int k = 0; k < HIVE_MAX_AUTHORITY_MEMBERSHIP; ++k )
        std::copy( mixed_signers[k].keys.begin(), mixed_signers[k].keys.end(), std::back_inserter( _keys ) );
      push_transaction( tx, _keys );
      tx.clear();
      generate_block();

      BOOST_TEST_MESSAGE( "victim notices a problem and asks agent for recovery" );
      request_account_recovery( "agent", "victim", alternative_auth, agent_private_key );
      generate_block();
      
      BOOST_TEST_MESSAGE( "victim gets account back" );
      tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
      recover_account_operation recover;
      recover.account_to_recover = "victim";
      recover.new_owner_authority = alternative_auth;
      recover.recent_owner_authority = victim_auth;
      std::swap( victim_auth, alternative_auth ); //change for next iteration
      tx.operations.push_back( recover );

      // sign transaction outside of time measurement
      full_transaction_ptr _ftx = full_transaction_type::create_from_signed_transaction( tx, pack_type::hf26, false );
      _ftx->sign_transaction( _keys, db->get_chain_id(), fc::ecc::fc_canonical, pack_type::hf26 );
      uint64_t start_time = std::chrono::duration_cast< std::chrono::nanoseconds >(
        std::chrono::system_clock::now().time_since_epoch() ).count();
      db->push_transaction( _ftx, 0 );
      uint64_t stop_time = std::chrono::duration_cast< std::chrono::nanoseconds >(
        std::chrono::system_clock::now().time_since_epoch() ).count();
      time += stop_time - start_time;
      tx.clear();
      generate_block();
    }
    ilog( "Average time for recovery transaction = ${t}ms",
      ( "t", ( time + 500'000 ) / ITERATIONS / 1'000'000 ) );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( rc_tx_order_bug )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing different transaction order in pending transactions vs actual block" );

    inject_hardfork( HIVE_BLOCKCHAIN_VERSION.minor_v() );
    auto skip_flags = rc_plugin->get_rc_plugin_skip_flags();
    skip_flags.skip_reject_not_enough_rc = 0;
    skip_flags.skip_reject_unknown_delta_vests = 0;
    rc_plugin->set_rc_plugin_skip_flags( skip_flags );

    ACTORS( (alice)(bob) )
    generate_block();
    vest( "initminer", "bob", ASSET( "70000.000 TESTS" ) ); //<- change that amount to tune RC cost
    fund( "alice", ASSET( "1000.000 TESTS" ) );

    const auto& alice_rc = db->get< rc_account_object, by_name >( "alice" );

    BOOST_TEST_MESSAGE( "Clear RC on alice and wait a bit so she has enough for one operation but not two" );
    clear_mana( db_plugin, alice_rc );
    generate_block();
    generate_block();

    signed_transaction tx1, tx2;
    tx1.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx2.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    transfer_operation transfer;
    transfer.from = "alice";
    transfer.to = "bob";
    transfer.amount = ASSET( "10.000 TESTS" );
    transfer.memo = "First transfer";
    tx1.operations.push_back( transfer );
    push_transaction( tx1, alice_private_key ); //t1
    BOOST_REQUIRE( get_balance( "alice" ) == ASSET( "990.000 TESTS" ) );
    BOOST_REQUIRE( get_balance( "bob" ) == ASSET( "10.000 TESTS" ) );
    transfer.amount = ASSET( "5.000 TESTS" );
    transfer.memo = "Second transfer";
    tx2.operations.push_back( transfer );
    HIVE_REQUIRE_EXCEPTION( push_transaction( tx2, alice_private_key ), "has_mana", plugin_exception ); //t2
    BOOST_REQUIRE( get_balance( "alice" ) == ASSET( "990.000 TESTS" ) );
    BOOST_REQUIRE( get_balance( "bob" ) == ASSET( "10.000 TESTS" ) );
    generate_block(); //t1 becomes part of block

    BOOST_TEST_MESSAGE( "Save aside and remove head block" );
    auto block = db->fetch_block_by_number( db->head_block_num() );
    BOOST_REQUIRE( block );
    db->pop_block(); //t1 becomes popped
    BOOST_REQUIRE( get_balance( "alice" ) == ASSET( "1000.000 TESTS" ) );
    BOOST_REQUIRE( get_balance( "bob" ) == ASSET( "0.000 TESTS" ) );

    BOOST_TEST_MESSAGE( "Reapply transaction that failed before putting it to pending" );
    push_transaction( tx2, alice_private_key, 0 ); //t2 becomes pending
    BOOST_REQUIRE( get_balance( "alice" ) == ASSET( "995.000 TESTS" ) );
    BOOST_REQUIRE( get_balance( "bob" ) == ASSET( "5.000 TESTS" ) );

    BOOST_TEST_MESSAGE( "Push previously popped block - pending transaction should run into lack of RC again" );
    //the only way to see if we run into problem is to observe ilog messages
    BOOST_REQUIRE( fc::logger::get( DEFAULT_LOGGER ).is_enabled( fc::log_level::info ) );
    {
      struct tcatcher : public fc::appender
      {
        virtual void log( const fc::log_message& m )
        {
          const char* PROBLEM_MSG = "Accepting transaction by alice";
          BOOST_REQUIRE_NE( std::memcmp( m.get_message().c_str(), PROBLEM_MSG, std::strlen( PROBLEM_MSG ) ), 0 );
        }
      };
      auto catcher = fc::shared_ptr<tcatcher>( new tcatcher() );
      autoscope auto_reset( [&]() { fc::logger::get( DEFAULT_LOGGER ).remove_appender( catcher ); } );
      fc::logger::get( DEFAULT_LOGGER ).add_appender( catcher );
      push_block( block );
      //t1 was applied as part of block, then popped version of t1 was skipped as duplicate and t2 was
      //applied as pending; since lack of RC does not block transaction when it is pending, it remains
      //as pending; we can check that by looking at balances
      BOOST_REQUIRE( get_balance( "alice" ) == ASSET( "985.000 TESTS" ) );
      BOOST_REQUIRE( get_balance( "bob" ) == ASSET( "15.000 TESTS" ) );

      generate_block();
      //testing fix for former 'is_producing() == false' when building new block; now witness actually
      //'is_in_control()' when producing block, which means pending t2 was not included during block production
      //however it remains pending
      BOOST_REQUIRE( get_balance( "alice" ) == ASSET( "985.000 TESTS" ) );
      BOOST_REQUIRE( get_balance( "bob" ) == ASSET( "15.000 TESTS" ) );
      block = db->fetch_block_by_number( db->head_block_num() );
      BOOST_REQUIRE( block );
      //check that block is indeed empty, without t2 and that tx2 waits as pending
      BOOST_REQUIRE( block->get_block().transactions.empty() && !db->_pending_tx.empty() );

      //transaction is going to wait in pending until alice gains enough RC or transaction expires
      int i = 0;
      do
      {
        generate_block();
        ++i;
      }
      while( !db->_pending_tx.empty() );

      //check that t2 did not expire but waited couple blocks for RC and finally got accepted
      BOOST_REQUIRE( get_balance( "alice" ) == ASSET( "985.000 TESTS" ) );
      BOOST_REQUIRE( get_balance( "bob" ) == ASSET( "15.000 TESTS" ) );
      BOOST_REQUIRE_EQUAL( i, 1 );
    }

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( rc_pending_data_reset )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing if rc_pending_data resets properly" );

    inject_hardfork( HIVE_BLOCKCHAIN_VERSION.minor_v() );
    auto skip_flags = rc_plugin->get_rc_plugin_skip_flags();
    skip_flags.skip_reject_not_enough_rc = 0;
    skip_flags.skip_reject_unknown_delta_vests = 0;
    rc_plugin->set_rc_plugin_skip_flags( skip_flags );

    ACTORS( (alice)(bob) )
    generate_block();
    fund( "alice", ASSET( "1000.000 TESTS" ) );
    generate_block();

    const auto& pending_data = db->get< rc_pending_data >();

    auto check_direction = []( const fc::int_array< int64_t, HIVE_RC_NUM_RESOURCE_TYPES >& values,
      const std::array< int, HIVE_RC_NUM_RESOURCE_TYPES >& sign )
    {
      for( int i = 0; i < HIVE_RC_NUM_RESOURCE_TYPES; ++i )
      {
        switch( sign[i] )
        {
          case -1:
            BOOST_REQUIRE_LT( values[i], 0 );
            break;
          case 0:
            BOOST_REQUIRE_EQUAL( values[i], 0 );
            break;
          case 1:
            BOOST_REQUIRE_GT( values[i], 0 );
            break;
          default:
            assert( false && "Incorrect use" );
            break;
        }
      }
    };
    auto check_compare = [&]( const fc::int_array< int64_t, HIVE_RC_NUM_RESOURCE_TYPES >& v1,
      const fc::int_array< int64_t, HIVE_RC_NUM_RESOURCE_TYPES >& v2,
      const std::array< int, HIVE_RC_NUM_RESOURCE_TYPES >& sign )
    {
      fc::int_array< int64_t, HIVE_RC_NUM_RESOURCE_TYPES > diff;
      for( int i = 0; i < HIVE_RC_NUM_RESOURCE_TYPES; ++i )
        diff[i] = v1[i] - v2[i];
      check_direction( diff, sign );
    };
    std::array< int, HIVE_RC_NUM_RESOURCE_TYPES > empty = { 0, 0, 0, 0, 0 };

    BOOST_TEST_MESSAGE( "Pending usage and pending cost are all zero at the start of block" );
    check_direction( pending_data.get_pending_usage(), empty );
    check_direction( pending_data.get_pending_cost(), empty );
    check_direction( pending_data.get_differential_usage(), empty ); //it is empty only because last tx was a transfer

    BOOST_TEST_MESSAGE( "Update active key to generate some usage" );
    account_update_operation update;
    update.account = "alice";
    update.active = authority( 1, generate_private_key( "alice_active" ).get_public_key(), 1 );
    push_transaction( update, alice_private_key );
    auto first_pending_usage = pending_data.get_pending_usage();
    auto first_pending_cost = pending_data.get_pending_cost();
    auto first_diff_usage = pending_data.get_differential_usage();
    check_direction( first_pending_usage, { 1, 0, 0, 1, 1 } );
    check_direction( first_pending_cost, { 1, 0, 0, 1, 1 } );
    check_direction( first_diff_usage, { 0, 0, 0, -1, 0 } );

    BOOST_TEST_MESSAGE( "Make a transfer - differential usage should reset, but pending should not" );
    transfer_operation transfer;
    transfer.from = "alice";
    transfer.to = "bob";
    transfer.amount = ASSET( "10.000 TESTS" );
    push_transaction( transfer, alice_private_key );
    auto second_pending_usage = pending_data.get_pending_usage();
    auto second_pending_cost = pending_data.get_pending_cost();
    auto second_diff_usage = pending_data.get_differential_usage();
    check_direction( second_pending_usage, { 1, 0, 1, 1, 1 } );
    check_direction( second_pending_cost, { 1, 0, 1, 1, 1 } );
    check_direction( second_diff_usage, empty ); //differential usage is per transaction so it was reset
    check_compare( first_pending_usage, second_pending_usage, { -1, 0, -1, -1, -1 } );
    check_compare( first_pending_cost, second_pending_cost, { -1, 0, -1, -1, -1 } );

    BOOST_TEST_MESSAGE( "Update active key for second time - differential usage resets again to the same value" );
    update.active = authority( 1, generate_private_key( "alice_active_2" ).get_public_key(), 1 );
    push_transaction( update, alice_private_key );
    auto third_pending_usage = pending_data.get_pending_usage();
    auto third_pending_cost = pending_data.get_pending_cost();
    auto third_diff_usage = pending_data.get_differential_usage();
    check_direction( third_pending_usage, { 1, 0, 1, 1, 1 } ); //market bytes usage from transfer remains
    check_direction( third_pending_cost, { 1, 0, 1, 1, 1 } ); //same with cost
    check_direction( third_diff_usage, { 0, 0, 0, -1, 0 } ); //differential usage reset again and only contains data on last tx
    check_compare( second_pending_usage, third_pending_usage, { -1, 0, 0, -1, -1 } );
    check_compare( second_pending_cost, third_pending_cost, { -1, 0, 0, -1, -1 } );
    check_compare( first_diff_usage, third_diff_usage, empty );

    BOOST_TEST_MESSAGE( "Attempt to update active key for third time but fail - all values revert to previous" );
    update.active = authority( 1, "nonexistent", 1 );
    HIVE_REQUIRE_ASSERT( push_transaction( update, alice_private_key ), "a != nullptr" );
    check_compare( third_pending_usage, pending_data.get_pending_usage(), empty );
    check_compare( third_pending_cost, pending_data.get_pending_cost(), empty );
    check_compare( third_diff_usage, pending_data.get_differential_usage(), empty );

    BOOST_TEST_MESSAGE( "Finalize block and move to new one - pending data and cost are reset (but not differential usage)" );
    generate_block();
    //Why two generate_block calls? To understand that we need to understand what generate_block actually
    //does. When transaction is pushed, it becomes pending (as if it was passed through API or P2P).
    //generate_block rewinds the state, then it opens (pre-apply block) and produces block out of pending
    //transactions (assuming they actually fit, because we might've pushed too many for max size of block),
    //closes the block (post-apply block), tries to reapply pending transactions on top of it (the ones
    //that became part of recent block will be dropped from list as "known") and finishes. The code after
    //generate_block call will see the state after reapplication of transactions and not freshly after
    //reset. To emulate fresh reset we need to actually produce empty block.
    generate_block();
    check_direction( pending_data.get_pending_usage(), empty );
    check_direction( pending_data.get_pending_cost(), empty );
    check_compare( pending_data.get_differential_usage(), third_diff_usage, empty );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( rc_differential_usage_operations )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing differential RC usage for selected operations" );

    inject_hardfork( HIVE_BLOCKCHAIN_VERSION.minor_v() );
    auto skip_flags = rc_plugin->get_rc_plugin_skip_flags();
    skip_flags.skip_reject_not_enough_rc = 0;
    skip_flags.skip_reject_unknown_delta_vests = 0;
    rc_plugin->set_rc_plugin_skip_flags( skip_flags );

    generate_block();
    const auto& pending_data = db->get< rc_pending_data >();

    ACTORS( (alice)(bob)(sam) )
    generate_block();
    fund( "alice", ASSET( "1000.000 TESTS" ) );
    vest( "initminer", "alice", ASSET( "10.000 TESTS" ) );
    vest( "initminer", "bob", ASSET( "10.000 TESTS" ) );
    generate_block();

    auto alice_owner_key = generate_private_key( "alice_owner" );
    auto bob_owner_key = generate_private_key( "bob_owner" );

    auto check = []( const fc::int_array< int64_t, HIVE_RC_NUM_RESOURCE_TYPES >& usage, bool empty = false )
    {
      BOOST_REQUIRE_EQUAL( usage[ resource_history_bytes ], 0 );
      BOOST_REQUIRE_EQUAL( usage[ resource_new_accounts ], 0 );
      BOOST_REQUIRE_EQUAL( usage[ resource_market_bytes ], 0 );
      if( empty )
        BOOST_REQUIRE_EQUAL( usage[ resource_state_bytes ], 0 );
      else
        BOOST_REQUIRE_LT( usage[ resource_state_bytes ], 0 );
      BOOST_REQUIRE_EQUAL( usage[ resource_execution_time ], 0 );
    };
    //rc_pending_data_reset test showed that differential usage is reset by transaction but not block
    //so let's make transfer to force reset to zero (that test also showed transfers have zero
    //differential usage)
    auto clean = [&]()
    {
      transfer_operation transfer;
      transfer.from = "alice";
      transfer.to = "bob";
      transfer.amount = ASSET( "0.001 TESTS" );
      push_transaction( transfer, alice_owner_key );
      generate_block();
      generate_block();
    };

    BOOST_TEST_MESSAGE( "Update owner key with account_update_operation" );
    account_update_operation update;
    update.account = "alice";
    update.owner = authority( 1, alice_owner_key.get_public_key(), 1 );
    push_transaction( update, alice_private_key );
    update.owner.reset();
    check( pending_data.get_differential_usage() );
    clean();

    BOOST_TEST_MESSAGE( "Update active key with account_update_operation" );
    update.active = authority( 1, generate_private_key( "alice_active" ).get_public_key(), 1 );
    push_transaction( update, alice_owner_key );
    check( pending_data.get_differential_usage() );
    update.active.reset();
    clean();

    BOOST_TEST_MESSAGE( "Update posting key with account_update_operation" );
    update.posting = authority( 1, generate_private_key( "alice_posting" ).get_public_key(), 1 );
    push_transaction( update, alice_owner_key );
    check( pending_data.get_differential_usage() );
    update.posting.reset();
    clean();

    BOOST_TEST_MESSAGE( "Update memo key with account_update_operation" );
    update.memo_key = generate_private_key( "alice_memo" ).get_public_key();
    push_transaction( update, alice_owner_key );
    check( pending_data.get_differential_usage(), true ); //memo key does not allocate new state
    update.memo_key = public_key_type();
    clean();

    BOOST_TEST_MESSAGE( "Update metadata with account_update_operation" );
    update.json_metadata = "{\"profile_image\":\"https://somewhere.com/myself.png\"}";
    push_transaction( update, alice_owner_key );
    check( pending_data.get_differential_usage(), true ); //while metadata allocates state it is
      //optionally stored which means nodes that don't store it would not be able to calculate
      //differential usage; that's why metadata is not subject to differential usage calculation
    update.json_metadata = "";
    clean();

    account_update2_operation update2;
    BOOST_TEST_MESSAGE( "Update owner key with account_update2_operation" );
    update2.account = "bob";
    update2.owner = authority( 1, bob_owner_key.get_public_key(), 1 );
    push_transaction( update2, bob_private_key );
    update2.owner.reset();
    check( pending_data.get_differential_usage() );
    clean();

    BOOST_TEST_MESSAGE( "Update active key with account_update2_operation" );
    update2.active = authority( 1, generate_private_key( "bob_active" ).get_public_key(), 1 );
    push_transaction( update2, bob_owner_key );
    check( pending_data.get_differential_usage() );
    update2.active.reset();
    clean();

    BOOST_TEST_MESSAGE( "Update posting key with account_update2_operation" );
    update2.posting = authority( 1, generate_private_key( "bob_posting" ).get_public_key(), 1 );
    push_transaction( update2, bob_owner_key );
    check( pending_data.get_differential_usage() );
    update2.posting.reset();
    clean();

    BOOST_TEST_MESSAGE( "Update memo key with account_update2_operation" );
    update2.memo_key = generate_private_key( "bob_memo" ).get_public_key();
    push_transaction( update2, bob_owner_key );
    check( pending_data.get_differential_usage(), true ); //memo key does not allocate new state
    update2.memo_key = public_key_type();
    clean();

    BOOST_TEST_MESSAGE( "Update metadata with account_update2_operation" );
    update2.json_metadata = "{\"profile_image\":\"https://somewhere.com/superman.png\"}";
    update2.posting_json_metadata = "{\"description\":\"I'm here just for test.\"}";
    push_transaction( update2, bob_owner_key );
    check( pending_data.get_differential_usage(), true ); //same as in case of account_update_operation
    update2.json_metadata = "";
    update2.posting_json_metadata = "";
    clean();

    BOOST_TEST_MESSAGE( "Register witness with witness_update_operation" );
    witness_update_operation witness;
    witness.owner = "alice";
    witness.url = "https://alice.has.cat";
    witness.block_signing_key = generate_private_key( "alice_witness" ).get_public_key();
    witness.props.account_creation_fee = legacy_hive_asset::from_amount( HIVE_MIN_ACCOUNT_CREATION_FEE );
    witness.props.maximum_block_size = HIVE_MIN_BLOCK_SIZE_LIMIT * 2;
    witness.props.hbd_interest_rate = 0;
    witness.fee = asset( 0, HIVE_SYMBOL );
    push_transaction( witness, alice_owner_key );
    check( pending_data.get_differential_usage(), true ); //when witness is created differential usage is empty
    clean();

    BOOST_TEST_MESSAGE( "Update witness with witness_update_operation" );
    witness.url = "https://alice.wonder.land/my.cat";
    push_transaction( witness, alice_owner_key );
    check( pending_data.get_differential_usage() ); //when witness is updated differential usage kicks in
    clean();

    auto alice_new_owner_key = generate_private_key( "alice_new_owner" );
    auto bob_new_owner_key = generate_private_key( "bob_new_owner" );
    auto thief_public_key = generate_private_key( "stolen" ).get_public_key();

    BOOST_TEST_MESSAGE( "Compromise account authority and request recovery with request_account_recovery_operation" );
    //we can't pretend previous change of owner was from a thief because in testnet we only have 20 blocks
    //worth of owner history and 3 blocks for recovery after request
    update.account = "alice";
    update.owner = authority( 1, thief_public_key, 1 );
    push_transaction( update, alice_owner_key );
    update.owner.reset();
    generate_block();
    request_account_recovery_operation request;
    request.account_to_recover = "alice";
    request.recovery_account = "initminer";
    request.new_owner_authority = authority( 1, alice_new_owner_key.get_public_key(), 1 );
    push_transaction( request, init_account_priv_key );
    check( pending_data.get_differential_usage(), true ); //no differential usage
    generate_block();
    generate_block();

    BOOST_TEST_MESSAGE( "Recover account with recover_account_operation - subsidized version" );
    recover_account_operation recover;
    recover.account_to_recover = "alice";
    recover.recent_owner_authority = authority( 1, alice_owner_key.get_public_key(), 1 );
    recover.new_owner_authority = authority( 1, alice_new_owner_key.get_public_key(), 1 );
    signed_transaction tx;
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.operations.push_back( recover );
    push_transaction( tx, {alice_owner_key, alice_new_owner_key} );
    tx.clear();
    alice_owner_key = alice_new_owner_key;
    check( pending_data.get_differential_usage() ); //differential usage does not depend on whether operation will be subsidized or not
    check( pending_data.get_pending_usage(), true ); //this is subsidized
    clean();

    BOOST_TEST_MESSAGE( "Compromise account authority and request recovery with request_account_recovery_operation" );
    update.account = "bob";
    update.owner = authority( 1, thief_public_key, 1 );
    push_transaction( update, bob_owner_key );
    update.owner.reset();
    generate_block();
    request.account_to_recover = "bob";
    request.new_owner_authority = authority( 1, bob_new_owner_key.get_public_key(), 1, generate_private_key( "bob_new_owner2" ).get_public_key(), 1 );
    push_transaction( request, init_account_priv_key );
    check( pending_data.get_differential_usage(), true ); //no differential usage
    generate_block();
    generate_block();

    BOOST_TEST_MESSAGE( "Recover account with recover_account_operation - fully paid version" );
    recover.account_to_recover = "bob";
    recover.recent_owner_authority = authority( 1, bob_owner_key.get_public_key(), 1 );
    recover.new_owner_authority = authority( 1, bob_new_owner_key.get_public_key(), 1, generate_private_key( "bob_new_owner2" ).get_public_key(), 1 );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.operations.push_back( recover );
    push_transaction( tx, {bob_owner_key, bob_new_owner_key} );
    tx.clear();
    bob_owner_key = bob_new_owner_key;
    check( pending_data.get_differential_usage() ); //differential usage does not depend on whether operation will be subsidized or not
    //this is fully paid:
    BOOST_REQUIRE_GT( pending_data.get_pending_usage()[ resource_history_bytes ], 0 );
    BOOST_REQUIRE_GT( pending_data.get_pending_usage()[ resource_state_bytes ], 0 );
    BOOST_REQUIRE_GT( pending_data.get_pending_usage()[ resource_execution_time ], 0 );
    clean();

    auto calculate_cost = []( const resource_cost_type& costs ) -> int64_t
    {
      int64_t result = 0;
      for( auto& cost : costs )
        result += cost;
      return result;
    };

    BOOST_TEST_MESSAGE( "Create RC delegation with delegate_rc_operation" );
    delegate_rc_operation rc_delegation;
    rc_delegation.from = "alice";
    rc_delegation.delegatees = { "bob" };
    rc_delegation.max_rc = 10000000;
    custom_json_operation custom_json;
    custom_json.required_posting_auths.insert( "alice" );
    custom_json.id = HIVE_RC_PLUGIN_NAME;
    custom_json.json = fc::json::to_string( rc_plugin_operation( rc_delegation ) );
    push_transaction( custom_json, alice_owner_key );
    auto first_delegation_extra_usage = pending_data.get_differential_usage();
    BOOST_REQUIRE_EQUAL( first_delegation_extra_usage[ resource_history_bytes ], 0 );
    BOOST_REQUIRE_EQUAL( first_delegation_extra_usage[ resource_new_accounts ], 0 );
    BOOST_REQUIRE_EQUAL( first_delegation_extra_usage[ resource_market_bytes ], 0 );
    //differential usage counters are also used to hold extra cost of custom ops:
    BOOST_REQUIRE_GT( first_delegation_extra_usage[ resource_state_bytes ], 0 );
    BOOST_REQUIRE_GT( first_delegation_extra_usage[ resource_execution_time ], 0 );
    auto first_delegation_cost = calculate_cost( pending_data.get_pending_cost() );
    clean();

    const auto diff_limit = ( first_delegation_cost + 99 ) / 100; //rounded up 1% of first delegation cost

    BOOST_TEST_MESSAGE( "Create and update RC delegation with delegate_rc_operation" );
    rc_delegation.delegatees = { "bob", "sam" };
    rc_delegation.max_rc = 5000000;
    custom_json.json = fc::json::to_string( rc_plugin_operation( rc_delegation ) );
    push_transaction( custom_json, alice_owner_key );
    auto second_delegation_extra_usage = pending_data.get_differential_usage();
    for( int i = 0; i < HIVE_RC_NUM_RESOURCE_TYPES; ++i ) //just one new delegation like in first case
      BOOST_REQUIRE_EQUAL( first_delegation_extra_usage[i], second_delegation_extra_usage[i] );
    auto second_delegation_cost = calculate_cost( pending_data.get_pending_cost() );
    //cost of first and second should be almost the same (allowing small difference)
    BOOST_REQUIRE_LT( abs( first_delegation_cost - second_delegation_cost ), diff_limit );
    clean();

    BOOST_TEST_MESSAGE( "Update RC delegations with delegate_rc_operation" );
    rc_delegation.max_rc = 7500000;
    custom_json.json = fc::json::to_string( rc_plugin_operation( rc_delegation ) );
    push_transaction( custom_json, alice_owner_key );
    auto third_delegation_extra_usage = pending_data.get_differential_usage();
    BOOST_REQUIRE_EQUAL( third_delegation_extra_usage[ resource_history_bytes ], 0 );
    BOOST_REQUIRE_EQUAL( third_delegation_extra_usage[ resource_new_accounts ], 0 );
    BOOST_REQUIRE_EQUAL( third_delegation_extra_usage[ resource_market_bytes ], 0 );
    BOOST_REQUIRE_EQUAL( third_delegation_extra_usage[ resource_state_bytes ], 0 ); //no extra state - all delegations are updates
    BOOST_REQUIRE_EQUAL( third_delegation_extra_usage[ resource_execution_time ], first_delegation_extra_usage[ resource_execution_time ] );
    auto third_delegation_cost = calculate_cost( pending_data.get_pending_cost() );
    //cost of third should be minuscule (allowing small value)
    BOOST_REQUIRE_LT( third_delegation_cost, diff_limit );
    clean();

    //for comparison we're doing the same custom json but with dummy tag, so it is not interpreted
    BOOST_TEST_MESSAGE( "Fake RC delegation update with delegate_rc_operation under dummy tag" );
    custom_json.id = "dummy";
    push_transaction( custom_json, alice_owner_key );
    check( pending_data.get_differential_usage(), true ); //no differential usage
    auto dummy_delegation_cost = calculate_cost( pending_data.get_pending_cost() );
    //cost should be even lower than that for third actual delegation
    BOOST_REQUIRE_LT( dummy_delegation_cost, third_delegation_cost );
    clean();

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( rc_differential_usage_negative )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing differential RC usage with potentially negative value" );

    inject_hardfork( HIVE_BLOCKCHAIN_VERSION.minor_v() );
    auto skip_flags = rc_plugin->get_rc_plugin_skip_flags();
    skip_flags.skip_reject_not_enough_rc = 0;
    skip_flags.skip_reject_unknown_delta_vests = 0;
    rc_plugin->set_rc_plugin_skip_flags( skip_flags );

    generate_block();
    const auto& pending_data = db->get< rc_pending_data >();

    PREP_ACTOR( alice )
      //alice will initially use HIVE_MAX_AUTHORITY_MEMBERSHIP of keys with the same full authority;
    PREP_ACTOR( barry )
      //similar to alice, but will be used in discount test at the end (less keys - to tune the usage)
    PREP_ACTOR( carol )
      //carol will use two keys (if usage changes, it might end up being not enough to outweight other
      //state usage of transaction, but for now it is enough to put her in negative when she changes
      //that to single key);
    PREP_ACTOR( diana )
      //diana uses single key from the start
      //this way only one signature will still be needed, making transactions for alice, carol and diana
      //weight the same, while alice's authority definition will be way heavier and carol a bit heavier

    account_create_operation create;
    create.fee = db->get_witness_schedule_object().median_props.account_creation_fee;
    create.creator = "initminer";
    create.new_account_name = "alice";
    create.owner = authority( 1, alice_public_key, 1 );
    {
      string name( "alice0" );
      for( int i = 1; i < HIVE_MAX_AUTHORITY_MEMBERSHIP; ++i )
      {
        name[5] = '0' + i;
        create.owner.add_authority( generate_private_key( name ).get_public_key(), 1 );
      }
    }
    create.active = create.owner;
    create.posting = authority( 1, alice_post_key.get_public_key(), 1 );
    push_transaction( create, init_account_priv_key );
    create.new_account_name = "barry";
    create.owner = authority( 1, barry_public_key, 1 );
    const int NUM_KEYS = 3; //<- use this value to tune usage so in the end we have it small but positive
    {
      string name( "barry0" );
      for( int i = 1; i < NUM_KEYS; ++i )
      {
        name[5] = '0' + i;
        create.owner.add_authority( generate_private_key( name ).get_public_key(), 1 );
      }
    }
    create.active = create.owner;
    create.posting = authority( 1, barry_post_key.get_public_key(), 1 );
    push_transaction( create, init_account_priv_key );
    create.new_account_name = "carol";
    create.owner = authority( 1, carol_public_key, 1, generate_private_key( "carol1" ).get_public_key(), 1 );
    create.active = create.owner;
    create.posting = authority( 1, carol_post_key.get_public_key(), 1 );
    push_transaction( create, init_account_priv_key );
    create.new_account_name = "diana";
    create.owner = authority( 1, diana_public_key, 1 );
    create.active = create.owner;
    create.posting = authority( 1, diana_post_key.get_public_key(), 1 );
    push_transaction( create, init_account_priv_key );
    generate_block();

    //add some source of RC so they can actually perform those (expensive) operations
    vest( "initminer", "alice", ASSET( "10.000 TESTS" ) );
    fund( "alice", 10000 );
    vest( "initminer", "barry", ASSET( "10.000 TESTS" ) );
    fund( "barry", 10000 );
    vest( "initminer", "carol", ASSET( "10.000 TESTS" ) );
    fund( "carol", 10000 );
    vest( "initminer", "diana", ASSET( "10.000 TESTS" ) );
    fund( "diana", 10000 );
    generate_block();
    //see explanation at the end of rc_pending_data_reset test
    //note that we don't actually have access to data on transaction but whole block, so it is
    //important for this test to always use just one transaction and fully reset before next one
    generate_block();

    BOOST_TEST_MESSAGE( "Changing authority and comparing RC usage" );

    account_update_operation update;
    update.account = "alice";
    update.active = authority( 1, generate_private_key( "alice_active" ).get_public_key(), 1 );
    push_transaction( update, alice_private_key );
    auto alice_diff_usage = pending_data.get_differential_usage();
    BOOST_REQUIRE_EQUAL( alice_diff_usage[ resource_history_bytes ], 0 );
    BOOST_REQUIRE_EQUAL( alice_diff_usage[ resource_new_accounts ], 0 );
    BOOST_REQUIRE_EQUAL( alice_diff_usage[ resource_market_bytes ], 0 );
    BOOST_REQUIRE_LT( alice_diff_usage[ resource_state_bytes ], 0 );
    BOOST_REQUIRE_EQUAL( alice_diff_usage[ resource_execution_time ], 0 );
    auto alice_rc_usage = pending_data.get_pending_usage();
    //check if state usage didn't end up being negative
    BOOST_REQUIRE_EQUAL( alice_rc_usage[ resource_state_bytes ], 0 );
    generate_block();
    generate_block();

    update.account = "carol";
    update.active = authority( 1, generate_private_key( "carol_active" ).get_public_key(), 1 );
    push_transaction( update, carol_private_key );
    auto carol_diff_usage = pending_data.get_differential_usage();
    BOOST_REQUIRE_EQUAL( carol_diff_usage[ resource_history_bytes ], 0 );
    BOOST_REQUIRE_EQUAL( carol_diff_usage[ resource_new_accounts ], 0 );
    BOOST_REQUIRE_EQUAL( carol_diff_usage[ resource_market_bytes ], 0 );
    BOOST_REQUIRE_LT( carol_diff_usage[ resource_state_bytes ], 0 );
    BOOST_REQUIRE_EQUAL( carol_diff_usage[ resource_execution_time ], 0 );
    auto carol_rc_usage = pending_data.get_pending_usage();
    //check if state usage didn't end up being negative
    BOOST_REQUIRE_EQUAL( carol_rc_usage[ resource_state_bytes ], 0 );
    generate_block();
    generate_block();

    update.account = "diana";
    update.active = authority( 1, generate_private_key( "diana_active" ).get_public_key(), 1 );
    push_transaction( update, diana_private_key );
    auto diana_diff_usage = pending_data.get_differential_usage();
    BOOST_REQUIRE_EQUAL( diana_diff_usage[ resource_history_bytes ], 0 );
    BOOST_REQUIRE_EQUAL( diana_diff_usage[ resource_new_accounts ], 0 );
    BOOST_REQUIRE_EQUAL( diana_diff_usage[ resource_market_bytes ], 0 );
    BOOST_REQUIRE_LT( diana_diff_usage[ resource_state_bytes ], 0 );
    BOOST_REQUIRE_EQUAL( diana_diff_usage[ resource_execution_time ], 0 );
    auto diana_rc_usage = pending_data.get_pending_usage();
    BOOST_REQUIRE_GT( diana_rc_usage[ resource_state_bytes ], 0 );
    generate_block();
    generate_block();

    BOOST_REQUIRE_EQUAL( alice_diff_usage[ resource_state_bytes ], HIVE_MAX_AUTHORITY_MEMBERSHIP * diana_diff_usage[ resource_state_bytes ] );
    BOOST_REQUIRE_EQUAL( carol_diff_usage[ resource_state_bytes ], 2 * diana_diff_usage[ resource_state_bytes ] );
    //check if test actually tests what it was supposed to (state usage would go negative if it wasn't for extra protection)
    auto no_diff_usage = diana_rc_usage[ resource_state_bytes ] - diana_diff_usage[ resource_state_bytes ];
    BOOST_REQUIRE_LT( no_diff_usage + alice_diff_usage[ resource_state_bytes ], 0 );
    BOOST_REQUIRE_LT( no_diff_usage + carol_diff_usage[ resource_state_bytes ], 0 );

    BOOST_TEST_MESSAGE( "Changing authority and using its negative usage to discount other operations" );
    signed_transaction tx;
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    transfer_to_vesting_operation powerup;
    powerup.from = "barry";
    powerup.to = "barry";
    powerup.amount = ASSET( "10.000 TESTS" );
    tx.operations.push_back( powerup );
    //that operation on itself has small positive state usage
    delegate_vesting_shares_operation delegation;
    delegation.delegator = "barry";
    delegation.delegatee = "alice";
    delegation.vesting_shares = ASSET( "5.000000 VESTS" );
    tx.operations.push_back( delegation );
    //that operation on itself has fairly noticable positive state usage
    update.account = "barry";
    update.active = authority( 1, generate_private_key( "barry_active" ).get_public_key(), 1 );
    tx.operations.push_back( update );
    //as we've seen above such update will have significant negative state usage
    //some more ops so we can get positive state usage at the end (also NUM_KEYS can be used for tuning)
    delegation.delegatee = "carol";
    delegation.vesting_shares = ASSET( "5.000000 VESTS" );
    tx.operations.push_back( delegation );
    //delegation.delegatee = "diana";
    //delegation.vesting_shares = ASSET( "5.000000 VESTS" );
    //tx.operations.push_back( delegation );
    push_transaction( tx, barry_private_key );
    tx.clear();
    auto barry_diff_usage = pending_data.get_differential_usage();
    BOOST_REQUIRE_EQUAL( barry_diff_usage[ resource_history_bytes ], 0 );
    BOOST_REQUIRE_EQUAL( barry_diff_usage[ resource_new_accounts ], 0 );
    BOOST_REQUIRE_EQUAL( barry_diff_usage[ resource_market_bytes ], 0 );
    BOOST_REQUIRE_LT( barry_diff_usage[ resource_state_bytes ], 0 );
    BOOST_REQUIRE_EQUAL( barry_diff_usage[ resource_execution_time ], 0 );
    auto barry_rc_usage = pending_data.get_pending_usage();
    //check if we filled transaction enough to overcome discount from account update
    BOOST_REQUIRE_GT( barry_rc_usage[ resource_state_bytes ], 0 );
    generate_block();
    generate_block();

    BOOST_TEST_MESSAGE( "Comparing state usage with delegation in separate transaction" );
    delegation.delegator = "alice";
    delegation.delegatee = "barry";
    push_transaction( delegation, alice_private_key );
    auto alice_delegation_rc_usage = pending_data.get_pending_usage();
    //check if we tuned the test so single delegation uses more state than authority reducing update mixed with couple delegations
    BOOST_REQUIRE_LT( barry_rc_usage[ resource_state_bytes ], alice_delegation_rc_usage[ resource_state_bytes ] );
    generate_block();
    generate_block();

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( rc_differential_usage_many_ops )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing differential RC usage with multiple operations" );

    inject_hardfork( HIVE_BLOCKCHAIN_VERSION.minor_v() );
    auto skip_flags = rc_plugin->get_rc_plugin_skip_flags();
    skip_flags.skip_reject_not_enough_rc = 0;
    skip_flags.skip_reject_unknown_delta_vests = 0;
    rc_plugin->set_rc_plugin_skip_flags( skip_flags );

    ACTORS( (alice)(carol) )
    generate_block();
    vest( "initminer", "alice", ASSET( "10.000 TESTS" ) );
    vest( "initminer", "carol", ASSET( "10.000 TESTS" ) );
    generate_block();
    //see explanation at the end of rc_pending_data_reset test
    //note that we don't actually have access to data on transaction but whole block, so it is
    //important for this test to always use just one transaction and fully reset before next one
    generate_block();

    const auto& pending_data = db->get< rc_pending_data >();

    BOOST_TEST_MESSAGE( "Testing when related witness does not exist before transaction" );
    signed_transaction tx;
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    witness_update_operation witness;
    witness.owner = "alice";
    witness.url = "https://alice.wonder.land/my.cat";
    witness.block_signing_key = generate_private_key( "alice_witness" ).get_public_key();
    witness.props.account_creation_fee = legacy_hive_asset::from_amount( HIVE_MIN_ACCOUNT_CREATION_FEE );
    witness.props.maximum_block_size = HIVE_MIN_BLOCK_SIZE_LIMIT * 2;
    witness.props.hbd_interest_rate = 0;
    witness.fee = asset( 0, HIVE_SYMBOL );
    tx.operations.push_back( witness );
    witness.url = "https://alice.has.cat";
    tx.operations.push_back( witness );
    push_transaction( tx, alice_private_key );
    tx.clear();
    auto alice_state_usage = pending_data.get_pending_usage()[ resource_state_bytes ];
    generate_block();
    generate_block();

    witness.owner = "carol";
    witness.url = "blocks@north.carolina";
    witness.block_signing_key = generate_private_key( "carol_witness" ).get_public_key();
    witness.props.account_creation_fee = legacy_hive_asset::from_amount( HIVE_MAX_ACCOUNT_CREATION_FEE / 2 );
    witness.props.maximum_block_size = HIVE_MAX_BLOCK_SIZE;
    witness.props.hbd_interest_rate = 30 * HIVE_1_PERCENT;
    witness.fee = asset( 100, HIVE_SYMBOL );
    push_transaction( witness, carol_private_key );
    auto carol_state_usage = pending_data.get_pending_usage()[ resource_state_bytes ];
    generate_block();
    generate_block();

    //both witnesses take the same state space, even though alice had hers built with two operations
    //it wouldn't calculate properly if visitor was run in pre-apply transaction instead of pre-apply
    //operation (second witness update for alice would not yet see state resulting from first update)
    BOOST_REQUIRE_EQUAL( alice_state_usage, carol_state_usage );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(rc_exception_during_modify)
{
  bool expected_exception_found = false;

  try
  {
    BOOST_TEST_MESSAGE("Testing exception throw during rc_account modify");
    inject_hardfork(HIVE_BLOCKCHAIN_VERSION.minor_v());
    auto skip_flags = rc_plugin->get_rc_plugin_skip_flags();
    skip_flags.skip_reject_not_enough_rc = 0;
    skip_flags.skip_reject_unknown_delta_vests = 0;
    rc_plugin->set_rc_plugin_skip_flags(skip_flags);

    ACTORS((dave))
    generate_block();
    vest("initminer", "dave", ASSET("70000.000 TESTS")); //<- change that amount to tune RC cost
    fund("dave", ASSET("1000.000 TESTS"));

    generate_block();

    const auto& dave_rc = db->get< rc_account_object, by_name >("dave");

    BOOST_TEST_MESSAGE("Clear RC on dave");
    clear_mana(db_plugin, dave_rc);

    transfer_operation transfer;
    transfer.from = "dave";
    transfer.to = "initminer";
    transfer.amount = ASSET("0.001 TESTS ");
    transfer.memo = "test";

    signed_transaction tx;
    tx.set_expiration(db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION);
    tx.operations.push_back(transfer);

    try
    {
      BOOST_TEST_MESSAGE("Attempting to push transaction");
      push_transaction(tx, dave_private_key);
    }
    catch(const hive::chain::not_enough_rc_exception& e)
    {
      BOOST_TEST_MESSAGE("Caught exception...");
      const auto& saved_log = e.get_log();

      for(const auto& msg : saved_log)
      {
        fc::variant_object data = msg.get_data();
        if(data.contains("tx_info"))
        {
          expected_exception_found =  true;
          const fc::variant_object& tx_info_data = data["tx_info"].get_object();
          BOOST_REQUIRE_EQUAL( tx_info_data[ "payer" ].as_string(), "dave" );
          break;
        }
      }
    }

    /// Find dave RC account once again and verify pointers to chain object (they should match)
    const auto* dave_rc2 = db->find< rc_account_object, by_name >("dave");
    BOOST_REQUIRE_EQUAL(&dave_rc, dave_rc2);
  }
  FC_LOG_AND_RETHROW()
  
  BOOST_REQUIRE_EQUAL(expected_exception_found, true);
}

BOOST_AUTO_TEST_SUITE_END()

#endif
