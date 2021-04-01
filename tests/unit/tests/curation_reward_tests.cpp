#include <boost/test/unit_test.hpp>

#include "../db_fixture/database_fixture.hpp"

#include <hive/chain/comment_object.hpp>

using namespace hive;
using namespace hive::chain;
using namespace hive::protocol;
using fc::string;

struct curation_reward_stat
{
  using rewards_stats = std::vector<curation_reward_stat>;

  uint64_t value = 0;

  public:

    static void display_stats( const rewards_stats& stats )
    {
      std::string str = " stats: {";

      for( size_t i = 0; i < stats.size(); ++i )
      {
        str += std::to_string( stats[i].value );
        if( i < stats.size() - 1 )
          str += ",";
      }

      str += "}";

      BOOST_TEST_MESSAGE( str.c_str() );
    }

    static void cmp( const rewards_stats& stats_a, const rewards_stats& stats_b, std::function<bool( const curation_reward_stat& item_a, const curation_reward_stat& item_b )> cmp )
    {
      display_stats( stats_a );
      display_stats( stats_b );

      auto iter_a = stats_a.begin();
      auto iter_b = stats_b.begin();

      while( iter_a != stats_a.end() && iter_b != stats_b.end() )
      {
        BOOST_REQUIRE_EQUAL( true, cmp( *iter_a, *iter_b ) );

        ++iter_a;
        ++iter_b;
      }
    }

    static void cmp_3_phases( const curation_reward_stat::rewards_stats& stats_phase_0,
                              const curation_reward_stat::rewards_stats& stats_phase_1,
                              const curation_reward_stat::rewards_stats& stats_phase_2 )
    {
      {
        BOOST_TEST_MESSAGE( "Comparison phases: 0 and 1" );
        auto cmp = []( const curation_reward_stat& item_a, const curation_reward_stat& item_b )
        {
          return item_a.value && ( item_a.value / 2 == item_b.value );
        };
        curation_reward_stat::cmp( stats_phase_0, stats_phase_1, cmp );
      }
      {
        BOOST_TEST_MESSAGE( "Comparison phases: 1 and 2" );
        auto cmp = []( const curation_reward_stat& item_a, const curation_reward_stat& item_b )
        {
          return item_a.value && ( item_a.value / 4 == item_b.value );
        };
        curation_reward_stat::cmp( stats_phase_1, stats_phase_2, cmp );
      }
    }
};

struct curve_printer
{
  struct curve_item
  {
    time_point_sec time;

    uint64_t weight = 0;
    uint32_t reward = 0;

    std::string account;
  };

  using curve_items_type = std::vector<curve_item>;

  time_point_sec start_time;

  curve_items_type curve_items;
};

void print_all(std::ostream& stream, const curve_printer& cp)
{
  stream << "start_time: "<<cp.start_time.to_iso_string()<<std::endl;

  for( auto& item : cp.curve_items )
  {
    stream << "account: "<<item.account<<" time: "<<item.time.to_iso_string()<<" weight: "<<item.weight<<" reward: "<<item.reward<<std::endl;
  }
}

void print(std::ostream& stream, const curve_printer& cp)
{
  uint32_t _start_sec = cp.start_time.sec_since_epoch();

  for( auto& item : cp.curve_items )
  {
    uint32_t _start_sec2 = item.time.sec_since_epoch();
    stream<<_start_sec2 - _start_sec<<":"<<item.reward<<std::endl;
  }
}

BOOST_FIXTURE_TEST_SUITE( curation_reward_tests, clean_database_fixture )

BOOST_AUTO_TEST_CASE( basic_test )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: curation rewards after HF25. Reward during whole rewards-time (7 days)" );

    auto old_hive_cashout_windows_seconds = initial_values::get_hive_cashout_windows_seconds();
    initial_values::set_hive_cashout_windows_seconds( 60*60*24*7/*7 days like in mainnet*/ );

    ACTORS( (alice)

            (aoa00)(aoa01)(aoa02)(aoa03)(aoa04)(aoa05)(aoa06)(aoa07)(aoa08)(aoa09)
            (aoa10)(aoa11)(aoa12)(aoa13)(aoa14)(aoa15)(aoa16)(aoa17)(aoa18)(aoa19)
            (aoa20)(aoa21)(aoa22)(aoa23)(aoa24)(aoa25)(aoa26)(aoa27)(aoa28)(aoa29)
            (aoa30)(aoa31)(aoa32)(aoa33)(aoa34)(aoa35)(aoa36)(aoa37)(aoa38)(aoa39)

            (bob00)(bob01)(bob02)(bob03)(bob04)(bob05)(bob06)(bob07)(bob08)(bob09)
            (bob10)(bob11)(bob12)(bob13)(bob14)(bob15)(bob16)(bob17)(bob18)(bob19)
            (bob20)(bob21)(bob22)(bob23)(bob24)(bob25)(bob26)(bob27)(bob28)(bob29)
            (bob30)(bob31)(bob32)(bob33)(bob34)(bob35)(bob36)(bob37)(bob38)(bob39)

            (coc00)(coc01)(coc02)(coc03)(coc04)(coc05)(coc06)(coc07)(coc08)(coc09)
            (coc10)(coc11)(coc12)(coc13)(coc14)(coc15)(coc16)(coc17)(coc18)(coc19)
            (coc20)(coc21)(coc22)(coc23)(coc24)(coc25)(coc26)(coc27)(coc28)(coc29)
            (coc30)(coc31)(coc32)(coc33)(coc34)(coc35)(coc36)(coc37)(coc38)(coc39)

            (dod00)(dod01)(dod02)(dod03)(dod04)(dod05)(dod06)(dod07)(dod08)(dod09)
            (dod10)(dod11)(dod12)(dod13)(dod14)(dod15)(dod16)(dod17)(dod18)(dod19)
            (dod20)(dod21)(dod22)(dod23)(dod24)(dod25)(dod26)(dod27)(dod28)(dod29)
            (dod30)(dod31)(dod32)(dod33)(dod34)(dod35)(dod36)(dod37)(dod38)(dod39)
          )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    //=====================preparing=====================
    const auto TESTS_1000 = ASSET( "1000.000 TESTS" );
    const auto TESTS_100 = ASSET( "100.000 TESTS" );

    std::vector<std::string> voters
    {
      "aoa00", "aoa01", "aoa02", "aoa03", "aoa04", "aoa05", "aoa06", "aoa07", "aoa08", "aoa09",
      "aoa10", "aoa11", "aoa12", "aoa13", "aoa14", "aoa15", "aoa16", "aoa17", "aoa18", "aoa19",
      "aoa20", "aoa21", "aoa22", "aoa23", "aoa24", "aoa25", "aoa26", "aoa27", "aoa28", "aoa29",
      "aoa30", "aoa31", "aoa32", "aoa33", "aoa34", "aoa35", "aoa36", "aoa37", "aoa38", "aoa39",

      "bob00", "bob01", "bob02", "bob03", "bob04", "bob05", "bob06", "bob07", "bob08", "bob09",
      "bob10", "bob11", "bob12", "bob13", "bob14", "bob15", "bob16", "bob17", "bob18", "bob19",
      "bob20", "bob21", "bob22", "bob23", "bob24", "bob25", "bob26", "bob27", "bob28", "bob29",
      "bob30", "bob31", "bob32", "bob33", "bob34", "bob35", "bob36", "bob37", "bob38", "bob39",

      "coc00", "coc01", "coc02", "coc03", "coc04", "coc05", "coc06", "coc07", "coc08", "coc09",
      "coc10", "coc11", "coc12", "coc13", "coc14", "coc15", "coc16", "coc17", "coc18", "coc19",
      "coc20", "coc21", "coc22", "coc23", "coc24", "coc25", "coc26", "coc27", "coc28", "coc29",
      "coc30", "coc31", "coc32", "coc33", "coc34", "coc35", "coc36", "coc37", "coc38", "coc39",

      "dod00", "dod01", "dod02", "dod03", "dod04", "dod05", "dod06", "dod07", "dod08", "dod09",
      "dod10", "dod11", "dod12", "dod13", "dod14", "dod15", "dod16", "dod17", "dod18", "dod19",
      "dod20", "dod21", "dod22", "dod23", "dod24", "dod25", "dod26", "dod27", "dod28", "dod29",
      "dod30", "dod31", "dod32", "dod33", "dod34", "dod35", "dod36", "dod37", "dod38", "dod39"
    };

    #define key(account) account ## _private_key

    std::vector<fc::ecc::private_key> keys
    {
      key(aoa00), key(aoa01), key(aoa02), key(aoa03), key(aoa04), key(aoa05), key(aoa06), key(aoa07), key(aoa08), key(aoa09),
      key(aoa10), key(aoa11), key(aoa12), key(aoa13), key(aoa14), key(aoa15), key(aoa16), key(aoa17), key(aoa18), key(aoa19),
      key(aoa20), key(aoa21), key(aoa22), key(aoa23), key(aoa24), key(aoa25), key(aoa26), key(aoa27), key(aoa28), key(aoa29),
      key(aoa30), key(aoa31), key(aoa32), key(aoa33), key(aoa34), key(aoa35), key(aoa36), key(aoa37), key(aoa38), key(aoa39),

      key(bob00), key(bob01), key(bob02), key(bob03), key(bob04), key(bob05), key(bob06), key(bob07), key(bob08), key(bob09),
      key(bob10), key(bob11), key(bob12), key(bob13), key(bob14), key(bob15), key(bob16), key(bob17), key(bob18), key(bob19),
      key(bob20), key(bob21), key(bob22), key(bob23), key(bob24), key(bob25), key(bob26), key(bob27), key(bob28), key(bob29),
      key(bob30), key(bob31), key(bob32), key(bob33), key(bob34), key(bob35), key(bob36), key(bob37), key(bob38), key(bob39),

      key(coc00), key(coc01), key(coc02), key(coc03), key(coc04), key(coc05), key(coc06), key(coc07), key(coc08), key(coc09),
      key(coc10), key(coc11), key(coc12), key(coc13), key(coc14), key(coc15), key(coc16), key(coc17), key(coc18), key(coc19),
      key(coc20), key(coc21), key(coc22), key(coc23), key(coc24), key(coc25), key(coc26), key(coc27), key(coc28), key(coc29),
      key(coc30), key(coc31), key(coc32), key(coc33), key(coc34), key(coc35), key(coc36), key(coc37), key(coc38), key(coc39),

      key(dod00), key(dod01), key(dod02), key(dod03), key(dod04), key(dod05), key(dod06), key(dod07), key(dod08), key(dod09),
      key(dod10), key(dod11), key(dod12), key(dod13), key(dod14), key(dod15), key(dod16), key(dod17), key(dod18), key(dod19),
      key(dod20), key(dod21), key(dod22), key(dod23), key(dod24), key(dod25), key(dod26), key(dod27), key(dod28), key(dod29),
      key(dod30), key(dod31), key(dod32), key(dod33), key(dod34), key(dod35), key(dod36), key(dod37), key(dod38), key(dod39)
    };

    uint32_t h = HIVE_1_PERCENT * 10;
    std::vector<uint32_t> vote_percent
    {
      h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9,
      h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9,
      h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9,
      h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9,

      h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9,
      h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9,
      h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9,
      h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9,

      h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9,
      h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9,
      h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9,
      h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9,

      h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9,
      h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9,
      h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9,
      h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9, h*9
    };

    for( uint32_t i = 0; i < voters.size(); ++i )
    {
      fund( voters[i], TESTS_1000 );
      vest( voters[i], voters[i], TESTS_100, keys[i] );
    }

    generate_block();

    std::string author    = "alice";
    auto author_key       = alice_private_key;
    std::string subject   = "hello";
    std::string permlink  = "somethingpermlink";

    post_comment( author, permlink, "title", "body", "test", author_key );
    generate_blocks( 1 );

    auto comment_id = db->get_comment( author, permlink ).get_id();

    //12h       = 43200
    //6.5 days  = 561600
    //7days     = 604800
    const uint32_t before_old_seconds = 12;     //25 times    12*25=300
    const uint32_t before_seconds     = 4480;   //135 times   4480*135=604800
    uint32_t current_seconds          = before_old_seconds;

    curve_printer cp;
    cp.start_time = db->head_block_time();

    {
      curation_reward_stat::rewards_stats stats_phase_0;
      curation_reward_stat::rewards_stats stats_phase_1;
      curation_reward_stat::rewards_stats stats_phase_2;

      for( uint32_t i = 0; i < voters.size(); ++i )
      {

        vote( author, permlink, voters[i], vote_percent[i], keys[i] );

        auto& cv = db->get< comment_vote_object, by_comment_voter >( boost::make_tuple( comment_id, get_account_id( voters[i] ) ) );

        cp.curve_items.emplace_back( curve_printer::curve_item{ db->head_block_time(), cv.weight, 0, voters[i] } );

        generate_blocks( db->head_block_time() + fc::seconds( current_seconds ) );

        switch( i )
        {
          case 24: current_seconds = before_seconds; break;
        }
      }

      const auto& dgpo = db->get_dynamic_global_properties();

      for( auto& item : cp.curve_items )
      {
        const auto& acc = db->get_account( item.account );
        item.reward = static_cast<uint32_t>( acc.curation_rewards.value );

        uint64_t _seconds = static_cast<uint64_t>( ( cp.start_time - item.time ).to_seconds() );

        if( _seconds >= dgpo.curation_rewards_phase_0_seconds )
        {
          if( _seconds < dgpo.curation_rewards_phase_1_seconds )
            stats_phase_1.emplace_back( curation_reward_stat{ item.reward } );
          else
            stats_phase_2.emplace_back( curation_reward_stat{ item.reward } );
        }
        else
        {
          stats_phase_0.emplace_back( curation_reward_stat{ item.reward } );
        }
      }

      curation_reward_stat::cmp_3_phases( stats_phase_0, stats_phase_1, stats_phase_2 );

      print_all( std::cout, cp );
      print( std::cout, cp );
    }

    initial_values::set_hive_cashout_windows_seconds( old_hive_cashout_windows_seconds );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()
