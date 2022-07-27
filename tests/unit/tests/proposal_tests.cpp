#include <boost/test/unit_test.hpp>

#include <hive/chain/hive_fwd.hpp>

#include <hive/protocol/exceptions.hpp>
#include <hive/protocol/hardfork.hpp>
#include <hive/protocol/dhf_operations.hpp>

#include <hive/chain/database.hpp>
#include <hive/chain/database_exceptions.hpp>
#include <hive/chain/hive_objects.hpp>

#include <hive/chain/util/reward.hpp>

#include <hive/plugins/rc/rc_objects.hpp>
#include <hive/plugins/rc/resource_count.hpp>

#include <hive/chain/dhf_objects.hpp>
#include <hive/chain/util/dhf_processor.hpp>

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

template< typename PROPOSAL_IDX >
int64_t calc_proposals( const PROPOSAL_IDX& proposal_idx, const std::vector< int64_t >& proposals_id )
{
  auto cnt = 0;
  for( auto pid : proposals_id )
    cnt += proposal_idx.find( pid ) != proposal_idx.end() ? 1 : 0;
  return cnt;
}

template< typename PROPOSAL_IDX >
uint64_t calc_total_votes( const PROPOSAL_IDX& proposal_idx, uint64_t proposal_id )
{
  auto found = proposal_idx.find( proposal_id );

  if( found == proposal_idx.end() )
    return 0;
  else
    return found->total_votes;
}

template< typename PROPOSAL_VOTE_IDX >
int64_t calc_proposal_votes( const PROPOSAL_VOTE_IDX& proposal_vote_idx, uint64_t proposal_id )
{
  auto cnt = 0;
  auto found = proposal_vote_idx.find( proposal_id );
  while( found != proposal_vote_idx.end() && found->proposal_id == proposal_id )
  {
    ++cnt;
    ++found;
  }
  return cnt;
}

template< typename PROPOSAL_VOTE_IDX >
int64_t calc_votes( const PROPOSAL_VOTE_IDX& proposal_vote_idx, const std::vector< int64_t >& proposals_id )
{
  auto cnt = 0;
  for( auto id : proposals_id )
    cnt += calc_proposal_votes( proposal_vote_idx, id );
  return cnt;
}

struct expired_account_notification_operation_visitor
{
  typedef account_name_type result_type;
  mutable std::string debug_type_name; //mangled visited type name - to make it easier to debug

  template<typename T>
  result_type operator()( const T& op ) const { debug_type_name = typeid( T ).name(); return account_name_type(); }
  result_type operator()( const expired_account_notification_operation& op ) const
  {
    debug_type_name = typeid( expired_account_notification_operation ).name(); return op.account;
  }
};

BOOST_FIXTURE_TEST_SUITE( proposal_tests, dhf_database_fixture )

BOOST_AUTO_TEST_CASE( inactive_proposals_have_votes )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: proposals before activation can have votes" );

    ACTORS( (alice)(bob)(carol)(dan) )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    //=====================preparing=====================
    uint64_t found_votes_00 = 0;
    uint64_t found_votes_01 = 0;

    auto creator = "alice";
    auto receiver = "bob";

    auto start_date = db->head_block_time();

    auto daily_pay = ASSET( "48.000 TBD" );
    auto hourly_pay = ASSET( "2.000 TBD" );

    FUND( creator, ASSET( "160.000 TESTS" ) );
    FUND( creator, ASSET( "80.000 TBD" ) );
    FUND( db->get_treasury_name(), ASSET( "5000.000 TBD" ) );

    auto voter_00 = "carol";
    auto voter_01 = "dan";

    vest(HIVE_INIT_MINER_NAME, voter_00, ASSET( "1.000 TESTS" ));
    vest(HIVE_INIT_MINER_NAME, voter_01, ASSET( "3.100 TESTS" ));

    //Due to the `delaying votes` algorithm, generate blocks for 30 days in order to activate whole votes' pool ( take a look at `HIVE_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS` )
    start_date += fc::seconds( HIVE_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS );
    generate_blocks( start_date );

    start_date = db->head_block_time();

    auto start_date_02 = start_date + fc::hours( 24 );

    auto end_date = start_date + fc::hours( 48 );
    auto end_date_02 = start_date_02 + fc::hours( 48 );
    //=====================preparing=====================

    const auto& proposal_idx = db->get_index< proposal_index, by_proposal_id >();

    //Needed basic operations
    int64_t id_proposal_00 = create_proposal( creator, receiver, start_date, end_date, daily_pay, alice_private_key );
    generate_block();
    int64_t id_proposal_01 = create_proposal( creator, receiver, start_date_02, end_date_02, daily_pay, alice_private_key );
    generate_block();

    /*
      proposal_00
      `start_date` = (now)          -> `end_date`  (now + 48h)

      proposal_01
      start_date` = (now + 24h) `   -> `end_date`  (now + 72h)
    */
    {
      found_votes_00 = calc_total_votes( proposal_idx, id_proposal_00 );
      found_votes_01 = calc_total_votes( proposal_idx, id_proposal_01 );
      BOOST_REQUIRE_EQUAL( found_votes_00, 0u );
      BOOST_REQUIRE_EQUAL( found_votes_01, 0u );
    }

    vote_proposal( voter_00, { id_proposal_00 }, true/*approve*/, carol_private_key );
    generate_block();

    {
      found_votes_00 = calc_total_votes( proposal_idx, id_proposal_00 );
      found_votes_01 = calc_total_votes( proposal_idx, id_proposal_01 );
      BOOST_REQUIRE_EQUAL( found_votes_00, 0u ); //votes are only counted during maintenance periods (so we don't have to remove them when proxy is set)
      BOOST_REQUIRE_EQUAL( found_votes_01, 0u );
    }

    vote_proposal( voter_01, { id_proposal_01 }, true/*approve*/, dan_private_key );
    generate_block();

    {
      found_votes_00 = calc_total_votes( proposal_idx, id_proposal_00 );
      found_votes_01 = calc_total_votes( proposal_idx, id_proposal_01 );
      BOOST_REQUIRE_EQUAL( found_votes_00, 0u );
      BOOST_REQUIRE_EQUAL( found_votes_01, 0u ); //like above
    }

    //skipping interest generating is necessary
    transfer( HIVE_INIT_MINER_NAME, receiver, ASSET( "0.001 TBD" ));
    generate_block();
    transfer( HIVE_INIT_MINER_NAME, db->get_treasury_name(), ASSET( "0.001 TBD" ) );
    generate_block();

    const auto& dgpo = db->get_dynamic_global_properties();
    auto old_hbd_supply = dgpo.current_hbd_supply;


    const account_object& _creator = db->get_account( creator );
    const account_object& _receiver = db->get_account( receiver );
    const account_object& _voter_01 = db->get_account( voter_01 );
    const account_object& _treasury = db->get_treasury();

    {
      BOOST_TEST_MESSAGE( "---Payment---" );

      auto before_creator_hbd_balance = _creator.hbd_balance;
      auto before_receiver_hbd_balance = _receiver.hbd_balance;
      auto before_voter_01_hbd_balance = _voter_01.hbd_balance;
      auto before_treasury_hbd_balance = _treasury.hbd_balance;

      auto next_block = get_nr_blocks_until_maintenance_block();
      generate_blocks( next_block - 1 );
      generate_block();

      auto treasury_hbd_inflation = dgpo.current_hbd_supply - old_hbd_supply;
      auto after_creator_hbd_balance = _creator.hbd_balance;
      auto after_receiver_hbd_balance = _receiver.hbd_balance;
      auto after_voter_01_hbd_balance = _voter_01.hbd_balance;
      auto after_treasury_hbd_balance = _treasury.hbd_balance;

      BOOST_REQUIRE( before_creator_hbd_balance == after_creator_hbd_balance );
      BOOST_REQUIRE( before_receiver_hbd_balance == after_receiver_hbd_balance - hourly_pay );
      BOOST_REQUIRE( before_voter_01_hbd_balance == after_voter_01_hbd_balance );
      BOOST_REQUIRE( before_treasury_hbd_balance == after_treasury_hbd_balance - treasury_hbd_inflation + hourly_pay );
    }
    /*
      Reminder:

      proposal_00
      `start_date` = (now)          -> `end_date`  (now + 48h)

      proposal_01
      start_date` = (now + 24h) `   -> `end_date`  (now + 72h)
    */
    {
      //Passed ~1h - one reward for `proposal_00` was paid out
      found_votes_00 = calc_total_votes( proposal_idx, id_proposal_00 );
      found_votes_01 = calc_total_votes( proposal_idx, id_proposal_01 );
      BOOST_REQUIRE_GT( found_votes_00, 0u );
      BOOST_REQUIRE_GT( found_votes_01, 0u );
    }
    {
      auto time_movement = start_date + fc::hours( 24 );
      generate_blocks( time_movement );

      //Passed ~25h - both proposals have `total_votes`
      auto _found_votes_00 = calc_total_votes( proposal_idx, id_proposal_00 );
      auto _found_votes_01 = calc_total_votes( proposal_idx, id_proposal_01 );
      BOOST_REQUIRE_EQUAL( found_votes_00, _found_votes_00 );
      BOOST_REQUIRE_EQUAL( found_votes_01, _found_votes_01 );
    }
    {
      auto time_movement = start_date + fc::hours( 27 );
      generate_blocks( time_movement );

      //Passed ~28h - both proposals have `total_votes`
      auto _found_votes_00 = calc_total_votes( proposal_idx, id_proposal_00 );
      auto _found_votes_01 = calc_total_votes( proposal_idx, id_proposal_01 );
      BOOST_REQUIRE_EQUAL( found_votes_00, _found_votes_00 );
      BOOST_REQUIRE_EQUAL( found_votes_01, _found_votes_01 );
    }
    {
      auto time_movement = start_date + fc::hours( 47 );
      generate_blocks( time_movement );

      //Passed ~28h - both proposals have `total_votes`
      auto _found_votes_00 = calc_total_votes( proposal_idx, id_proposal_00 );
      auto _found_votes_01 = calc_total_votes( proposal_idx, id_proposal_01 );
      BOOST_REQUIRE_EQUAL( found_votes_00, _found_votes_00 );
      BOOST_REQUIRE_EQUAL( found_votes_01, _found_votes_01 );
    }
    {
      auto time_movement = start_date + fc::hours( 71 );
      generate_blocks( time_movement );

      //Passed ~28h - both proposals have `total_votes`
      auto _found_votes_00 = calc_total_votes( proposal_idx, id_proposal_00 );
      auto _found_votes_01 = calc_total_votes( proposal_idx, id_proposal_01 );
      BOOST_REQUIRE_EQUAL( found_votes_00, _found_votes_00 );
      BOOST_REQUIRE_EQUAL( found_votes_01, _found_votes_01 );
    }

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( db_remove_expired_governance_votes )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: db_remove_expired_governance_votes" );
    ACTORS( (acc1)(acc2)(acc3)(acc4)(acc5)(acc6)(acc7)(acc8)(accw)(accw2)(accp)(pxy) )
    fund( "acc1", 1000 ); vest( "acc1", 1000 );
    fund( "acc2", 2000 ); vest( "acc2", 2000 );
    fund( "acc3", 3000 ); vest( "acc3", 3000 );
    fund( "acc4", 4000 ); vest( "acc4", 4000 );
    fund( "acc5", 5000 ); vest( "acc5", 5000 );
    fund( "acc6", 6000 ); vest( "acc6", 6000 );
    fund( "acc7", 7000 ); vest( "acc7", 7000 );
    fund( "acc8", 8000 ); vest( "acc8", 8000 );

    generate_block();
    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    const fc::time_point_sec LAST_POSSIBLE_OLD_VOTE_EXPIRE_TS = HARDFORK_1_25_FIRST_GOVERNANCE_VOTE_EXPIRE_TIMESTAMP + HIVE_HARDFORK_1_25_MAX_OLD_GOVERNANCE_VOTE_EXPIRE_SHIFT;

    auto proposal_creator = "accp";
    FUND( proposal_creator, ASSET( "10000.000 TBD" ) );

    auto start_1 = db->head_block_time();
    auto start_2 = db->head_block_time() + fc::seconds(15);
    auto start_3 = db->head_block_time() + fc::seconds(30);

    auto end_1 = LAST_POSSIBLE_OLD_VOTE_EXPIRE_TS + fc::days((100));
    auto end_2 = LAST_POSSIBLE_OLD_VOTE_EXPIRE_TS + fc::days((101));
    auto end_3 = LAST_POSSIBLE_OLD_VOTE_EXPIRE_TS + fc::days((102));

    int64_t proposal_1 = create_proposal( proposal_creator, "acc1",  start_1,  end_1, asset( 100, HBD_SYMBOL ), accp_private_key );
    int64_t proposal_2 = create_proposal( proposal_creator, "acc2",  start_2,  end_2, asset( 100, HBD_SYMBOL ), accp_private_key );
    int64_t proposal_3 = create_proposal( proposal_creator, "acc3",  start_3,  end_3, asset( 100, HBD_SYMBOL ), accp_private_key );

    private_key_type accw_witness_key = generate_private_key( "accw_key" );
    witness_create( "accw", accw_private_key, "foo.bar", accw_witness_key.get_public_key(), 1000 );
    private_key_type accw_witness2_key = generate_private_key( "accw2_key" );
    witness_create( "accw2", accw2_private_key, "foo.bar", accw_witness2_key.get_public_key(), 1000 );

    //if we vote before hardfork 25
    generate_block();
    fc::time_point_sec hardfork_25_time(HIVE_HARDFORK_1_25_TIME);
    db_plugin->debug_update( [=]( database& db )
    {
      db.modify( db.get_dynamic_global_properties(), [=]( dynamic_global_property_object& gpo )
      {
        //fake timestamp of current block so we don't need to wait for creation of 39mln blocks in next line
        //even though it is skipping it still takes a lot of time, especially under debugger
        gpo.time = hardfork_25_time - fc::days( 202 );
      } );
    } );
    generate_blocks(hardfork_25_time - fc::days(201));
    BOOST_REQUIRE(db->head_block_time() < hardfork_25_time - fc::days(200));
    witness_vote("acc1", "accw2", acc1_private_key); //201 days before HF25
    generate_days_blocks(25);
    vote_proposal("acc2", {proposal_1}, true, acc2_private_key); //176 days before HF25
    generate_days_blocks(25);
    vote_proposal("acc2", {proposal_2}, true, acc2_private_key); //151 days before HF25
    generate_days_blocks(25);
    witness_vote("acc3", "accw", acc3_private_key); //126 days before HF25
    generate_days_blocks(25);
    vote_proposal("acc4", {proposal_2}, true, acc4_private_key); //101 days before HF25
    generate_days_blocks(25);
    vote_proposal("acc5", {proposal_3}, true, acc5_private_key); //76 days before HF25
    generate_days_blocks(25);
    proxy("acc6", "pxy", acc6_private_key); //51 days before HF25
    {
      time_point_sec acc_6_vote_expiration_ts_before_proxy_action = db->get_account( "acc6" ).get_governance_vote_expiration_ts();
      //being set as someone's proxy does not affect expiration
      BOOST_REQUIRE( db->get_account( "pxy" ).get_governance_vote_expiration_ts() == fc::time_point_sec::maximum() );
      generate_days_blocks(25);
      witness_vote("pxy", "accw2", pxy_private_key); //26 days before HF25
      time_point_sec acc_6_vote_expiration_ts_after_proxy_action = db->get_account( "acc6" ).get_governance_vote_expiration_ts();
      //governance action on a proxy does not affect expiration on account that uses that proxy...
      BOOST_REQUIRE( acc_6_vote_expiration_ts_before_proxy_action == acc_6_vote_expiration_ts_after_proxy_action );
      proxy("acc6", "", acc6_private_key); //26 days before HF25
      time_point_sec acc_6_vote_expiration_ts_after_proxy_removal = db->get_account( "acc6" ).get_governance_vote_expiration_ts();
      //...but clearing proxy does
      BOOST_REQUIRE( acc_6_vote_expiration_ts_after_proxy_removal == db->get_account( "pxy" ).get_governance_vote_expiration_ts() );
    }
    //unvoting proposal (even the one that the account did not vote for before) also resets expiration (same with witness, but you can't unvote witness you didn't vote for)
    vote_proposal("acc7", {proposal_2}, false, acc7_private_key); //26 days before HF25
    generate_block();

    {
      time_point_sec acc_1_vote_expiration_ts = db->get_account( "acc1" ).get_governance_vote_expiration_ts();
      time_point_sec acc_2_vote_expiration_ts = db->get_account( "acc2" ).get_governance_vote_expiration_ts();
      time_point_sec acc_3_vote_expiration_ts = db->get_account( "acc3" ).get_governance_vote_expiration_ts();
      time_point_sec acc_4_vote_expiration_ts = db->get_account( "acc4" ).get_governance_vote_expiration_ts();
      time_point_sec acc_5_vote_expiration_ts = db->get_account( "acc5" ).get_governance_vote_expiration_ts();
      time_point_sec acc_6_vote_expiration_ts = db->get_account( "acc6" ).get_governance_vote_expiration_ts();
      time_point_sec acc_7_vote_expiration_ts = db->get_account( "acc7" ).get_governance_vote_expiration_ts();
      time_point_sec pxy_vote_expiration_ts   = db->get_account( "pxy" ).get_governance_vote_expiration_ts();

      BOOST_REQUIRE(acc_1_vote_expiration_ts > HARDFORK_1_25_FIRST_GOVERNANCE_VOTE_EXPIRE_TIMESTAMP && acc_1_vote_expiration_ts <= LAST_POSSIBLE_OLD_VOTE_EXPIRE_TS);
      BOOST_REQUIRE(acc_2_vote_expiration_ts > HARDFORK_1_25_FIRST_GOVERNANCE_VOTE_EXPIRE_TIMESTAMP && acc_2_vote_expiration_ts <= LAST_POSSIBLE_OLD_VOTE_EXPIRE_TS);
      BOOST_REQUIRE(acc_3_vote_expiration_ts > HARDFORK_1_25_FIRST_GOVERNANCE_VOTE_EXPIRE_TIMESTAMP && acc_3_vote_expiration_ts <= LAST_POSSIBLE_OLD_VOTE_EXPIRE_TS);
      BOOST_REQUIRE(acc_4_vote_expiration_ts > HARDFORK_1_25_FIRST_GOVERNANCE_VOTE_EXPIRE_TIMESTAMP && acc_4_vote_expiration_ts <= LAST_POSSIBLE_OLD_VOTE_EXPIRE_TS);
      BOOST_REQUIRE(acc_5_vote_expiration_ts > HARDFORK_1_25_FIRST_GOVERNANCE_VOTE_EXPIRE_TIMESTAMP && acc_5_vote_expiration_ts <= LAST_POSSIBLE_OLD_VOTE_EXPIRE_TS);
      BOOST_REQUIRE(acc_6_vote_expiration_ts > HARDFORK_1_25_FIRST_GOVERNANCE_VOTE_EXPIRE_TIMESTAMP && acc_6_vote_expiration_ts <= LAST_POSSIBLE_OLD_VOTE_EXPIRE_TS);
      BOOST_REQUIRE(acc_7_vote_expiration_ts > HARDFORK_1_25_FIRST_GOVERNANCE_VOTE_EXPIRE_TIMESTAMP && acc_7_vote_expiration_ts <= LAST_POSSIBLE_OLD_VOTE_EXPIRE_TS);
      BOOST_REQUIRE(pxy_vote_expiration_ts > HARDFORK_1_25_FIRST_GOVERNANCE_VOTE_EXPIRE_TIMESTAMP && pxy_vote_expiration_ts <= LAST_POSSIBLE_OLD_VOTE_EXPIRE_TS );

      const auto& witness_votes = db->get_index<witness_vote_index,by_account_witness>();
      BOOST_REQUIRE(witness_votes.size() == 3);
      BOOST_REQUIRE(witness_votes.count("acc1") == 1);
      BOOST_REQUIRE(witness_votes.count("acc3") == 1);
      BOOST_REQUIRE(witness_votes.count("pxy") == 1);

      const auto& proposal_votes = db->get_index<proposal_vote_index, by_voter_proposal>();
      BOOST_REQUIRE(proposal_votes.size() == 4);
      BOOST_REQUIRE(proposal_votes.count("acc2") == 2);
      BOOST_REQUIRE(proposal_votes.count("acc4") == 1);
      BOOST_REQUIRE(proposal_votes.count("acc5") == 1);
    }

    //make all votes expired. Now vote expiration time is vote time + HIVE_GOVERNANCE_VOTE_EXPIRATION_PERIOD
    generate_blocks(LAST_POSSIBLE_OLD_VOTE_EXPIRE_TS);

    {
      BOOST_REQUIRE(db->get_account( "acc1" ).get_governance_vote_expiration_ts() == fc::time_point_sec::maximum());
      BOOST_REQUIRE(db->get_account( "acc2" ).get_governance_vote_expiration_ts() == fc::time_point_sec::maximum());
      BOOST_REQUIRE(db->get_account( "acc3" ).get_governance_vote_expiration_ts() == fc::time_point_sec::maximum());
      BOOST_REQUIRE(db->get_account( "acc4" ).get_governance_vote_expiration_ts() == fc::time_point_sec::maximum());
      BOOST_REQUIRE(db->get_account( "acc5" ).get_governance_vote_expiration_ts() == fc::time_point_sec::maximum());
      BOOST_REQUIRE(db->get_account( "acc6" ).get_governance_vote_expiration_ts() == fc::time_point_sec::maximum());
      BOOST_REQUIRE(db->get_account( "acc7" ).get_governance_vote_expiration_ts() == fc::time_point_sec::maximum());
      BOOST_REQUIRE(db->get_account( "acc8" ).get_governance_vote_expiration_ts() == fc::time_point_sec::maximum());

      const auto& witness_votes = db->get_index<witness_vote_index, by_account_witness>();
      BOOST_REQUIRE(witness_votes.empty());

      const auto& proposal_votes = db->get_index<proposal_vote_index, by_voter_proposal>();
      BOOST_REQUIRE(proposal_votes.empty());
    }

    witness_vote("acc1", "accw", acc1_private_key);
    witness_vote("acc1", "accw2", acc1_private_key);
    witness_vote("acc2", "accw", acc2_private_key);
    time_point_sec expected_expiration_time = db->head_block_time() + HIVE_GOVERNANCE_VOTE_EXPIRATION_PERIOD;

    //this proposal vote should be removed
    vote_proposal("acc3", {proposal_1}, true, acc3_private_key);
    generate_block();
    vote_proposal("acc3", {proposal_1}, false, acc3_private_key);
    time_point_sec expected_expiration_time_2 = db->head_block_time() + HIVE_GOVERNANCE_VOTE_EXPIRATION_PERIOD;

    //this witness vote should not be removed
    generate_days_blocks(1);
    witness_vote("acc8", "accw", acc8_private_key);
    generate_blocks( expected_expiration_time - fc::seconds( HIVE_BLOCK_INTERVAL ) );

    {
      BOOST_REQUIRE(db->get_account( "acc1" ).get_governance_vote_expiration_ts() == expected_expiration_time);
      BOOST_REQUIRE(db->get_account( "acc2" ).get_governance_vote_expiration_ts() == expected_expiration_time);
      BOOST_REQUIRE(db->get_account( "acc3" ).get_governance_vote_expiration_ts() == expected_expiration_time_2);
      BOOST_REQUIRE(db->get_account( "acc4" ).get_governance_vote_expiration_ts() == fc::time_point_sec::maximum());
      BOOST_REQUIRE(db->get_account( "acc5" ).get_governance_vote_expiration_ts() == fc::time_point_sec::maximum());
      BOOST_REQUIRE(db->get_account( "acc6" ).get_governance_vote_expiration_ts() == fc::time_point_sec::maximum());
      BOOST_REQUIRE(db->get_account( "acc7" ).get_governance_vote_expiration_ts() == fc::time_point_sec::maximum());
      BOOST_REQUIRE(db->get_account( "acc8" ).get_governance_vote_expiration_ts() == expected_expiration_time_2 + fc::days(1));

      const auto& witness_votes = db->get_index<witness_vote_index, by_account_witness>();
      BOOST_REQUIRE(witness_votes.count("acc1") == 2);
      BOOST_REQUIRE(witness_votes.count("acc2") == 1);
      BOOST_REQUIRE(witness_votes.count("acc8") == 1);
      BOOST_REQUIRE(witness_votes.size() == 4);

      const auto& proposal_votes = db->get_index<proposal_vote_index, by_voter_proposal>();
      BOOST_REQUIRE(proposal_votes.empty());
    }
    generate_block();
    {
      BOOST_REQUIRE(db->get_account( "acc1" ).get_governance_vote_expiration_ts() == fc::time_point_sec::maximum());
      BOOST_REQUIRE(db->get_account( "acc2" ).get_governance_vote_expiration_ts() == fc::time_point_sec::maximum());
      BOOST_REQUIRE(db->get_account( "acc3" ).get_governance_vote_expiration_ts() == expected_expiration_time_2);
      BOOST_REQUIRE(db->get_account( "acc8" ).get_governance_vote_expiration_ts() == expected_expiration_time_2 + fc::days(1));
    }
    generate_block();
    {
      BOOST_REQUIRE(db->get_account( "acc3" ).get_governance_vote_expiration_ts() == fc::time_point_sec::maximum());
      BOOST_REQUIRE(db->get_account( "acc8" ).get_governance_vote_expiration_ts() == expected_expiration_time_2 + fc::days(1));

      const auto& witness_votes = db->get_index<witness_vote_index,by_account_witness>();
      BOOST_REQUIRE(witness_votes.count("acc8") == 1);
      BOOST_REQUIRE(witness_votes.size() == 1 );
    }
    generate_days_blocks(1, false);
    {
      BOOST_REQUIRE(db->get_account( "acc8" ).get_governance_vote_expiration_ts() == fc::time_point_sec::maximum());
      const auto& witness_votes = db->get_index<witness_vote_index,by_account_witness>();
      BOOST_REQUIRE(witness_votes.empty());
    }

    vote_proposal("acc3", {proposal_1}, true, acc3_private_key);
    vote_proposal("acc3", {proposal_2}, true, acc3_private_key);
    vote_proposal("acc4", {proposal_1}, true, acc4_private_key);
    expected_expiration_time = db->head_block_time() + HIVE_GOVERNANCE_VOTE_EXPIRATION_PERIOD;

    //in this case we should have 0 witness votes but expiration period should be updated.
    witness_vote("acc2", "accw", acc2_private_key);
    witness_vote("acc2", "accw2", acc2_private_key);
    witness_vote("acc6", "accw", acc6_private_key);
    generate_block();
    witness_vote("acc2", "accw", acc2_private_key, false);
    witness_vote("acc2", "accw2", acc2_private_key, false);
    witness_vote("acc6", "accw", acc6_private_key, false);
    expected_expiration_time_2 = db->head_block_time() + HIVE_GOVERNANCE_VOTE_EXPIRATION_PERIOD;

    //this proposal vote should not be removed
    generate_days_blocks(1);
    vote_proposal("acc8", {proposal_3}, true, acc8_private_key);

    generate_blocks( expected_expiration_time - fc::seconds( HIVE_BLOCK_INTERVAL ) );

    {
      BOOST_REQUIRE(db->get_account( "acc1" ).get_governance_vote_expiration_ts() == fc::time_point_sec::maximum());
      BOOST_REQUIRE(db->get_account( "acc2" ).get_governance_vote_expiration_ts() == expected_expiration_time_2);
      BOOST_REQUIRE(db->get_account( "acc3" ).get_governance_vote_expiration_ts() == expected_expiration_time);
      BOOST_REQUIRE(db->get_account( "acc4" ).get_governance_vote_expiration_ts() == expected_expiration_time);
      BOOST_REQUIRE(db->get_account( "acc5" ).get_governance_vote_expiration_ts() == fc::time_point_sec::maximum());
      BOOST_REQUIRE(db->get_account( "acc6" ).get_governance_vote_expiration_ts() == expected_expiration_time_2);
      BOOST_REQUIRE(db->get_account( "acc7" ).get_governance_vote_expiration_ts() == fc::time_point_sec::maximum());
      BOOST_REQUIRE(db->get_account( "acc8" ).get_governance_vote_expiration_ts() == expected_expiration_time_2 + fc::days(1));

      const auto& witness_votes = db->get_index<witness_vote_index, by_account_witness>();
      BOOST_REQUIRE(witness_votes.empty());

      const auto& proposal_votes = db->get_index<proposal_vote_index, by_voter_proposal>();
      BOOST_REQUIRE(proposal_votes.count("acc3") == 2);
      BOOST_REQUIRE(proposal_votes.count("acc4") == 1);
      BOOST_REQUIRE(proposal_votes.count("acc8") == 1);
      BOOST_REQUIRE(proposal_votes.size() == 4);
    }
    generate_block();
    {
      BOOST_REQUIRE(db->get_account( "acc2" ).get_governance_vote_expiration_ts() == expected_expiration_time_2);
      BOOST_REQUIRE(db->get_account( "acc3" ).get_governance_vote_expiration_ts() == fc::time_point_sec::maximum());
      BOOST_REQUIRE(db->get_account( "acc4" ).get_governance_vote_expiration_ts() == fc::time_point_sec::maximum());
      BOOST_REQUIRE(db->get_account( "acc6" ).get_governance_vote_expiration_ts() == expected_expiration_time_2);
      BOOST_REQUIRE(db->get_account( "acc8" ).get_governance_vote_expiration_ts() == expected_expiration_time_2 + fc::days(1));
    }
    generate_block();
    {
      BOOST_REQUIRE(db->get_account( "acc2" ).get_governance_vote_expiration_ts() == fc::time_point_sec::maximum());
      BOOST_REQUIRE(db->get_account( "acc6" ).get_governance_vote_expiration_ts() == fc::time_point_sec::maximum());
      BOOST_REQUIRE(db->get_account( "acc8" ).get_governance_vote_expiration_ts() == expected_expiration_time_2 + fc::days(1));

      const auto& proposal_votes = db->get_index<proposal_vote_index, by_voter_proposal>();
      BOOST_REQUIRE(proposal_votes.count("acc8") == 1);
      BOOST_REQUIRE(proposal_votes.size() == 1);
    }
    generate_days_blocks(1, false);
    {
      BOOST_REQUIRE(db->get_account( "acc8" ).get_governance_vote_expiration_ts() == fc::time_point_sec::maximum());
      const auto& proposal_votes = db->get_index<proposal_vote_index, by_voter_proposal>();
      BOOST_REQUIRE(proposal_votes.empty());
    }

    witness_vote("acc1", "accw2", acc1_private_key);
    vote_proposal("acc4", {proposal_1}, true, acc4_private_key);
    witness_vote("acc5", "accw", acc5_private_key);
    witness_vote("acc5", "accw2", acc5_private_key);
    vote_proposal("acc6", {proposal_1}, true, acc6_private_key);
    vote_proposal("acc6", {proposal_2}, true, acc6_private_key);
    vote_proposal("acc6", {proposal_3}, true, acc6_private_key);
    vote_proposal("acc7", {proposal_1}, true, acc7_private_key);
    witness_vote("acc7", "accw", acc7_private_key);
    witness_vote("acc8", "accw", acc8_private_key);
    expected_expiration_time = db->head_block_time() + HIVE_GOVERNANCE_VOTE_EXPIRATION_PERIOD;
    generate_blocks( expected_expiration_time - fc::seconds( HIVE_BLOCK_INTERVAL ) );

    {
      BOOST_REQUIRE(db->get_account( "acc1" ).get_governance_vote_expiration_ts() == expected_expiration_time);
      BOOST_REQUIRE(db->get_account( "acc2" ).get_governance_vote_expiration_ts() == fc::time_point_sec::maximum());
      BOOST_REQUIRE(db->get_account( "acc3" ).get_governance_vote_expiration_ts() == fc::time_point_sec::maximum());
      BOOST_REQUIRE(db->get_account( "acc4" ).get_governance_vote_expiration_ts() == expected_expiration_time);
      BOOST_REQUIRE(db->get_account( "acc5" ).get_governance_vote_expiration_ts() == expected_expiration_time);
      BOOST_REQUIRE(db->get_account( "acc6" ).get_governance_vote_expiration_ts() == expected_expiration_time);
      BOOST_REQUIRE(db->get_account( "acc7" ).get_governance_vote_expiration_ts() == expected_expiration_time);
      BOOST_REQUIRE(db->get_account( "acc8" ).get_governance_vote_expiration_ts() == expected_expiration_time);

      const auto& witness_votes = db->get_index<witness_vote_index, by_account_witness>();
      BOOST_REQUIRE(witness_votes.count("acc1") == 1);
      BOOST_REQUIRE(witness_votes.count("acc2") == 0);
      BOOST_REQUIRE(witness_votes.count("acc3") == 0);
      BOOST_REQUIRE(witness_votes.count("acc4") == 0);
      BOOST_REQUIRE(witness_votes.count("acc5") == 2);
      BOOST_REQUIRE(witness_votes.count("acc6") == 0);
      BOOST_REQUIRE(witness_votes.count("acc7") == 1);
      BOOST_REQUIRE(witness_votes.count("acc8") == 1);
      BOOST_REQUIRE(witness_votes.size() == 5);

      const auto& proposal_votes = db->get_index<proposal_vote_index, by_voter_proposal>();
      BOOST_REQUIRE(proposal_votes.count("acc1") == 0);
      BOOST_REQUIRE(proposal_votes.count("acc2") == 0);
      BOOST_REQUIRE(proposal_votes.count("acc3") == 0);
      BOOST_REQUIRE(proposal_votes.count("acc4") == 1);
      BOOST_REQUIRE(proposal_votes.count("acc5") == 0);
      BOOST_REQUIRE(proposal_votes.count("acc6") == 3);
      BOOST_REQUIRE(proposal_votes.count("acc7") == 1);
      BOOST_REQUIRE(proposal_votes.count("acc8") == 0);
      BOOST_REQUIRE(proposal_votes.size() == 5);
    }
    generate_block();
    {
      BOOST_REQUIRE(db->get_account( "acc1" ).get_governance_vote_expiration_ts() == fc::time_point_sec::maximum());
      BOOST_REQUIRE(db->get_account( "acc2" ).get_governance_vote_expiration_ts() == fc::time_point_sec::maximum());
      BOOST_REQUIRE(db->get_account( "acc3" ).get_governance_vote_expiration_ts() == fc::time_point_sec::maximum());
      BOOST_REQUIRE(db->get_account( "acc4" ).get_governance_vote_expiration_ts() == fc::time_point_sec::maximum());
      BOOST_REQUIRE(db->get_account( "acc5" ).get_governance_vote_expiration_ts() == fc::time_point_sec::maximum());
      BOOST_REQUIRE(db->get_account( "acc6" ).get_governance_vote_expiration_ts() == fc::time_point_sec::maximum());
      BOOST_REQUIRE(db->get_account( "acc7" ).get_governance_vote_expiration_ts() == fc::time_point_sec::maximum());
      BOOST_REQUIRE(db->get_account( "acc8" ).get_governance_vote_expiration_ts() == fc::time_point_sec::maximum());

      const auto& witness_votes = db->get_index<witness_vote_index, by_account_witness>();
      BOOST_REQUIRE(witness_votes.empty());

      const auto& proposal_votes = db->get_index<proposal_vote_index, by_voter_proposal>();
      BOOST_REQUIRE(proposal_votes.empty());

      // first one is producer_reward_operation, later we should have expired_account_notification_operation
      const auto& last_operations = get_last_operations(6);
      BOOST_REQUIRE(last_operations[0].get<expired_account_notification_operation>().account == "acc8");
      BOOST_REQUIRE(last_operations[1].get<expired_account_notification_operation>().account == "acc7");
      BOOST_REQUIRE(last_operations[2].get<expired_account_notification_operation>().account == "acc6");
      BOOST_REQUIRE(last_operations[3].get<expired_account_notification_operation>().account == "acc5");
      BOOST_REQUIRE(last_operations[4].get<expired_account_notification_operation>().account == "acc4");
      BOOST_REQUIRE(last_operations[5].get<expired_account_notification_operation>().account == "acc1");

      BOOST_REQUIRE(db->get_account( "acc1" ).witnesses_voted_for == 0);
      BOOST_REQUIRE(db->get_account( "acc2" ).witnesses_voted_for == 0);
      BOOST_REQUIRE(db->get_account( "acc3" ).witnesses_voted_for == 0);
      BOOST_REQUIRE(db->get_account( "acc4" ).witnesses_voted_for == 0);
      BOOST_REQUIRE(db->get_account( "acc5" ).witnesses_voted_for == 0);
      BOOST_REQUIRE(db->get_account( "acc6" ).witnesses_voted_for == 0);
      BOOST_REQUIRE(db->get_account( "acc7" ).witnesses_voted_for == 0);
      BOOST_REQUIRE(db->get_account( "acc8" ).witnesses_voted_for == 0);

      time_point_sec first_expiring_ts = db->get_index<account_index, by_governance_vote_expiration_ts>().begin()->get_governance_vote_expiration_ts();
      BOOST_REQUIRE(first_expiring_ts == fc::time_point_sec::maximum());
    }

    generate_block();

    //Check if proxy removing works well. acc1 is proxy, acc2 is grandparent proxy
    proxy("acc3", "acc1", acc3_private_key);
    proxy("acc4", "acc1", acc4_private_key);
    generate_blocks(db->head_block_time() + HIVE_GOVERNANCE_VOTE_EXPIRATION_PERIOD);
    proxy("acc5", "acc1", acc5_private_key);
    proxy("acc1", "acc2", acc1_private_key);

    generate_blocks(2);
    BOOST_REQUIRE(!db->get_account("acc3").has_proxy());
    BOOST_REQUIRE(!db->get_account("acc4").has_proxy());
    BOOST_REQUIRE(db->get_account("acc5").get_proxy() == db->get_account("acc1").get_id());
    BOOST_REQUIRE(db->get_account("acc1").get_proxy() == db->get_account("acc2").get_id());
    BOOST_REQUIRE(db->get_account("acc1").proxied_vsf_votes_total() == db->get_account("acc5").get_real_vesting_shares());
    BOOST_REQUIRE(db->get_account("acc2").proxied_vsf_votes_total() == (db->get_account("acc1").get_real_vesting_shares() + db->get_account("acc5").get_real_vesting_shares()));

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}
/*
BOOST_AUTO_TEST_CASE( proposals_with_decline_voting_rights )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: proposals_with_decline_voting_rights" );
    ACTORS((acc1)(acc2)(accp)(dwr))
    fund( "dwr", 1000 ); vest( "dwr", 1000 );

    generate_block();
    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    const fc::time_point_sec LAST_POSSIBLE_OLD_VOTE_EXPIRE_TS = HARDFORK_1_25_FIRST_GOVERNANCE_VOTE_EXPIRE_TIMESTAMP + HIVE_HARDFORK_1_25_MAX_OLD_GOVERNANCE_VOTE_EXPIRE_SHIFT;
    const fc::time_point_sec hardfork_25_time( HIVE_HARDFORK_1_25_TIME );

    FUND( "accp", ASSET( "10000.000 TBD" ) );
    int64_t proposal_1 = create_proposal( "accp", "acc1", hardfork_25_time - fc::days( 50 ), LAST_POSSIBLE_OLD_VOTE_EXPIRE_TS + fc::days( 50 ), asset( 100, HBD_SYMBOL ), accp_private_key );
    int64_t proposal_2 = create_proposal( "accp", "acc2", hardfork_25_time - fc::days( 150 ), LAST_POSSIBLE_OLD_VOTE_EXPIRE_TS + fc::days( 150 ), asset( 100, HBD_SYMBOL ), accp_private_key );
    generate_block();

    db_plugin->debug_update( [=]( database& db )
    {
      db.modify( db.get_dynamic_global_properties(), [=]( dynamic_global_property_object& gpo )
      {
        //fake timestamp of current block so we don't need to wait for creation of 39mln blocks in next line
        //even though it is skipping it still takes a lot of time, especially under debugger
        gpo.time = hardfork_25_time - fc::days( 202 );
      } );
    } );
    generate_blocks( hardfork_25_time - fc::days( 201 ) );
    BOOST_REQUIRE( db->head_block_time() < hardfork_25_time - fc::days( 200 ) );

    vote_proposal( "dwr", { proposal_1, proposal_2 }, true, dwr_private_key );
    generate_blocks( hardfork_25_time - fc::days( 100 ) );
    //TODO: check balance of acc1 (0) and acc2 (50 days worth of proposal pay)

    {
      signed_transaction tx;
      decline_voting_rights_operation op;
      op.account = "dwr";
      tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
      tx.operations.push_back( op );
      push_transaction( tx, dwr_private_key, 0 );
      generate_block();
      time_point_sec dwr_vote_expiration_ts = db->get_account( "dwr" ).get_governance_vote_expiration_ts();
      //it takes only 60 seconds in testnet to finish declining, but it is not finished yet
      BOOST_REQUIRE( dwr_vote_expiration_ts > HARDFORK_1_25_FIRST_GOVERNANCE_VOTE_EXPIRE_TIMESTAMP && dwr_vote_expiration_ts <= LAST_POSSIBLE_OLD_VOTE_EXPIRE_TS );
    }
    generate_blocks( hardfork_25_time - fc::days( 25 ) );
    //TODO: check balance of acc1 (0) and acc2 (50 days worth of proposal pay, maybe more if it caught one more payout before decline finalized)

    //at this point dwr successfully declined voting rights (long ago) - his expiration should be set in stone even if he tries to clear his (nonexisting) votes
    BOOST_REQUIRE( db->get_account( "dwr" ).get_governance_vote_expiration_ts() == fc::time_point_sec::maximum() );
    vote_proposal( "dwr", { proposal_2 }, false, dwr_private_key );
    BOOST_REQUIRE( db->get_account( "dwr" ).get_governance_vote_expiration_ts() == fc::time_point_sec::maximum() );
    //TODO: decide if finalization of decline should remove existing proposal votes (and if so add tests on number of active votes)

    generate_blocks( LAST_POSSIBLE_OLD_VOTE_EXPIRE_TS );
    generate_block();

    generate_days_blocks( 25 );
    //TODO: check balance of acc1 and acc2 (no change since last time)
    BOOST_REQUIRE( db->get_account( "dwr" ).get_governance_vote_expiration_ts() == fc::time_point_sec::maximum() );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}
*/

BOOST_AUTO_TEST_CASE( db_remove_expired_governance_votes_threshold_exceeded )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: db_remove_expired_governance_votes when threshold stops processing" );

    ACTORS(
    (a00)(a01)(a02)(a03)(a04)(a05)(a06)(a07)(a08)(a09)
    (a10)(a11)(a12)(a13)(a14)(a15)(a16)(a17)(a18)(a19)
    (a20)(a21)(a22)(a23)(a24)(a25)(a26)(a27)(a28)(a29)
    (a30)(a31)(a32)(a33)(a34)(a35)(a36)(a37)(a38)(a39)
    (a40)(a41)(a42)(a43)(a44)(a45)(a46)(a47)(a48)(a49)
    (a50)(a51)(a52)(a53)(a54)(a55)(a56)(a57)(a58)(a59)
    //witnesses
    (w00)(w01)(w02)(w03)(w04)(w05)(w06)(w07)(w08)(w09)
    (w10)(w11)(w12)(w13)(w14)(w15)(w16)(w17)(w18)(w19)
    (w20)(w21)(w22)(w23)(w24)(w25)(w26)(w27)(w28)(w29)
    )

    struct initial_data
    {
      std::string account;
      fc::ecc::private_key key;
    };

    std::vector< initial_data > users = {
      {"a00", a00_private_key }, {"a01", a01_private_key }, {"a02", a02_private_key }, {"a03", a03_private_key }, {"a04", a04_private_key }, {"a05", a05_private_key }, {"a06", a06_private_key }, {"a07", a07_private_key }, {"a08", a08_private_key }, {"a09", a09_private_key },
      {"a10", a10_private_key }, {"a11", a11_private_key }, {"a12", a12_private_key }, {"a13", a13_private_key }, {"a14", a14_private_key }, {"a15", a15_private_key }, {"a16", a16_private_key }, {"a17", a17_private_key }, {"a18", a18_private_key }, {"a19", a19_private_key },
      {"a20", a20_private_key }, {"a21", a21_private_key }, {"a22", a22_private_key }, {"a23", a23_private_key }, {"a24", a24_private_key }, {"a25", a25_private_key }, {"a26", a26_private_key }, {"a27", a27_private_key }, {"a28", a28_private_key }, {"a29", a29_private_key },
      {"a30", a30_private_key }, {"a31", a31_private_key }, {"a32", a32_private_key }, {"a33", a33_private_key }, {"a34", a34_private_key }, {"a35", a35_private_key }, {"a36", a36_private_key }, {"a37", a37_private_key }, {"a38", a38_private_key }, {"a39", a39_private_key },
      {"a40", a40_private_key }, {"a41", a41_private_key }, {"a42", a42_private_key }, {"a43", a43_private_key }, {"a44", a44_private_key }, {"a45", a45_private_key }, {"a46", a46_private_key }, {"a47", a47_private_key }, {"a48", a48_private_key }, {"a49", a49_private_key },
      {"a50", a50_private_key }, {"a51", a51_private_key }, {"a52", a52_private_key }, {"a53", a53_private_key }, {"a54", a54_private_key }, {"a55", a55_private_key }, {"a56", a56_private_key }, {"a57", a57_private_key }, {"a58", a58_private_key }, {"a59", a59_private_key },
    };

    //HIVE_MAX_ACCOUNT_WITNESS_VOTES is 30 for now so only 30 witnesses.
    std::vector< initial_data > witnesses = {
      {"w00", w00_private_key }, {"w01", w01_private_key }, {"w02", w02_private_key }, {"w03", w03_private_key }, {"w04", w04_private_key }, {"w05", w05_private_key }, {"w06", w06_private_key }, {"w07", w07_private_key }, {"w08", w08_private_key }, {"w09", w09_private_key },
      {"w10", w10_private_key }, {"w11", w11_private_key }, {"w12", w12_private_key }, {"w13", w13_private_key }, {"w14", w14_private_key }, {"w15", w15_private_key }, {"w16", w16_private_key }, {"w17", w17_private_key }, {"w18", w18_private_key }, {"w19", w19_private_key },
      {"w20", w20_private_key }, {"w21", w21_private_key }, {"w22", w22_private_key }, {"w23", w23_private_key }, {"w24", w24_private_key }, {"w25", w25_private_key }, {"w26", w26_private_key }, {"w27", w27_private_key }, {"w28", w28_private_key }, {"w29", w29_private_key }
    };

    for (const auto& witness : witnesses)
    {
      private_key_type witness_key = generate_private_key( witness.account + "_key" );
      witness_create( witness.account, witness.key, "foo.bar", witness_key.get_public_key(), 1000 );
    }
    generate_block();

    const auto& proposal_vote_idx = db->get_index< proposal_vote_index, by_id >();
    const auto& witness_vote_idx = db->get_index< witness_vote_index, by_id >();
    const auto& account_idx = db->get_index<account_index, by_governance_vote_expiration_ts>();

    const fc::time_point_sec LAST_POSSIBLE_OLD_VOTE_EXPIRE_TS = HARDFORK_1_25_FIRST_GOVERNANCE_VOTE_EXPIRE_TIMESTAMP + HIVE_HARDFORK_1_25_MAX_OLD_GOVERNANCE_VOTE_EXPIRE_SHIFT;
    db_plugin->debug_update( [=]( database& db )
    {
      db.modify( db.get_dynamic_global_properties(), [=]( dynamic_global_property_object& gpo )
      {
        //fake timestamp of current block so we don't need to wait for creation of 40mln blocks in next line
        //even though it is skipping it still takes a lot of time, especially under debugger
        gpo.time = LAST_POSSIBLE_OLD_VOTE_EXPIRE_TS - fc::days( 1 );
      } );
    } );
    generate_blocks(LAST_POSSIBLE_OLD_VOTE_EXPIRE_TS);

    std::vector<int64_t> proposals;
    proposals.reserve( users.size() );
    time_point_sec proposals_start_time = db->head_block_time();
    time_point_sec proposals_end_time = proposals_start_time + fc::days(450);
    std::string receiver = db->get_treasury_name();

    for(const auto& user : users )
    {
      FUND( user.account, ASSET( "100000.000 TBD" ) );
      proposals.push_back(create_proposal( user.account, receiver,  proposals_start_time,  proposals_end_time, asset( 100, HBD_SYMBOL ), user.key));
    }

    generate_block();
    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    fc::time_point_sec expiration_time = db->head_block_time() + HIVE_GOVERNANCE_VOTE_EXPIRATION_PERIOD;
    for (const auto& user : users)
    {
      for (const auto proposal : proposals)
        vote_proposal(user.account, {proposal}, true, user.key);

      for (const auto& witness : witnesses)
        witness_vote(user.account, witness.account, user.key);

      generate_block();
    }

    size_t expected_votes=0, expected_witness_votes=0, expected_expirations=0;
    int i;
    auto check_vote_count = [&]()
    {
      auto found_votes = proposal_vote_idx.size();
      auto found_witness_votes = witness_vote_idx.size();
      size_t found_expirations = std::distance( account_idx.begin(), account_idx.upper_bound( expiration_time ) );
      BOOST_REQUIRE_EQUAL( found_votes, expected_votes );
      BOOST_REQUIRE_EQUAL( found_witness_votes, expected_witness_votes );
      BOOST_REQUIRE_EQUAL( found_expirations, expected_expirations );
    };

    //check that even though accounts expire and fail to clear before another account expires, it still clears in the end, just can take longer
    {
      auto threshold = db->get_remove_threshold();
      BOOST_REQUIRE_EQUAL( threshold, 20 );

      generate_blocks( expiration_time - fc::seconds( HIVE_BLOCK_INTERVAL ) ); //note that during skipping automated actions don't work

      expected_votes = 60 * 60;
      expected_witness_votes = 60 * 30;
      expected_expirations = 0;
      expiration_time = db->head_block_time() - fc::seconds(1);
      check_vote_count();

      auto userI = users.begin();
      i = users.size();
      while( expected_votes > 0 )
      {
        generate_block();
        expiration_time = db->head_block_time();
        if( i > 0 )
        {
          ++expected_expirations;
          --i;
        }
        expected_votes -= threshold; //not all proposal votes of "expired user" are removed due to limitation (user's proposal votes will be removed completely every 60/20 blocks)
        if( (expected_votes%60) == 0 )
        {
          --expected_expirations;
          expected_witness_votes -= 30; //witness votes are always removed completely because there is fixed max number, but we need to actually reach that point - it also moves to next user
          const auto& last_operations = get_last_operations( 1 );
          expired_account_notification_operation_visitor v;
          account_name_type acc_name = last_operations.back().visit( v );
          BOOST_REQUIRE( acc_name == userI->account );
          ++userI;
        }
        
        check_vote_count();
      }
    }

    generate_block();

    //revote, but in slightly different configuration
    //we will be filling 5 times 10 users expiring in the same time, each has 3 proposal votes, with exception of 30..39 range
    //however in the same time, with shift of 10, we will be also applying [multi-level] proxy; in the end we expect the following:
    //(note that establishing proxy clears witness votes but not proposal votes)
    //users[0] .. user[9] - vote for themselves (and become proxy for others)
    //users[10] .. user[19] - proxy to above (and have inactive proposal votes)
    //users[20] .. user[29] - proxy to above (and have inactive proposal votes)
    //users[30] .. user[39] - proxy to above
    //users[40] .. user[49] - vote for themselves
    //users[50] .. user[59] - don't vote at all
    expiration_time = db->head_block_time() + HIVE_GOVERNANCE_VOTE_EXPIRATION_PERIOD;
    {
      auto userI = users.begin();
      i = 0;
      while( i<50 )
      {
        if( i < 30 || ( i >= 40 && i < 50 ) )
          vote_proposal( userI->account, { proposals.front(), proposals[1], proposals.back() }, true, userI->key );

        for( const auto& witness : witnesses )
          witness_vote( userI->account, witness.account, userI->key );

        if( i >= 10 && i < 40 )
          proxy( userI->account, users[ i-10 ].account, userI->key );

        if( (++i % 10) == 0 )
          generate_block();
        ++userI;
      }
    }

    generate_blocks( expiration_time - fc::seconds(HIVE_BLOCK_INTERVAL) );
    //couple of accounts do governance action almost at the last moment (removing of proposal vote, for some that was even inactive, for some there was no such vote anymore - still counts)
    for ( i = 0; i < 6; ++i )
      vote_proposal( users[10*i].account, { proposals.front() }, false, users[10*i].key );

    //check multiple accounts expiring in the same time
    {
      expected_votes = 3 * 40 - 4;
      expected_witness_votes = 20 * 30;
      expected_expirations = 0;
      expiration_time = db->head_block_time();
      check_vote_count();

      generate_block(); //users 1..9 expire, but only 1..6 fully clear (2 votes from 7)
      expiration_time = db->head_block_time();
      expected_expirations += 9 - 6;
      expected_votes -= 20;
      expected_witness_votes -= 6 * 30;
      check_vote_count();

      generate_block(); //users 11..19 expire, but 7..9,11..14 clear (1 vote from 15)
      expiration_time = db->head_block_time();
      expected_expirations += 9 - 7;
      expected_votes -= 20;
      expected_witness_votes -= 3 * 30; //11..14 had proxy so no witness votes
      check_vote_count();

      generate_block(); //users 21..29 expire, but 15..19,21..22 clear (0 votes from 23)
      expiration_time = db->head_block_time();
      expected_expirations += 9 - 7;
      expected_votes -= 20;
      expected_witness_votes -= 0 * 30; //all cleared had proxy, no witness votes
      check_vote_count();

      generate_block(); //users 31..39 expire, but 23..28 clear (2 votes from 29)
      expiration_time = db->head_block_time();
      expected_expirations += 9 - 6;
      expected_votes -= 20;
      expected_witness_votes -= 0 * 30; //all cleared had proxy, no witness votes
      check_vote_count();

      generate_block(); //users 41..49 expire, but 29,31..39,41..46 clear (1 vote from 47)
      expiration_time = db->head_block_time();
      expected_expirations += 9 - 16;
      expected_votes -= 20;
      expected_witness_votes -= 6 * 30; //only 41..46 had witness votes
      check_vote_count();

      generate_block(); //no new users expire, 47..49 clear (unused limit is 12)
      expiration_time = db->head_block_time();
      expected_expirations = 0; //6 users are active (long expiration time), rest is fully expired
      expected_votes = 8; //0,10,20,40 have 2 votes active each
      expected_witness_votes = 2 * 30; //0,30 have 30 witness votes active each
      check_vote_count();
    }

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}


BOOST_AUTO_TEST_CASE( generating_payments )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: generating payments" );

    ACTORS( (alice)(bob)(carol) )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    //=====================preparing=====================
    auto creator = "alice";
    auto receiver = "bob";

    auto start_date = db->head_block_time();

    auto daily_pay = ASSET( "48.000 TBD" );
    auto hourly_pay = ASSET( "2.000 TBD" );

    FUND( creator, ASSET( "160.000 TESTS" ) );
    FUND( creator, ASSET( "80.000 TBD" ) );
    FUND( db->get_treasury_name(), ASSET( "5000.000 TBD" ) );

    auto voter_01 = "carol";

    vest(HIVE_INIT_MINER_NAME, voter_01, ASSET( "1.000 TESTS" ));

    //Due to the `delaying votes` algorithm, generate blocks for 30 days in order to activate whole votes' pool ( take a look at `HIVE_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS` )
    start_date += fc::seconds( HIVE_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS );
    generate_blocks( start_date );

    auto end_date = start_date + fc::days( 2 );
    //=====================preparing=====================

    //Needed basic operations
    int64_t id_proposal_00 = create_proposal( creator, receiver, start_date, end_date, daily_pay, alice_private_key );
    generate_blocks( 1 );

    vote_proposal( voter_01, { id_proposal_00 }, true/*approve*/, carol_private_key );
    generate_blocks( 1 );
    //skipping interest generating is necessary
    transfer( HIVE_INIT_MINER_NAME, receiver, ASSET( "0.001 TBD" ));
    generate_block( 5 );
    transfer( HIVE_INIT_MINER_NAME, db->get_treasury_name(), ASSET( "0.001 TBD" ) );
    generate_block( 5 );

    const auto& dgpo = db->get_dynamic_global_properties();
    auto old_hbd_supply = dgpo.get_current_hbd_supply();


    const account_object& _creator = db->get_account( creator );
    const account_object& _receiver = db->get_account( receiver );
    const account_object& _voter_01 = db->get_account( voter_01 );
    const account_object& _treasury = db->get_treasury();

    {
      BOOST_TEST_MESSAGE( "---Payment---" );

      auto before_creator_hbd_balance = _creator.get_hbd_balance();
      auto before_receiver_hbd_balance = _receiver.get_hbd_balance();
      auto before_voter_01_hbd_balance = _voter_01.get_hbd_balance();
      auto before_treasury_hbd_balance = _treasury.get_hbd_balance();

      auto next_block = get_nr_blocks_until_maintenance_block();
      generate_blocks( next_block - 1 );
      generate_blocks( 1 );

      auto treasury_hbd_inflation = dgpo.get_current_hbd_supply() - old_hbd_supply;
      auto after_creator_hbd_balance = _creator.get_hbd_balance();
      auto after_receiver_hbd_balance = _receiver.get_hbd_balance();
      auto after_voter_01_hbd_balance = _voter_01.get_hbd_balance();
      auto after_treasury_hbd_balance = _treasury.get_hbd_balance();

      BOOST_REQUIRE( before_creator_hbd_balance == after_creator_hbd_balance );
      BOOST_REQUIRE( before_receiver_hbd_balance == after_receiver_hbd_balance - hourly_pay );
      BOOST_REQUIRE( before_voter_01_hbd_balance == after_voter_01_hbd_balance );
      BOOST_REQUIRE( before_treasury_hbd_balance == after_treasury_hbd_balance - treasury_hbd_inflation + hourly_pay );
    }

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( generating_payments_01 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: generating payments" );

    ACTORS( (tester001)(tester002)(tester003)(tester004)(tester005) )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    //=====================preparing=====================
    const auto nr_proposals = 5;
    std::vector< int64_t > proposals_id;
    flat_map< std::string, asset > before_tbds;

    struct initial_data
    {
      std::string account;
      fc::ecc::private_key key;
    };

    std::vector< initial_data > inits = {
                          {"tester001", tester001_private_key },
                          {"tester002", tester002_private_key },
                          {"tester003", tester003_private_key },
                          {"tester004", tester004_private_key },
                          {"tester005", tester005_private_key },
                          };

    for( auto item : inits )
    {
      FUND( item.account, ASSET( "400.000 TESTS" ) );
      FUND( item.account, ASSET( "400.000 TBD" ) );
      vest(HIVE_INIT_MINER_NAME, item.account, ASSET( "300.000 TESTS" ));
    }

    auto start_date = db->head_block_time();
    const auto end_time_shift = fc::hours( 5 );

    //Due to the `delaying votes` algorithm, generate blocks for 30 days in order to activate whole votes' pool ( take a look at `HIVE_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS` )
    start_date += fc::seconds( HIVE_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS );
    generate_blocks( start_date );

    auto end_date = start_date + end_time_shift;

    auto daily_pay = ASSET( "24.000 TBD" );
    auto paid = ASSET( "5.000 TBD" );

    FUND( db->get_treasury_name(), ASSET( "5000000.000 TBD" ) );
    //=====================preparing=====================
    for( int32_t i = 0; i < nr_proposals; ++i )
    {
      auto item = inits[ i % inits.size() ];
      proposals_id.push_back( create_proposal( item.account, item.account, start_date, end_date, daily_pay, item.key ) );
      generate_block();
    }

    for( int32_t i = 0; i < nr_proposals; ++i )
    {
      auto item = inits[ i % inits.size() ];
      vote_proposal( item.account, proposals_id, true/*approve*/, item.key );
      generate_block();
    }

    for( auto item : inits )
    {
      const account_object& account = db->get_account( item.account );
      before_tbds[ item.account ] = account.get_hbd_balance();
    }

    generate_blocks( start_date + end_time_shift + fc::seconds( 10 ), false );

    for( auto item : inits )
    {
      const account_object& account = db->get_account( item.account );
      auto after_tbd = account.get_hbd_balance();
      auto before_tbd = before_tbds[ item.account ];
      BOOST_REQUIRE( before_tbd == after_tbd - paid );
    }

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( generating_payments_02 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: generating payments, but proposal are removed" );

    ACTORS(
          (a00)(a01)(a02)(a03)(a04)(a05)(a06)(a07)(a08)(a09)
          (a10)(a11)(a12)(a13)(a14)(a15)(a16)(a17)(a18)(a19)
          (a20)(a21)(a22)(a23)(a24)(a25)(a26)(a27)(a28)(a29)
          (a30)(a31)(a32)(a33)(a34)(a35)(a36)(a37)(a38)(a39)
          (a40)(a41)(a42)(a43)(a44)(a45)(a46)(a47)(a48)(a49)
        )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    //=====================preparing=====================
    flat_map< std::string, asset > before_tbds;

    struct initial_data
    {
      std::string account;
      fc::ecc::private_key key;
    };

    std::vector< initial_data > inits = {
      {"a00", a00_private_key }, {"a01", a01_private_key }, {"a02", a02_private_key }, {"a03", a03_private_key }, {"a04", a04_private_key },
      {"a05", a05_private_key }, {"a06", a06_private_key }, {"a07", a07_private_key }, {"a08", a08_private_key }, {"a09", a09_private_key },
      {"a10", a10_private_key }, {"a11", a11_private_key }, {"a12", a12_private_key }, {"a13", a13_private_key }, {"a14", a14_private_key },
      {"a15", a15_private_key }, {"a16", a16_private_key }, {"a17", a17_private_key }, {"a18", a18_private_key }, {"a19", a19_private_key },
      {"a20", a20_private_key }, {"a21", a21_private_key }, {"a22", a22_private_key }, {"a23", a23_private_key }, {"a24", a24_private_key },
      {"a25", a25_private_key }, {"a26", a26_private_key }, {"a27", a27_private_key }, {"a28", a28_private_key }, {"a29", a29_private_key },
      {"a30", a30_private_key }, {"a31", a31_private_key }, {"a32", a32_private_key }, {"a33", a33_private_key }, {"a34", a34_private_key },
      {"a35", a35_private_key }, {"a36", a36_private_key }, {"a37", a37_private_key }, {"a38", a38_private_key }, {"a39", a39_private_key },
      {"a40", a40_private_key }, {"a41", a41_private_key }, {"a42", a42_private_key }, {"a43", a43_private_key }, {"a44", a44_private_key },
      {"a45", a45_private_key }, {"a46", a46_private_key }, {"a47", a47_private_key }, {"a48", a48_private_key }, {"a49", a49_private_key }
    };

    for( auto item : inits )
    {
      FUND( item.account, ASSET( "400.000 TESTS" ) );
      FUND( item.account, ASSET( "400.000 TBD" ) );
      vest(HIVE_INIT_MINER_NAME, item.account, ASSET( "300.000 TESTS" ));
    }

    const auto& proposal_idx = db->get_index< proposal_index >().indices().get< by_proposal_id >();
    const auto& proposal_vote_idx = db->get_index< proposal_vote_index >().indices().get< by_proposal_voter >();

    auto start_date = db->head_block_time() + fc::hours( 2 );
    auto end_date = start_date + fc::days( 15 );

    const auto block_interval = fc::seconds( HIVE_BLOCK_INTERVAL );

    FUND( db->get_treasury_name(), ASSET( "5000000.000 TBD" ) );
    //=====================preparing=====================
    auto item_creator = inits[ 0 ];
    create_proposal( item_creator.account, item_creator.account, start_date, end_date, ASSET( "24.000 TBD" ), item_creator.key );
    generate_block();

    for( auto item : inits )
    {
      vote_proposal( item.account, {0}, true/*approve*/, item.key);
      generate_block();

      const account_object& account = db->get_account( item.account );
      before_tbds[ item.account ] = account.get_hbd_balance();
    }

    generate_blocks( start_date, false );
    generate_blocks( db->get_dynamic_global_properties().next_maintenance_time - block_interval, false );

    {
      remove_proposal( item_creator.account, {0}, item_creator.key );
      auto found_proposals = calc_proposals( proposal_idx, {0} );
      auto found_votes = calc_proposal_votes( proposal_vote_idx, 0 );
      BOOST_REQUIRE( found_proposals == 1 );
      BOOST_REQUIRE( found_votes == 30 );
    }

    {
      generate_block();
      auto found_proposals = calc_proposals( proposal_idx, {0} );
      auto found_votes = calc_proposal_votes( proposal_vote_idx, 0 );
      BOOST_REQUIRE( found_proposals == 1 );
      BOOST_REQUIRE( found_votes == 10 );
    }

    {
      generate_block();
      auto found_proposals = calc_proposals( proposal_idx, { 0 } );
      auto found_votes = calc_proposal_votes( proposal_vote_idx, 0 );
      BOOST_REQUIRE( found_proposals == 0 );
      BOOST_REQUIRE( found_votes == 0 );
    }

    for( auto item : inits )
    {
      const account_object& account = db->get_account( item.account );
      auto after_tbd = account.get_hbd_balance();
      auto before_tbd = before_tbds[ item.account ];
      BOOST_REQUIRE( before_tbd == after_tbd );
    }

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( generating_payments_03 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: generating payments" );

    std::string tester00_account = "tester00";
    std::string tester01_account = "tester01";
    std::string tester02_account = "tester02";

    ACTORS( (tester00)(tester01)(tester02) )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    //=====================preparing=====================
    std::vector< int64_t > proposals_id;
    flat_map< std::string, asset > before_tbds;

    flat_map< std::string, fc::ecc::private_key > inits;
    inits[ "tester00" ] = tester00_private_key;
    inits[ "tester01" ] = tester01_private_key;
    inits[ "tester02" ] = tester02_private_key;

    for( auto item : inits )
    {
      if( item.first == tester02_account )
      {
        FUND( item.first, ASSET( "41.000 TESTS" ) );
        FUND( item.first, ASSET( "41.000 TBD" ) );
        vest(HIVE_INIT_MINER_NAME, item.first, ASSET( "31.000 TESTS" ));
      }
      else
      {
        FUND( item.first, ASSET( "40.000 TESTS" ) );
        FUND( item.first, ASSET( "40.000 TBD" ) );
        vest(HIVE_INIT_MINER_NAME, item.first, ASSET( "30.000 TESTS" ));
      }
    }

    auto start_date = db->head_block_time();

    //Due to the `delaying votes` algorithm, generate blocks for 30 days in order to activate whole votes' pool ( take a look at `HIVE_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS` )
    start_date += fc::seconds( HIVE_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS );
    generate_blocks( start_date );
    generate_block();

    uint16_t interval = 0;
    std::vector< fc::microseconds > end_time_shift = { fc::hours( 1 ), fc::hours( 2 ), fc::hours( 3 ), fc::hours( 4 ) };
    auto end_date = start_date + end_time_shift[ 3 ];

    auto huge_daily_pay = ASSET( "50000001.000 TBD" );
    auto daily_pay = ASSET( "24.000 TBD" );

    FUND( db->get_treasury_name(), ASSET( "5000000.000 TBD" ) );
    //=====================preparing=====================
    uint16_t i = 0;
    for( auto item : inits )
    {
      auto _pay = ( item.first == tester02_account ) ? huge_daily_pay : daily_pay;
      proposals_id.push_back( create_proposal( item.first, item.first, start_date, end_date, _pay, item.second ) );
      generate_block();

      if( item.first == tester02_account )
        continue;

      vote_proposal( item.first, {i++}, true/*approve*/, item.second );
      generate_block();
    }

    for( auto item : inits )
    {
      const account_object& account = db->get_account( item.first );
      before_tbds[ item.first ] = account.get_hbd_balance();
    }

    auto payment_checker = [&]( const std::vector< asset >& payouts )
    {
      idump( (inits) );
      idump( (payouts) );
      uint16_t i = 0;
      for( const auto& item : inits )
      {
        const account_object& account = db->get_account( item.first );
        auto after_tbd = account.get_hbd_balance();
        auto before_tbd = before_tbds[ item.first ];
        idump( (before_tbd) );
        idump( (after_tbd) );
        idump( (payouts[i]) );
        //idump( (after_tbd - payouts[i++]) );
        BOOST_REQUIRE( before_tbd == after_tbd - payouts[i++] );
      }
    };

    /*
      Initial conditions.
        `tester00` has own proposal id = 0 : voted for it
        `tester01` has own proposal id = 1 : voted for it
        `tester02` has own proposal id = 2 : lack of votes
    */

    generate_blocks( start_date + end_time_shift[ interval++ ] + fc::seconds( 10 ), false );
    /*
      `tester00` - got payout
      `tester01` - got payout
      `tester02` - no payout, because of lack of votes
    */
    ilog("");
    payment_checker( { ASSET("1.000 TBD" ), ASSET( "1.000 TBD" ), ASSET( "0.000 TBD" ) } );

    {
      BOOST_TEST_MESSAGE( "Setting proxy. The account `tester01` don't want to vote. Every decision is made by account `tester00`" );
      proxy( tester01_account, tester00_account, inits[ tester01_account ] );
    }

    generate_blocks( start_date + end_time_shift[ interval++ ] + fc::seconds( 10 ), false );
    /*
      `tester00` - got payout
      `tester01` - no payout, because this account set proxy
      `tester02` - no payout, because of lack of votes
    */
    ilog("");
    payment_checker( { ASSET( "2.000 TBD" ), ASSET( "1.000 TBD" ), ASSET( "0.000 TBD" ) } );

    vote_proposal( tester02_account, {2}, true/*approve*/, inits[ tester02_account ] );
    generate_block();

    generate_blocks( start_date + end_time_shift[ interval++ ] + fc::seconds( 10 ), false );
    /*
      `tester00` - got payout, `tester02` has less votes than `tester00`
      `tester01` - no payout, because this account set proxy
      `tester02` - got payout, because voted for his proposal
    */
    ilog("");
    payment_checker( { ASSET( "3.000 TBD" ), ASSET( "1.000 TBD" ), ASSET( "2082.367 TBD" ) } );

    {
      BOOST_TEST_MESSAGE( "Proxy doesn't exist. Now proposal with id = 3 has the most votes. This proposal grabs all payouts." );
      proxy( tester01_account, "", inits[ tester01_account ] );
    }

    generate_blocks( start_date + end_time_shift[ interval++ ] + fc::seconds( 10 ), false );
    /*
      `tester00` - no payout, because is not enough money. `tester01` removed proxy and the most votes has `tester02`. Whole payout goes to `tester02`
      `tester01` - no payout, because is not enough money
      `tester02` - got payout, because voted for his proposal
    */
    ilog("");
    payment_checker( { ASSET( "3.000 TBD" ), ASSET( "1.000 TBD" ), ASSET( "4164.872 TBD" ) } );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( generating_payments_04 )
{
try
  {
    BOOST_TEST_MESSAGE( "Testing: payment truncating" );

    ACTORS( (alice)(bob)(carol)(dave) )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    //=====================preparing=====================
    auto creator = "alice";
    auto receiver = "bob";

    auto start_date = db->head_block_time();

    auto daily_pay = ASSET( "2378.447 TBD" );
    auto hourly_pay = ASSET( "99.101 TBD" ); //99.101958(3)

    FUND( creator, ASSET( "160.000 TESTS" ) );
    FUND( creator, ASSET( "80.000 TBD" ) );
    FUND( db->get_treasury_name(), ASSET( "500000.000 TBD" ) );

    auto voter_01 = "carol";

    vest(HIVE_INIT_MINER_NAME, voter_01, ASSET( "1.000 TESTS" ));

    //Due to the `delaying votes` algorithm, generate blocks for 30 days in order to activate whole votes' pool ( take a look at `HIVE_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS` )
    start_date += fc::seconds( HIVE_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS );
    generate_blocks( start_date );

    auto end_date = start_date + fc::days( 2 );
    //=====================preparing=====================

    //Needed basic operations
    int64_t id_proposal_00 = create_proposal( creator, receiver, start_date, end_date, daily_pay, alice_private_key );
    generate_block();

    vote_proposal( voter_01, { id_proposal_00 }, true/*approve*/, carol_private_key );
    generate_block();

    //skipping interest generating is necessary
    transfer( HIVE_INIT_MINER_NAME, receiver, ASSET( "0.001 TBD" ));
    generate_block();
    transfer( HIVE_INIT_MINER_NAME, db->get_treasury_name(), ASSET( "0.001 TBD" ) );
    generate_block();

    const auto& dgpo = db->get_dynamic_global_properties();
    auto old_hbd_supply = dgpo.get_current_hbd_supply();


    const account_object& _creator = db->get_account( creator );
    const account_object& _receiver = db->get_account( receiver );
    const account_object& _voter_01 = db->get_account( voter_01 );
    const account_object& _treasury = db->get_treasury();

    {
      BOOST_TEST_MESSAGE( "---Payment---" );

      auto before_creator_hbd_balance = _creator.get_hbd_balance();
      auto before_receiver_hbd_balance = _receiver.get_hbd_balance();
      auto before_voter_01_hbd_balance = _voter_01.get_hbd_balance();
      auto before_treasury_hbd_balance = _treasury.get_hbd_balance();

      auto next_block = get_nr_blocks_until_maintenance_block();
      generate_blocks( next_block - 1 );
      generate_block();

      auto treasury_hbd_inflation = dgpo.get_current_hbd_supply() - old_hbd_supply;
      auto after_creator_hbd_balance = _creator.get_hbd_balance();
      auto after_receiver_hbd_balance = _receiver.get_hbd_balance();
      auto after_voter_01_hbd_balance = _voter_01.get_hbd_balance();
      auto after_treasury_hbd_balance = _treasury.get_hbd_balance();

      BOOST_REQUIRE( before_creator_hbd_balance == after_creator_hbd_balance );

      BOOST_REQUIRE( before_receiver_hbd_balance == after_receiver_hbd_balance - hourly_pay );
      BOOST_REQUIRE( before_voter_01_hbd_balance == after_voter_01_hbd_balance );
      BOOST_REQUIRE( before_treasury_hbd_balance == after_treasury_hbd_balance - treasury_hbd_inflation + hourly_pay );
    }

    validate_database();
  }
FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( generating_payments_05 )
{
try
  {
    BOOST_TEST_MESSAGE( "Testing: payment not truncating" );

    ACTORS( (alice)(bob)(carol)(dave) )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    //=====================preparing=====================
    auto creator = "alice";
    auto receiver = "bob";

    auto start_date = db->head_block_time();

    auto daily_pay = ASSET( "2378.448 TBD" );
    auto hourly_pay = ASSET( "99.102 TBD" );

    FUND( creator, ASSET( "160.000 TESTS" ) );
    FUND( creator, ASSET( "80.000 TBD" ) );
    FUND( db->get_treasury_name(), ASSET( "500000.000 TBD" ) );

    auto voter_01 = "carol";

    vest(HIVE_INIT_MINER_NAME, voter_01, ASSET( "1.000 TESTS" ));

    //Due to the `delaying votes` algorithm, generate blocks for 30 days in order to activate whole votes' pool ( take a look at `HIVE_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS` )
    start_date += fc::seconds( HIVE_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS );
    generate_blocks( start_date );

    auto end_date = start_date + fc::days( 2 );
    //=====================preparing=====================

    //Needed basic operations
    int64_t id_proposal_00 = create_proposal( creator, receiver, start_date, end_date, daily_pay, alice_private_key );
    generate_block();

    vote_proposal( voter_01, { id_proposal_00 }, true/*approve*/, carol_private_key );
    generate_block();

    //skipping interest generating is necessary
    transfer( HIVE_INIT_MINER_NAME, receiver, ASSET( "0.001 TBD" ));
    generate_block();
    transfer( HIVE_INIT_MINER_NAME, db->get_treasury_name(), ASSET( "0.001 TBD" ) );
    generate_block();

    const auto& dgpo = db->get_dynamic_global_properties();
    auto old_hbd_supply = dgpo.get_current_hbd_supply();


    const account_object& _creator = db->get_account( creator );
    const account_object& _receiver = db->get_account( receiver );
    const account_object& _voter_01 = db->get_account( voter_01 );
    const account_object& _treasury = db->get_treasury();

    {
      BOOST_TEST_MESSAGE( "---Payment---" );

      auto before_creator_hbd_balance = _creator.get_hbd_balance();
      auto before_receiver_hbd_balance = _receiver.get_hbd_balance();
      auto before_voter_01_hbd_balance = _voter_01.get_hbd_balance();
      auto before_treasury_hbd_balance = _treasury.get_hbd_balance();

      auto next_block = get_nr_blocks_until_maintenance_block();
      generate_blocks( next_block - 1 );
      generate_block();

      auto treasury_hbd_inflation = dgpo.get_current_hbd_supply() - old_hbd_supply;
      auto after_creator_hbd_balance = _creator.get_hbd_balance();
      auto after_receiver_hbd_balance = _receiver.get_hbd_balance();
      auto after_voter_01_hbd_balance = _voter_01.get_hbd_balance();
      auto after_treasury_hbd_balance = _treasury.get_hbd_balance();

      BOOST_REQUIRE( before_creator_hbd_balance == after_creator_hbd_balance );
      BOOST_REQUIRE( before_receiver_hbd_balance == after_receiver_hbd_balance - hourly_pay );
      BOOST_REQUIRE( before_voter_01_hbd_balance == after_voter_01_hbd_balance );
      BOOST_REQUIRE( before_treasury_hbd_balance == after_treasury_hbd_balance - treasury_hbd_inflation + hourly_pay );
    }

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( expired_proposals_forbidden_voting)
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: when proposals are expired, then voting on such proposals are not allowed" );

    ACTORS( (alice)(bob)(carol)(carol1)(carol2) )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    //=====================preparing=====================
    auto creator = "alice";
    auto receiver = "bob";

    auto start_time = db->head_block_time();

    auto start_date_00 = start_time + fc::seconds( 30 );
    auto end_date_00 = start_time + fc::minutes( 100 );

    auto start_date_01 = start_time + fc::seconds( 40 );
    auto end_date_01 = start_time + fc::minutes( 300 );

    auto start_date_02 = start_time + fc::seconds( 50 );
    auto end_date_02 = start_time + fc::minutes( 200 );

    auto daily_pay = asset( 100, HBD_SYMBOL );

    FUND( creator, ASSET( "100.000 TBD" ) );
    //=====================preparing=====================

    int64_t id_proposal_00 = create_proposal( creator, receiver, start_date_00, end_date_00, daily_pay, alice_private_key );
    generate_block();

    int64_t id_proposal_01 = create_proposal( creator, receiver, start_date_01, end_date_01, daily_pay, alice_private_key );
    generate_block();

    int64_t id_proposal_02 = create_proposal( creator, receiver, start_date_02, end_date_02, daily_pay, alice_private_key );
    generate_block();

    {
      vote_proposal( "carol", { id_proposal_00, id_proposal_01, id_proposal_02 }, true/*approve*/, carol_private_key );
      generate_blocks( 1 );
      vote_proposal( "carol", { id_proposal_00, id_proposal_01, id_proposal_02 }, false/*approve*/, carol_private_key );
      generate_blocks( 1 );
    }
    start_time = db->head_block_time();
    {
      generate_blocks( start_time + fc::minutes( 110 ) );
      HIVE_REQUIRE_THROW( vote_proposal( "carol", { id_proposal_00 }, true/*approve*/, carol_private_key ), fc::exception);
      vote_proposal( "carol", { id_proposal_01 }, true/*approve*/, carol_private_key );
      vote_proposal( "carol", { id_proposal_02 }, true/*approve*/, carol_private_key );
      generate_blocks( 1 );
    }
    {
      generate_blocks( start_time + fc::minutes( 210 ) );
      HIVE_REQUIRE_THROW( vote_proposal( "carol1", { id_proposal_00 }, true/*approve*/, carol1_private_key ), fc::exception);
      vote_proposal( "carol1", { id_proposal_01 }, true/*approve*/, carol1_private_key );
      HIVE_REQUIRE_THROW( vote_proposal( "carol1", { id_proposal_02 }, true/*approve*/, carol1_private_key ), fc::exception);
      generate_blocks( 1 );
    }
    {
      generate_blocks( start_time + fc::minutes( 310 ) );
      HIVE_REQUIRE_THROW( vote_proposal( "carol2", { id_proposal_00 }, true/*approve*/, carol2_private_key ), fc::exception);
      HIVE_REQUIRE_THROW( vote_proposal( "carol2", { id_proposal_01 }, true/*approve*/, carol2_private_key ), fc::exception);
      HIVE_REQUIRE_THROW( vote_proposal( "carol2", { id_proposal_02 }, true/*approve*/, carol2_private_key ), fc::exception);
      generate_blocks( 1 );
    }

    BOOST_REQUIRE( exist_proposal( id_proposal_00 ) );
    BOOST_REQUIRE( exist_proposal( id_proposal_01 ) );
    BOOST_REQUIRE( exist_proposal( id_proposal_02 ) );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( proposals_maintenance)
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: removing inactive proposals" );

    ACTORS( (alice)(bob) )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    //=====================preparing=====================
    auto creator = "alice";
    auto receiver = "bob";

    auto start_time = db->head_block_time();

    auto start_date_00 = start_time + fc::seconds( 30 );
    auto end_date_00 = start_time + fc::minutes( 10 );

    auto start_date_01 = start_time + fc::seconds( 40 );
    auto end_date_01 = start_time + fc::minutes( 30 );

    auto start_date_02 = start_time + fc::seconds( 50 );
    auto end_date_02 = start_time + fc::minutes( 20 );

    auto daily_pay = asset( 100, HBD_SYMBOL );

    FUND( creator, ASSET( "100.000 TBD" ) );
    //=====================preparing=====================

    int64_t id_proposal_00 = create_proposal( creator, receiver, start_date_00, end_date_00, daily_pay, alice_private_key );
    generate_block();

    int64_t id_proposal_01 = create_proposal( creator, receiver, start_date_01, end_date_01, daily_pay, alice_private_key );
    generate_block();

    int64_t id_proposal_02 = create_proposal( creator, receiver, start_date_02, end_date_02, daily_pay, alice_private_key );
    generate_block();

    {
      BOOST_REQUIRE( exist_proposal( id_proposal_00 ) );
      BOOST_REQUIRE( exist_proposal( id_proposal_01 ) );
      BOOST_REQUIRE( exist_proposal( id_proposal_02 ) );

      generate_blocks( start_time + fc::seconds( HIVE_PROPOSAL_MAINTENANCE_CLEANUP ) );
      start_time = db->head_block_time();

      BOOST_REQUIRE( exist_proposal( id_proposal_00 ) );
      BOOST_REQUIRE( exist_proposal( id_proposal_01 ) );
      BOOST_REQUIRE( exist_proposal( id_proposal_02 ) );

      /*
            Take a look at comment in `dhf_processor::remove_proposals`
      */
      generate_blocks( start_time + fc::minutes( 11 ) );
      
      //BOOST_REQUIRE( !exist_proposal( id_proposal_00 ) ); //earlier
      BOOST_REQUIRE( exist_proposal( id_proposal_00 ) );    //now
      
      
      BOOST_REQUIRE( exist_proposal( id_proposal_01 ) );
      BOOST_REQUIRE( exist_proposal( id_proposal_02 ) );

      generate_blocks( start_time + fc::minutes( 21 ) );
      //BOOST_REQUIRE( !exist_proposal( id_proposal_00 ) ); //earlier
      BOOST_REQUIRE( exist_proposal( id_proposal_00 ) );    //now
      BOOST_REQUIRE( exist_proposal( id_proposal_01 ) );
      //BOOST_REQUIRE( !exist_proposal( id_proposal_02 ) ); //earlier
      BOOST_REQUIRE( exist_proposal( id_proposal_02 ) );    //now

      generate_blocks( start_time + fc::minutes( 31 ) );
      // BOOST_REQUIRE( !exist_proposal( id_proposal_00 ) ); //earlier
      // BOOST_REQUIRE( !exist_proposal( id_proposal_01 ) ); //earlier
      // BOOST_REQUIRE( !exist_proposal( id_proposal_02 ) ); //earlier
      BOOST_REQUIRE( exist_proposal( id_proposal_00 ) );    //now
      BOOST_REQUIRE( exist_proposal( id_proposal_01 ) );    //now
      BOOST_REQUIRE( exist_proposal( id_proposal_02 ) );    //now
    }

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}


BOOST_AUTO_TEST_CASE( proposal_object_apply )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: create_proposal_operation" );

    ACTORS( (alice)(bob) )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    auto fee = asset( HIVE_TREASURY_FEE, HBD_SYMBOL );

    auto creator = "alice";
    auto receiver = "bob";

    auto start_date = db->head_block_time() + fc::days( 1 );
    auto end_date = start_date + fc::days( 20 );

    auto daily_pay = asset( 100, HBD_SYMBOL );

    auto subject = "hello";
    auto permlink = "somethingpermlink";

    post_comment_with_block_generation(creator, permlink, "title", "body", "test", alice_private_key);

    FUND( creator, ASSET( "80.000 TBD" ) );

    signed_transaction tx;

    const account_object& before_treasury_account = db->get_treasury();
    const account_object& before_alice_account = db->get_account( creator );
    const account_object& before_bob_account = db->get_account( receiver );

    auto before_alice_hbd_balance = before_alice_account.hbd_balance;
    auto before_bob_hbd_balance = before_bob_account.hbd_balance;
    auto before_treasury_balance = before_treasury_account.hbd_balance;

    create_proposal_operation op;

    op.creator = creator;
    op.receiver = receiver;

    op.start_date = start_date;
    op.end_date = end_date;

    op.daily_pay = daily_pay;

    op.subject = subject;
    op.permlink = permlink;

    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    push_transaction( tx, alice_private_key, 0 );
    tx.operations.clear();
    tx.signatures.clear();

    {
      auto recent_ops = get_last_operations( 1 );
      auto fee_op = recent_ops.back().get< proposal_fee_operation >();
      BOOST_REQUIRE( fee_op.creator == creator );
      BOOST_REQUIRE( fee_op.treasury == db->get_treasury_name() );
      BOOST_REQUIRE( fee_op.fee == fee );
    }

    const auto& after_treasury_account = db->get_treasury();
    const account_object& after_alice_account = db->get_account( creator );
    const account_object& after_bob_account = db->get_account( receiver );

    auto after_alice_hbd_balance = after_alice_account.hbd_balance;
    auto after_bob_hbd_balance = after_bob_account.hbd_balance;
    auto after_treasury_balance = after_treasury_account.hbd_balance;

    BOOST_REQUIRE( before_alice_hbd_balance == after_alice_hbd_balance + fee );
    BOOST_REQUIRE( before_bob_hbd_balance == after_bob_hbd_balance );
    /// Fee shall be paid to treasury account.
    BOOST_REQUIRE(before_treasury_balance == after_treasury_balance - fee);

    const auto& proposal_idx = db->get_index< proposal_index >().indices().get< by_creator >();
    auto found = proposal_idx.find( creator );
    BOOST_REQUIRE( found != proposal_idx.end() );

    BOOST_REQUIRE( found->creator == creator );
    BOOST_REQUIRE( found->receiver == receiver );
    BOOST_REQUIRE( found->start_date == start_date );
    BOOST_REQUIRE( found->end_date == end_date );
    BOOST_REQUIRE( found->daily_pay == daily_pay );
    BOOST_REQUIRE( found->subject == subject );
    BOOST_REQUIRE( found->permlink == permlink );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( proposal_object_apply_fee_increase )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: create_proposal_operation with fee increase" );

    ACTORS( (alice)(bob) )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    auto creator = "alice";
    auto receiver = "bob";
    auto proposal_duration = 90;
    auto extra_days = proposal_duration - HIVE_PROPOSAL_FEE_INCREASE_DAYS <= 0 ? 0 : proposal_duration - HIVE_PROPOSAL_FEE_INCREASE_DAYS;

    auto start_date = db->head_block_time() + fc::days( 1 );
    auto end_date = start_date + fc::days( proposal_duration );
    auto fee = asset( HIVE_TREASURY_FEE + extra_days * HIVE_PROPOSAL_FEE_INCREASE_AMOUNT, HBD_SYMBOL );

    auto daily_pay = asset( 100, HBD_SYMBOL );

    auto subject = "hello";
    auto permlink = "somethingpermlink";

    post_comment_with_block_generation(creator, permlink, "title", "body", "test", alice_private_key);

    FUND( creator, ASSET( "80.000 TBD" ) );

    signed_transaction tx;

    const account_object& before_treasury_account = db->get_treasury();
    const account_object& before_alice_account = db->get_account( creator );
    const account_object& before_bob_account = db->get_account( receiver );

    auto before_alice_hbd_balance = before_alice_account.get_hbd_balance();
    auto before_bob_hbd_balance = before_bob_account.get_hbd_balance();
    auto before_treasury_balance = before_treasury_account.get_hbd_balance();

    create_proposal_operation op;

    op.creator = creator;
    op.receiver = receiver;

    op.start_date = start_date;
    op.end_date = end_date;

    op.daily_pay = daily_pay;

    op.subject = subject;
    op.permlink = permlink;

    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    push_transaction( tx, alice_private_key, 0 );
    tx.operations.clear();
    tx.signatures.clear();

    {
      auto recent_ops = get_last_operations( 1 );
      auto fee_op = recent_ops.back().get< proposal_fee_operation >();
      BOOST_REQUIRE( fee_op.creator == creator );
      BOOST_REQUIRE( fee_op.treasury == db->get_treasury_name() );
      BOOST_REQUIRE( fee_op.fee == fee );
    }

    const auto& after_treasury_account = db->get_treasury();
    const account_object& after_alice_account = db->get_account( creator );
    const account_object& after_bob_account = db->get_account( receiver );

    auto after_alice_hbd_balance = after_alice_account.get_hbd_balance();
    auto after_bob_hbd_balance = after_bob_account.get_hbd_balance();
    auto after_treasury_balance = after_treasury_account.get_hbd_balance();

    BOOST_REQUIRE( before_alice_hbd_balance == after_alice_hbd_balance + fee );
    BOOST_REQUIRE( before_bob_hbd_balance == after_bob_hbd_balance );
    /// Fee shall be paid to treasury account.
    BOOST_REQUIRE(before_treasury_balance == after_treasury_balance - fee);

    const auto& proposal_idx = db->get_index< proposal_index >().indices().get< by_creator >();
    auto found = proposal_idx.find( creator );
    BOOST_REQUIRE( found != proposal_idx.end() );

    BOOST_REQUIRE( found->creator == creator );
    BOOST_REQUIRE( found->receiver == receiver );
    BOOST_REQUIRE( found->start_date == start_date );
    BOOST_REQUIRE( found->end_date == end_date );
    BOOST_REQUIRE( found->daily_pay == daily_pay );
    BOOST_REQUIRE( found->subject == subject );
    BOOST_REQUIRE( found->permlink == permlink );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( proposal_vote_object_apply )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: proposal_vote_object_operation" );

    ACTORS( (alice)(bob)(carol)(dan) )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    auto creator = "alice";
    auto receiver = "bob";

    auto start_date = db->head_block_time() + fc::days( 1 );
    auto end_date = start_date + fc::days( 2 );

    auto daily_pay = asset( 100, HBD_SYMBOL );

    FUND( creator, ASSET( "80.000 TBD" ) );

    int64_t id_proposal_00 = create_proposal( creator, receiver, start_date, end_date, daily_pay, alice_private_key );

    signed_transaction tx;
    update_proposal_votes_operation op;
    const auto& proposal_vote_idx = db->get_index< proposal_vote_index >().indices().get< by_voter_proposal >();

    auto voter_01 = "carol";
    auto voter_01_key = carol_private_key;

    {
      BOOST_TEST_MESSAGE( "---Voting for proposal( `id_proposal_00` )---" );
      op.voter = voter_01;
      op.proposal_ids.insert( id_proposal_00 );
      op.approve = true;

      tx.operations.push_back( op );
      tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
      push_transaction( tx, voter_01_key, 0 );
      tx.operations.clear();
      tx.signatures.clear();

      auto found = proposal_vote_idx.find( boost::make_tuple( voter_01, id_proposal_00 ) );
      BOOST_REQUIRE( found->voter == voter_01 );
      BOOST_REQUIRE( found->proposal_id == id_proposal_00 );
    }

    {
      BOOST_TEST_MESSAGE( "---Unvoting proposal( `id_proposal_00` )---" );
      op.voter = voter_01;
      op.proposal_ids.clear();
      op.proposal_ids.insert( id_proposal_00 );
      op.approve = false;

      tx.operations.push_back( op );
      tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
      push_transaction( tx, voter_01_key, 0 );
      tx.operations.clear();
      tx.signatures.clear();

      auto found = proposal_vote_idx.find( boost::make_tuple( voter_01, id_proposal_00 ) );
      BOOST_REQUIRE( found == proposal_vote_idx.end() );
    }

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( proposal_vote_object_01_apply )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: proposal_vote_object_operation" );

    ACTORS( (alice)(bob)(carol)(dan) )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    auto creator = "alice";
    auto receiver = "bob";

    auto start_date = db->head_block_time() + fc::days( 1 );
    auto end_date = start_date + fc::days( 2 );

    auto daily_pay_00 = asset( 100, HBD_SYMBOL );
    auto daily_pay_01 = asset( 101, HBD_SYMBOL );
    auto daily_pay_02 = asset( 102, HBD_SYMBOL );

    FUND( creator, ASSET( "80.000 TBD" ) );

    int64_t id_proposal_00 = create_proposal( creator, receiver, start_date, end_date, daily_pay_00, alice_private_key );
    int64_t id_proposal_01 = create_proposal( creator, receiver, start_date, end_date, daily_pay_01, alice_private_key );

    signed_transaction tx;
    update_proposal_votes_operation op;
    const auto& proposal_vote_idx = db->get_index< proposal_vote_index >().indices().get< by_voter_proposal >();

    std::string voter_01 = "carol";
    auto voter_01_key = carol_private_key;

    {
      BOOST_TEST_MESSAGE( "---Voting by `voter_01` for proposals( `id_proposal_00`, `id_proposal_01` )---" );
      op.voter = voter_01;
      op.proposal_ids.insert( id_proposal_00 );
      op.proposal_ids.insert( id_proposal_01 );
      op.approve = true;

      tx.operations.push_back( op );
      tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
      push_transaction( tx, voter_01_key, 0 );
      tx.operations.clear();
      tx.signatures.clear();

      int32_t cnt = 0;
      auto found = proposal_vote_idx.find( voter_01 );
      while( found != proposal_vote_idx.end() && found->voter == voter_01 )
      {
        ++cnt;
        ++found;
      }
      BOOST_REQUIRE( cnt == 2 );
    }

    int64_t id_proposal_02 = create_proposal( creator, receiver, start_date, end_date, daily_pay_02, alice_private_key );
    std::string voter_02 = "dan";
    auto voter_02_key = dan_private_key;

    {
      BOOST_TEST_MESSAGE( "---Voting by `voter_02` for proposals( `id_proposal_00`, `id_proposal_01`, `id_proposal_02` )---" );
      op.voter = voter_02;
      op.proposal_ids.clear();
      op.proposal_ids.insert( id_proposal_02 );
      op.proposal_ids.insert( id_proposal_00 );
      op.proposal_ids.insert( id_proposal_01 );
      op.approve = true;

      tx.operations.push_back( op );
      tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
      push_transaction( tx, voter_02_key, 0 );
      tx.operations.clear();
      tx.signatures.clear();

      int32_t cnt = 0;
      auto found = proposal_vote_idx.find( voter_02 );
      while( found != proposal_vote_idx.end() && found->voter == voter_02 )
      {
        ++cnt;
        ++found;
      }
      BOOST_REQUIRE( cnt == 3 );
    }

    {
      BOOST_TEST_MESSAGE( "---Voting by `voter_02` for proposals( `id_proposal_00` )---" );
      op.voter = voter_02;
      op.proposal_ids.clear();
      op.proposal_ids.insert( id_proposal_00 );
      op.approve = true;

      tx.operations.push_back( op );
      tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
      push_transaction( tx, voter_02_key, 0 );
      tx.operations.clear();
      tx.signatures.clear();

      int32_t cnt = 0;
      auto found = proposal_vote_idx.find( voter_02 );
      while( found != proposal_vote_idx.end() && found->voter == voter_02 )
      {
        ++cnt;
        ++found;
      }
      BOOST_REQUIRE( cnt == 3 );
    }

    {
      BOOST_TEST_MESSAGE( "---Unvoting by `voter_01` proposals( `id_proposal_02` )---" );
      op.voter = voter_01;
      op.proposal_ids.clear();
      op.proposal_ids.insert( id_proposal_02 );
      op.approve = false;

      tx.operations.push_back( op );
      tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
      push_transaction( tx, voter_01_key, 0 );
      tx.operations.clear();
      tx.signatures.clear();

      int32_t cnt = 0;
      auto found = proposal_vote_idx.find( voter_01 );
      while( found != proposal_vote_idx.end() && found->voter == voter_01 )
      {
        ++cnt;
        ++found;
      }
      BOOST_REQUIRE( cnt == 2 );
    }

    {
      BOOST_TEST_MESSAGE( "---Unvoting by `voter_01` proposals( `id_proposal_00` )---" );
      op.voter = voter_01;
      op.proposal_ids.clear();
      op.proposal_ids.insert( id_proposal_00 );
      op.approve = false;

      tx.operations.push_back( op );
      tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
      push_transaction( tx, voter_01_key, 0 );
      tx.operations.clear();
      tx.signatures.clear();

      int32_t cnt = 0;
      auto found = proposal_vote_idx.find( voter_01 );
      while( found != proposal_vote_idx.end() && found->voter == voter_01 )
      {
        ++cnt;
        ++found;
      }
      BOOST_REQUIRE( cnt == 1 );
    }

    {
      BOOST_TEST_MESSAGE( "---Unvoting by `voter_02` proposals( `id_proposal_00`, `id_proposal_01`, `id_proposal_02` )---" );
      op.voter = voter_02;
      op.proposal_ids.clear();
      op.proposal_ids.insert( id_proposal_02 );
      op.proposal_ids.insert( id_proposal_01 );
      op.proposal_ids.insert( id_proposal_00 );
      op.approve = false;

      tx.operations.push_back( op );
      tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
      push_transaction( tx, voter_02_key, 0 );
      tx.operations.clear();
      tx.signatures.clear();

      int32_t cnt = 0;
      auto found = proposal_vote_idx.find( voter_02 );
      while( found != proposal_vote_idx.end() && found->voter == voter_02 )
      {
        ++cnt;
        ++found;
      }
      BOOST_REQUIRE( cnt == 0 );
    }

    {
      BOOST_TEST_MESSAGE( "---Unvoting by `voter_01` proposals( `id_proposal_01` )---" );
      op.voter = voter_01;
      op.proposal_ids.clear();
      op.proposal_ids.insert( id_proposal_01 );
      op.approve = false;

      tx.operations.push_back( op );
      tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
      push_transaction( tx, voter_01_key, 0 );
      tx.operations.clear();
      tx.signatures.clear();

      int32_t cnt = 0;
      auto found = proposal_vote_idx.find( voter_01 );
      while( found != proposal_vote_idx.end() && found->voter == voter_01 )
      {
        ++cnt;
        ++found;
      }
      BOOST_REQUIRE( cnt == 0 );
    }

    {
      BOOST_TEST_MESSAGE( "---Voting by `voter_01` for nothing---" );
      op.voter = voter_01;
      op.proposal_ids.clear();
      op.approve = true;

      tx.operations.push_back( op );
      tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
      HIVE_REQUIRE_THROW(push_transaction( tx, voter_01_key, 0 ), fc::exception);
      tx.operations.clear();
      tx.signatures.clear();

      int32_t cnt = 0;
      auto found = proposal_vote_idx.find( voter_01 );
      while( found != proposal_vote_idx.end() && found->voter == voter_01 )
      {
        ++cnt;
        ++found;
      }
      BOOST_REQUIRE( cnt == 0 );
    }

    {
      BOOST_TEST_MESSAGE( "---Unvoting by `voter_01` nothing---" );
      op.voter = voter_01;
      op.proposal_ids.clear();
      op.approve = false;

      tx.operations.push_back( op );
      tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
      HIVE_REQUIRE_THROW(push_transaction( tx, voter_01_key, 0 ), fc::exception);
      tx.operations.clear();
      tx.signatures.clear();

      int32_t cnt = 0;
      auto found = proposal_vote_idx.find( voter_01 );
      while( found != proposal_vote_idx.end() && found->voter == voter_01 )
      {
        ++cnt;
        ++found;
      }
      BOOST_REQUIRE( cnt == 0 );
    }

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( create_proposal_000 )
{
  try {
    BOOST_TEST_MESSAGE( "Testing: create proposal: operation arguments validation - all args are ok" );
    ACTORS( (alice)(bob) )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    auto creator    = "alice";
    auto receiver   = "bob";
    auto start_date = db->head_block_time() + fc::days( 1 );
    auto end_date   = start_date + fc::days( 2 );
    auto daily_pay  = asset( 100, HBD_SYMBOL );

    FUND( creator, ASSET( "80.000 TBD" ) );
    {
      int64_t proposal = create_proposal( creator, receiver, start_date, end_date, daily_pay, alice_private_key );
      BOOST_REQUIRE( proposal >= 0 );
    }
    validate_database();
  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( create_proposal_001 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: create proposal: operation arguments validation - invalid creator" );
    {
      create_proposal_data cpd(db->head_block_time());
      ACTORS( (alice)(bob) )
      generate_block();
      FUND( cpd.creator, ASSET( "80.000 TBD" ) );
      HIVE_REQUIRE_THROW( create_proposal( "", cpd.receiver, cpd.start_date, cpd.end_date, cpd.daily_pay, alice_private_key ), fc::exception);

    }
    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( create_proposal_002 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: create proposal: operation arguments validation - invalid receiver" );
    create_proposal_data cpd(db->head_block_time());
    ACTORS( (alice)(bob) )
    generate_block();
    FUND( cpd.creator, ASSET( "80.000 TBD" ) );
    HIVE_REQUIRE_THROW(create_proposal( cpd.creator, "", cpd.start_date, cpd.end_date, cpd.daily_pay, alice_private_key ), fc::exception);
    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( create_proposal_003 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: create proposal: operation arguments validation - invalid start date" );
    create_proposal_data cpd(db->head_block_time());
    ACTORS( (alice)(bob) )
    generate_block();
    FUND( cpd.creator, ASSET( "80.000 TBD" ) );
    cpd.start_date = cpd.end_date + fc::days(2);
    HIVE_REQUIRE_THROW(create_proposal( cpd.creator, cpd.receiver, cpd.start_date, cpd.end_date, cpd.daily_pay, alice_private_key ), fc::exception);
    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( create_proposal_004 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: create proposal: operation arguments validation - invalid end date" );
    create_proposal_data cpd(db->head_block_time());
    ACTORS( (alice)(bob) )
    generate_block();
    FUND( cpd.creator, ASSET( "80.000 TBD" ) );
    cpd.end_date = cpd.start_date - fc::days(2);
    HIVE_REQUIRE_THROW(create_proposal( cpd.creator, cpd.receiver, cpd.start_date, cpd.end_date, cpd.daily_pay, alice_private_key ), fc::exception);
    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( create_proposal_005 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: create proposal: operation arguments validation - invalid subject(empty)" );
    ACTORS( (alice)(bob) )
    generate_block();
    create_proposal_operation cpo;
    cpo.creator    = "alice";
    cpo.receiver   = "bob";
    cpo.start_date = db->head_block_time() + fc::days( 1 );
    cpo.end_date   = cpo.start_date + fc::days( 2 );
    cpo.daily_pay  = asset( 100, HBD_SYMBOL );
    cpo.subject    = "";
    cpo.permlink        = "http:://something.html";
    FUND( cpo.creator, ASSET( "80.000 TBD" ) );
    generate_block();
    signed_transaction tx;
    tx.operations.push_back( cpo );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    HIVE_REQUIRE_THROW(push_transaction( tx, alice_private_key, 0 ), fc::exception);
    tx.operations.clear();
    tx.signatures.clear();
    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( create_proposal_006 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: create proposal: operation arguments validation - invalid subject(too long)" );
    ACTORS( (alice)(bob) )
    generate_block();
    create_proposal_operation cpo;
    cpo.creator    = "alice";
    cpo.receiver   = "bob";
    cpo.start_date = db->head_block_time() + fc::days( 1 );
    cpo.end_date   = cpo.start_date + fc::days( 2 );
    cpo.daily_pay  = asset( 100, HBD_SYMBOL );
    cpo.subject    = "very very very very very very long long long long long long subject subject subject subject subject subject";
    cpo.permlink        = "http:://something.html";
    FUND( cpo.creator, ASSET( "80.000 TBD" ) );
    generate_block();
    signed_transaction tx;
    tx.operations.push_back( cpo );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    HIVE_REQUIRE_THROW(push_transaction( tx, alice_private_key, 0 ), fc::exception);
    tx.operations.clear();
    tx.signatures.clear();
    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( create_proposal_007 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: create proposal: authorization test" );
    ACTORS( (alice)(bob) )
    generate_block();
    create_proposal_operation cpo;
    cpo.creator    = "alice";
    cpo.receiver   = "bob";
    cpo.start_date = db->head_block_time() + fc::days( 1 );
    cpo.end_date   = cpo.start_date + fc::days( 2 );
    cpo.daily_pay  = asset( 100, HBD_SYMBOL );
    cpo.subject    = "subject";
    cpo.permlink        = "http:://something.html";

    flat_set< account_name_type > auths;
    flat_set< account_name_type > expected;

    cpo.get_required_owner_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    cpo.get_required_posting_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    expected.insert( "alice" );
    cpo.get_required_active_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( create_proposal_008 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: create proposal: operation arguments validation - invalid daily payement (negative value)" );
    create_proposal_data cpd(db->head_block_time());
    ACTORS( (alice)(bob) )
    generate_block();
    FUND( cpd.creator, ASSET( "80.000 TBD" ) );
    generate_block();
    generate_block();
    cpd.end_date = cpd.start_date + fc::days(20);
    cpd.daily_pay = asset( -10, HBD_SYMBOL );
    HIVE_REQUIRE_THROW(create_proposal( cpd.creator, cpd.receiver, cpd.start_date, cpd.end_date, cpd.daily_pay, alice_private_key ), fc::exception);
    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( create_proposal_009 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: create proposal: insufficient funds for operation" );
    create_proposal_data cpd(db->head_block_time());
    ACTORS( (alice)(bob) )
    generate_block();
    FUND( cpd.creator, ASSET( "15.000 TBD" ) );
    generate_block();
    generate_block();
    cpd.end_date = cpd.start_date + fc::days(80);
    cpd.daily_pay = asset( 10, HBD_SYMBOL );
    HIVE_REQUIRE_THROW(create_proposal( cpd.creator, cpd.receiver, cpd.start_date, cpd.end_date, cpd.daily_pay, alice_private_key ), fc::exception);
    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( update_proposal_votes_000 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: update proposal votes: operation arguments validation - all ok (approve true)" );
    create_proposal_data cpd(db->head_block_time());
    ACTORS( (alice)(bob)(carol) )
    generate_block();
    FUND( cpd.creator, ASSET( "80.000 TBD" ) );
    generate_block();

    int64_t proposal_1 = create_proposal( cpd.creator, cpd.receiver, cpd.start_date, cpd.end_date, cpd.daily_pay, alice_private_key );
    BOOST_REQUIRE(proposal_1 >= 0);
    std::vector< int64_t > proposals = {proposal_1};
    vote_proposal("carol", proposals, true, carol_private_key);
    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( update_proposal_votes_001 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: update proposal votes: operation arguments validation - all ok (approve false)" );
    create_proposal_data cpd(db->head_block_time());
    ACTORS( (alice)(bob)(carol) )
    generate_block();
    FUND( cpd.creator, ASSET( "80.000 TBD" ) );
    generate_block();

    int64_t proposal_1 = create_proposal( cpd.creator, cpd.receiver, cpd.start_date, cpd.end_date, cpd.daily_pay, alice_private_key );
    BOOST_REQUIRE(proposal_1 >= 0);
    std::vector< int64_t > proposals = {proposal_1};
    vote_proposal("carol", proposals, false, carol_private_key);
    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( update_proposal_votes_002 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: update proposal votes: operation arguments validation - all ok (empty array)" );
    create_proposal_data cpd(db->head_block_time());
    ACTORS( (alice)(bob)(carol) )
    generate_block();
    FUND( cpd.creator, ASSET( "80.000 TBD" ) );
    generate_block();

    int64_t proposal_1 = create_proposal( cpd.creator, cpd.receiver, cpd.start_date, cpd.end_date, cpd.daily_pay, alice_private_key );
    BOOST_REQUIRE(proposal_1 >= 0);
    std::vector< int64_t > proposals;
    HIVE_REQUIRE_THROW( vote_proposal("carol", proposals, true, carol_private_key), fc::exception);
    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( update_proposal_votes_003 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: update proposal votes: operation arguments validation - all ok (array with negative digits)" );
    create_proposal_data cpd(db->head_block_time());
    ACTORS( (alice)(bob)(carol) )
    generate_block();
    FUND( cpd.creator, ASSET( "80.000 TBD" ) );
    generate_block();

    std::vector< int64_t > proposals = {-1, -2, -3, -4, -5};
    vote_proposal("carol", proposals, true, carol_private_key);
    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( update_proposal_votes_004 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: update proposal votes: operation arguments validation - invalid voter" );
    create_proposal_data cpd(db->head_block_time());
    ACTORS( (alice)(bob)(carol) )
    generate_block();
    FUND( cpd.creator, ASSET( "80.000 TBD" ) );
    generate_block();

    int64_t proposal_1 = create_proposal( cpd.creator, cpd.receiver, cpd.start_date, cpd.end_date, cpd.daily_pay, alice_private_key );
    BOOST_REQUIRE(proposal_1 >= 0);
    std::vector< int64_t > proposals = {proposal_1};
    HIVE_REQUIRE_THROW(vote_proposal("urp", proposals, false, carol_private_key), fc::exception);
    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( update_proposal_votes_005 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: update proposal votes: operation arguments validation - invalid id array (array with greater number of digits than allowed)" );
    create_proposal_data cpd(db->head_block_time());
    ACTORS( (alice)(bob)(carol) )
    generate_block();
    FUND( cpd.creator, ASSET( "80.000 TBD" ) );
    generate_block();

    std::vector< int64_t > proposals;
    for(int i = 0; i <= HIVE_PROPOSAL_MAX_IDS_NUMBER; i++) {
      proposals.push_back(i);
    }
    HIVE_REQUIRE_THROW(vote_proposal("carol", proposals, true, carol_private_key), fc::exception);

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( update_proposal_votes_006 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: update proposal votes: authorization test" );
    ACTORS( (alice)(bob) )
    generate_block();
    update_proposal_votes_operation upv;
    upv.voter = "alice";
    upv.proposal_ids = {0};
    upv.approve = true;

    flat_set< account_name_type > auths;
    flat_set< account_name_type > expected;

    upv.get_required_owner_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    upv.get_required_posting_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    expected.insert( "alice" );
    upv.get_required_active_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( remove_proposal_000 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: remove proposal: basic verification operation - proposal removal (only one)." );
    create_proposal_data cpd(db->head_block_time());
    ACTORS( (alice)(bob) )
    generate_block();
    FUND( cpd.creator, ASSET( "80.000 TBD" ) );
    generate_block();

    int64_t proposal_1 = create_proposal( cpd.creator, cpd.receiver, cpd.start_date, cpd.end_date, cpd.daily_pay, alice_private_key );
    BOOST_REQUIRE(proposal_1 >= 0);

    auto& proposal_idx = db->get_index< proposal_index >().indices().get< by_creator >();
    auto found = proposal_idx.find( cpd.creator );
    BOOST_REQUIRE( found != proposal_idx.end() );
    BOOST_REQUIRE( proposal_idx.size() == 1 );

    flat_set<int64_t> proposals = { proposal_1 };
    remove_proposal(cpd.creator, proposals, alice_private_key);

    found = proposal_idx.find( cpd.creator );
    BOOST_REQUIRE( found == proposal_idx.end() );
    BOOST_REQUIRE( proposal_idx.empty() );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( remove_proposal_001 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: remove proposal: basic verification operation - proposal removal (one from many)." );
    create_proposal_data cpd(db->head_block_time());
    ACTORS( (alice)(bob) )
    generate_block();
    FUND( cpd.creator, ASSET( "80.000 TBD" ) );
    generate_block();


    int64_t proposal_1 = create_proposal( cpd.creator, cpd.receiver, cpd.start_date, cpd.end_date, cpd.daily_pay, alice_private_key );
    int64_t proposal_2 = create_proposal( cpd.creator, cpd.receiver, cpd.start_date, cpd.end_date, cpd.daily_pay, alice_private_key );
    int64_t proposal_3 = create_proposal( cpd.creator, cpd.receiver, cpd.start_date, cpd.end_date, cpd.daily_pay, alice_private_key );
    BOOST_REQUIRE(proposal_1 >= 0);
    BOOST_REQUIRE(proposal_2 >= 0);
    BOOST_REQUIRE(proposal_3 >= 0);

    auto& proposal_idx = db->get_index< proposal_index >().indices().get< by_creator >();
    auto found = proposal_idx.find( cpd.creator );
    BOOST_REQUIRE( found != proposal_idx.end() );
    BOOST_REQUIRE( proposal_idx.size() == 3 );

    flat_set<int64_t> proposals = { proposal_1 };
    remove_proposal(cpd.creator, proposals, alice_private_key);

    found = proposal_idx.find( cpd.creator );
    BOOST_REQUIRE( found != proposal_idx.end() );   //two left
    BOOST_REQUIRE( proposal_idx.size() == 2 );

    proposals.clear();
    proposals.insert(proposal_2);
    remove_proposal(cpd.creator, proposals, alice_private_key);

    found = proposal_idx.find( cpd.creator );
    BOOST_REQUIRE( found != proposal_idx.end() );   //one left
    BOOST_REQUIRE( proposal_idx.size() == 1 );

    proposals.clear();
    proposals.insert(proposal_3);
    remove_proposal(cpd.creator, proposals, alice_private_key);

    found = proposal_idx.find( cpd.creator );
    BOOST_REQUIRE( found == proposal_idx.end() );   //none
    BOOST_REQUIRE( proposal_idx.empty() );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( remove_proposal_002 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: remove proposal: basic verification operation - proposal removal (n from many in two steps)." );
    create_proposal_data cpd(db->head_block_time());
    ACTORS( (alice)(bob) )
    generate_block();
    FUND( cpd.creator, ASSET( "80.000 TBD" ) );
    generate_block();

    int64_t proposal = -1;
    std::vector<int64_t> proposals;

    for(int i = 0; i < 6; i++) {
      proposal = create_proposal( cpd.creator, cpd.receiver, cpd.start_date, cpd.end_date, cpd.daily_pay, alice_private_key );
      BOOST_REQUIRE(proposal >= 0);
      proposals.push_back(proposal);
    }
    BOOST_REQUIRE(proposals.size() == 6);

    auto& proposal_idx = db->get_index< proposal_index >().indices().get< by_creator >();
    auto found = proposal_idx.find( cpd.creator );
    BOOST_REQUIRE( found != proposal_idx.end() );
    BOOST_REQUIRE(proposal_idx.size() == 6);

    flat_set<int64_t> proposals_to_erase = {proposals[0], proposals[1], proposals[2], proposals[3]};
    remove_proposal(cpd.creator, proposals_to_erase, alice_private_key);

    found = proposal_idx.find( cpd.creator );
    BOOST_REQUIRE( found != proposal_idx.end() );
    BOOST_REQUIRE( proposal_idx.size() == 2 );

    proposals_to_erase.clear();
    proposals_to_erase.insert(proposals[4]);
    proposals_to_erase.insert(proposals[5]);

    remove_proposal(cpd.creator, proposals_to_erase, alice_private_key);
    found = proposal_idx.find( cpd.creator );
    BOOST_REQUIRE( found == proposal_idx.end() );
    BOOST_REQUIRE( proposal_idx.empty() );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( remove_proposal_003 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: remove proposal: basic verification operation - proper proposal deletion check (one at time)." );
    create_proposal_data cpd(db->head_block_time());
    ACTORS( (alice)(bob) )
    generate_block();
    FUND( cpd.creator, ASSET( "80.000 TBD" ) );
    generate_block();

    int64_t proposal = -1;
    std::vector<int64_t> proposals;

    for(int i = 0; i < 2; i++) {
      proposal = create_proposal( cpd.creator, cpd.receiver, cpd.start_date, cpd.end_date, cpd.daily_pay, alice_private_key );
      BOOST_REQUIRE(proposal >= 0);
      proposals.push_back(proposal);
    }
    BOOST_REQUIRE(proposals.size() == 2);

    auto& proposal_idx = db->get_index< proposal_index >().indices().get< by_creator >();
    auto found = proposal_idx.find( cpd.creator );
    BOOST_REQUIRE( found != proposal_idx.end() );
    BOOST_REQUIRE(proposal_idx.size() == 2);

    flat_set<int64_t> proposals_to_erase = {proposals[0]};
    remove_proposal(cpd.creator, proposals_to_erase, alice_private_key);

    found = proposal_idx.find( cpd.creator );
    BOOST_REQUIRE( found != proposal_idx.end() );
    BOOST_REQUIRE( found->proposal_id == proposals[1]);
    BOOST_REQUIRE( proposal_idx.size() == 1 );

    proposals_to_erase.clear();
    proposals_to_erase.insert(proposals[1]);

    remove_proposal(cpd.creator, proposals_to_erase, alice_private_key);
    found = proposal_idx.find( cpd.creator );
    BOOST_REQUIRE( found == proposal_idx.end() );
    BOOST_REQUIRE( proposal_idx.empty() );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( remove_proposal_004 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: remove proposal: basic verification operation - proper proposal deletion check (two at one time)." );
    create_proposal_data cpd(db->head_block_time());
    ACTORS( (alice)(bob) )
    generate_block();
    FUND( cpd.creator, ASSET( "80.000 TBD" ) );
    generate_block();

    int64_t proposal = -1;
    std::vector<int64_t> proposals;

    for(int i = 0; i < 6; i++) {
      proposal = create_proposal( cpd.creator, cpd.receiver, cpd.start_date, cpd.end_date, cpd.daily_pay, alice_private_key );
      BOOST_REQUIRE(proposal >= 0);
      proposals.push_back(proposal);
    }
    BOOST_REQUIRE(proposals.size() == 6);

    auto& proposal_idx = db->get_index< proposal_index >().indices().get< by_creator >();
    auto found = proposal_idx.find( cpd.creator );
    BOOST_REQUIRE( found != proposal_idx.end() );
    BOOST_REQUIRE(proposal_idx.size() == 6);

    flat_set<int64_t> proposals_to_erase = {proposals[0], proposals[5]};
    remove_proposal(cpd.creator, proposals_to_erase, alice_private_key);

    found = proposal_idx.find( cpd.creator );
    BOOST_REQUIRE( found != proposal_idx.end() );
    for(auto& it : proposal_idx) {
      BOOST_REQUIRE( it.proposal_id != proposals[0] );
      BOOST_REQUIRE( it.proposal_id != proposals[5] );
    }
    BOOST_REQUIRE( proposal_idx.size() == 4 );

    proposals_to_erase.clear();
    proposals_to_erase.insert(proposals[1]);
    proposals_to_erase.insert(proposals[4]);

    remove_proposal(cpd.creator, proposals_to_erase, alice_private_key);
    found = proposal_idx.find( cpd.creator );
    for(auto& it : proposal_idx) {
      BOOST_REQUIRE( it.proposal_id != proposals[0] );
      BOOST_REQUIRE( it.proposal_id != proposals[1] );
      BOOST_REQUIRE( it.proposal_id != proposals[4] );
      BOOST_REQUIRE( it.proposal_id != proposals[5] );
    }
    BOOST_REQUIRE( found != proposal_idx.end() );
    BOOST_REQUIRE( proposal_idx.size() == 2 );

    proposals_to_erase.clear();
    proposals_to_erase.insert(proposals[2]);
    proposals_to_erase.insert(proposals[3]);
    remove_proposal(cpd.creator, proposals_to_erase, alice_private_key);
    found = proposal_idx.find( cpd.creator );
    BOOST_REQUIRE( found == proposal_idx.end() );
    BOOST_REQUIRE( proposal_idx.empty() );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( remove_proposal_005 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: remove proposal: basic verification operation - proposal with votes removal (only one)." );
    create_proposal_data cpd(db->head_block_time());
    ACTORS( (alice)(bob) )
    generate_block();
    FUND( cpd.creator, ASSET( "80.000 TBD" ) );
    generate_block();

    int64_t proposal_1 = create_proposal( cpd.creator, cpd.receiver, cpd.start_date, cpd.end_date, cpd.daily_pay, alice_private_key );
    BOOST_REQUIRE(proposal_1 >= 0);

    auto& proposal_idx      = db->get_index< proposal_index >().indices().get< by_creator >();
    auto found       = proposal_idx.find( cpd.creator );

    BOOST_REQUIRE( found != proposal_idx.end() );
    BOOST_REQUIRE( proposal_idx.size() == 1 );

    std::vector<int64_t> vote_proposals = {proposal_1};

    vote_proposal( "bob", vote_proposals, true, bob_private_key );
    BOOST_REQUIRE( find_vote_for_proposal("bob", proposal_1) );

    flat_set<int64_t> proposals = { proposal_1 };
    remove_proposal(cpd.creator, proposals, alice_private_key);

    found = proposal_idx.find( cpd.creator );
    BOOST_REQUIRE( found == proposal_idx.end() );
    BOOST_REQUIRE( proposal_idx.empty() );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( remove_proposal_006 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: remove proposal: basic verification operation - remove proposal with votes and one voteless at same time." );
    create_proposal_data cpd(db->head_block_time());
    ACTORS( (alice)(bob) )
    generate_block();
    FUND( cpd.creator, ASSET( "80.000 TBD" ) );
    generate_block();

    int64_t proposal_1 = create_proposal( cpd.creator, cpd.receiver, cpd.start_date, cpd.end_date, cpd.daily_pay, alice_private_key );
    int64_t proposal_2 = create_proposal( cpd.creator, cpd.receiver, cpd.start_date, cpd.end_date, cpd.daily_pay, alice_private_key );
    BOOST_REQUIRE(proposal_1 >= 0);
    BOOST_REQUIRE(proposal_2 >= 0);

    auto& proposal_idx = db->get_index< proposal_index >().indices().get< by_creator >();
    auto found = proposal_idx.find( cpd.creator );
    BOOST_REQUIRE( found != proposal_idx.end() );
    BOOST_REQUIRE( proposal_idx.size() == 2 );

    std::vector<int64_t> vote_proposals = {proposal_1};

    vote_proposal( "bob",   vote_proposals, true, bob_private_key );
    BOOST_REQUIRE( find_vote_for_proposal("bob", proposal_1) );

    flat_set<int64_t> proposals = { proposal_1, proposal_2 };
    remove_proposal(cpd.creator, proposals, alice_private_key);

    found = proposal_idx.find( cpd.creator );
    BOOST_REQUIRE( found == proposal_idx.end() );
    BOOST_REQUIRE( proposal_idx.empty() );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( remove_proposal_007 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: remove proposal: basic verification operation - remove proposals with votes at same time." );
    create_proposal_data cpd(db->head_block_time());
    ACTORS( (alice)(bob)(carol) )
    generate_block();
    FUND( cpd.creator, ASSET( "80.000 TBD" ) );
    generate_block();

    int64_t proposal_1 = create_proposal( cpd.creator, cpd.receiver, cpd.start_date, cpd.end_date, cpd.daily_pay, alice_private_key );
    int64_t proposal_2 = create_proposal( cpd.creator, cpd.receiver, cpd.start_date, cpd.end_date, cpd.daily_pay, alice_private_key );
    BOOST_REQUIRE(proposal_1 >= 0);
    BOOST_REQUIRE(proposal_2 >= 0);

    auto& proposal_idx = db->get_index< proposal_index >().indices().get< by_creator >();
    auto found = proposal_idx.find( cpd.creator );
    BOOST_REQUIRE( found != proposal_idx.end() );
    BOOST_REQUIRE( proposal_idx.size() == 2 );

    std::vector<int64_t> vote_proposals = {proposal_1};
    vote_proposal( "bob",   vote_proposals, true, bob_private_key );
    BOOST_REQUIRE( find_vote_for_proposal("bob", proposal_1) );
    vote_proposals.clear();
    vote_proposals.push_back(proposal_2);
    vote_proposal( "carol", vote_proposals, true, carol_private_key );
    BOOST_REQUIRE( find_vote_for_proposal("carol", proposal_2) );

    flat_set<int64_t> proposals = { proposal_1, proposal_2 };
    remove_proposal(cpd.creator, proposals, alice_private_key);

    found = proposal_idx.find( cpd.creator );
    BOOST_REQUIRE( found == proposal_idx.end() );
    BOOST_REQUIRE( proposal_idx.empty() );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( remove_proposal_008 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: remove proposal: operation arguments validation - all ok" );
    create_proposal_data cpd(db->head_block_time());
    ACTORS( (alice)(bob) )
    generate_block();
    FUND( cpd.creator, ASSET( "80.000 TBD" ) );
    generate_block();
    flat_set<int64_t> proposals = { 0 };
    remove_proposal(cpd.creator, proposals, alice_private_key);
    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( remove_proposal_009 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: remove proposal: operation arguments validation - invalid deleter" );
    create_proposal_data cpd(db->head_block_time());
    ACTORS( (alice)(bob) )
    generate_block();
    FUND( cpd.creator, ASSET( "80.000 TBD" ) );
    generate_block();
    int64_t proposal_1 = create_proposal( cpd.creator, cpd.receiver, cpd.start_date, cpd.end_date, cpd.daily_pay, alice_private_key );
    flat_set<int64_t> proposals = { proposal_1 };
    HIVE_REQUIRE_THROW(remove_proposal(cpd.receiver, proposals, bob_private_key), fc::exception);
    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( remove_proposal_010 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: remove proposal: operation arguments validation - invalid array(empty array)" );
    create_proposal_data cpd(db->head_block_time());
    ACTORS( (alice)(bob) )
    generate_block();
    FUND( cpd.creator, ASSET( "80.000 TBD" ) );
    generate_block();
    flat_set<int64_t> proposals;
    HIVE_REQUIRE_THROW(remove_proposal(cpd.creator, proposals, bob_private_key), fc::exception);
    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( remove_proposal_011 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: remove proposal: operation arguments validation - invalid array(array with greater number of digits than allowed)" );
    create_proposal_data cpd(db->head_block_time());
    ACTORS( (alice)(bob) )
    generate_block();
    FUND( cpd.creator, ASSET( "80.000 TBD" ) );
    generate_block();
    flat_set<int64_t> proposals;
    for(int i = 0; i <= HIVE_PROPOSAL_MAX_IDS_NUMBER; i++) {
      proposals.insert(create_proposal( cpd.creator, cpd.receiver, cpd.start_date, cpd.end_date, cpd.daily_pay, alice_private_key ));
    }
    HIVE_REQUIRE_THROW(remove_proposal(cpd.creator, proposals, bob_private_key), fc::exception);
    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( remove_proposal_012 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: remove proposal: authorization test" );
    ACTORS( (alice)(bob) )
    generate_block();
    remove_proposal_operation rpo;
    rpo.proposal_owner = "alice";
    rpo.proposal_ids = {1,2,3};

    flat_set< account_name_type > auths;
    flat_set< account_name_type > expected;

    rpo.get_required_owner_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    rpo.get_required_posting_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    expected.insert( "alice" );
    rpo.get_required_active_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( proposals_maintenance_01 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: removing of old proposals using threshold" );

    ACTORS( (a00)(a01)(a02)(a03)(a04) )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    //=====================preparing=====================
    auto receiver = db->get_treasury_name();

    auto start_time = db->head_block_time();

    const auto start_time_shift = fc::hours( 11 );
    const auto end_time_shift = fc::hours( 10 );
    const auto block_interval = fc::seconds( HIVE_BLOCK_INTERVAL );

    auto start_date_00 = start_time + start_time_shift;
    auto end_date_00 = start_date_00 + end_time_shift;

    auto daily_pay = asset( 100, HBD_SYMBOL );

    const auto nr_proposals = 200;
    std::vector< int64_t > proposals_id;

    struct initial_data
    {
      std::string account;
      fc::ecc::private_key key;
    };

    std::vector< initial_data > inits = {
                          {"a00", a00_private_key },
                          {"a01", a01_private_key },
                          {"a02", a02_private_key },
                          {"a03", a03_private_key },
                          {"a04", a04_private_key },
                          };

    for( auto item : inits )
    {
      FUND( item.account, ASSET( "10000.000 TBD" ) );
    }
    //=====================preparing=====================

    for( int32_t i = 0; i < nr_proposals; ++i )
    {
      auto item = inits[ i % inits.size() ];
      proposals_id.push_back( create_proposal( item.account, receiver, start_date_00, end_date_00, daily_pay, item.key ) );
      generate_block();
    }

    const auto& proposal_idx = db->get_index< proposal_index >().indices().get< by_proposal_id >();

    auto current_active_proposals = nr_proposals;
    BOOST_REQUIRE( calc_proposals( proposal_idx, proposals_id ) == current_active_proposals );

    generate_blocks( start_time + fc::seconds( HIVE_PROPOSAL_MAINTENANCE_CLEANUP ) );
    start_time = db->head_block_time();

    generate_blocks( start_time + ( start_time_shift + end_time_shift - block_interval ) );

    auto threshold = db->get_remove_threshold();
    auto nr_stages = current_active_proposals / threshold;

    for( auto i = 0; i < nr_stages; ++i )
    {
      generate_block();

      current_active_proposals -= threshold;
      auto found = calc_proposals( proposal_idx, proposals_id );

      /*
        Take a look at comment in `dhf_processor::remove_proposals`
      */
      //BOOST_REQUIRE( current_active_proposals == found ); //earlier
      BOOST_REQUIRE( nr_proposals == found );               //now
    }

    BOOST_REQUIRE( current_active_proposals == 0 );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( proposals_maintenance_02 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: removing of old proposals + votes using threshold" );

    ACTORS( (a00)(a01)(a02)(a03)(a04) )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    //=====================preparing=====================
    auto receiver = db->get_treasury_name();

    auto start_time = db->head_block_time();

    const auto start_time_shift = fc::hours( 11 );
    const auto end_time_shift = fc::hours( 10 );
    const auto block_interval = fc::seconds( HIVE_BLOCK_INTERVAL );

    auto start_date_00 = start_time + start_time_shift;
    auto end_date_00 = start_date_00 + end_time_shift;

    auto daily_pay = asset( 100, HBD_SYMBOL );

    const auto nr_proposals = 10;
    std::vector< int64_t > proposals_id;

    struct initial_data
    {
      std::string account;
      fc::ecc::private_key key;
    };

    std::vector< initial_data > inits = {
                          {"a00", a00_private_key },
                          {"a01", a01_private_key },
                          {"a02", a02_private_key },
                          {"a03", a03_private_key },
                          {"a04", a04_private_key },
                          };

    for( auto item : inits )
    {
      FUND( item.account, ASSET( "10000.000 TBD" ) );
    }
    //=====================preparing=====================

    for( auto i = 0; i < nr_proposals; ++i )
    {
      auto item = inits[ i % inits.size() ];
      proposals_id.push_back( create_proposal( item.account, receiver, start_date_00, end_date_00, daily_pay, item.key ) );
      generate_block();
    }

    auto itr_begin_2 = proposals_id.begin() + HIVE_PROPOSAL_MAX_IDS_NUMBER;
    std::vector< int64_t > v1( proposals_id.begin(), itr_begin_2 );
    std::vector< int64_t > v2( itr_begin_2, proposals_id.end() );

    for( auto item : inits )
    {
      vote_proposal( item.account, v1, true/*approve*/, item.key);
      generate_blocks( 1 );
      vote_proposal( item.account, v2, true/*approve*/, item.key);
      generate_blocks( 1 );
    }

    const auto& proposal_idx = db->get_index< proposal_index >().indices().get< by_proposal_id >();
    const auto& proposal_vote_idx = db->get_index< proposal_vote_index >().indices().get< by_proposal_voter >();

    auto current_active_proposals = nr_proposals;
    BOOST_REQUIRE( calc_proposals( proposal_idx, proposals_id ) == current_active_proposals );

    auto current_active_votes = current_active_proposals * static_cast< int16_t > ( inits.size() );
    BOOST_REQUIRE( calc_votes( proposal_vote_idx, proposals_id ) == current_active_votes );

    generate_blocks( start_time + fc::seconds( HIVE_PROPOSAL_MAINTENANCE_CLEANUP ) );
    start_time = db->head_block_time();

    generate_blocks( start_time + ( start_time_shift + end_time_shift - block_interval ) );

    auto threshold = db->get_remove_threshold();
    auto current_active_anything = current_active_proposals + current_active_votes;
    auto nr_stages = current_active_anything / threshold;

    for( auto i = 0; i < nr_stages; ++i )
    {
      generate_block();

      current_active_anything -= threshold;
      auto found_proposals = calc_proposals( proposal_idx, proposals_id );
      auto found_votes = calc_votes( proposal_vote_idx, proposals_id );

      /*
        Take a look at comment in `dhf_processor::remove_proposals`
      */
      //BOOST_REQUIRE( current_active_anything == found_proposals + found_votes );                            //earlier
      BOOST_REQUIRE( ( current_active_proposals + current_active_votes ) == found_proposals + found_votes );  //now
    }

    BOOST_REQUIRE( current_active_anything == 0 );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( proposals_removing_with_threshold )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: removing of old proposals + votes using threshold" );

    ACTORS( (a00)(a01)(a02)(a03)(a04) )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    //=====================preparing=====================
    auto receiver = db->get_treasury_name();

    auto start_time = db->head_block_time();

    const auto start_time_shift = fc::days( 100 );
    const auto end_time_shift = fc::days( 101 );

    auto start_date_00 = start_time + start_time_shift;
    auto end_date_00 = start_date_00 + end_time_shift;

    auto daily_pay = asset( 100, HBD_SYMBOL );

    const auto nr_proposals = 5;
    std::vector< int64_t > proposals_id;

    struct initial_data
    {
      std::string account;
      fc::ecc::private_key key;
    };

    std::vector< initial_data > inits = {
                          {"a00", a00_private_key },
                          {"a01", a01_private_key },
                          {"a02", a02_private_key },
                          {"a03", a03_private_key },
                          {"a04", a04_private_key },
                          };

    for( auto item : inits )
    {
      FUND( item.account, ASSET( "10000.000 TBD" ) );
    }
    //=====================preparing=====================

    auto item_creator = inits[ 0 ];
    for( auto i = 0; i < nr_proposals; ++i )
    {
      proposals_id.push_back( create_proposal( item_creator.account, receiver, start_date_00, end_date_00, daily_pay, item_creator.key ) );
      generate_block();
    }

    for( auto item : inits )
    {
      vote_proposal( item.account, proposals_id, true/*approve*/, item.key);
      generate_blocks( 1 );
    }

    const auto& proposal_idx = db->get_index< proposal_index >().indices().get< by_proposal_id >();
    const auto& proposal_vote_idx = db->get_index< proposal_vote_index >().indices().get< by_proposal_voter >();

    auto current_active_proposals = nr_proposals;
    BOOST_REQUIRE( calc_proposals( proposal_idx, proposals_id ) == current_active_proposals );

    auto current_active_votes = current_active_proposals * static_cast< int16_t > ( inits.size() );
    BOOST_REQUIRE( calc_votes( proposal_vote_idx, proposals_id ) == current_active_votes );

    auto threshold = db->get_remove_threshold();
    BOOST_REQUIRE( threshold == 20 );

    flat_set< int64_t > _proposals_id( proposals_id.begin(), proposals_id.end() );

    {
      remove_proposal( item_creator.account,  _proposals_id, item_creator.key );
      auto found_proposals = calc_proposals( proposal_idx, proposals_id );
      auto found_votes = calc_votes( proposal_vote_idx, proposals_id );
      BOOST_REQUIRE( found_proposals == 2 );
      BOOST_REQUIRE( found_votes == 8 );
      generate_blocks( 1 );
    }

    {
      remove_proposal( item_creator.account,  _proposals_id, item_creator.key );
      auto found_proposals = calc_proposals( proposal_idx, proposals_id );
      auto found_votes = calc_votes( proposal_vote_idx, proposals_id );
      BOOST_REQUIRE( found_proposals == 0 );
      BOOST_REQUIRE( found_votes == 0 );
      generate_blocks( 1 );
    }

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( proposals_removing_with_threshold_01 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: removing of old proposals/votes using threshold " );

    ACTORS( (a00)(a01)(a02)(a03)(a04)(a05) )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    //=====================preparing=====================
    auto receiver = db->get_treasury_name();

    auto start_time = db->head_block_time();

    const auto start_time_shift = fc::days( 100 );
    const auto end_time_shift = fc::days( 101 );

    auto start_date_00 = start_time + start_time_shift;
    auto end_date_00 = start_date_00 + end_time_shift;

    auto daily_pay = asset( 100, HBD_SYMBOL );

    const auto nr_proposals = 10;
    std::vector< int64_t > proposals_id;

    struct initial_data
    {
      std::string account;
      fc::ecc::private_key key;
    };

    std::vector< initial_data > inits = {
                          {"a00", a00_private_key },
                          {"a01", a01_private_key },
                          {"a02", a02_private_key },
                          {"a03", a03_private_key },
                          {"a04", a04_private_key },
                          {"a05", a05_private_key },
                          };

    for( auto item : inits )
    {
      FUND( item.account, ASSET( "10000.000 TBD" ) );
    }
    //=====================preparing=====================

    auto item_creator = inits[ 0 ];
    for( auto i = 0; i < nr_proposals; ++i )
    {
      proposals_id.push_back( create_proposal( item_creator.account, receiver, start_date_00, end_date_00, daily_pay, item_creator.key ) );
      generate_block();
    }

    auto itr_begin_2 = proposals_id.begin() + HIVE_PROPOSAL_MAX_IDS_NUMBER;
    std::vector< int64_t > v1( proposals_id.begin(), itr_begin_2 );
    std::vector< int64_t > v2( itr_begin_2, proposals_id.end() );

    for( auto item : inits )
    {
      vote_proposal( item.account, v1, true/*approve*/, item.key);
      generate_block();
      vote_proposal( item.account, v2, true/*approve*/, item.key);
      generate_block();
    }

    const auto& proposal_idx = db->get_index< proposal_index >().indices().get< by_proposal_id >();
    const auto& proposal_vote_idx = db->get_index< proposal_vote_index >().indices().get< by_proposal_voter >();

    auto current_active_proposals = nr_proposals;
    BOOST_REQUIRE( calc_proposals( proposal_idx, proposals_id ) == current_active_proposals );

    auto current_active_votes = current_active_proposals * static_cast< int16_t > ( inits.size() );
    BOOST_REQUIRE( calc_votes( proposal_vote_idx, proposals_id ) == current_active_votes );

    auto threshold = db->get_remove_threshold();
    BOOST_REQUIRE( threshold == 20 );

    /*
      nr_proposals = 10
      nr_votes_per_proposal = 6

      Info:
        Pn - number of proposal
        vn - number of vote
        xx - elements removed in given block
        oo - elements removed earlier
        rr - elements with status `removed`

      Below in matrix is situation before removal any element.

      P0 P1 P2 P3 P4 P5 P6 P7 P8 P9

      v0 v0 v0 v0 v0 v0 v0 v0 v0 v0
      v1 v1 v1 v1 v1 v1 v1 v1 v1 v1
      v2 v2 v2 v2 v2 v2 v2 v2 v2 v2
      v3 v3 v3 v3 v3 v3 v3 v3 v3 v3
      v4 v4 v4 v4 v4 v4 v4 v4 v4 v4
      v5 v5 v5 v5 v5 v5 v5 v5 v5 v5

      Total number of elements to be removed:
      Total = nr_proposals * nr_votes_per_proposal + nr_proposals = 10 * 6 + 10 = 70
    */

    /*
      P0 P1 P2 xx P4 P5 P6 P7 P8 P9

      v0 v0 v0 xx v0 v0 v0 v0 v0 v0
      v1 v1 v1 xx v1 v1 v1 v1 v1 v1
      v2 v2 v2 xx v2 v2 v2 v2 v2 v2
      v3 v3 v3 xx v3 v3 v3 v3 v3 v3
      v4 v4 v4 xx v4 v4 v4 v4 v4 v4
      v5 v5 v5 xx v5 v5 v5 v5 v5 v5
    */
    {
      remove_proposal( item_creator.account, {3}, item_creator.key );
      generate_block();
      auto found_proposals = calc_proposals( proposal_idx, proposals_id );
      auto found_votes = calc_votes( proposal_vote_idx, proposals_id );
      BOOST_REQUIRE( found_proposals == 9 );
      BOOST_REQUIRE( found_votes == 54 );

      int32_t cnt = 0;
      for( auto id : proposals_id )
        cnt += ( id != 3 ) ? calc_proposal_votes( proposal_vote_idx, id ) : 0;
      BOOST_REQUIRE( cnt == found_votes );
    }

    /*
      P0 xx xx oo P4 P5 P6 rr P8 P9

      v0 xx xx oo v0 v0 v0 xx v0 v0
      v1 xx xx oo v1 v1 v1 xx v1 v1
      v2 xx xx oo v2 v2 v2 xx v2 v2
      v3 xx xx oo v3 v3 v3 xx v3 v3
      v4 xx xx oo v4 v4 v4 xx v4 v4
      v5 xx xx oo v5 v5 v5 xx v5 v5
    */
    {
      remove_proposal( item_creator.account, {1,2,7}, item_creator.key );
      auto found_proposals = calc_proposals( proposal_idx, proposals_id );
      auto found_votes = calc_votes( proposal_vote_idx, proposals_id );
      BOOST_REQUIRE( exist_proposal( 7 ) && find_proposal( 7 )->removed );
      BOOST_REQUIRE( found_proposals == 7 );
      BOOST_REQUIRE( found_votes == 36 );
    }


    /*
      P0 oo oo oo P4 P5 P6 xx P8 P9

      v0 oo oo oo v0 v0 v0 oo v0 v0
      v1 oo oo oo v1 v1 v1 oo v1 v1
      v2 oo oo oo v2 v2 v2 oo v2 v2
      v3 oo oo oo v3 v3 v3 oo v3 v3
      v4 oo oo oo v4 v4 v4 oo v4 v4
      v5 oo oo oo v5 v5 v5 oo v5 v5
    */
    {
      //Only the proposal P7 is removed.
      generate_block();

      auto found_proposals = calc_proposals( proposal_idx, proposals_id );
      auto found_votes = calc_votes( proposal_vote_idx, proposals_id );
      BOOST_REQUIRE( !exist_proposal( 7 ) );
      BOOST_REQUIRE( found_proposals == 6 );
      BOOST_REQUIRE( found_votes == 36 );
    }

    /*
      P0 oo oo oo P4 xx xx oo rr rr

      v0 oo oo oo v0 xx xx oo xx rr
      v1 oo oo oo v1 xx xx oo xx rr
      v2 oo oo oo v2 xx xx oo xx rr
      v3 oo oo oo v3 xx xx oo xx rr
      v4 oo oo oo v4 xx xx oo xx rr
      v5 oo oo oo v5 xx xx oo xx rr
    */
    {
      remove_proposal( item_creator.account, {5,6,7/*doesn't exist*/,8,9}, item_creator.key );
      auto found_proposals = calc_proposals( proposal_idx, proposals_id );
      auto found_votes = calc_votes( proposal_vote_idx, proposals_id );
      BOOST_REQUIRE( exist_proposal( 8 ) && find_proposal( 8 )->removed );
      BOOST_REQUIRE( found_proposals == 4 );
      BOOST_REQUIRE( found_votes == 18 );

      int32_t cnt = 0;
      for( auto id : proposals_id )
        cnt += ( id == 0 || id == 4 || id == 8 || id == 9 ) ? calc_proposal_votes( proposal_vote_idx, id ) : 0;
      BOOST_REQUIRE( cnt == found_votes );
    }

    /*
      xx oo oo oo xx oo oo oo xx rr

      xx oo oo oo xx oo oo oo oo rr
      xx oo oo oo xx oo oo oo oo xx
      xx oo oo oo xx oo oo oo oo xx
      xx oo oo oo xx oo oo oo oo xx
      xx oo oo oo xx oo oo oo oo xx
      xx oo oo oo xx oo oo oo oo xx
    */
    {
      remove_proposal( item_creator.account, {0,4,8,9}, item_creator.key );
      auto found_proposals = calc_proposals( proposal_idx, proposals_id );
      auto found_votes = calc_votes( proposal_vote_idx, proposals_id );
      BOOST_REQUIRE( exist_proposal( 9 ) && find_proposal( 9 )->removed );
      BOOST_REQUIRE( found_proposals == 1 );
      BOOST_REQUIRE( found_votes == 1 );
    }

    /*
      oo oo oo oo oo oo oo oo oo xx

      oo oo oo oo oo oo oo oo oo xx
      oo oo oo oo oo oo oo oo oo oo
      oo oo oo oo oo oo oo oo oo oo
      oo oo oo oo oo oo oo oo oo oo
      oo oo oo oo oo oo oo oo oo oo
      oo oo oo oo oo oo oo oo oo oo
    */
    {
      //Only the proposal P9 is removed.
      generate_block();

      auto found_proposals = calc_proposals( proposal_idx, proposals_id );
      auto found_votes = calc_votes( proposal_vote_idx, proposals_id );
      BOOST_REQUIRE( found_proposals == 0 );
      BOOST_REQUIRE( found_votes == 0 );
    }

  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( proposals_removing_with_threshold_02 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: removing of old proposals/votes using threshold " );

    ACTORS(
          (a00)(a01)(a02)(a03)(a04)(a05)(a06)(a07)(a08)(a09)
          (a10)(a11)(a12)(a13)(a14)(a15)(a16)(a17)(a18)(a19)
          (a20)(a21)(a22)(a23)(a24)(a25)(a26)(a27)(a28)(a29)
          (a30)(a31)(a32)(a33)(a34)(a35)(a36)(a37)(a38)(a39)
          (a40)(a41)(a42)(a43)(a44)(a45)(a46)(a47)(a48)(a49)
        )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    //=====================preparing=====================
    auto receiver = db->get_treasury_name();

    auto start_time = db->head_block_time();

    const auto start_time_shift = fc::days( 100 );
    const auto end_time_shift = fc::days( 101 );

    auto start_date_00 = start_time + start_time_shift;
    auto end_date_00 = start_date_00 + end_time_shift;

    auto daily_pay = asset( 100, HBD_SYMBOL );

    const auto nr_proposals = 5;
    std::vector< int64_t > proposals_id;

    struct initial_data
    {
      std::string account;
      fc::ecc::private_key key;
    };

    std::vector< initial_data > inits = {
      {"a00", a00_private_key }, {"a01", a01_private_key }, {"a02", a02_private_key }, {"a03", a03_private_key }, {"a04", a04_private_key },
      {"a05", a05_private_key }, {"a06", a06_private_key }, {"a07", a07_private_key }, {"a08", a08_private_key }, {"a09", a09_private_key },
      {"a10", a10_private_key }, {"a11", a11_private_key }, {"a12", a12_private_key }, {"a13", a13_private_key }, {"a14", a14_private_key },
      {"a15", a15_private_key }, {"a16", a16_private_key }, {"a17", a17_private_key }, {"a18", a18_private_key }, {"a19", a19_private_key },
      {"a20", a20_private_key }, {"a21", a21_private_key }, {"a22", a22_private_key }, {"a23", a23_private_key }, {"a24", a24_private_key },
      {"a25", a25_private_key }, {"a26", a26_private_key }, {"a27", a27_private_key }, {"a28", a28_private_key }, {"a29", a29_private_key },
      {"a30", a30_private_key }, {"a31", a31_private_key }, {"a32", a32_private_key }, {"a33", a33_private_key }, {"a34", a34_private_key },
      {"a35", a35_private_key }, {"a36", a36_private_key }, {"a37", a37_private_key }, {"a38", a38_private_key }, {"a39", a39_private_key },
      {"a40", a40_private_key }, {"a41", a41_private_key }, {"a42", a42_private_key }, {"a43", a43_private_key }, {"a44", a44_private_key },
      {"a45", a45_private_key }, {"a46", a46_private_key }, {"a47", a47_private_key }, {"a48", a48_private_key }, {"a49", a49_private_key }
    };

    for( auto item : inits )
    {
      FUND( item.account, ASSET( "10000.000 TBD" ) );
    }
    //=====================preparing=====================

    auto item_creator = inits[ 0 ];
    for( auto i = 0; i < nr_proposals; ++i )
    {
      proposals_id.push_back( create_proposal( item_creator.account, receiver, start_date_00, end_date_00, daily_pay, item_creator.key ) );
      generate_block();
    }

    for( auto item : inits )
    {
      vote_proposal( item.account, proposals_id, true/*approve*/, item.key);
      generate_block();
    }

    const auto& proposal_idx = db->get_index< proposal_index >().indices().get< by_proposal_id >();
    const auto& proposal_vote_idx = db->get_index< proposal_vote_index >().indices().get< by_proposal_voter >();

    auto current_active_proposals = nr_proposals;
    BOOST_REQUIRE( calc_proposals( proposal_idx, proposals_id ) == current_active_proposals );

    auto current_active_votes = current_active_proposals * static_cast< int16_t > ( inits.size() );
    BOOST_REQUIRE( calc_votes( proposal_vote_idx, proposals_id ) == current_active_votes );

    auto threshold = db->get_remove_threshold();
    BOOST_REQUIRE( threshold == 20 );

    /*
      nr_proposals = 5
      nr_votes_per_proposal = 50

      Info:
        Pn - number of proposal
        vn - number of vote
        xx - elements removed in given block
        oo - elements removed earlier
        rr - elements with status `removed`

      Below in matrix is situation before removal any element.

      P0    v0 v1 v2 v3 v4 v5 v6 v7 v8 v9 v10 v11 v12 v13 v14 v15 v16 v17 v18 v19 v20 v21 v22 v23 v24 v25 v26 v27 v28 v29 v30 v31 v32 v33 v34 v35 v36 v37 v38 v39 v40 v41 v42 v43 v44 v45 v46 v47 v48 v49
      P1    v0 v1 v2 v3 v4 v5 v6 v7 v8 v9 v10 v11 v12 v13 v14 v15 v16 v17 v18 v19 v20 v21 v22 v23 v24 v25 v26 v27 v28 v29 v30 v31 v32 v33 v34 v35 v36 v37 v38 v39 v40 v41 v42 v43 v44 v45 v46 v47 v48 v49
      P2    v0 v1 v2 v3 v4 v5 v6 v7 v8 v9 v10 v11 v12 v13 v14 v15 v16 v17 v18 v19 v20 v21 v22 v23 v24 v25 v26 v27 v28 v29 v30 v31 v32 v33 v34 v35 v36 v37 v38 v39 v40 v41 v42 v43 v44 v45 v46 v47 v48 v49
      P3    v0 v1 v2 v3 v4 v5 v6 v7 v8 v9 v10 v11 v12 v13 v14 v15 v16 v17 v18 v19 v20 v21 v22 v23 v24 v25 v26 v27 v28 v29 v30 v31 v32 v33 v34 v35 v36 v37 v38 v39 v40 v41 v42 v43 v44 v45 v46 v47 v48 v49
      P4    v0 v1 v2 v3 v4 v5 v6 v7 v8 v9 v10 v11 v12 v13 v14 v15 v16 v17 v18 v19 v20 v21 v22 v23 v24 v25 v26 v27 v28 v29 v30 v31 v32 v33 v34 v35 v36 v37 v38 v39 v40 v41 v42 v43 v44 v45 v46 v47 v48 v49

      Total number of elements to be removed:
      Total = nr_proposals * nr_votes_per_proposal + nr_proposals = 5 * 50 + 5 = 255
    */

    /*
      rr    xx xx xx xx xx xx xx xx xx xx rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr
      P1    v0 v1 v2 v3 v4 v5 v6 v7 v8 v9 v10 v11 v12 v13 v14 v15 v16 v17 v18 v19 v20 v21 v22 v23 v24 v25 v26 v27 v28 v29 v30 v31 v32 v33 v34 v35 v36 v37 v38 v39 v40 v41 v42 v43 v44 v45 v46 v47 v48 v49
      P2    v0 v1 v2 v3 v4 v5 v6 v7 v8 v9 v10 v11 v12 v13 v14 v15 v16 v17 v18 v19 v20 v21 v22 v23 v24 v25 v26 v27 v28 v29 v30 v31 v32 v33 v34 v35 v36 v37 v38 v39 v40 v41 v42 v43 v44 v45 v46 v47 v48 v49
      rr    xx xx xx xx xx xx xx xx xx xx rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr
      P4    v0 v1 v2 v3 v4 v5 v6 v7 v8 v9 v10 v11 v12 v13 v14 v15 v16 v17 v18 v19 v20 v21 v22 v23 v24 v25 v26 v27 v28 v29 v30 v31 v32 v33 v34 v35 v36 v37 v38 v39 v40 v41 v42 v43 v44 v45 v46 v47 v48 v49
    */
    {
      remove_proposal( item_creator.account, {0,3}, item_creator.key );
      auto found_proposals = calc_proposals( proposal_idx, proposals_id );
      auto found_votes = calc_votes( proposal_vote_idx, proposals_id );
      BOOST_REQUIRE( exist_proposal( 0 ) && find_proposal( 0 )->removed );
      BOOST_REQUIRE( exist_proposal( 3 ) && find_proposal( 3 )->removed );
      BOOST_REQUIRE( found_proposals == 5 );
      BOOST_REQUIRE( found_votes == 230 );
    }

    /*
      rr    xx xx xx xx xx xx xx xx xx xx xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr
      P1    v0 v1 v2 v3 v4 v5 v6 v7 v8 v9 v10 v11 v12 v13 v14 v15 v16 v17 v18 v19 v20 v21 v22 v23 v24 v25 v26 v27 v28 v29 v30 v31 v32 v33 v34 v35 v36 v37 v38 v39 v40 v41 v42 v43 v44 v45 v46 v47 v48 v49
      P2    v0 v1 v2 v3 v4 v5 v6 v7 v8 v9 v10 v11 v12 v13 v14 v15 v16 v17 v18 v19 v20 v21 v22 v23 v24 v25 v26 v27 v28 v29 v30 v31 v32 v33 v34 v35 v36 v37 v38 v39 v40 v41 v42 v43 v44 v45 v46 v47 v48 v49
      rr    xx xx xx xx xx xx xx xx xx xx xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr
      P4    v0 v1 v2 v3 v4 v5 v6 v7 v8 v9 v10 v11 v12 v13 v14 v15 v16 v17 v18 v19 v20 v21 v22 v23 v24 v25 v26 v27 v28 v29 v30 v31 v32 v33 v34 v35 v36 v37 v38 v39 v40 v41 v42 v43 v44 v45 v46 v47 v48 v49
    */
    {
      generate_block();

      auto found_proposals = calc_proposals( proposal_idx, proposals_id );
      auto found_votes = calc_votes( proposal_vote_idx, proposals_id );
      BOOST_REQUIRE( exist_proposal( 0 ) && find_proposal( 0 )->removed );
      BOOST_REQUIRE( exist_proposal( 3 ) && find_proposal( 3 )->removed );
      BOOST_REQUIRE( found_proposals == 5 );
      BOOST_REQUIRE( found_votes == 210 );
    }

    /*
      xx    oo oo oo oo oo oo oo oo oo oo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx
      P1    v0 v1 v2 v3 v4 v5 v6 v7 v8 v9 v10 v11 v12 v13 v14 v15 v16 v17 v18 v19 v20 v21 v22 v23 v24 v25 v26 v27 v28 v29 v30 v31 v32 v33 v34 v35 v36 v37 v38 v39 v40 v41 v42 v43 v44 v45 v46 v47 v48 v49
      P2    v0 v1 v2 v3 v4 v5 v6 v7 v8 v9 v10 v11 v12 v13 v14 v15 v16 v17 v18 v19 v20 v21 v22 v23 v24 v25 v26 v27 v28 v29 v30 v31 v32 v33 v34 v35 v36 v37 v38 v39 v40 v41 v42 v43 v44 v45 v46 v47 v48 v49
      xx    oo oo oo oo oo oo oo oo oo oo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx
      P4    v0 v1 v2 v3 v4 v5 v6 v7 v8 v9 v10 v11 v12 v13 v14 v15 v16 v17 v18 v19 v20 v21 v22 v23 v24 v25 v26 v27 v28 v29 v30 v31 v32 v33 v34 v35 v36 v37 v38 v39 v40 v41 v42 v43 v44 v45 v46 v47 v48 v49
    */
    {
      generate_block();
      generate_block();
      generate_block();
      generate_block();

      auto found_proposals = calc_proposals( proposal_idx, proposals_id );
      auto found_votes = calc_votes( proposal_vote_idx, proposals_id );
      BOOST_REQUIRE( !exist_proposal( 0 ) );
      BOOST_REQUIRE( !exist_proposal( 3 ) );
      BOOST_REQUIRE( found_proposals == 3 );
      BOOST_REQUIRE( found_votes == 150 );
    }

    /*
      nothing changed - the same state as in previous block

      oo    oo oo oo oo oo oo oo oo oo oo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo
      P1    v0 v1 v2 v3 v4 v5 v6 v7 v8 v9 v10 v11 v12 v13 v14 v15 v16 v17 v18 v19 v20 v21 v22 v23 v24 v25 v26 v27 v28 v29 v30 v31 v32 v33 v34 v35 v36 v37 v38 v39 v40 v41 v42 v43 v44 v45 v46 v47 v48 v49
      P2    v0 v1 v2 v3 v4 v5 v6 v7 v8 v9 v10 v11 v12 v13 v14 v15 v16 v17 v18 v19 v20 v21 v22 v23 v24 v25 v26 v27 v28 v29 v30 v31 v32 v33 v34 v35 v36 v37 v38 v39 v40 v41 v42 v43 v44 v45 v46 v47 v48 v49
      oo    oo oo oo oo oo oo oo oo oo oo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo
      P4    v0 v1 v2 v3 v4 v5 v6 v7 v8 v9 v10 v11 v12 v13 v14 v15 v16 v17 v18 v19 v20 v21 v22 v23 v24 v25 v26 v27 v28 v29 v30 v31 v32 v33 v34 v35 v36 v37 v38 v39 v40 v41 v42 v43 v44 v45 v46 v47 v48 v49
    */
    {
      generate_block();

      auto found_proposals = calc_proposals( proposal_idx, proposals_id );
      auto found_votes = calc_votes( proposal_vote_idx, proposals_id );
      BOOST_REQUIRE( !exist_proposal( 0 ) );
      BOOST_REQUIRE( !exist_proposal( 3 ) );
      BOOST_REQUIRE( found_proposals == 3 );
      BOOST_REQUIRE( found_votes == 150 );
    }

    /*
      nothing changed - the same state as in previous block
    */
    {
      generate_block();

      auto found_proposals = calc_proposals( proposal_idx, proposals_id );
      auto found_votes = calc_votes( proposal_vote_idx, proposals_id );
      BOOST_REQUIRE( !exist_proposal( 0 ) );
      BOOST_REQUIRE( !exist_proposal( 3 ) );
      BOOST_REQUIRE( found_proposals == 3 );
      BOOST_REQUIRE( found_votes == 150 );
    }

    /*
      oo    oo oo oo oo oo oo oo oo oo oo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo
      rr    xx xx xx xx xx xx xx xx xx xx xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr
      P2    v0 v1 v2 v3 v4 v5 v6 v7 v8 v9 v10 v11 v12 v13 v14 v15 v16 v17 v18 v19 v20 v21 v22 v23 v24 v25 v26 v27 v28 v29 v30 v31 v32 v33 v34 v35 v36 v37 v38 v39 v40 v41 v42 v43 v44 v45 v46 v47 v48 v49
      oo    oo oo oo oo oo oo oo oo oo oo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo
      P4    v0 v1 v2 v3 v4 v5 v6 v7 v8 v9 v10 v11 v12 v13 v14 v15 v16 v17 v18 v19 v20 v21 v22 v23 v24 v25 v26 v27 v28 v29 v30 v31 v32 v33 v34 v35 v36 v37 v38 v39 v40 v41 v42 v43 v44 v45 v46 v47 v48 v49
    */
    {
      remove_proposal( item_creator.account, {1}, item_creator.key );
      auto found_proposals = calc_proposals( proposal_idx, proposals_id );
      auto found_votes = calc_votes( proposal_vote_idx, proposals_id );
      BOOST_REQUIRE( exist_proposal( 1 ) && find_proposal( 1 )->removed );
      BOOST_REQUIRE( found_proposals == 3 );
      BOOST_REQUIRE( found_votes == 130 );
    }

    /*
      oo    oo oo oo oo oo oo oo oo oo oo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo
      rr    xx xx xx xx xx xx xx xx xx xx xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr
      rr    xx xx xx xx xx xx xx xx xx xx xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr rrr
      oo    oo oo oo oo oo oo oo oo oo oo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo
      P4    v0 v1 v2 v3 v4 v5 v6 v7 v8 v9 v10 v11 v12 v13 v14 v15 v16 v17 v18 v19 v20 v21 v22 v23 v24 v25 v26 v27 v28 v29 v30 v31 v32 v33 v34 v35 v36 v37 v38 v39 v40 v41 v42 v43 v44 v45 v46 v47 v48 v49
    */
    {
      remove_proposal( item_creator.account, {2}, item_creator.key );
      auto found_proposals = calc_proposals( proposal_idx, proposals_id );
      auto found_votes = calc_votes( proposal_vote_idx, proposals_id );
      BOOST_REQUIRE( exist_proposal( 1 ) && find_proposal( 1 )->removed );
      BOOST_REQUIRE( exist_proposal( 2 ) && find_proposal( 2 )->removed );
      BOOST_REQUIRE( found_proposals == 3 );
      BOOST_REQUIRE( found_votes == 110 );
    }

    /*
      oo    oo oo oo oo oo oo oo oo oo oo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo
      oo    oo oo oo oo oo oo oo oo oo oo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo
      rr    oo oo oo oo oo oo oo oo oo oo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo rrr
      oo    oo oo oo oo oo oo oo oo oo oo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo
      P4    v0 v1 v2 v3 v4 v5 v6 v7 v8 v9 v10 v11 v12 v13 v14 v15 v16 v17 v18 v19 v20 v21 v22 v23 v24 v25 v26 v27 v28 v29 v30 v31 v32 v33 v34 v35 v36 v37 v38 v39 v40 v41 v42 v43 v44 v45 v46 v47 v48 v49
    */
    {
      generate_block();
      generate_block();
      generate_block();

      auto found_proposals = calc_proposals( proposal_idx, proposals_id );
      auto found_votes = calc_votes( proposal_vote_idx, proposals_id );
      BOOST_REQUIRE( !exist_proposal( 1 ) );
      BOOST_REQUIRE( exist_proposal( 2 ) && find_proposal( 2 )->removed );
      BOOST_REQUIRE( found_proposals == 2 );
      BOOST_REQUIRE( found_votes == 51 );
    }

    /*
      oo    oo oo oo oo oo oo oo oo oo oo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo
      oo    oo oo oo oo oo oo oo oo oo oo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo
      xx    oo oo oo oo oo oo oo oo oo oo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo xxx
      oo    oo oo oo oo oo oo oo oo oo oo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo
      P4    v0 v1 v2 v3 v4 v5 v6 v7 v8 v9 v10 v11 v12 v13 v14 v15 v16 v17 v18 v19 v20 v21 v22 v23 v24 v25 v26 v27 v28 v29 v30 v31 v32 v33 v34 v35 v36 v37 v38 v39 v40 v41 v42 v43 v44 v45 v46 v47 v48 v49
    */
    {
      generate_block();

      auto found_proposals = calc_proposals( proposal_idx, proposals_id );
      auto found_votes = calc_votes( proposal_vote_idx, proposals_id );
      BOOST_REQUIRE( !exist_proposal( 1 ) );
      BOOST_REQUIRE( !exist_proposal( 2 ) );
      BOOST_REQUIRE( found_proposals == 1 );
      BOOST_REQUIRE( found_votes == 50 );
    }

    /*
      oo    oo oo oo oo oo oo oo oo oo oo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo
      oo    oo oo oo oo oo oo oo oo oo oo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo
      xx    oo oo oo oo oo oo oo oo oo oo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo xxx
      oo    oo oo oo oo oo oo oo oo oo oo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo
      P4    xx xx xx xx xx xx xx xx xx xx xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx v20 v21 v22 v23 v24 v25 v26 v27 v28 v29 v30 v31 v32 v33 v34 v35 v36 v37 v38 v39 v40 v41 v42 v43 v44 v45 v46 v47 v48 v49
    */
    {
      remove_proposal( item_creator.account, {4}, item_creator.key );
      auto found_proposals = calc_proposals( proposal_idx, proposals_id );
      auto found_votes = calc_votes( proposal_vote_idx, proposals_id );
      BOOST_REQUIRE( exist_proposal( 4 ) && find_proposal( 4 )->removed );
      BOOST_REQUIRE( found_proposals == 1 );
      BOOST_REQUIRE( found_votes == 30 );

      for( auto item : inits )
        vote_proposal( item.account, {4}, true/*approve*/, item.key);

      found_votes = calc_votes( proposal_vote_idx, proposals_id );
      BOOST_REQUIRE( found_votes == 30 );
    }

    /*
      oo    oo oo oo oo oo oo oo oo oo oo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo
      oo    oo oo oo oo oo oo oo oo oo oo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo
      rr    oo oo oo oo oo oo oo oo oo oo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo rrr
      oo    oo oo oo oo oo oo oo oo oo oo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo ooo
      P4    v0 v1 v2 v3 v4 v5 v6 v7 v8 v9 v10 v11 v12 v13 v14 v15 v16 v17 v18 v19 v20 v21 v22 v23 v24 v25 v26 v27 v28 v29 v30 v31 v32 v33 v34 v35 v36 v37 v38 v39 v40 v41 v42 v43 v44 v45 v46 v47 v48 v49
    */
    {
      generate_block();
      generate_block();

      auto found_proposals = calc_proposals( proposal_idx, proposals_id );
      auto found_votes = calc_votes( proposal_vote_idx, proposals_id );
      BOOST_REQUIRE( !exist_proposal( 4 ) );
      BOOST_REQUIRE( found_proposals == 0 );
      BOOST_REQUIRE( found_votes == 0 );

      for( auto item : inits )
        vote_proposal( item.account, {4}, true/*approve*/, item.key);

      found_votes = calc_votes( proposal_vote_idx, proposals_id );
      BOOST_REQUIRE( found_votes == 0 );
    }
  }
  FC_LOG_AND_RETHROW()
}



BOOST_AUTO_TEST_CASE( update_proposal_000 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: update proposal - update subject" );

    ACTORS( (alice)(bob) )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    auto creator = "alice";
    auto receiver = "bob";

    auto start_date = db->head_block_time() + fc::days( 1 );
    auto end_date = start_date + fc::days( 2 );

    auto daily_pay = asset( 100, HBD_SYMBOL );

    auto subject = "hello";
    auto permlink = "somethingpermlink";
    auto new_permlink = "somethingpermlink2";

    post_comment_with_block_generation(creator, permlink, "title", "body", "test", alice_private_key);
    post_comment_with_block_generation(creator, new_permlink, "title", "body", "test", alice_private_key);

    FUND( creator, ASSET( "80.000 TBD" ) );

    signed_transaction tx;

    BOOST_TEST_MESSAGE("-- creating");
    create_proposal_operation op;

    op.creator = creator;
    op.receiver = receiver;

    op.start_date = start_date;
    op.end_date = end_date;

    op.daily_pay = daily_pay;

    op.subject = subject;
    op.permlink = permlink;

    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );

    push_transaction( tx, alice_private_key, 0 );
    tx.operations.clear();
    tx.signatures.clear();

    const auto& proposal_idx = db->get_index< proposal_index >().indices().get< by_creator >();
    auto found = proposal_idx.find( creator );
    BOOST_REQUIRE( found != proposal_idx.end() );

    BOOST_REQUIRE( found->creator == creator );
    BOOST_REQUIRE( found->receiver == receiver );
    BOOST_REQUIRE( found->start_date == start_date );
    BOOST_REQUIRE( found->end_date == end_date );
    BOOST_REQUIRE( found->daily_pay == daily_pay );
    BOOST_REQUIRE( found->subject == subject );
    BOOST_REQUIRE( found->permlink == permlink );

    BOOST_TEST_MESSAGE("-- updating");

    time_point_sec new_end_date = end_date - fc::days( 1 );
    auto new_subject = "Other subject";
    auto new_daily_pay = asset( 100, HBD_SYMBOL );

    update_proposal(found->proposal_id, creator, new_daily_pay, new_subject, new_permlink, alice_private_key);
    generate_block();
    found = proposal_idx.find( creator );
    BOOST_REQUIRE( found->creator == creator );
    BOOST_REQUIRE( found->receiver == receiver );
    BOOST_REQUIRE( found->start_date == start_date );
    BOOST_REQUIRE( found->end_date == end_date );
    BOOST_REQUIRE( found->daily_pay == new_daily_pay );
    BOOST_REQUIRE( found->subject == new_subject );
    BOOST_REQUIRE( found->permlink == new_permlink );

    update_proposal(found->proposal_id, creator, new_daily_pay, new_subject, new_permlink, alice_private_key, &new_end_date);
    generate_block();
    found = proposal_idx.find( creator );
    BOOST_REQUIRE( found->creator == creator );
    BOOST_REQUIRE( found->receiver == receiver );
    BOOST_REQUIRE( found->start_date == start_date );
    BOOST_REQUIRE( found->end_date == new_end_date );
    BOOST_REQUIRE( found->daily_pay == new_daily_pay );
    BOOST_REQUIRE( found->subject == new_subject );
    BOOST_REQUIRE( found->permlink == new_permlink );

    validate_database();
  } FC_LOG_AND_RETHROW()
}


BOOST_AUTO_TEST_CASE( update_proposal_001 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: update proposal: operation arguments validation - invalid id" );
    ACTORS( (alice)(bob) )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    auto creator    = "alice";
    auto receiver   = "bob";
    time_point_sec start_date = db->head_block_time() + fc::days( 1 );
    auto end_date   = start_date + fc::days( 2 );
    auto daily_pay  = asset( 100, HBD_SYMBOL );
    auto subject = "hello";
    auto permlink = "somethingpermlink";

    FUND( creator, ASSET( "80.000 TBD" ) );

    signed_transaction tx;

    post_comment_with_block_generation(creator, permlink, "title", "body", "test", alice_private_key);

    create_proposal_operation op;
    op.creator = creator;
    op.receiver = receiver;
    op.start_date = start_date;
    op.end_date = end_date;
    op.daily_pay = daily_pay;
    op.subject = subject;
    op.permlink = permlink;
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    push_transaction( tx, alice_private_key, 0 );

    tx.operations.clear();
    tx.signatures.clear();

    HIVE_REQUIRE_THROW( update_proposal(50, creator, daily_pay, subject, permlink, alice_private_key), fc::exception);
    HIVE_REQUIRE_THROW( update_proposal(-50, creator, daily_pay, subject, permlink, alice_private_key), fc::exception);

    validate_database();
  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( update_proposal_002 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: update proposal: operation arguments validation - invalid creator" );
    ACTORS( (alice)(bob) )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    auto creator    = "alice";
    auto receiver   = "bob";
    auto start_date = db->head_block_time() + fc::days( 1 );
    auto end_date   = start_date + fc::days( 2 );
    auto daily_pay  = asset( 100, HBD_SYMBOL );
    auto subject = "hello";
    auto permlink = "somethingpermlink";

    FUND( creator, ASSET( "80.000 TBD" ) );

    signed_transaction tx;

    post_comment_with_block_generation(creator, permlink, "title", "body", "test", alice_private_key);

    create_proposal_operation op;
    op.creator = creator;
    op.receiver = receiver;
    op.start_date = start_date;
    op.end_date = end_date;
    op.daily_pay = daily_pay;
    op.subject = subject;
    op.permlink = permlink;
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    push_transaction( tx, alice_private_key, 0 );

    tx.operations.clear();
    tx.signatures.clear();

    const auto& proposal_idx = db->get_index< proposal_index >().indices().get< by_creator >();
    auto proposal = proposal_idx.find( creator );

    HIVE_REQUIRE_THROW( update_proposal(proposal->proposal_id, "bob", daily_pay, subject, permlink, alice_private_key), fc::exception);

    validate_database();
  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( update_proposal_003 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: update proposal: operation arguments validation - invalid daily pay" );
    ACTORS( (alice)(bob) )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    auto creator    = "alice";
    auto receiver   = "bob";
    auto start_date = db->head_block_time() + fc::days( 1 );
    auto end_date   = start_date + fc::days( 2 );
    auto daily_pay  = asset( 100, HBD_SYMBOL );
    auto subject = "hello";
    auto permlink = "somethingpermlink";

    FUND( creator, ASSET( "80.000 TBD" ) );

    signed_transaction tx;

    post_comment_with_block_generation(creator, permlink, "title", "body", "test", alice_private_key);

    create_proposal_operation op;
    op.creator = creator;
    op.receiver = receiver;
    op.start_date = start_date;
    op.end_date = end_date;
    op.daily_pay = daily_pay;
    op.subject = subject;
    op.permlink = permlink;
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    push_transaction( tx, alice_private_key, 0 );

    tx.operations.clear();
    tx.signatures.clear();

    const auto& proposal_idx = db->get_index< proposal_index >().indices().get< by_creator >();
    auto proposal = proposal_idx.find( creator );

    // Superior than current proposal
    HIVE_REQUIRE_THROW( update_proposal(proposal->proposal_id, creator, asset( 110, HBD_SYMBOL ), subject, permlink, alice_private_key), fc::exception);
    // Negative value
    HIVE_REQUIRE_THROW( update_proposal(proposal->proposal_id, creator, asset( -110, HBD_SYMBOL ), subject, permlink, alice_private_key), fc::exception);

    validate_database();
  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( update_proposal_004 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: update proposal: operation arguments validation - invalid subject" );
    ACTORS( (alice)(bob) )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    auto creator    = "alice";
    auto receiver   = "bob";
    auto start_date = db->head_block_time() + fc::days( 1 );
    auto end_date   = start_date + fc::days( 2 );
    auto daily_pay  = asset( 100, HBD_SYMBOL );
    auto subject = "hello";
    auto permlink = "somethingpermlink";

    FUND( creator, ASSET( "80.000 TBD" ) );

    signed_transaction tx;

    post_comment_with_block_generation(creator, permlink, "title", "body", "test", alice_private_key);

    create_proposal_operation op;
    op.creator = creator;
    op.receiver = receiver;
    op.start_date = start_date;
    op.end_date = end_date;
    op.daily_pay = daily_pay;
    op.subject = subject;
    op.permlink = permlink;
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    push_transaction( tx, alice_private_key, 0 );

    tx.operations.clear();
    tx.signatures.clear();

    const auto& proposal_idx = db->get_index< proposal_index >().indices().get< by_creator >();
    auto proposal = proposal_idx.find( creator );
    // Empty subject
    HIVE_REQUIRE_THROW( update_proposal(proposal->proposal_id, creator, asset( 110, HBD_SYMBOL ), "", permlink, alice_private_key), fc::exception);
    // Subject too long
    HIVE_REQUIRE_THROW( update_proposal(proposal->proposal_id, creator, asset( 110, HBD_SYMBOL ), "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
                                        permlink, alice_private_key), fc::exception);

    validate_database();
  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( update_proposal_005 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: update proposal: operation arguments validation - invalid permlink" );
    ACTORS( (alice)(bob)(dave) )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    auto creator    = "alice";
    auto receiver   = "bob";
    fc::time_point_sec start_date = db->head_block_time() + fc::days( 1 );
    auto end_date   = start_date + fc::days( 2 );
    auto daily_pay  = asset( 100, HBD_SYMBOL );
    auto subject = "hello";
    auto permlink = "somethingpermlink";

    FUND( creator, ASSET( "80.000 TBD" ) );

    signed_transaction tx;

    post_comment_with_block_generation(creator, permlink, "title", "body", "test", alice_private_key);
    post_comment_with_block_generation("dave", "davepermlink", "title", "body", "test", dave_private_key);

    create_proposal_operation op;
    op.creator = creator;
    op.receiver = receiver;
    op.start_date = start_date;
    op.end_date = end_date;
    op.daily_pay = daily_pay;
    op.subject = subject;
    op.permlink = permlink;
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    push_transaction( tx, alice_private_key, 0 );

    tx.operations.clear();
    tx.signatures.clear();

    const auto& proposal_idx = db->get_index< proposal_index >().indices().get< by_creator >();
    auto proposal = proposal_idx.find( creator );
    // Empty permlink
    HIVE_REQUIRE_THROW( update_proposal(proposal->proposal_id, creator, asset( 110, HBD_SYMBOL ), subject, "", alice_private_key), fc::exception);
    // Post doesn't exist
    HIVE_REQUIRE_THROW( update_proposal(proposal->proposal_id, creator, asset( 110, HBD_SYMBOL ), subject, "doesntexist", alice_private_key), fc::exception);
    // Post exists but is made by an user that is neither the receiver or the creator
    HIVE_REQUIRE_THROW( update_proposal(proposal->proposal_id, creator, asset( 110, HBD_SYMBOL ), subject, "davepermlink", alice_private_key), fc::exception);

    validate_database();
  } FC_LOG_AND_RETHROW()
}


BOOST_AUTO_TEST_CASE( update_proposal_006 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: update proposal: operation arguments validation - invalid end_date" );
    ACTORS( (alice)(bob) )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    auto creator    = "alice";
    auto receiver   = "bob";
    fc::time_point_sec start_date = db->head_block_time() + fc::days( 1 );
    auto end_date   = start_date + fc::days( 2 );
    fc::time_point_sec end_date_invalid = start_date + fc::days( 3 );
    auto daily_pay  = asset( 100, HBD_SYMBOL );
    auto subject = "hello";
    auto permlink = "somethingpermlink";

    FUND( creator, ASSET( "80.000 TBD" ) );

    signed_transaction tx;

    post_comment_with_block_generation(creator, permlink, "title", "body", "test", alice_private_key);

    create_proposal_operation op;
    op.creator = creator;
    op.receiver = receiver;
    op.start_date = start_date;
    op.end_date = end_date;
    op.daily_pay = daily_pay;
    op.subject = subject;
    op.permlink = permlink;
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    push_transaction( tx, alice_private_key, 0 );

    tx.operations.clear();
    tx.signatures.clear();

    const auto& proposal_idx = db->get_index< proposal_index >().indices().get< by_creator >();
    auto proposal = proposal_idx.find( creator );
    HIVE_REQUIRE_THROW( update_proposal(proposal->proposal_id, creator, asset( 110, HBD_SYMBOL ), subject, permlink, alice_private_key, &start_date), fc::exception);
    HIVE_REQUIRE_THROW( update_proposal(proposal->proposal_id, creator, asset( 110, HBD_SYMBOL ), subject, permlink, alice_private_key, &end_date_invalid), fc::exception);

    validate_database();
  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()


BOOST_FIXTURE_TEST_SUITE( proposal_tests_performance, dhf_database_fixture_performance )

int32_t get_time( database& db, const std::string& name )
{
  int32_t benchmark_time = -1;

  db.get_benchmark_dumper().dump();

  fc::path benchmark_file( "advanced_benchmark.json" );

  if( !fc::exists( benchmark_file ) )
    return benchmark_time;

  /*
  {
  "total_time": 1421,
  "items": [{
      "op_name": "dhf_processor",
      "time": 80
    },...
  */
  auto file = fc::json::from_file( benchmark_file ).as< fc::variant >();
  if( file.is_object() )
  {
    auto vo = file.get_object();
    if( vo.contains( "items" ) )
    {
      auto items = vo[ "items" ];
      if( items.is_array() )
      {
        auto v = items.as< std::vector< fc::variant > >();
        for( auto& item : v )
        {
          if( !item.is_object() )
            continue;

          auto vo_item = item.get_object();
          if( vo_item.contains( "op_name" ) && vo_item[ "op_name" ].is_string() )
          {
            std::string str = vo_item[ "op_name" ].as_string();
            if( str == name )
            {
              if( vo_item.contains( "time" ) && vo_item[ "time" ].is_uint64() )
              {
                benchmark_time = vo_item[ "time" ].as_int64();
                break;
              }
            }
          }
        }
      }
    }
  }
  return benchmark_time;
}

struct initial_data
{
  std::string account;

  fc::ecc::private_key key;

  initial_data( database_fixture* db, const std::string& _account ): account( _account )
  {
    key = db->generate_private_key( account );

    db->account_create( account, key.get_public_key(), db->generate_private_key( account + "_post" ).get_public_key() );
  }
};

std::vector< initial_data > generate_accounts( database_fixture* db, int32_t number_accounts )
{
  const std::string basic_name = "tester";

  std::vector< initial_data > res;

  for( int32_t i = 0; i< number_accounts; ++i  )
  {
    std::string name = basic_name + std::to_string( i );
    res.push_back( initial_data( db, name ) );

    if( ( i + 1 ) % 100 == 0 )
      db->generate_block();

    if( ( i + 1 ) % 1000 == 0 )
      ilog( "Created: ${accs} accounts",( "accs", i+1 ) );
  }

  db->validate_database();
  db->generate_block();

  return res;
}

BOOST_AUTO_TEST_CASE( proposals_removing_with_threshold_03 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: removing of all proposals/votes in one block using threshold = -1" );

    std::vector< initial_data > inits = generate_accounts( this, 200 );

    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    //=====================preparing=====================
    auto receiver = db->get_treasury_name();

    auto start_time = db->head_block_time();

    const auto start_time_shift = fc::hours( 20 );
    const auto end_time_shift = fc::hours( 6 );
    const auto block_interval = fc::seconds( HIVE_BLOCK_INTERVAL );

    auto start_date_00 = start_time + start_time_shift;
    auto end_date_00 = start_date_00 + end_time_shift;

    auto daily_pay = asset( 100, HBD_SYMBOL );

    const auto nr_proposals = 200;
    std::vector< int64_t > proposals_id;

    struct initial_data
    {
      std::string account;
      fc::ecc::private_key key;
    };

    for( auto item : inits )
    {
      FUND( item.account, ASSET( "10000.000 TBD" ) );
    }
    //=====================preparing=====================

    for( auto i = 0; i < nr_proposals; ++i )
    {
      auto item = inits[ i ];
      proposals_id.push_back( create_proposal( item.account, receiver, start_date_00, end_date_00, daily_pay, item.key ) );
      if( ( i + 1 ) % 10 == 0 )
        generate_block();
    }
    generate_block();

    std::vector< int64_t > ids;
    uint32_t i = 0;
    for( auto id : proposals_id )
    {
      ids.push_back( id );
      if( ids.size() == HIVE_PROPOSAL_MAX_IDS_NUMBER )
      {
        for( auto item : inits )
        {
          ++i;
          vote_proposal( item.account, ids, true/*approve*/, item.key );
          if( ( i + 1 ) % 10 == 0 )
            generate_block();
        }
        ids.clear();
      }
    }
    generate_block();

    const auto& proposal_idx = db->get_index< proposal_index >().indices().get< by_proposal_id >();
    const auto& proposal_vote_idx = db->get_index< proposal_vote_index >().indices().get< by_proposal_voter >();

    auto current_active_proposals = nr_proposals;
    BOOST_REQUIRE( calc_proposals( proposal_idx, proposals_id ) == current_active_proposals );

    auto current_active_votes = current_active_proposals * static_cast< int16_t > ( inits.size() );
    BOOST_REQUIRE( calc_votes( proposal_vote_idx, proposals_id ) == current_active_votes );

    auto threshold = db->get_remove_threshold();
    BOOST_REQUIRE( threshold == -1 );

    generate_blocks( start_time + fc::seconds( HIVE_PROPOSAL_MAINTENANCE_CLEANUP ) );
    start_time = db->head_block_time();

    generate_blocks( start_time + ( start_time_shift + end_time_shift - block_interval ) );

    generate_blocks( 1 );

    /*
      Take a look at comment in `dhf_processor::remove_proposals`
    */
    //BOOST_REQUIRE( calc_proposals( proposal_idx, proposals_id ) == 0 );                       //earlier
    //BOOST_REQUIRE( calc_votes( proposal_vote_idx, proposals_id ) == 0 );                      //earlier
    BOOST_REQUIRE( calc_proposals( proposal_idx, proposals_id ) == current_active_proposals );  //now
    BOOST_REQUIRE( calc_votes( proposal_vote_idx, proposals_id ) == current_active_votes );     //now

    int32_t benchmark_time = get_time( *db, dhf_processor::get_removing_name() );
    idump( (benchmark_time) );
    BOOST_REQUIRE( benchmark_time == -1 || benchmark_time < 100 );

    /*
      Local test: 4 cores/16 MB RAM
      nr objects to be removed: `40200`
      time of removing: 80 ms
      speed of removal: ~502/ms
    */
  }
  FC_LOG_AND_RETHROW()
}

// This generating payment is part of proposal_tests_performance
BOOST_AUTO_TEST_CASE( generating_payments )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: generating payments for a lot of accounts" );

    std::vector< initial_data > inits = generate_accounts( this, 30000 );

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    //=====================preparing=====================
    const auto nr_proposals = 5;
    std::vector< int64_t > proposals_id;
    flat_map< std::string, asset > before_tbds;

    auto call = [&]( uint32_t& i, uint32_t max, const std::string& info )
    {
      ++i;
      if( i % max == 0 )
        generate_block();

      if( i % 1000 == 0 )
        ilog( info.c_str(),( "x", i ) );
    };

    uint32_t i = 0;
    for( auto item : inits )
    {
      if( i < 5 )
      {
        FUND( item.account, ASSET( "11.000 TBD" ) );
      }
      vest(HIVE_INIT_MINER_NAME, item.account, ASSET( "30.000 TESTS" ));

      call( i, 50, "${x} accounts got VESTS" );
    }

    auto start_time = db->head_block_time();
    //Due to the `delaying votes` algorithm, generate blocks for 30 days in order to activate whole votes' pool ( take a look at `HIVE_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS` )
    start_time += fc::seconds( HIVE_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS );
    generate_blocks( start_time );

    const auto start_time_shift = fc::hours( 10 );
    const auto end_time_shift = fc::hours( 1 );
    const auto block_interval = fc::seconds( HIVE_BLOCK_INTERVAL );

    auto start_date = start_time + start_time_shift;
    auto end_date = start_date + end_time_shift;

    auto daily_pay = ASSET( "24.000 TBD" );
    auto paid = ASSET( "1.000 TBD" ); // because only 1 hour

    /* This unit test triggers an edge case where the maintenance window for proposals happens after 1h and 3s (one block)
      due to block skipping (in mainnet that can happen if maintenance block is missed).
      normal case:
      3600 (passed seconds) * 24000 (daily pay) / 86400 (seconds in day) = 1000 => 1.000 TBD
      edge case:
      3603 * 24000 / 86400 = 1000.833333 -> 1000 => still 1.000 TBD
      It is possible to miss more blocks and as a result get even longer time covered by maintenance giving more than
      expected pay in single maintenance, but in the same time payout periods will drift even more.
     */

    FUND( db->get_treasury_name(), ASSET( "5000000.000 TBD" ) );
    //=====================preparing=====================
    for( int32_t i = 0; i < nr_proposals; ++i )
    {
      auto item = inits[ i % inits.size() ];
      proposals_id.push_back( create_proposal( item.account, item.account, start_date, end_date, daily_pay, item.key ) );
      generate_block();
    }

    i = 0;
    for( auto item : inits )
    {
      vote_proposal( item.account, proposals_id, true/*approve*/, item.key );

      call( i, 100, "${x} accounts voted" );
    }

    for( int32_t i = 0; i < nr_proposals; ++i )
    {
      auto item = inits[ i % inits.size() ];
      const account_object& account = db->get_account( item.account );
      before_tbds[ item.account ] = account.get_hbd_balance();
    }

    generate_blocks( start_time + ( start_time_shift - block_interval ) );
    start_time = db->head_block_time();
    generate_blocks( start_time + end_time_shift + block_interval );

    for( int32_t i = 0; i < nr_proposals; ++i )
    {
      auto item = inits[ i % inits.size() ];
      const account_object& account = db->get_account( item.account );

      auto after_tbd = account.get_hbd_balance();
      auto before_tbd = before_tbds[ item.account ];
      idump( (before_tbd) );
      idump( (after_tbd) );
      idump( (paid) );
      BOOST_REQUIRE( before_tbd == after_tbd - paid );
    }

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( converting_hive_to_dhf )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: converting hive to hbd in the dhf" );
    const auto& dgpo = db->get_dynamic_global_properties();
    const account_object& _treasury = db->get_treasury();
    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );

    auto before_inflation_treasury_hbd_balance =  _treasury.get_hbd_balance();
    generate_block();
    auto treasury_per_block_inflation =  _treasury.get_hbd_balance() - before_inflation_treasury_hbd_balance;

    FUND( db->get_treasury_name(), ASSET( "100.000 TESTS" ) );
    generate_block();

    auto before_treasury_hbd_balance =  _treasury.get_hbd_balance();
    auto before_treasury_hive_balance =  _treasury.get_balance();

    const auto hive_converted = asset(HIVE_PROPOSAL_CONVERSION_RATE * before_treasury_hive_balance.amount / HIVE_100_PERCENT, HIVE_SYMBOL);
    // Same because of the 1:1 tests to tbd ratio
    const auto hbd_converted = asset(hive_converted.amount, HBD_SYMBOL);
    // Generate until the next daily maintenance
    auto next_block = get_nr_blocks_until_daily_maintenance_block();
    auto before_daily_maintenance_time = dgpo.next_daily_maintenance_time;
    generate_blocks( next_block - 1);

    auto treasury_hbd_inflation =  _treasury.get_hbd_balance() - before_treasury_hbd_balance;
    generate_block();

    treasury_hbd_inflation += treasury_per_block_inflation;
    auto after_daily_maintenance_time = dgpo.next_daily_maintenance_time;
    auto after_treasury_hbd_balance =  _treasury.get_hbd_balance();
    auto after_treasury_hive_balance =  _treasury.get_balance();

    BOOST_REQUIRE( before_treasury_hbd_balance == after_treasury_hbd_balance - treasury_hbd_inflation - hbd_converted );
    BOOST_REQUIRE( before_treasury_hive_balance == after_treasury_hive_balance + hive_converted );
    BOOST_REQUIRE( before_daily_maintenance_time == after_daily_maintenance_time - fc::seconds( HIVE_DAILY_PROPOSAL_MAINTENANCE_PERIOD )  );
    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()
