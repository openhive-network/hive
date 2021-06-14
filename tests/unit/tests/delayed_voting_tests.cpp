//#if defined(IS_TEST_NET)

#include <boost/test/unit_test.hpp>

#include <hive/chain/hive_fwd.hpp>

#include <hive/protocol/exceptions.hpp>
#include <hive/protocol/hardfork.hpp>

#include <hive/chain/database.hpp>
#include <hive/chain/database_exceptions.hpp>
#include <hive/chain/hive_objects.hpp>

#include <hive/chain/sps_objects.hpp>

#include <hive/plugins/rc/rc_objects.hpp>
#include <hive/plugins/rc/resource_count.hpp>

#include <hive/chain/util/delayed_voting_processor.hpp>
#include <hive/chain/util/delayed_voting.hpp>

#include <fc/macros.hpp>
#include <fc/crypto/digest.hpp>

#include "../db_fixture/database_fixture.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <deque>
#include <array>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"

using namespace hive;
using namespace hive::chain;
using namespace hive::protocol;
using fc::string;

constexpr int NR_INTERVALS_IN_DELAYED_VOTING{ (HIVE_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS / HIVE_DELAYED_VOTING_INTERVAL_SECONDS) };

#define VOTING_POWER( account ) db->get_account( account ).witness_vote_weight().value
#define PROXIED_VSF( account ) db->get_account( account ).proxied_vsf_votes_total().value
#define DELAYED_VOTES( account ) static_cast<int64_t>( db->get_account( account ).sum_delayed_votes.value )
#define CAN_VOTE( account ) db->get_account( account ).can_vote

namespace
{

std::string asset_to_string( const asset& a )
{
  return hive::plugins::condenser_api::legacy_asset::from_asset( a ).to_string();
}

} // namespace

// Tests with combined delayed voting and proposals
BOOST_FIXTURE_TEST_SUITE( delayed_voting_proposal_tests, delayed_vote_proposal_database_fixture )

BOOST_AUTO_TEST_CASE( delayed_proposal_test_01 )
{
  try
  {
    BOOST_TEST_MESSAGE( R"(Scenario:

  * create one proposal 
  * carol vests
  * carol sets `update_proposal_votes_operation`
  * each hour check if votes are constant
  * after 30 days proposal receives votes
    )");

    // const
    const auto TESTS_1000 = ASSET( "1000.000 TESTS" );
    const auto TBD_100 = ASSET( "100.000 TBD" );
    
    //setup
    ACTORS( (alice)(bob)(carol) )

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    FUND( "alice", TESTS_1000 );
    FUND( "bob",   TESTS_1000 );
    FUND( "carol", TESTS_1000 );

    FUND( "alice", TBD_100 );
    FUND( "bob", TBD_100 );
    FUND( "carol", TBD_100 );
    generate_block();
    
    // create one proposal
    create_proposal_data cpd(db->head_block_time());
    cpd.end_date = cpd.start_date + fc::days( 2* NR_INTERVALS_IN_DELAYED_VOTING );
    int64_t proposal_1 = create_proposal( cpd.creator, cpd.receiver, cpd.start_date, cpd.end_date, cpd.daily_pay, alice_private_key, false/*with_block_generation*/ );
    BOOST_REQUIRE(proposal_1 >= 0);

    // carol vest
    vest("carol", "carol", ASSET("10.000 TESTS"), carol_private_key);

    // carol votes
    vote_proposal("carol", { proposal_1 }, true, carol_private_key);

    const int cycle = 24;
    const int time_offset = HIVE_DELAYED_VOTING_INTERVAL_SECONDS / cycle;
    // check
    for(int i = 0; i < (cycle*NR_INTERVALS_IN_DELAYED_VOTING) - 1; i++)
    {
      generate_blocks( db->head_block_time() + fc::seconds( time_offset ).to_seconds());
      auto * ptr = find_proposal(proposal_1);
      BOOST_REQUIRE( ptr != nullptr );
      BOOST_REQUIRE( ptr->total_votes == 0ul );
    }

    generate_seconds_blocks( time_offset, true );
    generate_seconds_blocks( 3600, true );

    auto * ptr = find_proposal(proposal_1);
    BOOST_REQUIRE( ptr != nullptr );
    BOOST_REQUIRE_EQUAL( static_cast<long>( ptr->total_votes ), get_vesting( "carol" ).amount.value );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( delayed_proposal_test_02 )
{
  try
  {
    BOOST_TEST_MESSAGE( R"(Scenario:

  * create two proposals
  * carol vests
  * carol sets `update_proposal_votes_operation` for first proposal
  * each hour check if votes are constant
  * carol vests again and `update_proposal_votes_operation` for second proposal
  * after 30 days first proposal receives votes
  * after 15 days second one, too
    )");

    // const
    const auto TESTS_1000 = ASSET( "1000.000 TESTS" );
    const auto TBD_100 = ASSET( "100.000 TBD" );
    
    //setup
    ACTORS( (alice)(bob)(carol) )

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    FUND( "alice", TESTS_1000 );
    FUND( "bob",   TESTS_1000 );
    FUND( "carol", TESTS_1000 );

    FUND( "alice", TBD_100 );
    FUND( "bob", TBD_100 );
    FUND( "carol", TBD_100 );
    generate_block();
    
    // create one proposal
    create_proposal_data cpd1(db->head_block_time());
    cpd1.end_date = cpd1.start_date + fc::days( 2* NR_INTERVALS_IN_DELAYED_VOTING );
    cpd1.creator = "alice";
    int64_t proposal_1 = create_proposal( cpd1.creator, cpd1.receiver, cpd1.start_date, cpd1.end_date, cpd1.daily_pay, alice_private_key, false/*with_block_generation*/ );
    BOOST_REQUIRE(proposal_1 >= 0);

    // create one proposal
    create_proposal_data cpd2(db->head_block_time());
    cpd2.end_date = cpd2.start_date + fc::days( 2* NR_INTERVALS_IN_DELAYED_VOTING );
    cpd2.creator = "bob";
    int64_t proposal_2 = create_proposal( cpd2.creator, cpd2.receiver, cpd2.start_date, cpd2.end_date, cpd2.daily_pay, bob_private_key, false/*with_block_generation*/ );
    BOOST_REQUIRE(proposal_2 >= 0);

    // carol vest
    vest("carol", "carol", ASSET("10.000 TESTS"), carol_private_key);

    // carol votes
    vote_proposal("carol", { proposal_1 }, true, carol_private_key);
    const auto carol_power_1 = get_vesting( "carol" ).amount.value;

    auto * ptr = find_proposal(proposal_1); //<- just init

    // check
    const int cycle = 24;
    const int time_offset = HIVE_DELAYED_VOTING_INTERVAL_SECONDS / cycle;
    for(int i = 0; i < (cycle*NR_INTERVALS_IN_DELAYED_VOTING) - 1; i++)
    {
      generate_blocks( db->head_block_time() + fc::seconds(time_offset).to_seconds());
      ptr = find_proposal(proposal_1);
      BOOST_REQUIRE( ptr != nullptr );
      BOOST_REQUIRE( ptr->total_votes == 0ul );

      if( i == (12 * NR_INTERVALS_IN_DELAYED_VOTING))
      {
        vest("carol", "carol", ASSET("10.000 TESTS"), carol_private_key);
        vote_proposal("carol", { proposal_1, proposal_2 }, true, carol_private_key);
      }

      if(i > (12 * NR_INTERVALS_IN_DELAYED_VOTING))
      {
        ptr = find_proposal(proposal_2);
        BOOST_REQUIRE( ptr != nullptr );
        BOOST_REQUIRE( ptr->total_votes == 0ul );
      }
    }

    generate_seconds_blocks( time_offset, true );
    generate_seconds_blocks( 3600, true );
    ptr = find_proposal(proposal_1);
    BOOST_REQUIRE( ptr != nullptr );
    BOOST_REQUIRE( static_cast<long>(ptr->total_votes) == carol_power_1 );
    
    ptr = find_proposal(proposal_2);
    BOOST_REQUIRE( ptr != nullptr );
    BOOST_REQUIRE( static_cast<long>(ptr->total_votes) == carol_power_1 );

    for(int i = 0; i < ( 12 * ( NR_INTERVALS_IN_DELAYED_VOTING / 2 ) ) - 1; i++)
    {
      generate_blocks( db->head_block_time() + fc::seconds(time_offset).to_seconds());
      ptr = find_proposal(proposal_2);
      BOOST_REQUIRE( ptr != nullptr );
      BOOST_REQUIRE( static_cast<long>(ptr->total_votes) == carol_power_1 );
    }
    
    for(int _ = 0; _ < 8; _++ )
    {
      generate_seconds_blocks( 3600, true );
      ptr = find_proposal(proposal_2);
    }
    BOOST_REQUIRE( get_vesting( "carol" ).amount.value == static_cast<long>(ptr->total_votes) );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()


BOOST_FIXTURE_TEST_SUITE( delayed_voting_tests, delayed_vote_database_fixture )

BOOST_AUTO_TEST_CASE( delayed_voting_proxy_02 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: Setting proxy - more complex tests" );

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    auto start_time = db->head_block_time();

    ACTORS( (alice)(bob)(celine)(witness1)(witness2) )
    generate_block();

    asset _1 = ASSET( "1.000 TESTS" );
    asset _2 = ASSET( "2.000 TESTS" );
    asset _3 = ASSET( "3.000 TESTS" );
    asset _4 = ASSET( "4.000 TESTS" );
    asset _5 = ASSET( "5.000 TESTS" );

    auto amount = []( const asset& _asset )
    {
      return _asset.amount.value;
    };

    auto vamount = [ &amount, this ]( const asset& _asset )
    {
      return amount( to_vest( _asset ) );
    };

    using proxy_data = std::map< std::string, delayed_votes_data >;
    proxy_data proxy_0;
    proxy_data proxy_1;
    proxy_data proxy_2;
    proxy_data proxy_15;
    proxy_data proxy_30;
    proxy_data proxy_31;


    auto fill = [ this ]( proxy_data& proxy, const std::string& account_name, size_t nr_interval, size_t seconds )
    {
      auto dq = db->get_account( account_name ).delayed_votes;

      fc::optional< size_t > idx = get_position_in_delayed_voting_array( dq, nr_interval, seconds );
      if( !idx.valid() )
        return;

      auto found = proxy.find( account_name );

      if( found != proxy.end() )
        proxy.erase( found );

      proxy[ account_name ] = dq[ *idx ];
    };

    auto cmp = [ this ]( proxy_data& proxy, const std::string& account_name, size_t val, size_t nr_interval, size_t seconds )
    {
      auto dq = db->get_account( account_name ).delayed_votes;

      fc::optional< size_t > idx = get_position_in_delayed_voting_array( dq, nr_interval, seconds );
      if( !idx.valid() )
        return true;

      auto found = proxy.find( account_name );
      if( found == proxy.end() )
      {
        //current number of interval doesn't exist yet, but previous number of interval must exist
        BOOST_REQUIRE( dq.size() > 0 );
        return true;
      }
      else
      {
        auto _val = val + found->second.val;
        return check_collection( dq, *idx, found->second.time, _val );
      }
    };

    share_type votes_witness1 = get_votes( "witness1" );
    share_type votes_witness2 = get_votes( "witness2" );

    size_t nr_interval = 0;


    {
      BOOST_TEST_MESSAGE( "Getting init data..." );
      fill( proxy_0, "alice",    nr_interval, 0/*seconds*/ );
      fill( proxy_0, "bob",      nr_interval, 0/*seconds*/ );
      fill( proxy_0, "celine",   nr_interval, 0/*seconds*/ );
      fill( proxy_0, "witness1", nr_interval, 0/*seconds*/ );
      fill( proxy_0, "witness2", nr_interval, 0/*seconds*/ );
    }
    {
      BOOST_TEST_MESSAGE( "Preparing accounts..." );

      FUND( "alice", ASSET( "10000.000 TESTS" ) );
      FUND( "bob", ASSET( "10000.000 TESTS" ) );
      FUND( "celine", ASSET( "10000.000 TESTS" ) );
      FUND( "witness1", ASSET( "10000.000 TESTS" ) );
      FUND( "witness2", ASSET( "10000.000 TESTS" ) );

      witness_create( "witness1", witness1_private_key, "url.witness1", witness1_private_key.get_public_key(), HIVE_MIN_PRODUCER_REWARD.amount );
      witness_create( "witness2", witness2_private_key, "url.witness2", witness2_private_key.get_public_key(), HIVE_MIN_PRODUCER_REWARD.amount );
      generate_block();

      auto _v1 = vamount( _1 );
      vest( "alice", "alice", _1, alice_private_key );
      BOOST_REQUIRE( cmp( proxy_0, "alice", _v1, nr_interval, 0/*seconds*/ ) );
      fill( proxy_0, "alice", nr_interval, 0/*seconds*/ );
      generate_block();

      auto _v2 = vamount( _2 );
      vest( "alice", "bob", _2, alice_private_key );
      BOOST_REQUIRE( cmp( proxy_0, "bob", _v2, nr_interval, 0/*seconds*/ ) );
      fill( proxy_0, "bob", nr_interval, 0/*seconds*/ );
      generate_block();

      auto _v3 = vamount( _3 );
      vest( "alice", "celine", _3, alice_private_key );
      BOOST_REQUIRE( cmp( proxy_0, "celine", _v3, nr_interval, 0/*seconds*/ ) );
      fill( proxy_0, "celine", nr_interval, 0/*seconds*/ );
      generate_block();

      auto _v4 = vamount( _4 );
      vest( "alice", "witness1", _4, alice_private_key );
      BOOST_REQUIRE( cmp( proxy_0, "witness1", _v4, nr_interval, 0/*seconds*/ ) );
      fill( proxy_0, "witness1", nr_interval, 0/*seconds*/ );
      generate_block();

      auto _v5 = vamount( _5 );
      vest( "alice", "witness2", _5, alice_private_key );
      BOOST_REQUIRE( cmp( proxy_0, "witness2", _v5, nr_interval, 0/*seconds*/ ) );
      fill( proxy_0, "witness2", nr_interval, 0/*seconds*/ );
      generate_block();

      BOOST_REQUIRE( get_votes( "witness1" ) == votes_witness1 );
      BOOST_REQUIRE( get_votes( "witness2" ) == votes_witness2 );
    }
    {
      /*
        *****intial actions*****
        `celine` votes `witness1`
        `celine` votes `witness2`

        `alice` sets proxy `celine`

        `bob`   votes `witness2`
      */
      witness_vote( "celine", "witness1", true/*approve*/, celine_private_key );
      witness_vote( "celine", "witness2", true/*approve*/, celine_private_key );
      generate_block();

      BOOST_REQUIRE( get_votes( "witness1" ) == votes_witness1 );
      BOOST_REQUIRE( get_votes( "witness2" ) == votes_witness2 );

      proxy( "alice", "celine", alice_private_key );
      generate_block();

      BOOST_REQUIRE( get_votes( "witness1" ) == votes_witness1 );
      BOOST_REQUIRE( get_votes( "witness2" ) == votes_witness2 );

      witness_vote( "bob", "witness2", true/*approve*/, bob_private_key );
      generate_block();

      BOOST_REQUIRE( get_votes( "witness1" ) == votes_witness1 );
      BOOST_REQUIRE( get_votes( "witness2" ) == votes_witness2 );

      /*
        info: `->` ==`proxy`

        Finally there are set following connections:

          alice -> celine   : witness1

          alice -> celine   : witness2
          bob               : witness2
      */
    }

    size_t seconds = 0;

    {
      /*
        *****`+ 1 nr_interval`*****
        `alice`  makes vests
        `bob`    makes vests
        `alice`  makes vests
        `bob`    makes vests
        `celine` makes vests
      */
      start_time += HIVE_DELAYED_VOTING_INTERVAL_SECONDS;
      generate_blocks( start_time, true );
      nr_interval = 1;

      seconds += 3;
      generate_blocks( start_time + fc::seconds( seconds/*1*/ ), true );
      auto _v1 = vamount( _1 );
      vest( "alice", "alice", _1, alice_private_key );
      BOOST_REQUIRE( cmp( proxy_1, "alice",  _v1, nr_interval, seconds ) );
      fill( proxy_1, "alice", nr_interval, seconds );
      generate_block();

      seconds += 3;
      generate_blocks( start_time + fc::seconds( seconds/*2*/ ), true );
      auto _v2 = vamount( _2 );
      vest( "alice", "bob", _2, alice_private_key );
      BOOST_REQUIRE( cmp( proxy_1, "bob", _v2, nr_interval, seconds ) );
      fill( proxy_1, "bob", nr_interval, seconds );
      generate_block();

      seconds += 3;
      generate_blocks( start_time + fc::seconds( seconds/*3*/ ), true );
      _v1 = vamount( _1 );
      vest( "alice", "alice", _1, alice_private_key );
      BOOST_REQUIRE( cmp( proxy_1, "alice", _v1, nr_interval, seconds ) );
      fill( proxy_1, "alice", nr_interval, seconds );
      generate_block();

      seconds += 3;
      generate_blocks( start_time + fc::seconds( seconds/*4*/ ), true );
      _v2 = vamount( _2 );
      vest( "alice", "bob", _2, alice_private_key );
      BOOST_REQUIRE( cmp( proxy_1, "bob", _v2, nr_interval, seconds ) );
      fill( proxy_1, "bob", nr_interval, seconds );
      generate_block();

      seconds += 3;
      generate_blocks( start_time + fc::seconds( seconds/*5*/ ), true );
      _v1 = vamount( _1 );
      vest( "alice", "celine", _1, alice_private_key );
      BOOST_REQUIRE( cmp( proxy_1, "celine", _v1, nr_interval, seconds ) );
      fill( proxy_1, "celine", nr_interval, seconds );
      generate_block();

      BOOST_REQUIRE( get_votes( "witness1" ) == votes_witness1 );
      BOOST_REQUIRE( get_votes( "witness2" ) == votes_witness2 );
    }
    {
      /*
        *****`+ 2 nr_intervals`*****
        `alice`  makes vests
        `celine` makes vests
      */
      start_time += HIVE_DELAYED_VOTING_INTERVAL_SECONDS;
      generate_blocks( start_time, true );
      nr_interval = 2;

      seconds += 3;
      generate_blocks( start_time + fc::seconds( seconds/*6*/ ), true );
      auto _v5 = vamount( _5 );
      vest( "alice", "alice", _5, alice_private_key );
      BOOST_REQUIRE( cmp( proxy_2, "alice", _v5, nr_interval, seconds ) );
      fill( proxy_2, "alice", nr_interval, seconds );
      generate_block();

      seconds += 3;
      generate_blocks( start_time + fc::seconds( seconds/*7*/ ), true );
      auto _v2 = vamount( _2 );
      vest( "alice", "celine", _2, alice_private_key );
      BOOST_REQUIRE( cmp( proxy_2, "celine", _v2, nr_interval, seconds ) );
      fill( proxy_2, "celine", nr_interval, seconds );
      generate_block();

      BOOST_REQUIRE( get_votes( "witness1" ) == votes_witness1 );
      BOOST_REQUIRE( get_votes( "witness2" ) == votes_witness2 );
    }
    {
      /*
        *****`+ 15 nr_intervals`*****
          `alice`  makes vests
          `bob`    makes vests
          `celine` makes vests
      */
      start_time += 13 * HIVE_DELAYED_VOTING_INTERVAL_SECONDS;
      generate_blocks( start_time, true );
      nr_interval = 15;

      seconds += 3;
      generate_blocks( start_time + fc::seconds( seconds/*8*/ ), true );
      auto _v2 = vamount( _2 );
      vest( "alice", "alice", _2, alice_private_key );
      BOOST_REQUIRE( cmp( proxy_15, "alice", _v2, nr_interval, seconds ) );
      fill( proxy_15, "alice", nr_interval, seconds );
      generate_block();

      seconds += 3;
      generate_blocks( start_time + fc::seconds( seconds/*9*/ ), true );
      auto _v3 = vamount( _3 );
      vest( "alice", "bob", _3, alice_private_key );
      BOOST_REQUIRE( cmp( proxy_15, "bob", _v3, nr_interval, seconds ) );
      fill( proxy_15, "bob", nr_interval, seconds );
      generate_block();

      seconds += 3;
      generate_blocks( start_time + fc::seconds( seconds/*10*/ ), true );
      auto _v4 = vamount( _4 );
      vest( "alice", "celine", _4, alice_private_key );
      BOOST_REQUIRE( cmp( proxy_15, "celine", _v4, nr_interval, seconds ) );
      fill( proxy_15, "celine", nr_interval, seconds );
      generate_block();

      BOOST_REQUIRE( get_votes( "witness1" ) == votes_witness1 );
      BOOST_REQUIRE( get_votes( "witness2" ) == votes_witness2 );
    }
    {
      /*
        *****`30 nr_intervals - 1 block`*****
      */
      start_time += 15 * HIVE_DELAYED_VOTING_INTERVAL_SECONDS - HIVE_BLOCK_INTERVAL;
      generate_blocks( start_time, true );

      BOOST_REQUIRE( get_votes( "witness1" ) == votes_witness1 );
      BOOST_REQUIRE( get_votes( "witness2" ) == votes_witness2 );
    }

    using proxy_data_map = std::map< uint32_t, proxy_data >;

    using name_items = std::vector< std::string >;
    using witness_map = std::map< std::string, name_items >;

    /*
      info: `->` ==`proxy`

      Finally there are set following connections:

        alice -> celine   : witness1

        alice -> celine   : witness2
        bob               : witness2
    */
    proxy_data_map pd_map{
                    { 0, proxy_0 },
                    { 1, proxy_1 },
                    { 2, proxy_2 },
                    { 15, proxy_15 }
                  };
    witness_map m_map {
                  { "witness1", { "alice", "celine" } },
                  { "witness2", { "alice", "celine", "bob" } }
                };

    auto calculate_votes = [ &pd_map, &m_map ]( uint32_t nr_day_after_30days_interval, const std::string& witness ) -> ushare_type
    {
      auto found_proxy = pd_map.find( nr_day_after_30days_interval );
      if( found_proxy == pd_map.end() )
        return 0;

      auto found_voters = m_map.find( witness );
      if( found_voters == m_map.end() )
        return 0;

      proxy_data& proxy = found_proxy->second;
      name_items& voters = found_voters->second;

      ushare_type res = 0;
      for( auto& item : voters )
      {
        auto found_voter = proxy.find( item );
        if( found_voter != proxy.end() )
          res += found_voter->second.val;
      }

      return res;
    };

    auto witness1_result_00 = calculate_votes( 0/*nr_day_after_30days_interval*/, "witness1" );
    auto witness2_result_00 = calculate_votes( 0/*nr_day_after_30days_interval*/, "witness2" );

    {
      /*
        *****`30 nr_intervals`*****
        `alice`  makes vests
        `bob`    makes vests
        `celine` makes vests
      */
      start_time += HIVE_BLOCK_INTERVAL;
      generate_blocks( start_time, true );
      nr_interval = 30;
      size_t diff = 1;//because `1` element in delayed_votes has been removed already

      BOOST_REQUIRE( witness1_result_00 >= 0 );
      BOOST_REQUIRE( witness2_result_00 >= 0 );

      BOOST_REQUIRE( get_votes( "witness1" ) == ( votes_witness1 + witness1_result_00.value ) );
      BOOST_REQUIRE( get_votes( "witness2" ) == ( votes_witness2 + witness2_result_00.value ) );

      seconds += 3;
      generate_blocks( start_time + fc::seconds( seconds/*11*/ ), true );
      auto _v5 = vamount( _5 );
      vest( "alice", "alice", _5, alice_private_key );
      BOOST_REQUIRE( cmp( proxy_30, "alice", _v5, nr_interval, seconds ) );
      fill( proxy_30, "alice", nr_interval - diff, seconds );
      generate_block();

      seconds += 3;
      generate_blocks( start_time + fc::seconds( seconds/*12*/ ), true );
      auto _v1 = vamount( _1 );
      vest( "alice", "bob", _1, alice_private_key );
      BOOST_REQUIRE( cmp( proxy_30, "bob", _v1, nr_interval, seconds ) );
      fill( proxy_30, "bob", nr_interval - diff, seconds );
      generate_block();

      seconds += 3;
      generate_blocks( start_time + fc::seconds( seconds/*13*/ ), true );
      auto _v2 = vamount( _2 );
      vest( "alice", "celine", _2, alice_private_key );
      BOOST_REQUIRE( cmp( proxy_30, "celine", _v2, nr_interval, seconds ) );
      fill( proxy_30, "celine", nr_interval - diff, seconds );
      generate_block();
    }

    auto witness1_result_01 = calculate_votes( 1/*nr_day_after_30days_interval*/, "witness1" );
    auto witness2_result_01 = calculate_votes( 1/*nr_day_after_30days_interval*/, "witness2" );

    //Displaying diagnostic data.
    for( auto& item : pd_map )
    {
      std::string str_day = "day: " +std::to_string( item.first/*day number*/ );
      proxy_data& voters = item.second;//std::map< std::string, delayed_votes_data >;
      std::string str_voters;
      std::string c = std::string( " : " );
      for( auto& voter : voters )
        str_voters += std::string( "(" ) + voter.first + c + std::to_string( voter.second.val.value ) + c + voter.second.time.to_iso_string() + std::string( ")" );
      std::string str = str_day + str_voters;
      idump( (str) );
    }

    {
      /*
        *****`+ 31 nr_intervals`*****
        `celine` makes vests
      */
      start_time += fc::seconds( HIVE_DELAYED_VOTING_INTERVAL_SECONDS ) ;
      generate_blocks( start_time, true );
      nr_interval = 31;
      size_t diff = 2;//because `2` elements in delayed_votes have been removed already

      seconds += 3;
      generate_blocks( start_time + fc::seconds( seconds/*14*/ ), true );
      auto _v5 = vamount( _5 );
      vest( "alice", "celine", _5, alice_private_key );
      BOOST_REQUIRE( cmp( proxy_31, "celine", _v5, nr_interval, seconds ) );
      fill( proxy_31, "celine", nr_interval - diff, seconds );
      generate_block();
    }
    {
      /*
        *****`lasted 31 nr_intervals`*****
      */
      start_time += fc::seconds( HIVE_DELAYED_VOTING_INTERVAL_SECONDS ) - fc::seconds( 30 );
      generate_blocks( start_time, true );

      BOOST_REQUIRE( witness1_result_01 > 0 );
      BOOST_REQUIRE( witness2_result_01 > 0 );

      idump( ( VOTING_POWER( "alice" ) ) );
      idump( ( VOTING_POWER( "bob" ) ) );
      idump( ( VOTING_POWER( "celine" ) ) );
      idump( (votes_witness1) );
      idump( (witness1_result_00) );
      idump( (witness1_result_01) );
      BOOST_REQUIRE_EQUAL( get_votes( "witness1" ).value, ( votes_witness1 + ( witness1_result_00 + witness1_result_01 ).value ).value );

      idump( (votes_witness2) );
      idump( (witness2_result_00) );
      idump( (witness2_result_01) );
      BOOST_REQUIRE_EQUAL( get_votes( "witness2" ).value, ( votes_witness2 + ( witness2_result_00 + witness2_result_01 ).value ).value );

      start_time += fc::minutes( 1 );
      generate_blocks( start_time, true );
    }

    auto witness1_result_02 = calculate_votes( 2/*nr_day_after_30days_interval*/, "witness1" );
    auto witness2_result_02 = calculate_votes( 2/*nr_day_after_30days_interval*/, "witness2" );

    {
      /*
        *****`lasted 32 nr_intervals`*****
      */
      start_time += fc::seconds( HIVE_DELAYED_VOTING_INTERVAL_SECONDS ) - fc::seconds( 30 );
      generate_blocks( start_time, true );

      BOOST_REQUIRE( witness1_result_02 > 0 );
      BOOST_REQUIRE( witness1_result_02 > 0 );

      idump( ( VOTING_POWER( "alice" ) ) );
      idump( ( VOTING_POWER( "bob" ) ) );
      idump( ( VOTING_POWER( "celine" ) ) );
      idump( (votes_witness1) );
      idump( (witness1_result_00) );
      idump( (witness1_result_01) );
      idump( (witness1_result_02) );
      BOOST_REQUIRE( get_votes( "witness1" ) == ( votes_witness1 + ( witness1_result_00 + witness1_result_01 + witness1_result_02 ).value ) );

      idump( (votes_witness2) );
      idump( (witness2_result_00) );
      idump( (witness2_result_01) );
      idump( (witness2_result_02) );
      BOOST_REQUIRE( get_votes( "witness2" ) == ( votes_witness2 + ( witness2_result_00 + witness2_result_01 + witness2_result_02 ).value ) );

      start_time += fc::minutes( 1 );
      generate_blocks( start_time, true );
    }
    {
      /*
        *****`lasted 40 nr_intervals`*****
      */
      start_time += 8 * HIVE_DELAYED_VOTING_INTERVAL_SECONDS;
      generate_blocks( start_time, true );

      BOOST_REQUIRE( get_votes( "witness1" ) == ( votes_witness1 + ( witness1_result_00 + witness1_result_01 + witness1_result_02 ).value ) );
      BOOST_REQUIRE( get_votes( "witness2" ) == ( votes_witness2 + ( witness2_result_00 + witness2_result_01 + witness2_result_02 ).value ) );

      proxy( "alice", "", alice_private_key );
      generate_block();

      auto _copy_m_map = m_map;

      m_map.clear();
      m_map =  {
              { "witness1", { "celine" } },
              { "witness2", { "celine", "bob" } }
            };

      auto witness1_result_no_proxy_00 = calculate_votes( 0/*nr_day_after_30days_interval*/, "witness1" );
      auto witness2_result_no_proxy_00 = calculate_votes( 0/*nr_day_after_30days_interval*/, "witness2" );

      auto witness1_result_no_proxy_01 = calculate_votes( 1/*nr_day_after_30days_interval*/, "witness1" );
      auto witness2_result_no_proxy_01 = calculate_votes( 1/*nr_day_after_30days_interval*/, "witness2" );

      auto witness1_result_no_proxy_02 = calculate_votes( 2/*nr_day_after_30days_interval*/, "witness1" );
      auto witness2_result_no_proxy_02 = calculate_votes( 2/*nr_day_after_30days_interval*/, "witness2" );

      BOOST_REQUIRE( get_votes( "witness1" ) == ( votes_witness1 + ( witness1_result_no_proxy_00 + witness1_result_no_proxy_01 + witness1_result_no_proxy_02 ).value ) );
      BOOST_REQUIRE( get_votes( "witness2" ) == ( votes_witness2 + ( witness2_result_no_proxy_00 + witness2_result_no_proxy_01 + witness2_result_no_proxy_02 ).value ) );

      proxy( "alice", "celine", alice_private_key );
      generate_block();

      BOOST_REQUIRE( get_votes( "witness1" ) == ( votes_witness1 + ( witness1_result_00 + witness1_result_01 + witness1_result_02 ).value ) );
      BOOST_REQUIRE( get_votes( "witness2" ) == ( votes_witness2 + ( witness2_result_00 + witness2_result_01 + witness2_result_02 ).value  ) );

      m_map = _copy_m_map;
    }

    pd_map[ 30 ] = proxy_30;
    pd_map[ 31 ] = proxy_31;

    {
      /*
        *****`lasted 80 nr_intervals`*****
      */
      start_time += 40 * HIVE_DELAYED_VOTING_INTERVAL_SECONDS;
      generate_blocks( start_time, true );

      idump( ( VOTING_POWER( "alice" ) ) );
      idump( ( VOTING_POWER( "bob" ) ) );
      idump( ( VOTING_POWER( "celine" ) ) );

      auto calculate_total = [ &calculate_votes ]( const std::string& witness_name )
      {
        auto v_0 = calculate_votes( 0/*nr_day_after_30days_interval*/, witness_name );
        idump( (v_0) );

        auto v_1 = calculate_votes( 1/*nr_day_after_30days_interval*/, witness_name );
        idump( (v_1) );

        auto v_2 = calculate_votes( 2/*nr_day_after_30days_interval*/, witness_name );
        idump( (v_2) );

        auto v_15 = calculate_votes( 15/*nr_day_after_30days_interval*/, witness_name );
        idump( (v_15) );

        auto v_30 = calculate_votes( 30/*nr_day_after_30days_interval*/, witness_name );
        idump( (v_30) );

        auto v_31 = calculate_votes( 31/*nr_day_after_30days_interval*/, witness_name );
        idump( (v_31) );

        return v_0 + v_1 + v_2 + v_15 + v_30 + v_31;
      };

      BOOST_REQUIRE( get_votes( "witness1" ) == ( votes_witness1 + calculate_total( "witness1" ).value ) );
      BOOST_REQUIRE( get_votes( "witness2" ) == ( votes_witness2 + calculate_total( "witness2" ).value ) );
    }

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( delayed_voting_proxy_01 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: Setting proxy" );

    ACTORS( (alice)(bob)(celine)(witness) )
    generate_block();

    {
      BOOST_TEST_MESSAGE( "Preparing accounts..." );

      set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
      generate_block();

      FUND( "alice", ASSET( "10000.000 TESTS" ) );
      FUND( "bob", ASSET( "10000.000 TESTS" ) );
      FUND( "celine", ASSET( "10000.000 TESTS" ) );
      FUND( "witness", ASSET( "10000.000 TESTS" ) );
    }
    {
      BOOST_TEST_MESSAGE( "Preparing witnesses..." );

      witness_create( "witness", witness_private_key, "url.witness", witness_private_key.get_public_key(), HIVE_MIN_PRODUCER_REWARD.amount );
      generate_block();
    }

    asset _1 = ASSET( "1.000 TESTS" );
    asset _2 = ASSET( "2.000 TESTS" );
    asset _3 = ASSET( "3.000 TESTS" );
    asset _4 = ASSET( "4.000 TESTS" );

    auto start_time = db->head_block_time();

    share_type votes_01 = get_votes( "witness" );

    {
      BOOST_TEST_MESSAGE( "Transform TESTS->VESTS for every account..." );

      vest( "alice", "alice",    _1, alice_private_key );
      vest( "alice", "bob",      _2, alice_private_key );
      vest( "alice", "celine",   _3, alice_private_key );
      vest( "alice", "witness",  _4, alice_private_key );
    }
    {
      BOOST_TEST_MESSAGE( "Creating proxies..." );

      auto v_alice   = get_vesting( "alice" );
      auto v_bob     = get_vesting( "bob" );
      auto v_celine  = get_vesting( "celine" );
      auto v_witness = get_vesting( "witness" );

      proxy( "alice", "celine", alice_private_key );
      proxy( "bob", "celine", bob_private_key );

      BOOST_REQUIRE( get_vesting( "alice" ) == v_alice );
      BOOST_REQUIRE( get_vesting( "bob" )   == v_bob );
      BOOST_REQUIRE( get_vesting( "celine" ) == v_celine );
      BOOST_REQUIRE( get_vesting( "witness" ) == v_witness );
      BOOST_REQUIRE( get_votes( "witness" ) == votes_01 );
    }
    {
      BOOST_TEST_MESSAGE( "Voting..." );

      auto v_alice   = get_vesting( "alice" );
      auto v_bob     = get_vesting( "bob" );
      auto v_celine  = get_vesting( "celine" );
      auto v_witness = get_vesting( "witness" );

      witness_vote( "celine", "witness", true/*approve*/, celine_private_key );

      BOOST_REQUIRE( get_vesting( "alice" ) == v_alice );
      BOOST_REQUIRE( get_vesting( "bob" ) == v_bob );
      BOOST_REQUIRE( get_vesting( "celine" ) == v_celine );
      BOOST_REQUIRE( get_vesting( "witness" ) == v_witness );
      BOOST_REQUIRE( get_votes( "witness" ) == votes_01 );
    }
    {
      BOOST_TEST_MESSAGE( "Move time forward..." );

      auto v_alice   = get_vesting( "alice" );
      auto v_bob     = get_vesting( "bob" );
      auto v_celine  = get_vesting( "celine" );
      auto v_witness = get_vesting( "witness" );

      generate_blocks( start_time + HIVE_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS , true );

      BOOST_REQUIRE( get_vesting( "alice" ) == v_alice );
      BOOST_REQUIRE( get_vesting( "bob" ) == v_bob );
      BOOST_REQUIRE( get_vesting( "celine" ) == v_celine );
      BOOST_REQUIRE( get_vesting( "witness" ) == v_witness );

      auto sum = v_alice + v_bob + v_celine;
      share_type votes_02 = get_votes( "witness" );
      BOOST_REQUIRE( votes_02 == votes_01 + sum.amount.value );
    }
    {
      BOOST_TEST_MESSAGE( "Remove account `alice` with proxy..." );

      auto v_alice   = get_vesting( "alice" );
      auto v_bob     = get_vesting( "bob" );
      auto v_celine  = get_vesting( "celine" );
      auto v_witness = get_vesting( "witness" );

      proxy( "alice", "", alice_private_key );
      generate_block();

      BOOST_REQUIRE( get_vesting( "alice" ) == v_alice );
      BOOST_REQUIRE( get_vesting( "bob" ) == v_bob );
      BOOST_REQUIRE( get_vesting( "celine" ) == v_celine );
      BOOST_REQUIRE( get_vesting( "witness" ) == v_witness );

      auto sum = v_bob + v_celine;
      share_type votes_02 = get_votes( "witness" );
      BOOST_REQUIRE( votes_02 == votes_01 + sum.amount.value );
    }
    {
      BOOST_TEST_MESSAGE( "Remove account `bob` with proxy..." );

      auto v_alice   = get_vesting( "alice" );
      auto v_bob     = get_vesting( "bob" );
      auto v_celine  = get_vesting( "celine" );
      auto v_witness = get_vesting( "witness" );

      proxy( "bob", "", bob_private_key );
      generate_block();

      BOOST_REQUIRE( get_vesting( "alice" ) == v_alice );
      BOOST_REQUIRE( get_vesting( "bob" ) == v_bob );
      BOOST_REQUIRE( get_vesting( "celine" ) == v_celine );
      BOOST_REQUIRE( get_vesting( "witness" ) == v_witness );

      share_type votes_02 = get_votes( "witness" );
      BOOST_REQUIRE( votes_02 == votes_01 + v_celine.amount.value );
    }

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( delayed_voting_many_vesting_01 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: Transferring vests many times from one person to another" );

    ACTORS( (alice)(bob)(witness) )
    generate_block();

    {
      BOOST_TEST_MESSAGE( "Preparing accounts..." );

      set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
      generate_block();

      FUND( "alice", ASSET( "10000.000 TESTS" ) );
      FUND( "bob", ASSET( "10000.000 TESTS" ) );
      FUND( "witness", ASSET( "10000.000 TESTS" ) );
    }
    {
      BOOST_TEST_MESSAGE( "Preparing witnesses..." );

      witness_create( "witness", witness_private_key, "url.witness", witness_private_key.get_public_key(), HIVE_MIN_PRODUCER_REWARD.amount );
      generate_block();
    }

    asset _one = ASSET( "1.000 TESTS" );

    auto start_time = db->head_block_time();

    share_type votes_01 = get_votes( "witness" );

    {
      BOOST_TEST_MESSAGE( "Transform a few times TESTS->VESTS..." );

      auto v_alice = get_vesting( "alice" );
      auto v_bob = get_vesting( "bob" );
      auto v_witness = get_vesting( "witness" );

      asset v_alice_00 = asset( 0, VESTS_SYMBOL );
      asset v_bob_00 = asset( 0, VESTS_SYMBOL );

      for( int i = 0; i < 3; ++i )
      {
        vest( "bob", "bob", _one, bob_private_key );
        v_bob_00 += to_vest( _one );

        vest( "alice", "bob", _one, alice_private_key );
        v_alice_00 += to_vest( _one );

        vest( "bob", "alice", _one, bob_private_key );
        v_bob_00 += to_vest( _one );

        generate_block();
      }

      BOOST_REQUIRE( get_vesting( "alice" ).amount.value == ( v_alice + v_alice_00 ).amount.value );
      BOOST_REQUIRE( get_vesting( "bob" ).amount.value == ( v_bob + v_bob_00 ).amount.value );
      BOOST_REQUIRE( get_vesting( "witness" ).amount.value == v_witness.amount.value );
      BOOST_REQUIRE( get_votes( "witness" ) == votes_01 );
    }
    {
      BOOST_TEST_MESSAGE( "Witness voting..." );

      auto v_alice = get_vesting( "alice" );
      auto v_bob = get_vesting( "bob" );
      auto v_witness = get_vesting( "witness" );

      witness_vote( "alice", "witness", true/*approve*/, alice_private_key );
      generate_block();
      witness_vote( "bob", "witness", true/*approve*/, bob_private_key );
      generate_block();
      witness_vote( "bob", "witness", false/*approve*/, bob_private_key );
      generate_block();
      witness_vote( "bob", "witness", true/*approve*/, bob_private_key );
      generate_block();

      BOOST_REQUIRE( get_vesting( "alice" ).amount.value == v_alice.amount.value );
      BOOST_REQUIRE( get_vesting( "bob" ).amount.value == v_bob.amount.value );
      BOOST_REQUIRE( get_vesting( "witness" ).amount.value == v_witness.amount.value );
      BOOST_REQUIRE( get_votes( "witness" ) == votes_01 );

      generate_blocks( start_time + HIVE_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS , true );

      auto sum = v_alice + v_bob;
      BOOST_REQUIRE( get_votes( "witness" ) == votes_01 + sum.amount.value );

      witness_vote( "bob", "witness", false/*approve*/, bob_private_key );
      generate_block();
      BOOST_REQUIRE( get_votes( "witness" ) == votes_01 + v_alice.amount.value );

      witness_vote( "alice", "witness", false/*approve*/, alice_private_key );
      generate_block();
      BOOST_REQUIRE( get_votes( "witness" ) == votes_01 );
    }

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( delayed_voting_01 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: delaying voting" );

    ACTORS( (alice)(bob)(witness) )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    //auto start_time = db->head_block_time();

    FUND( "alice", ASSET( "10000.000 TESTS" ) );
    FUND( "bob", ASSET( "10000.000 TESTS" ) );
    FUND( "witness", ASSET( "10000.000 TESTS" ) );
    //Prepare witnesses
    
    witness_create( "witness", witness_private_key, "url.witness", witness_private_key.get_public_key(), HIVE_MIN_PRODUCER_REWARD.amount );
    generate_block();

    auto start_time = db->head_block_time();
    witness_vote( "alice", "witness", true/*approve*/, alice_private_key );
    generate_block();

    share_type basic_votes = get_votes( "witness" );

    //Make some vests
    vest( "bob", "alice", ASSET( "1000.000 TESTS" ), bob_private_key );
    generate_block();

    share_type votes_01 = get_votes( "witness" );
    BOOST_REQUIRE( votes_01 == basic_votes );

    generate_blocks( start_time + HIVE_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS , true );
    generate_block();

    auto votes_power = get_vesting( "alice" );
    share_type votes_02 = get_votes( "witness" );
    BOOST_REQUIRE( votes_02 == basic_votes + votes_power.amount.value );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( delayed_voting_04 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: delaying voting v4" );

    ACTORS( (bob)(witness) )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    //auto start_time = db->head_block_time();

    FUND( "bob", ASSET( "10000.000 TESTS" ) );
    FUND( "witness", ASSET( "10000.000 TESTS" ) );
    
    //Prepare witnesses
    witness_create( "witness", witness_private_key, "url.witness", witness_private_key.get_public_key(), HIVE_MIN_PRODUCER_REWARD.amount );
    generate_block();

    auto start_time = db->head_block_time();
    const share_type basic_votes = get_votes( "witness" );

    //Make some vests
    vest( "bob", "bob", ASSET( "1000.000 TESTS" ), bob_private_key );
    generate_block();

    witness_vote( "bob", "witness", true/*approve*/, bob_private_key );
    generate_block();

    BOOST_REQUIRE( basic_votes == get_votes( "witness" ) );
    for(int i = 1; i < (HIVE_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS / HIVE_DELAYED_VOTING_INTERVAL_SECONDS) - 1; i++)
    {
      generate_blocks( start_time + (i * HIVE_DELAYED_VOTING_INTERVAL_SECONDS) , true );
      BOOST_REQUIRE( get_votes( "witness" ) == basic_votes );
    }
    generate_blocks( start_time + HIVE_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS , true );
    generate_block();

    auto votes_power = get_vesting( "bob" );
    BOOST_REQUIRE( get_votes( "witness" ) == basic_votes + votes_power.amount.value );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( delayed_voting_05 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: delaying voting v5" );

    ACTORS( (bob)(witness1)(witness2) )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    //auto start_time = db->head_block_time();

    FUND( "bob", ASSET( "10000.000 TESTS" ) );
    FUND( "witness1", ASSET( "10000.000 TESTS" ) );
    FUND( "witness2", ASSET( "10000.000 TESTS" ) );
    
    //Prepare witnesses
    witness_create( "witness1", witness1_private_key, "url.witness1", witness1_private_key.get_public_key(), HIVE_MIN_PRODUCER_REWARD.amount );
    witness_create( "witness2", witness2_private_key, "url.witness2", witness2_private_key.get_public_key(), HIVE_MIN_PRODUCER_REWARD.amount );
    generate_block();

    auto start_time = db->head_block_time();
    share_type basic_votes_1 = get_votes( "witness1" );
    share_type basic_votes_2 = get_votes( "witness2" );

    //Make some vests
    vest( "bob", "bob", ASSET( "1000.000 TESTS" ), bob_private_key );
    generate_block();

    witness_vote( "bob", "witness1", true/*approve*/, bob_private_key );
    generate_block();

    share_type votes_01_1 = get_votes( "witness1" );
    share_type votes_01_2 = get_votes( "witness2" );
    BOOST_REQUIRE( basic_votes_1 == votes_01_1 );
    BOOST_REQUIRE( basic_votes_2 == votes_01_2 );

    for(int i = 1; i < NR_INTERVALS_IN_DELAYED_VOTING - 1; i++)
    {
      generate_blocks( start_time + (i * HIVE_DELAYED_VOTING_INTERVAL_SECONDS) , true );
      if( i == static_cast<int>( NR_INTERVALS_IN_DELAYED_VOTING/2 )) witness_vote( "bob", "witness2", true/*approve*/, bob_private_key );
      BOOST_REQUIRE( get_votes( "witness1" ) == votes_01_1 );
      BOOST_REQUIRE( get_votes( "witness2" ) == votes_01_2 );
    }
    generate_blocks( start_time + HIVE_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS , true );
    generate_block();

    auto votes_power = get_vesting( "bob" );
    BOOST_REQUIRE( get_votes( "witness1" ) == basic_votes_1 + votes_power.amount.value );
    BOOST_REQUIRE( get_votes( "witness2" ) == basic_votes_2 + votes_power.amount.value );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( delayed_voting_06 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: delaying voting v6" );

    ACTORS( (bob)(witness) )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    //auto start_time = db->head_block_time();

    FUND( "bob", ASSET( "10000.000 TESTS" ) );
    FUND( "witness", ASSET( "10000.000 TESTS" ) );
    
    //Prepare witnesses
    const auto start_time = db->head_block_time();
    witness_create( "witness", witness_private_key, "url.witness", witness_private_key.get_public_key(), HIVE_MIN_PRODUCER_REWARD.amount );
    generate_block();

    const share_type basic_votes = get_votes( "witness" );

    //Make some vests
    vest( "bob", "bob", ASSET( "1000.000 TESTS" ), bob_private_key );
    generate_block();

    HIVE_REQUIRE_THROW( witness_vote( "bob", "witness", false/*approve*/, bob_private_key ), fc::assert_exception) ;
    generate_block();
    BOOST_REQUIRE( get_votes( "witness" ) == basic_votes );

    witness_vote( "bob", "witness", true/*approve*/, bob_private_key );
    generate_block();
    BOOST_REQUIRE( get_votes( "witness" ) == basic_votes );

    witness_vote( "bob", "witness", false/*approve*/, bob_private_key );
    generate_block();
    BOOST_REQUIRE( get_votes( "witness" ) == basic_votes );

    generate_blocks( start_time + HIVE_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS );
    BOOST_REQUIRE( get_votes( "witness" ) == basic_votes );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( delayed_voting_basic_06 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Complex scenario for `delayed_voting` class" );

    ACTORS( (user0)(user1)(user2)(user3)(user4)(user5)(user6)(user7)(user8)(user9) )

    std::array< std::string, 10 > accs{
                            "user0",
                            "user1",
                            "user2",
                            "user3",
                            "user4",
                            "user5",
                            "user6",
                            "user7",
                            "user8",
                            "user9"
                          };

    delayed_voting dv = delayed_voting( *db );

    delayed_voting::opt_votes_update_data_items withdraw_items = delayed_voting::votes_update_data_items();
    auto start_time = db->head_block_time();

    for( uint32_t i = 0; i < 10; ++i )
    {
      size_t cnt = 0;
      for( auto& item : accs )
      {
        const uint64_t pre_size{ withdraw_items->size() };
        for(int j = 0; j < 10; j++)
        {
          dv.add_delayed_value( db->get_account( item ), start_time + fc::days(1), 10'000 );
          start_time = move_forward_with_update( fc::days( 1 ), withdraw_items );

          const bool withdraw_executor = cnt == accs.size() - 1;
          dv.add_votes( withdraw_items, withdraw_executor, 10'000, db->get_account( item ) );

          if( withdraw_executor )
            dv.add_votes( withdraw_items, withdraw_executor, -10'000, db->get_account( item ) );
          
          BOOST_REQUIRE( i == 0 || pre_size == withdraw_items->size());
        }
        ++cnt;
      }
      dv.update_votes( withdraw_items, start_time );
      BOOST_REQUIRE_EQUAL( withdraw_items->size(), accs.size() );
      start_time = move_forward_with_update( fc::days( 40 ), withdraw_items );
    }

    for( auto& item : accs )
      BOOST_REQUIRE( DELAYED_VOTES( item ) >= 0 );

    start_time = move_forward_with_update( fc::days( 13 * 7 ), withdraw_items );

    for( auto& item : accs )
      BOOST_REQUIRE( DELAYED_VOTES( item ) == 0 );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( delayed_voting_basic_05 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Complex scenario for `delayed_voting` class" );

    ACTORS( (user0)(user1)(user2)(user3)(user4)(user5)(user6)(user7)(user8)(user9) )

    std::array< std::string, 10 > accs{
                            "user0",
                            "user1",
                            "user2",
                            "user3",
                            "user4",
                            "user5",
                            "user6",
                            "user7",
                            "user8",
                            "user9"
                          };

    delayed_voting dv = delayed_voting( *db );

    auto start_time = db->head_block_time();
    delayed_voting::opt_votes_update_data_items withdraw_items = delayed_voting::votes_update_data_items();


    {
      for( uint32_t i = 0; i < 29; ++i )
      {
        size_t cnt = 0;
        for( auto& item : accs )
        {
          dv.add_delayed_value( db->get_account( item ), start_time + fc::days(1), 10'000 );

          const bool withdraw_executor = cnt == accs.size() - 1;
          dv.add_votes( withdraw_items, withdraw_executor, 10'000, db->get_account( item ) );

          if( withdraw_executor )
            dv.add_votes( withdraw_items, withdraw_executor, -10'000, db->get_account( item ) );
          ++cnt;
        }
        start_time = move_forward_with_update( fc::days( 1 ), withdraw_items );
      }
      dv.update_votes( withdraw_items, start_time );

      BOOST_REQUIRE_EQUAL( withdraw_items->size(), accs.size() );
    }
    {
      for( uint32_t i = 0; i < 29; ++i )
      {
        start_time = move_forward_with_update( fc::days( 1 ), withdraw_items );

        for( auto& item : accs )
          BOOST_REQUIRE( DELAYED_VOTES( item ) >= 0 );
      }

      start_time = move_forward_with_update( fc::days( 1 ), withdraw_items );

      for( auto& item : accs )
        BOOST_REQUIRE( DELAYED_VOTES( item ) == 0 );
    }

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( delayed_voting_basic_03 )
{
  try
  {
    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    // support function
    const auto get_delayed_vote_count = [&]( const account_name_type& name = "bob", const std::vector<uint64_t>& data_to_compare )
    {
      const auto& idx = db->get_index< account_index, by_delayed_voting >();
      for(const auto& usr : idx)
        if(usr.name == name)
          return std::equal(
                usr.delayed_votes.begin(), 
                usr.delayed_votes.end(), 
                data_to_compare.begin(), 
                data_to_compare.end(), 
                [](const delayed_votes_data& x, const uint64_t y){ return x.val == y; });
      return false;
    };

    // user setup
    BOOST_TEST_MESSAGE( "Testing: `delayed_voting::run` method" );
    const auto start_time = db->head_block_time();
    ACTORS( (alice)(celine)(bob)(witness) )
    generate_block();
    FUND( "bob", ASSET( "100000.000 TESTS" ) );
    FUND( "celine", ASSET( "100000.000 TESTS" ) );
    FUND( "alice", ASSET( "100000.000 TESTS" ) );
    FUND( "witness", ASSET( "100000.000 TESTS" ) );
    
    //Prepare witnesses
    witness_create( "witness", witness_private_key, "url.witness", witness_private_key.get_public_key(), HIVE_MIN_PRODUCER_REWARD.amount );
    generate_block();
    vest( "bob", "bob", ASSET( "100.000 TESTS" ), bob_private_key );
    vest( "alice", "alice", ASSET( "100.000 TESTS" ), alice_private_key );
    vest( "celine", "celine", ASSET( "100.000 TESTS" ), celine_private_key );
    generate_block();

    // validation data
    const auto basic_votes{ get_votes( "witness" ) };
    std::vector<uint64_t> alice_values; alice_values.reserve(30);

    // initial voting
    witness_vote("bob", "witness", true, bob_private_key);
    generate_block();
    witness_vote("alice", "witness", true, alice_private_key);
    generate_block();

    // validation data pt 2
    alice_values.push_back(get_vesting( "alice" ).amount.value);
    uint64_t last{ alice_values.back() };

    // entry check
    BOOST_REQUIRE( get_delayed_vote_count("bob", { { static_cast<uint64_t>(get_vesting( "bob" ).amount.value) } }) );
    BOOST_REQUIRE( get_delayed_vote_count("alice", { { static_cast<uint64_t>(get_vesting( "alice" ).amount.value)} }) );

    // check everyday for month
    bool s = false;
    for(int i = 1; i < NR_INTERVALS_IN_DELAYED_VOTING - 1; i++)
    {
      // 1 day
      generate_blocks( start_time + (i * HIVE_DELAYED_VOTING_INTERVAL_SECONDS) , true );

      // base checks for witness, bob and celine
      BOOST_REQUIRE( get_votes("witness") == basic_votes );
      BOOST_REQUIRE( get_delayed_vote_count("bob", { { static_cast<uint64_t>(get_vesting( "bob" ).amount.value)} }) );
      BOOST_REQUIRE( get_delayed_vote_count("celine", { { static_cast<uint64_t>(get_vesting( "celine" ).amount.value)} }) );

      // celine arythmia
      s=!s;
      witness_vote("celine", "witness", s, celine_private_key);

      // alice tap
      vest( "alice", "alice", ASSET( "100.000 TESTS" ), alice_private_key );

      // alice check
      const uint64_t val = static_cast<uint64_t>(get_vesting( "alice" ).amount.value);
      alice_values.push_back(val - last);
      last = val;
      BOOST_REQUIRE( get_delayed_vote_count("alice", alice_values) );
    }

    // check is bob ok
    generate_blocks( start_time + HIVE_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS, true );
    BOOST_REQUIRE( get_delayed_vote_count("bob", {}) );

    // check is alice ok (after another month)
    generate_blocks( start_time + (2 * HIVE_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS) , true );
    BOOST_REQUIRE( get_delayed_vote_count("alice", {}) );

    // check is witness ok
    const auto alice_power = get_vesting( "alice" ).amount.value;
    const auto bob_power = get_vesting( "bob" ).amount.value;
    const auto celine_power = (s ? get_vesting( "celine" ).amount.value : 0l);

    BOOST_REQUIRE( get_votes("witness") == basic_votes + alice_power + bob_power + celine_power);
    validate_database();

  }
  FC_LOG_AND_RETHROW();
}

BOOST_AUTO_TEST_CASE( delayed_voting_basic_04 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Complex scenario for `delayed_voting` class" );

    ACTORS( (user0)(user1)(user2)(user3)(user4)(user5)(user6)(user7)(user8)(user9) )

    std::array< std::string, 10 > accs{
                            "user0",
                            "user1",
                            "user2",
                            "user3",
                            "user4",
                            "user5",
                            "user6",
                            "user7",
                            "user8",
                            "user9"
                          };

    delayed_voting dv = delayed_voting( *db );

    auto start_time = db->head_block_time();

    auto move_forward = [ &start_time, this ]( const fc::microseconds& time )
    {
      generate_blocks( db->head_block_time() + time );
      start_time = db->head_block_time();
    };

    {
      for( uint32_t i = 0; i < 36; ++i )
      {
        for( auto& item : accs )
          dv.add_delayed_value( db->get_account( item ), start_time + fc::hours( i ), ( i + 1 ) * 1'000'000/*milion*/ );
      }

      move_forward( fc::days( 4 ) );
    }
    {
      size_t cnt = 0;
      delayed_voting::opt_votes_update_data_items withdraw_items = delayed_voting::votes_update_data_items();

      for( auto& item : accs )
      {
        bool withdraw_executor = cnt == accs.size() - 1;

        dv.add_votes( withdraw_items, withdraw_executor, ( cnt + 1 ) * 10, db->get_account( item ) );

        if( withdraw_executor )
          dv.add_votes( withdraw_items, withdraw_executor, -8888, db->get_account( item ) );

        ++cnt;
      }
      BOOST_REQUIRE( withdraw_items->size() == accs.size() );
      dv.update_votes( withdraw_items, start_time );
    }
    {
      for( uint32_t i = 0; i < 34; ++i )
      {
        move_forward( fc::days( 1 ) );

        for( auto& item : accs )
          BOOST_REQUIRE( DELAYED_VOTES( item ) >= 0 );
      }

      move_forward( fc::days( 1 ) );

      for( auto& item : accs )
        BOOST_REQUIRE( DELAYED_VOTES( item ) == 0 );
    }

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( delayed_voting_basic_02 )
{
  auto vcmp = []( const std::vector< delayed_votes_data >& a, const account_object::t_delayed_votes& b )
  {
    return std::equal( a.begin(), a.end(), b.begin() );
  };

  BOOST_TEST_MESSAGE( "Testing: `delayed_voting::update_votes` method" );

  set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
  generate_block();

  ACTORS( (alice)(bob) )
  generate_block();

  FUND( "alice", ASSET( "10000.000 TESTS" ) );
  FUND( "bob", ASSET( "10000.000 TESTS" ) );

  generate_block();

  /*
    The collection `items` stores `votes_update_data` structure. Below is the definition.
      struct votes_update_data
      {
        bool                    withdraw_executor;
        mutable int64_t         val;;
        const account_object*   account;
      };
  */
  delayed_voting::opt_votes_update_data_items items = delayed_voting::votes_update_data_items();

  delayed_voting dv = delayed_voting( *db );

  fc::time_point_sec time = db->head_block_time() + fc::minutes( 5 );

  auto& __alice = db->get_account( "alice" );
  auto alice_dv = __alice.delayed_votes;
  auto alice_sum = __alice.sum_delayed_votes;

  auto& __bob = db->get_account( "bob" );
  auto bob_dv = __bob.delayed_votes;
  auto bob_sum = __bob.sum_delayed_votes;

  {
    delayed_voting::opt_votes_update_data_items _items;
    dv.update_votes( _items, time );
    auto& _alice = db->get_account( "alice" );
    BOOST_REQUIRE( vcmp( { { alice_dv[0].time, alice_dv[0].val } }, _alice.delayed_votes ) );
    BOOST_REQUIRE( alice_sum == _alice.sum_delayed_votes );
  }
  {
    dv.update_votes( items, time );
    auto& _alice = db->get_account( "alice" );
    BOOST_REQUIRE( vcmp( { { alice_dv[0].time, alice_dv[0].val } }, _alice.delayed_votes ) );
    BOOST_REQUIRE( alice_sum == _alice.sum_delayed_votes );
  }
  {
    dv.add_votes( items, false/*withdraw_executor*/, 0/*val*/, db->get_account( "alice" ) );
    dv.update_votes( items, time );
    auto& _alice = db->get_account( "alice" );
    BOOST_REQUIRE( vcmp( { { alice_dv[0].time, alice_dv[0].val } }, _alice.delayed_votes ) );
    BOOST_REQUIRE( alice_sum == _alice.sum_delayed_votes );
  }
  {
    dv.add_votes( items, false/*withdraw_executor*/, -5/*val*/, db->get_account( "alice" ) );
    //delayed_voting_messages::incorrect_votes_update
    HIVE_REQUIRE_THROW( dv.update_votes( items, time ), fc::exception );
    items->clear();

    dv.add_votes( items, true/*withdraw_executor*/, 1/*val*/, db->get_account( "alice" ) );
    //delayed_voting_messages::incorrect_votes_update
    HIVE_REQUIRE_THROW( dv.update_votes( items, time ), fc::exception );
    items->clear();
  }
  {
    auto& alice = db->get_account( "alice" );
    auto& bob = db->get_account( "bob" );

    dv.add_delayed_value( alice, time, 70 );
    dv.add_delayed_value( bob, time, 88 );

    dv.add_votes( items, false/*withdraw_executor*/, 7/*val*/, db->get_account( "alice" ) );
    dv.add_votes( items, true/*withdraw_executor*/, -8/*val*/, db->get_account( "bob" ) );
    dv.update_votes( items, time );

    auto& alice2 = db->get_account( "alice" );
    BOOST_REQUIRE( vcmp( { { alice_dv[0].time, alice_dv[0].val + 70 + 7 } }, alice2.delayed_votes ) );
    BOOST_REQUIRE( alice_sum + 70 + 7 == alice2.sum_delayed_votes );

    auto& bob2 = db->get_account( "bob" );
    BOOST_REQUIRE( vcmp( { { bob_dv[0].time, bob_dv[0].val + 88 - 8 } }, bob2.delayed_votes ) );
    BOOST_REQUIRE( bob_sum + 88 - 8 == bob2.sum_delayed_votes );
  }

}

BOOST_AUTO_TEST_CASE( delayed_voting_basic_01 )
{
  /*
    The collection `items` stores `votes_update_data` structure. Below is the definition.
      struct votes_update_data
      {
        bool                    withdraw_executor;
        mutable int64_t         val;;
        const account_object*   account;
      };
  */
  delayed_voting::opt_votes_update_data_items items = delayed_voting::votes_update_data_items();

  auto cmp = [ &items, this ]( bool withdraw_executor, int64_t val, const account_object& obj )
  {
    return check_collection( items, withdraw_executor, val, obj );
  };

  BOOST_TEST_MESSAGE( "Testing: `delayed_voting::add_votes` method" );

  ACTORS( (alice)(bob)(celine) )

  std::array< const account_object*, 3 > accs{ &db->get_account( "alice" ),
                                &db->get_account( "bob" ),
                                &db->get_account( "celine" )
                                };

  delayed_voting dv = delayed_voting( *db );

  {
    delayed_voting::opt_votes_update_data_items _items;
    dv.add_votes( _items, true/*withdraw_executor*/, 1/*val*/, *accs[0] );
    BOOST_REQUIRE( !_items.valid() );
  }
  {
    dv.add_votes( items, true/*withdraw_executor*/, 0/*val*/, *accs[0] );
    BOOST_REQUIRE( items->size() == 0 );
  }
  {
    dv.add_votes( items, true/*withdraw_executor*/, 1/*val*/, *accs[0] );
    BOOST_REQUIRE( items->size() == 1 );
    BOOST_REQUIRE( cmp( true, 1, *accs[0] ) );
  }
  {
    //delayed_voting_messages::incorrect_withdraw_data
    HIVE_REQUIRE_THROW( dv.add_votes( items, false/*withdraw_executor*/, 88/*val*/, *accs[0] ), fc::exception );
  }
  {
    dv.add_votes( items, true/*withdraw_executor*/, 2/*val*/, *accs[1] );
    BOOST_REQUIRE( items->size() == 2 );
    BOOST_REQUIRE( cmp( true, 1, *accs[0] ) );
    BOOST_REQUIRE( cmp( true, 2, *accs[1] ) );
  }
  {
    dv.add_votes( items, true/*withdraw_executor*/, 3/*val*/, *accs[1] );
    BOOST_REQUIRE( items->size() == 2 );
    BOOST_REQUIRE( cmp( true, 1, *accs[0] ) );
    BOOST_REQUIRE( cmp( true, 5, *accs[1] ) );
  }
  {
    dv.add_votes( items, true/*withdraw_executor*/, 4/*val*/, *accs[2] );
    BOOST_REQUIRE( items->size() == 3 );
    BOOST_REQUIRE( cmp( true, 1, *accs[0] ) );
    BOOST_REQUIRE( cmp( true, 5, *accs[1] ) );
    BOOST_REQUIRE( cmp( true, 4, *accs[2] ) );
  }
  {
    //delayed_voting_messages::incorrect_withdraw_data
    HIVE_REQUIRE_THROW( dv.add_votes( items, false/*withdraw_executor*/, 4/*val*/, *accs[1] ), fc::exception );
  }
}

BOOST_AUTO_TEST_CASE( delayed_voting_processor_03 )
{
  std::vector< delayed_votes_data > dq;

  ushare_type sum = 0;

  fc::time_point_sec time = fc::variant( "2020-02-02T03:04:05" ).as< fc::time_point_sec >();

  {
    //nothing to do
    delayed_voting_processor::erase( dq, sum, 0ul );
    delayed_voting_processor::add( dq, sum, time/*time*/, 0/*val*/ );
    delayed_voting_processor::erase_front( dq, sum );
  }

  {
    BOOST_REQUIRE( dq.size() == 0 );
    //delayed_voting_messages::incorrect_erased_votes
    HIVE_REQUIRE_THROW( delayed_voting_processor::erase( dq, sum, 1 ), fc::exception );
  }

  {
    delayed_voting_processor::add( dq, sum, time/*time*/, 666ul/*val*/ );
    delayed_voting_processor::erase( dq, sum, 666ul );
    BOOST_REQUIRE( dq.size() == 0 );
  }

  {
    delayed_voting_processor::add( dq, sum, time + fc::days( 1 )/*time*/, 22ul/*val*/ );
    delayed_voting_processor::add( dq, sum, time + fc::days( 2 )/*time*/, 33ul/*val*/ );
    delayed_voting_processor::erase( dq, sum, 55 );
    BOOST_REQUIRE( dq.size() == 0 );
  }
}

BOOST_AUTO_TEST_CASE( delayed_voting_processor_02 )
{
  std::vector< delayed_votes_data > dq;

  auto cmp = [ &dq, this ]( ushare_type idx, const fc::time_point_sec& time, ushare_type val )
  {
    return check_collection( dq, idx, time, val );
  };

  ushare_type sum = 0;

  fc::time_point_sec time = fc::variant( "2020-01-02T03:04:05" ).as< fc::time_point_sec >();

  const ushare_type _max = 10;
  ushare_type calculated_sum = 0;

  //dq={1,2,3,4,5,6,7,8,9,10}
  for( ushare_type i = 0; i < _max; ++i )
  {
    calculated_sum += i + 1;
    delayed_voting_processor::add( dq, sum, time + fc::days( ( i + 1 ).value )/*time*/, i + 1/*val*/ );
  }
  BOOST_REQUIRE( dq.size() == _max );
  BOOST_REQUIRE( sum == calculated_sum );

  for( ushare_type i = 0; i < _max; ++i )
    BOOST_REQUIRE( cmp( i/*idx*/, time + fc::days( ( i + 1 ).value )/*time*/, i + 1/*val*/ ) );

  //Decrease gradually last element by `1` in `dq`.
  for( ushare_type cnt = 0; cnt < _max - 1; ++cnt )
  {
    idump( (cnt) );

    delayed_voting_processor::erase( dq, sum, 1 );
    BOOST_REQUIRE( dq.size() == _max );
    BOOST_REQUIRE( sum == calculated_sum - ( cnt + 1 ) );

    for( ushare_type i = 0; i < _max; ++i )
    {
      if( i == _max - 1 )
        BOOST_REQUIRE( cmp( i/*idx*/, time + fc::days( ( i + 1 ).value )/*time*/, i - cnt/*val*/ ) );
      else
        BOOST_REQUIRE( cmp( i/*idx*/, time + fc::days( ( i + 1 ).value )/*time*/, i + 1/*val*/ ) );
    }
  }

  //dq={1,2,3,4,5,6,7,8,9,1}
  //Last element has only `1`, so after below operation last element should be removed
  delayed_voting_processor::erase( dq, sum, 1ul );
  BOOST_REQUIRE( dq.size() == _max - 1 );
  BOOST_REQUIRE( sum == calculated_sum - 10 );

  //dq={1,2,3,4,5,6,7,8,9}
  //Two last elements will disappear.
  delayed_voting_processor::erase( dq, sum, 17ul );
  BOOST_REQUIRE( dq.size() == _max - 3 );
  BOOST_REQUIRE( sum == calculated_sum - 27 );

  //dq={1,2,3,4,5,6,7}
  delayed_voting_processor::erase( dq, sum, 7ul );
  BOOST_REQUIRE( dq.size() == _max - 4 );
  BOOST_REQUIRE( sum == calculated_sum - 34 );

  //dq={1,2,3,4,5,6}
  delayed_voting_processor::erase( dq, sum, 18ul );
  BOOST_REQUIRE( dq.size() == _max - 8 );
  BOOST_REQUIRE( sum == calculated_sum - 52 );

  //delayed_voting_messages::incorrect_erased_votes
  //dq={1,2}
  HIVE_REQUIRE_THROW( delayed_voting_processor::erase( dq, sum, 10ul ), fc::exception );

  //dq={1,2}
  delayed_voting_processor::erase( dq, sum, 3ul );
  BOOST_REQUIRE( dq.size() == 0 );
  BOOST_REQUIRE( sum == 0 );
}

BOOST_AUTO_TEST_CASE( delayed_voting_processor_01 )
{
  std::vector< delayed_votes_data > dq;

  auto cmp = [ &dq, this ]( size_t idx, const fc::time_point_sec& time, uint64_t val )
  {
    return check_collection( dq, idx, time, val );
  };

  ushare_type sum = 0;

  fc::time_point_sec time = fc::variant( "2000-01-01T00:00:00" ).as< fc::time_point_sec >();

  {
    delayed_voting_processor::add( dq, sum, time + fc::minutes( 1 )/*time*/, 1ul/*val*/ );
    BOOST_REQUIRE( dq.size() == 1 );
    BOOST_REQUIRE( sum == 1 );
    BOOST_REQUIRE( cmp( 0/*idx*/, time + fc::minutes( 1 )/*time*/, 1ul/*val*/ ) );
  }
  {
    delayed_voting_processor::add( dq, sum, time + fc::minutes( 1 ) + fc::minutes( 30 )/*time*/, 2ul/*val*/ );
    BOOST_REQUIRE( dq.size() == 1 );
    BOOST_REQUIRE( sum == 3 );
    BOOST_REQUIRE( cmp( 0/*idx*/, time + fc::minutes( 1 )/*time*/, 3/*val*/ ) );
  }
  {
    delayed_voting_processor::add( dq, sum, time + fc::minutes( 1 ) + fc::seconds( HIVE_DELAYED_VOTING_INTERVAL_SECONDS ) - fc::seconds( 3 )/*time*/, 3ul/*val*/ );
    BOOST_REQUIRE( dq.size() == 1 );
    BOOST_REQUIRE( sum == 6 );
    BOOST_REQUIRE( cmp( 0/*idx*/, time + fc::minutes( 1 )/*time*/, 6/*val*/ ) );
  }
  {
    delayed_voting_processor::add( dq, sum, time + fc::minutes( 1 ) + fc::seconds( HIVE_DELAYED_VOTING_INTERVAL_SECONDS )/*time*/, 4ul/*val*/ );
    BOOST_REQUIRE( dq.size() == 2 );
    BOOST_REQUIRE( sum == 10 );
    BOOST_REQUIRE( cmp( 0/*idx*/, time + fc::minutes( 1 )/*time*/, 6/*val*/ ) );
    BOOST_REQUIRE( cmp( 1/*idx*/, time + fc::minutes( 1 ) + fc::seconds( HIVE_DELAYED_VOTING_INTERVAL_SECONDS )/*time*/, 4/*val*/ ) );
  }
  {
    delayed_voting_processor::add( dq, sum, time + fc::minutes( 1 ) + fc::seconds( 2*HIVE_DELAYED_VOTING_INTERVAL_SECONDS )/*time*/, 5ul/*val*/ );
    BOOST_REQUIRE( dq.size() == 3 );
    BOOST_REQUIRE( sum == 15 );
    BOOST_REQUIRE( cmp( 0/*idx*/, time + fc::minutes( 1 )/*time*/, 6/*val*/ ) );
    BOOST_REQUIRE( cmp( 1/*idx*/, time + fc::minutes( 1 ) + fc::seconds( HIVE_DELAYED_VOTING_INTERVAL_SECONDS )/*time*/, 4/*val*/ ) );
    BOOST_REQUIRE( cmp( 2/*idx*/, time + fc::minutes( 1 ) + fc::seconds( 2*HIVE_DELAYED_VOTING_INTERVAL_SECONDS )/*time*/, /*val*/5 ) );
  }
  {
    delayed_voting_processor::erase_front( dq, sum );
    BOOST_REQUIRE( dq.size() == 2 );
    BOOST_REQUIRE( sum == 9 );
    BOOST_REQUIRE( cmp( 0/*idx*/, time + fc::minutes( 1 ) + fc::seconds( HIVE_DELAYED_VOTING_INTERVAL_SECONDS )/*time*/, 4/*val*/ ) );
    BOOST_REQUIRE( cmp( 1/*idx*/, time + fc::minutes( 1 ) + fc::seconds( 2*HIVE_DELAYED_VOTING_INTERVAL_SECONDS )/*time*/, 5/*val*/ ) );
  }
  {
    delayed_voting_processor::erase_front( dq, sum );
    BOOST_REQUIRE( dq.size() == 1 );
    BOOST_REQUIRE( sum == 5 );
    BOOST_REQUIRE( cmp( 0/*idx*/, time + fc::minutes( 1 ) + fc::seconds( 2*HIVE_DELAYED_VOTING_INTERVAL_SECONDS )/*time*/, 5/*val*/ ) );
  }
  {
    delayed_voting_processor::add( dq, sum, time + fc::minutes( 2 ) + fc::seconds( 2*HIVE_DELAYED_VOTING_INTERVAL_SECONDS )/*time*/, 6/*val*/ );
    BOOST_REQUIRE( dq.size() == 1 );
    BOOST_REQUIRE( sum == 11 );
    BOOST_REQUIRE( cmp( 0/*idx*/, time + fc::minutes( 1 ) + fc::seconds( 2*HIVE_DELAYED_VOTING_INTERVAL_SECONDS )/*time*/, 11/*val*/ ) );
  }
  {
    delayed_voting_processor::add( dq, sum, time + fc::minutes( 1 ) + fc::seconds( 3*HIVE_DELAYED_VOTING_INTERVAL_SECONDS )/*time*/, 7/*val*/ );
    BOOST_REQUIRE( dq.size() == 2 );
    BOOST_REQUIRE( sum == 18 );
    BOOST_REQUIRE( cmp( 0/*idx*/, time + fc::minutes( 1 ) + fc::seconds( 2*HIVE_DELAYED_VOTING_INTERVAL_SECONDS )/*time*/, 11/*val*/ ) );
    BOOST_REQUIRE( cmp( 1/*idx*/, time + fc::minutes( 1 ) + fc::seconds( 3*HIVE_DELAYED_VOTING_INTERVAL_SECONDS )/*time*/, 7/*val*/ ) );
  }
  {
    delayed_voting_processor::add( dq, sum, time + fc::minutes( 1 ) + fc::seconds( 3*HIVE_DELAYED_VOTING_INTERVAL_SECONDS ) + fc::seconds( 3 )/*time*/, 8/*val*/ );
    BOOST_REQUIRE( dq.size() == 2 );
    BOOST_REQUIRE( sum == 26 );
    BOOST_REQUIRE( cmp( 0/*idx*/, time + fc::minutes( 1 ) + fc::seconds( 2*HIVE_DELAYED_VOTING_INTERVAL_SECONDS )/*time*/, 11/*val*/ ) );
    BOOST_REQUIRE( cmp( 1/*idx*/, time + fc::minutes( 1 ) + fc::seconds( 3*HIVE_DELAYED_VOTING_INTERVAL_SECONDS )/*time*/, 15/*val*/ ) );
  }
  {
    delayed_voting_processor::erase_front( dq, sum );
    BOOST_REQUIRE( dq.size() == 1 );
    BOOST_REQUIRE( sum == 15 );
    BOOST_REQUIRE( cmp( 0/*idx*/, time + fc::minutes( 1 ) + fc::seconds( 3*HIVE_DELAYED_VOTING_INTERVAL_SECONDS )/*time*/, 15/*val*/ ) );
  }
  {
    delayed_voting_processor::erase_front( dq, sum );
    BOOST_REQUIRE( dq.size() == 0 );
    BOOST_REQUIRE( sum == 0 );

    delayed_voting_processor::erase_front( dq, sum );
    BOOST_REQUIRE( dq.size() == 0 );
    BOOST_REQUIRE( sum == 0 );

    delayed_voting_processor::erase_front( dq, sum );
    BOOST_REQUIRE( dq.size() == 0 );
    BOOST_REQUIRE( sum == 0 );
  }
  {
    //delayed_voting_messages::incorrect_sum_equal
    sum = 1;
    HIVE_REQUIRE_THROW( delayed_voting_processor::erase_front( dq, sum ), fc::exception );

    sum = 0;
    delayed_voting_processor::erase_front( dq, sum );
    BOOST_REQUIRE( dq.size() == 0 );
  }
  {
    delayed_voting_processor::add( dq, sum, time, 2/*val*/ );
    --sum;
    //delayed_voting_messages::incorrect_sum_greater_equal
    HIVE_REQUIRE_THROW( delayed_voting_processor::erase_front( dq, sum ), fc::exception );

    ++sum;
    delayed_voting_processor::erase_front( dq, sum );
    BOOST_REQUIRE( dq.size() == 0 );
    BOOST_REQUIRE( sum == 0 );
  }
  {
    delayed_voting_processor::add( dq, sum, time + fc::seconds( 30 )/*time*/, 1000/*val*/ );
    BOOST_REQUIRE( dq.size() == 1 );
    BOOST_REQUIRE( sum == 1000 );
    BOOST_REQUIRE( cmp( 0/*idx*/, time + fc::seconds( 30 )/*time*/, 1000/*val*/ ) );

    //delayed_voting_messages::incorrect_head_time
    HIVE_REQUIRE_THROW( delayed_voting_processor::add( dq, sum, time + fc::seconds( 29 )/*time*/, 1000/*val*/ ), fc::exception );

    delayed_voting_processor::erase_front( dq, sum );
    BOOST_REQUIRE( dq.size() == 0 );
  }
}

BOOST_AUTO_TEST_CASE( decline_voting_rights_01 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: decline voting rights: casual use" );

    ACTORS( (bob)(alice)(witness) )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    // auto start_time = db->head_block_time();
    FUND( "bob", ASSET( "10000.000 TESTS" ) );
    FUND( "alice", ASSET( "10000.000 TESTS" ) );
    FUND( "witness", ASSET( "10000.000 TESTS" ) );
    
    //Prepare witnesses
    witness_create( "witness", witness_private_key, "url.witness", witness_private_key.get_public_key(), HIVE_MIN_PRODUCER_REWARD.amount );
    generate_block();

    //Make some vests
    const share_type basic_votes = get_votes( "witness" );
    vest( "bob", "bob", ASSET( "1000.000 TESTS" ), bob_private_key );
    vest( "alice", "alice", ASSET( "1000.000 TESTS" ), alice_private_key );
    generate_block();
    
    // alice - do
    witness_vote( "alice", "witness", true/*approve*/, alice_private_key );
    generate_block();

    // alice - check
    generate_days_blocks( 30, true );
    const auto alice_power = get_vesting( "alice" ).amount.value;
    BOOST_REQUIRE( get_votes("witness") == basic_votes + alice_power );

    // blocked bob - do
    decline_voting_rights("bob", true, bob_private_key);
    generate_blocks( db->head_block_time() + HIVE_OWNER_AUTH_RECOVERY_PERIOD, true );
    BOOST_REQUIRE( CAN_VOTE("bob") == false );
    HIVE_REQUIRE_THROW( witness_vote( "bob", "witness", true, bob_private_key ), fc::assert_exception );
    generate_block();

    // blocked bob - check
    generate_days_blocks( 30, true );
    BOOST_REQUIRE( get_votes("witness") == basic_votes + alice_power );
  }
  FC_LOG_AND_RETHROW();
}

BOOST_AUTO_TEST_CASE( decline_voting_rights_02 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: decline voting rights: casual use with spontaneus vote" );

    ACTORS( (bob)(alice)(witness) )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    FUND( "bob", ASSET( "10000.000 TESTS" ) );
    FUND( "alice", ASSET( "10000.000 TESTS" ) );
    FUND( "witness", ASSET( "10000.000 TESTS" ) );
    
    //Prepare witnesses
    witness_create( "witness", witness_private_key, "url.witness", witness_private_key.get_public_key(), HIVE_MIN_PRODUCER_REWARD.amount );
    generate_block();

    //Make some vests
    const share_type basic_votes = get_votes( "witness" );
    vest( "bob", "bob", ASSET( "1000.000 TESTS" ), bob_private_key );
    vest( "alice", "alice", ASSET( "1000.000 TESTS" ), alice_private_key );
    generate_days_blocks( 30, true );
    
    // alice - do
    witness_vote( "alice", "witness", true/*approve*/, alice_private_key );
    generate_block();

    // alice - check
    const auto alice_power = get_vesting( "alice" ).amount.value;
    BOOST_REQUIRE( get_votes("witness") == basic_votes + alice_power );

    // bob block
    decline_voting_rights("bob", true, bob_private_key);
    generate_blocks( db->head_block_time() + (HIVE_OWNER_AUTH_RECOVERY_PERIOD.to_seconds() / 2), true );

    // bob check
    BOOST_REQUIRE( CAN_VOTE("bob") );
    
    // bob account is voting
    witness_vote( "bob", "witness", true, bob_private_key );
    generate_blocks( db->head_block_time() + (HIVE_OWNER_AUTH_RECOVERY_PERIOD.to_seconds() / 3), true );
    
    // bob check
    BOOST_REQUIRE( CAN_VOTE("bob") );
    BOOST_REQUIRE( get_user_voted_witness_count( "bob" ) == 1 );
    const auto bob_power = get_vesting( "bob" ).amount.value;
    BOOST_REQUIRE( get_votes("witness") == basic_votes + alice_power + bob_power );

    // bobs block request is active
    generate_blocks( db->head_block_time() + (HIVE_OWNER_AUTH_RECOVERY_PERIOD.to_seconds() / 3), true );
    
    // blocked bob - check
    HIVE_REQUIRE_THROW( witness_vote( "bob", "witness", true, bob_private_key ), fc::assert_exception );
    BOOST_REQUIRE( get_votes("witness") == basic_votes + alice_power );
    BOOST_REQUIRE( CAN_VOTE("bob") == false );
  }
  FC_LOG_AND_RETHROW();
}

BOOST_AUTO_TEST_CASE( decline_voting_rights_03 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: decline voting rights: casual use with cancel" );

    const std::function<bool(const account_name_type)>  is_cancelled = [&](const account_name_type name)
    {
      const auto& request_idx = db->get_index< decline_voting_rights_request_index >().indices().get< by_account >();
      return request_idx.find( name ) == request_idx.end();
    };

    ACTORS( (bob)(alice)(witness)(witness2) )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    // auto start_time = db->head_block_time();
    FUND( "bob", ASSET( "10000.000 TESTS" ) );
    FUND( "alice", ASSET( "10000.000 TESTS" ) );
    FUND( "witness", ASSET( "10000.000 TESTS" ) );
    FUND( "witness2", ASSET( "10000.000 TESTS" ) );
    
    //Prepare witnesses
    witness_create( "witness", witness_private_key, "url.witness", witness_private_key.get_public_key(), HIVE_MIN_PRODUCER_REWARD.amount );
    witness_create( "witness2", witness2_private_key, "url.witness2", witness2_private_key.get_public_key(), HIVE_MIN_PRODUCER_REWARD.amount );
    generate_block();

    //Make some vests
    const share_type basic_votes = get_votes( "witness" );
    const share_type basic_votes2 = get_votes( "witness2" );
    vest( "bob", "bob", ASSET( "1000.000 TESTS" ), bob_private_key );
    vest( "alice", "alice", ASSET( "1000.000 TESTS" ), alice_private_key );
    generate_days_blocks( 30, true );
    
    // alice - do
    witness_vote( "alice", "witness", true/*approve*/, alice_private_key );
    generate_block();

    // alice - check
    const auto alice_power = get_vesting( "alice" ).amount.value;
    BOOST_REQUIRE( get_votes("witness") == basic_votes + alice_power );

    // bob block
    decline_voting_rights("bob", true, bob_private_key);
    generate_blocks( db->head_block_time() + (HIVE_OWNER_AUTH_RECOVERY_PERIOD.to_seconds() / 2), true );

    // bob check
    BOOST_REQUIRE( CAN_VOTE("bob") );
    
    // bob account tries to vote
    witness_vote( "bob", "witness", true, bob_private_key );
    BOOST_REQUIRE( get_user_voted_witness_count( "bob" ) == 1 );
    const auto bob_power = get_vesting( "bob" ).amount.value;
    BOOST_REQUIRE( get_votes("witness") == basic_votes + alice_power + bob_power );

    // bob cancel block request
    decline_voting_rights("bob", false, bob_private_key);
    generate_block();

    // check
    BOOST_REQUIRE( CAN_VOTE("bob") );
    BOOST_REQUIRE( is_cancelled( "bob" ) );

    // bob account is voting again
    witness_vote( "bob", "witness2", true, bob_private_key );
    generate_block();

    // check
    BOOST_REQUIRE( get_votes("witness") == basic_votes + alice_power + bob_power );
    BOOST_REQUIRE( get_votes("witness2") == basic_votes2 + bob_power );
  }
  FC_LOG_AND_RETHROW();
}

BOOST_AUTO_TEST_CASE( decline_voting_rights_04 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: decline voting rights: casual use with cancel" );

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    const std::function<bool(const account_name_type)>  is_cancelled = [&](const account_name_type name)
    {
      const auto& request_idx = db->get_index< decline_voting_rights_request_index >().indices().get< by_account >();
      return request_idx.find( name ) == request_idx.end();
    };

    ACTORS( (bob)(alice)(witness) )
    generate_block();

    // auto start_time = db->head_block_time();
    FUND( "bob", ASSET( "10000.000 TESTS" ) );
    FUND( "alice", ASSET( "10000.000 TESTS" ) );
    FUND( "witness", ASSET( "10000.000 TESTS" ) );
    
    //Prepare witnesses
    witness_create( "witness", witness_private_key, "url.witness", witness_private_key.get_public_key(), HIVE_MIN_PRODUCER_REWARD.amount );
    generate_block();

    //Make some vests
    const share_type basic_votes = get_votes( "witness" );
    vest( "bob", "bob", ASSET( "1000.000 TESTS" ), bob_private_key ); 
    witness_vote( "bob", "witness", true, bob_private_key );
    generate_block();
    BOOST_REQUIRE( get_votes("witness") == basic_votes );
    generate_blocks( db->head_block_time() + ( HIVE_DELAYED_VOTING_INTERVAL_SECONDS * 29 ) , true ); // 120s
    
    // bob check
    BOOST_REQUIRE( CAN_VOTE("bob") );
    BOOST_REQUIRE( get_user_voted_witness_count( "bob" ) == 1 );
    const auto bob_power = get_vesting( "bob" ).amount.value;
    BOOST_REQUIRE( get_votes("witness") == basic_votes );

    // bob block
    decline_voting_rights("bob", true, bob_private_key);
    generate_blocks( db->head_block_time() + (HIVE_OWNER_AUTH_RECOVERY_PERIOD.to_seconds() / 2), true ); 

    // bob check
    BOOST_REQUIRE( CAN_VOTE("bob") );
    BOOST_REQUIRE( get_user_voted_witness_count( "bob" ) == 1 );
    BOOST_REQUIRE( get_votes("witness") == basic_votes );


    decline_voting_rights("bob", false, bob_private_key);

    generate_blocks( db->head_block_time() + HIVE_DELAYED_VOTING_INTERVAL_SECONDS , true ); 

    BOOST_REQUIRE( get_votes("witness") == basic_votes + bob_power );
    BOOST_REQUIRE( CAN_VOTE("bob") );  // block has been cancelled
    BOOST_REQUIRE( get_user_voted_witness_count( "bob" ) == 1 );
  }
  FC_LOG_AND_RETHROW();
}

BOOST_AUTO_TEST_CASE( small_common_test_01 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: simulation of trying to overcome system" );

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    ACTORS( (alice)(witness) )
    generate_block();

    //auto start_time = db->head_block_time();

    FUND( "alice", ASSET( "1000000.001 TESTS" ) );
    FUND( "witness", ASSET( "10000.000 TESTS" ) );
    //Prepare witnesses
    
    witness_create( "witness", witness_private_key, "url.witness", witness_private_key.get_public_key(), HIVE_MIN_PRODUCER_REWARD.amount );
    generate_block();

    auto start_time = db->head_block_time();
    witness_vote( "alice", "witness", true/*approve*/, alice_private_key );
    generate_block();

    share_type basic_votes = get_votes( "witness" );

    //First start timer with low, not suspicious amount of vests
    vest( "alice", "alice", ASSET( "0.001 TESTS" ), alice_private_key );
    generate_block();

    share_type votes_01 = get_votes( "witness" );
    BOOST_REQUIRE( votes_01 == basic_votes );

    generate_blocks( start_time + fc::seconds( (NR_INTERVALS_IN_DELAYED_VOTING - 1) * HIVE_DELAYED_VOTING_INTERVAL_SECONDS ) , true );
    generate_block();

    // just before lock down alice vests huge amount of TESTs
    const auto alice_vp = get_vesting( "alice" ).amount.value;
    vest( "alice", "alice", ASSET( "1000000.000 TESTS" ), alice_private_key );
    const auto alice_vp_2 = get_vesting( "alice" ).amount.value;
    generate_block();

    generate_blocks( start_time + HIVE_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS , true );
    generate_block();

    // lock is working and alice doesn't have huge amount of votes in sneaky way
    BOOST_REQUIRE( get_votes( "witness" ) == basic_votes + alice_vp );
    generate_blocks( start_time + (2 * HIVE_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS ) , true );
    BOOST_REQUIRE( get_votes( "witness" ) == basic_votes + alice_vp_2 );


    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

#define ACCOUNT_REPORT( account ) \
  BOOST_TEST_MESSAGE( "[scenario_01]: " << account << " has " << asset_to_string( get_vesting( account )) ); \
  BOOST_TEST_MESSAGE( "[scenario_01]: " << account << " has " << asset_to_string( get_balance( account )) ); \
  BOOST_TEST_MESSAGE( "[scenario_01]: " << account << " has " << VOTING_POWER( account ) << " voting power" ); \
  BOOST_TEST_MESSAGE( "[scenario_01]: " << account << " has " << PROXIED_VSF( account ) << " proxied vsf" )

#define WITNESS_VOTES( witness ) db->get_witness( witness ).votes.value

#define INTERVAL_REPORT( day ) \
  BOOST_TEST_MESSAGE( "[scenario_01]: current interval: " << day ); \
  ACCOUNT_REPORT( "alice" ); \
  ACCOUNT_REPORT( "alice0bp" ); \
  BOOST_TEST_MESSAGE( "[scenario_01]: " << "alice0bp" << " has " << WITNESS_VOTES( "alice0bp" ) << " votes" ); \
  BOOST_TEST_MESSAGE( "[scenario_01]: " << "alice0bp" << " has " << DELAYED_VOTES( "alice0bp" ) << " delayed votes" ); \
  ACCOUNT_REPORT( "bob" ); \
  BOOST_TEST_MESSAGE( "[scenario_01]: " << "expected_bob_vests = " << asset_to_string( expected_bob_vests ) ); \
  ACCOUNT_REPORT( "bob0bp" ); \
  BOOST_TEST_MESSAGE( "[scenario_01]: " << "bob0bp" << " has " << WITNESS_VOTES( "bob0bp" ) << " votes" ); \
  BOOST_TEST_MESSAGE( "[scenario_01]: " << "bob0bp" << " has " << DELAYED_VOTES( "bob0bp" ) << " delayed votes" ); \
  ACCOUNT_REPORT( "carol" )

#define CHECK_ACCOUNT_VESTS( account ) \
  BOOST_REQUIRE_EQUAL( get_vesting( #account ).amount.value, ( expected_ ## account ## _vests ).amount.value )

#define CHECK_ACCOUNT_HIVE( account ) \
  BOOST_REQUIRE( get_balance( #account ) == expected_ ## account ## _hive )

#define CHECK_ACCOUNT_VP( account ) \
  BOOST_REQUIRE( VOTING_POWER( #account ) == expected_ ## account ## _vp )

#define CHECK_WITNESS_VOTES( witness ) \
  BOOST_REQUIRE( WITNESS_VOTES( #witness ) == expected_ ## witness ## _votes )

#define DAY_CHECK_VOTES_BALANCES \
  BOOST_REQUIRE( get_balance( "alice" ).amount.value != 0 ); \
  BOOST_REQUIRE_EQUAL( get_balance( "alice" ).amount.value, get_balance( "carol" ).amount.value ); \
  BOOST_TEST_MESSAGE( "[scenario_01]: " << "expected_alice_vests = " << asset_to_string( expected_alice_vests ) ); \
  CHECK_ACCOUNT_VESTS( alice ); \
  BOOST_REQUIRE_EQUAL( VOTING_POWER( "alice" ), WITNESS_VOTES( "alice0bp" ) ); \
  BOOST_TEST_MESSAGE( "[scenario_01]: " << "expected_bob_vests = " << asset_to_string( expected_bob_vests ) ); \
  CHECK_ACCOUNT_VESTS( bob ); \
  BOOST_REQUIRE_EQUAL( VOTING_POWER( "bob" ), WITNESS_VOTES( "bob0bp" ) );

#define DAY_CHECK_DELAYED_VOTES \
  BOOST_REQUIRE_EQUAL( DELAYED_VOTES( "alice0bp" ), expected_alice0bp_delayed_votes ); \
  BOOST_REQUIRE_EQUAL( DELAYED_VOTES( "bob0bp" ), expected_bob0bp_delayed_votes );

#define DAY_CHECK \
  DAY_CHECK_VOTES_BALANCES \
  DAY_CHECK_DELAYED_VOTES

#define GOTO_DAY( day ) \
  generate_days_blocks( day - today ); \
  today = day

#define GOTO_INTERVAL( interval ) \
  generate_seconds_blocks( ( interval * (HIVE_DELAYED_VOTING_INTERVAL_SECONDS) ) - today ); \

BOOST_AUTO_TEST_CASE( scenario_01 )
{
  try {

  asset initial_alice_vests;
  asset initial_alice0bp_vests;
  int64_t initial_alice0bp_vp;
  int64_t initial_alice0bp_votes;
  int64_t initial_alice0bp_delayed_votes;
  asset initial_bob_vests;
  asset initial_bob0bp_vests;
  int64_t initial_bob0bp_vp;
  int64_t initial_bob0bp_votes;
  int64_t initial_bob0bp_delayed_votes;
  asset initial_carol_hive;

  asset expected_alice_vests = initial_alice_vests;
  asset expected_alice0bp_vests;
  int64_t expected_alice0bp_vp;
  int64_t expected_alice0bp_votes;
  int64_t expected_alice0bp_delayed_votes;
  asset expected_bob_vests;
  asset expected_bob0bp_vests;
  int64_t expected_bob0bp_vp;
  int64_t expected_bob0bp_votes;
  int64_t expected_bob0bp_delayed_votes;
  asset expected_carol_hive;

/* https://gitlab.syncad.com/hive-group/hive/issues/5#note_24084

  actors: alice, alice.bp (witness of alice choice), bob, bob.bp (witness of bob choice), carol
*/
  ACTORS( (alice)(alice0bp)(bob)(bob0bp)(carol) );
  generate_block();

  BOOST_TEST_MESSAGE( "[scenario_01]: after account creation" );
  BOOST_TEST_MESSAGE( "[scenario_01]: alice has " << asset_to_string( get_vesting( "alice" )) );
  BOOST_TEST_MESSAGE( "[scenario_01]: alice has " << asset_to_string( get_balance( "alice" )) );

  witness_create( "alice0bp", alice0bp_private_key, "url.alice.bp", alice0bp_private_key.get_public_key(), HIVE_MIN_PRODUCER_REWARD.amount );
  witness_create( "bob0bp", bob0bp_private_key, "url.bob.bp", bob0bp_private_key.get_public_key(), HIVE_MIN_PRODUCER_REWARD.amount );
  generate_block();

/*
  For simplicity we are going to assume 1 vest == 1 HIVE at all times but in reality all conversions
  are done using rate from the time when action is performed.
  
  Before test starts alice has 1300 HIVE in liquid form. She sets up vest route in the following way:
  - to alice in vest form 25%
  - to bob in vest form 25%
  - to carol in liquid form 25%

  leaving remaining 25% for herself in liquid form.
*/
  signed_transaction tx;
  fund( "alice", ASSET( "1300.000 TESTS" ) );

  {
  BOOST_TEST_MESSAGE( "[scenario_01]: Setting up alice destination" );
  set_withdraw_vesting_route_operation op;
  op.from_account = "alice";
  op.to_account = "alice";
  op.percent = HIVE_1_PERCENT * 25;
  op.auto_vest = true;
  tx.operations.push_back( op );
  }

  {
  BOOST_TEST_MESSAGE( "[scenario_01]: Setting up bob destination" );
  set_withdraw_vesting_route_operation op;
  op.from_account = "alice";
  op.to_account = "bob";
  op.percent = HIVE_1_PERCENT * 25;
  op.auto_vest = true;
  tx.operations.push_back( op );
  }

  {
  BOOST_TEST_MESSAGE( "[scenario_01]: Setting up carol destination" );
  set_withdraw_vesting_route_operation op;
  op.from_account = "alice";
  op.to_account = "carol";
  op.percent = HIVE_1_PERCENT * 25;
  op.auto_vest = false;
  tx.operations.push_back( op );
  }

  tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
  sign( tx, alice_private_key );
  db->push_transaction( tx, 0 );
  validate_database();

  // set alice.bp as witness of alice choice
  witness_vote( "alice", "alice0bp", true/*approve*/, alice_private_key );
  // set bob.bp as witness of bob choice
  witness_vote( "bob", "bob0bp", true/*approve*/, bob_private_key );

  generate_block();

/*
  Day 0: alice powers up 1000 HIVE; she has 1000 vests, including delayed maturing on day 30 and 300 HIVE
*/
  uint32_t today = 0;
  asset origin_alice_vests = get_vesting( "alice" );
  BOOST_TEST_MESSAGE( "[scenario_01]: day_zero = " << today );
  BOOST_TEST_MESSAGE( "[scenario_01]: head_block_num = " << db->head_block_num() );

  BOOST_TEST_MESSAGE( "[scenario_01]: alice powers up 1000" );
  vest( "alice", "alice", ASSET( "1000.000 TESTS" ), alice_private_key );
  validate_database();

  INTERVAL_REPORT( today );
  BOOST_REQUIRE( get_balance( "alice" ) == ASSET( "300.000 TESTS" ) );

/*
  Analyzing delayed votes before they are removed.
*/
  uint32_t nr_voting_intervals = 15;
  BOOST_TEST_MESSAGE( "[scenario_01]: before set_interval( 15 ): " << get_current_time_iso_string() );
  GOTO_INTERVAL( nr_voting_intervals );
  BOOST_TEST_MESSAGE( "[scenario_01]: after set_interval( 15 ): " << get_current_time_iso_string() );

  expected_alice0bp_delayed_votes = get_vesting( "alice0bp" ).amount.value;
  expected_bob0bp_delayed_votes = get_vesting( "bob0bp" ).amount.value;

  DAY_CHECK_DELAYED_VOTES;
  
  BOOST_TEST_MESSAGE( "[scenario_01]: before set_interval( 15 ): " << get_current_time_iso_string() );
  GOTO_INTERVAL( nr_voting_intervals );
  BOOST_TEST_MESSAGE( "[scenario_01]: after set_interval( 15 ): " << get_current_time_iso_string() );

  uint32_t today_sec = ( HIVE_DELAYED_VOTING_INTERVAL_SECONDS * ( nr_voting_intervals + nr_voting_intervals ) );
  BOOST_REQUIRE_EQUAL( today_sec % 24*60*60, 0 );
  today = today_sec / ( 24*60*60 );

  expected_alice0bp_delayed_votes = 0;
  expected_bob0bp_delayed_votes = 0;

  DAY_CHECK_DELAYED_VOTES;
/*
  Day 5: alice powers up 300 HIVE; she has 1300 vests, including delayed 1000 maturing on day 30 and 300 maturing on day 35, 0 HIVE
*/
  BOOST_TEST_MESSAGE( "[scenario_01]: before set_current_day( 5 ): " << get_current_time_iso_string() );
  GOTO_DAY( 5 );
  BOOST_TEST_MESSAGE( "[scenario_01]: after set_current_day( 5 ): " << get_current_time_iso_string() );

  BOOST_TEST_MESSAGE( "[scenario_01]: alice powers up 300" );
  vest( "alice", "alice", ASSET( "300.000 TESTS" ), alice_private_key );
  validate_database();

  INTERVAL_REPORT( today );
  BOOST_REQUIRE( get_balance( "alice" ) == ASSET( "0.000 TESTS" ) );

/*
  Day 10: alice powers down 1300 vests; this schedules virtual PD actions on days 17, 24, 31, 38, 45, 52, 59, 66, 73, 80, 87, 94 and 101
*/
  GOTO_DAY( 10 );

  int64_t new_vests = (get_vesting( "alice" ) - origin_alice_vests).amount.value;
  int64_t new_vests_portion = new_vests / HIVE_VESTING_WITHDRAW_INTERVALS;

  int64_t quarter = new_vests / 4;
  int64_t portion = quarter / HIVE_VESTING_WITHDRAW_INTERVALS;

  BOOST_TEST_MESSAGE( "[scenario_01]: alice powers down all new vests" );
  withdraw_vesting( "alice", asset( new_vests, VESTS_SYMBOL ), alice_private_key );
  validate_database();

  INTERVAL_REPORT( today );
  BOOST_REQUIRE( get_balance( "alice" ) == ASSET( "0.000 TESTS" ) );

/*
  Day 17: PD of 100 vests from alice gives 25 HIVE to carol, 25 HIVE to alice, powers up 25 vests to bob (maturing on day 47)
  and ignores 25 vests of power down (power down canceled in 25% by vest route to alice);
  since there is nonzero balance as delayed, 75 vests are subtracted from second record leaving 1000 (30day) + 225 (35day)
  alice 25S+1225v=0+1000(30d)+225(35d); bob 25v=0+25(47d); carol 25S
*/
  initial_alice_vests = get_vesting( "alice" );
  initial_alice0bp_vests = get_vesting( "alice0bp" );
  initial_alice0bp_vp = VOTING_POWER( "alice0bp" );
  initial_alice0bp_votes = WITNESS_VOTES( "alice0bp" );
  initial_alice0bp_delayed_votes = DELAYED_VOTES( "alice0bp" );
  initial_bob_vests = get_vesting( "bob" );
  initial_bob0bp_vests = get_vesting( "bob0bp" );
  initial_bob0bp_vp = VOTING_POWER( "bob0bp");
  initial_bob0bp_votes = WITNESS_VOTES( "bob0bp" );
  initial_bob0bp_delayed_votes = DELAYED_VOTES( "bob0bp" );
  initial_carol_hive = get_balance( "carol" );

  expected_alice_vests = initial_alice0bp_vests;
  expected_alice0bp_vests = initial_alice0bp_vests;
  expected_alice0bp_vp = initial_alice0bp_vp;
  expected_alice0bp_votes = initial_alice0bp_votes;
  expected_bob_vests = initial_bob_vests;
  expected_bob0bp_vests = initial_bob0bp_vests;
  expected_bob0bp_vp = initial_bob0bp_vp;
  expected_bob0bp_votes = initial_bob0bp_votes;
  expected_carol_hive = initial_carol_hive;

  GOTO_DAY( 17 );
  INTERVAL_REPORT( today );

  expected_alice_vests  = initial_alice_vests - asset( new_vests_portion + 1, VESTS_SYMBOL );
  expected_alice_vests += asset( portion, VESTS_SYMBOL );
  expected_bob_vests = initial_bob_vests + asset( portion, VESTS_SYMBOL );
  DAY_CHECK;

/*
  Day 24: PD of 100 vests from alice split like above
  alice 50S+1150v=0+1000(30d)+150(35d); bob 50v=0+25(47d)+25(54d); carol 50S
*/
  GOTO_DAY( 24 );
  INTERVAL_REPORT( today );

  expected_alice_vests  = initial_alice_vests - asset( 2 * (new_vests_portion + 1), VESTS_SYMBOL );
  expected_alice_vests += asset( 2 * portion, VESTS_SYMBOL );
  expected_bob_vests = initial_bob_vests + asset( 2 * portion, VESTS_SYMBOL );
  DAY_CHECK;

/*
  Day 30: 1000 vests matures on alice, alice.bp receives 1000 vests of new voting power (1000v)
  alice 50S+1150v=1000+150(35d); bob 50v=0+25(47d)+25(54d); carol 50S
*/
  GOTO_DAY( 30 );
  INTERVAL_REPORT( today );

  DAY_CHECK;
  
/*
  Day 31: PD of 100 vests from alice
  alice 75S+1075v=1000+75(35d); bob 75v=0+25(47d)+25(54d)+25(61d); carol 75S
*/
  GOTO_DAY( 31 );
  INTERVAL_REPORT( today );

  expected_alice_vests  = initial_alice_vests - asset( 3 * (new_vests_portion + 1), VESTS_SYMBOL );
  expected_alice_vests += asset( 3 * portion, VESTS_SYMBOL );
  expected_bob_vests = initial_bob_vests + asset( 3 * portion, VESTS_SYMBOL );
  DAY_CHECK;
  
/*
  Day 35: remaining 75 vests mature on alice, alice.bp receives 75 vests of new voting power (1075v)
  alice 75S+1075v=1075; bob 75v=0+25(47d)+25(54d)+25(61d); carol 75S
*/
  GOTO_DAY( 35 );
  INTERVAL_REPORT( today );
  DAY_CHECK;
  
/*
  Day 38: PD of 100 vests from alice, alice.bp loses 75 vests of voting power (1000v)
  alice 100S+1000v=1000; bob 100v=0+25(47d)+25(54d)+25(61d)+25(68d); carol 100S
*/
  GOTO_DAY( 38 );
  INTERVAL_REPORT( today );

  expected_alice_vests  = initial_alice_vests - asset( 4 * (new_vests_portion + 1), VESTS_SYMBOL );
  expected_alice_vests += asset( 4 * portion, VESTS_SYMBOL );
  expected_bob_vests = initial_bob_vests + asset( 4 * portion, VESTS_SYMBOL );
  DAY_CHECK;
  
/*
  Day 45: PD of 100 vests from alice, alice.bp loses 75 vests of voting power (925v)
  alice 125S+925v=925; bob 125v=0+25(47d)+25(54d)+25(61d)+25(68d)+25(75d); carol 125S
*/
  GOTO_DAY( 45 );
  INTERVAL_REPORT( today );

  expected_alice_vests  = initial_alice_vests - asset( 5 * (new_vests_portion + 1), VESTS_SYMBOL );
  expected_alice_vests += asset( 5 * portion, VESTS_SYMBOL );
  expected_bob_vests = initial_bob_vests + asset( 5 * portion, VESTS_SYMBOL );
  DAY_CHECK;
  
/*
  Day 47: first 25 vests mature on bob, bob.bp receives 25 vests of new voting power (25v)
  alice 125S+925v=925; bob 125v=25+25(54d)+25(61d)+25(68d)+25(75d); carol 125S
*/
  GOTO_DAY( 47 );
  INTERVAL_REPORT( today );
  DAY_CHECK;
  
/*
  Day 52: PD of 100 vests from alice, alice.bp loses 75 vests of voting power (850v)
  alice 150S+850v=850; bob 150v=25+25(54d)+25(61d)+25(68d)+25(75d)+25(82d); carol 150S
*/
  GOTO_DAY( 52 );
  INTERVAL_REPORT( today );

  expected_alice_vests  = initial_alice_vests - asset( 6 * (new_vests_portion + 1), VESTS_SYMBOL );
  expected_alice_vests += asset( 6 * portion, VESTS_SYMBOL );
  expected_bob_vests = initial_bob_vests + asset( 6 * portion, VESTS_SYMBOL );
  DAY_CHECK;
  
/*
  Day 54: 25 vests mature on bob, bob.bp receives 25 vests of new voting power (50v)
  alice 150S+850v=850; bob 150v=50+25(61d)+25(68d)+25(75d)+25(82d); carol 150S
*/
  GOTO_DAY( 54 );
  INTERVAL_REPORT( today );
  DAY_CHECK;
  
/*
  Day 59: PD of 100 vests from alice, alice.bp loses 75 vests of voting power (775v)
  alice 175S+775v=775; bob 175v=50+25(61d)+25(68d)+25(75d)+25(82d)+25(89d); carol 175S
*/
  GOTO_DAY( 59 );
  INTERVAL_REPORT( today );

  expected_alice_vests  = initial_alice_vests - asset( 7 * (new_vests_portion + 1), VESTS_SYMBOL );
  expected_alice_vests += asset( 7 * portion, VESTS_SYMBOL );
  expected_bob_vests = initial_bob_vests + asset( 7 * portion, VESTS_SYMBOL );
  DAY_CHECK;
  
/*
  Day 61: 25 vests mature on bob, bob.bp receives 25 vests of new voting power (75v)
  alice 175S+775v=775; bob 175v=75+25(68d)+25(75d)+25(82d)+25(89d); carol 175S
*/
  GOTO_DAY( 61 );
  INTERVAL_REPORT( today );
  DAY_CHECK;
  
/*
  Day 66: PD of 100 vests from alice, alice.bp loses 75 vests of voting power (700v)
  alice 200S+700v=700; bob 200v=75+25(68d)+25(75d)+25(82d)+25(89d)+25(96d); carol 200S
*/
  GOTO_DAY( 66 );
  INTERVAL_REPORT( today );

  expected_alice_vests  = initial_alice_vests - asset( 8 * (new_vests_portion + 1), VESTS_SYMBOL );
  expected_alice_vests += asset( 8 * portion, VESTS_SYMBOL );
  expected_bob_vests = initial_bob_vests + asset( 8 * portion, VESTS_SYMBOL );
  DAY_CHECK;
  
/*
  Day 68: 25 vests mature on bob, bob.bp receives 25 vests of new voting power (100v)
  alice 200S+700v=700; bob 200v=100+25(75d)+25(82d)+25(89d)+25(96d); carol 200S
*/
  GOTO_DAY( 68 );
  INTERVAL_REPORT( today );
  DAY_CHECK;
  
/*
  Day 73: PD of 100 vests from alice, alice.bp loses 75 vests of voting power (625v)
  alice 225S+625v=625; bob 225v=100+25(75d)+25(82d)+25(89d)+25(96d)+25(103d); carol 225S
*/
  GOTO_DAY( 73 );
  INTERVAL_REPORT( today );

  expected_alice_vests  = initial_alice_vests - asset( 9 * (new_vests_portion + 1), VESTS_SYMBOL );
  expected_alice_vests += asset( 9 * portion, VESTS_SYMBOL );
  expected_bob_vests = initial_bob_vests + asset( 9 * portion, VESTS_SYMBOL );
  DAY_CHECK;
  
/*
  Day 75: 25 vests mature on bob, bob.bp receives 25 vests of new voting power (125v)
  alice 225S+625v=625; bob 225v=125+25(82d)+25(89d)+25(96d)+25(103d); carol 225S
*/
  GOTO_DAY( 75 );
  INTERVAL_REPORT( today );
  DAY_CHECK;
  
/*
  Day 80: PD of 100 vests from alice, alice.bp loses 75 vests of voting power (550v)
  alice 250S+550v=550; bob 250v=125+25(82d)+25(89d)+25(96d)+25(103d)+25(110d); carol 250S
*/
  GOTO_DAY( 80 );
  INTERVAL_REPORT( today );

  expected_alice_vests  = initial_alice_vests - asset( 10 * (new_vests_portion + 1), VESTS_SYMBOL );
  expected_alice_vests += asset( 10 * portion, VESTS_SYMBOL );
  expected_bob_vests = initial_bob_vests + asset( 10 * portion, VESTS_SYMBOL );
  DAY_CHECK;
  
/*
  Day 82: 25 vests mature on bob, bob.bp receives 25 vests of new voting power (150v)
  alice 250S+550v=550; bob 250v=150+25(89d)+25(96d)+25(103d)+25(110d); carol 250S
*/
  GOTO_DAY( 82 );
  INTERVAL_REPORT( today );
  DAY_CHECK;
  
/*
  Day 87: PD of 100 vests from alice, alice.bp loses 75 vests of voting power (475v)
  alice 275S+475v=475; bob 275v=150+25(89d)+25(96d)+25(103d)+25(110d)+25(117d); carol 275S
*/
  GOTO_DAY( 87 );
  INTERVAL_REPORT( today );

  expected_alice_vests  = initial_alice_vests - asset( 11 * (new_vests_portion + 1), VESTS_SYMBOL );
  expected_alice_vests += asset( 11 * portion, VESTS_SYMBOL );
  expected_bob_vests = initial_bob_vests + asset( 11 * portion, VESTS_SYMBOL );
  DAY_CHECK;
  
/*
  Day 89: 25 vests mature on bob, bob.bp receives 25 vests of new voting power (175v)
  alice 275S+475v=475; bob 275v=175+25(96d)+25(103d)+25(110d)+25(117d); carol 275S
*/
  GOTO_DAY( 89 );
  INTERVAL_REPORT( today );
  DAY_CHECK;
  
/*
  Day 94: PD of 100 vests from alice, alice.bp loses 75 vests of voting power (400v)
  alice 300S+400v=400; bob 300v=175+25(96d)+25(103d)+25(110d)+25(117d)+25(124d); carol 300S
*/
  GOTO_DAY( 94 );
  INTERVAL_REPORT( today );

  expected_alice_vests  = initial_alice_vests - asset( 12 * (new_vests_portion + 1), VESTS_SYMBOL );
  expected_alice_vests += asset( 12 * portion, VESTS_SYMBOL );
  expected_bob_vests = initial_bob_vests + asset( 12 * portion, VESTS_SYMBOL );
  DAY_CHECK;
  
/*
  Day 96: 25 vests mature on bob, bob.bp receives 25 vests of new voting power (200v)
  alice 300S+400v=400; bob 300v=200+25(103d)+25(110d)+25(117d)+25(124d); carol 300S
*/
  GOTO_DAY( 96 );
  INTERVAL_REPORT( today );
  DAY_CHECK;
  
/*
  Day 101: last scheduled PD of 100 vests from alice, alice.bp loses 75 vests of voting power (325v)
  alice 325S+325v=325; bob 325v=200+25(103d)+25(110d)+25(117d)+25(124d)+25(131d); carol 325S
*/
  GOTO_DAY( 101 );
  INTERVAL_REPORT( today );

  expected_alice_vests  = initial_alice_vests - asset( new_vests, VESTS_SYMBOL );
  expected_alice_vests += asset( quarter - 6, VESTS_SYMBOL );
  expected_bob_vests = initial_bob_vests + asset( quarter - 6, VESTS_SYMBOL );
  BOOST_REQUIRE( get_vesting( "alice" ) == get_vesting( "bob" ) );
  DAY_CHECK;
  
/*
  Day 103: 25 vests mature on bob, bob.bp receives 25 vests of new voting power (225v)
  alice 325S+325v=325; bob 325v=225+25(110d)+25(117d)+25(124d)+25(131d); carol 325S
*/
  GOTO_DAY( 103 );
  INTERVAL_REPORT( today );
  DAY_CHECK;
  
/*
  Day 110: 25 vests mature on bob, bob.bp receives 25 vests of new voting power (250v)
  alice 325S+325v=325; bob 325v=250+25(117d)+25(124d)+25(131d); carol 325S
*/
  GOTO_DAY( 110 );
  INTERVAL_REPORT( today );
  DAY_CHECK;
  
/*
  Day 117: 25 vests mature on bob, bob.bp receives 25 vests of new voting power (275v)
  alice 325S+325v=325; bob 325v=275+25(124d)+25(131d); carol 325S
*/
  GOTO_DAY( 117 );
  INTERVAL_REPORT( today );
  DAY_CHECK;
  
/*
  Day 124: 25 vests mature on bob, bob.bp receives 25 vests of new voting power (300v)
  alice 325S+325v=325; bob 325v=300+25(131d); carol 325S
*/
  GOTO_DAY( 124 );
  INTERVAL_REPORT( today );
  DAY_CHECK;
  
/*
  Day 131: last 25 vests mature on bob, bob.bp receives 25 vests of new voting power (325v)
  alice 325S+325v=325; bob 325v=325; carol 325S
*/
  GOTO_DAY( 131 );
  INTERVAL_REPORT( today );
  DAY_CHECK;

  }
  FC_LOG_AND_RETHROW()
}

#undef VOTING_POWER
#undef PROXIED_VSF
#undef ACCOUNT_REPORT
#undef WITNESS_VOTES
#undef DELAYED_VOTES
#undef INTERVAL_REPORT
#undef CHECK_ACCOUNT_VESTS
#undef CHECK_ACCOUNT_HIVE
#undef CHECK_ACCOUNT_VP
#undef CHECK_WITNESS_VOTES
#undef DAY_CHECK
#undef GOTO_DAY
#undef GOTO_INTERVAL

BOOST_AUTO_TEST_SUITE_END()

#pragma GCC diagnostic pop

//#endif // #if defined(IS_TEST_NET)
