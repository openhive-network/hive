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

    static void check_phase( const rewards_stats& stats, std::function<bool( const curation_reward_stat& item )> cmp )
    {
      display_stats( stats );

      for( auto& item : stats )
      {
        BOOST_REQUIRE( cmp( item ) );
      }
    }

    static void check_phases( const rewards_stats& stats_a, const rewards_stats& stats_b, std::function<bool( const curation_reward_stat& item_a, const curation_reward_stat& item_b )> cmp )
    {
      display_stats( stats_a );
      display_stats( stats_b );

      auto iter_a = stats_a.begin();
      auto iter_b = stats_b.begin();

      while( iter_a != stats_a.end() && iter_b != stats_b.end() )
      {
        BOOST_REQUIRE( cmp( *iter_a, *iter_b ) );

        ++iter_a;
        ++iter_b;
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

#define key(account) account ## _private_key

using namespace hive::protocol::testnet_blockchain_configuration;

struct curation_rewards_handler
{
  const uint32_t seven_days         = 60*60*24*7;

  /*
    early window(24 hours)                  <0;86400)
    mid window(24 hours from 24 hours)      <86400;259200)
    late window(until 7 days from 48 hours) <259200;604800)
  */
   const int32_t early_window       = 86400;
   const int32_t mid_window         = 259200;

  configuration                     configuration_data_copy;

  curve_printer                     cp;

  std::deque<std::string>           authors;
  std::deque<fc::ecc::private_key>  author_keys;

  std::deque<std::string>           voters;
  std::deque<fc::ecc::private_key>  voter_keys;

  clean_database_fixture&           test_object;
  chain::database&                  db;

  curation_rewards_handler( clean_database_fixture& _test_object, chain::database& _db )
  : test_object( _test_object ), db( _db )
  {
    configuration_data_copy = configuration_data;
    configuration_data.set_hive_cashout_windows_seconds( seven_days );/*7 days like in mainnet*/
  }

  ~curation_rewards_handler()
  {
    configuration_data = configuration_data_copy;
  }

  void prepare_creator( uint32_t nr_creators )
  {
    BOOST_REQUIRE_GT( nr_creators, 0 );

    switch( nr_creators )
    {
      case 3:
      {
        ACTORS_EXT( test_object, (creator2) );
        authors.push_front( "creator2" );
        author_keys.push_front( key(creator2) );
      }
      case 2:
      {
        ACTORS_EXT( test_object, (creator1) );
        authors.push_front( "creator1" );
        author_keys.push_front( key(creator1) );
      }
      case 1:
      {
        ACTORS_EXT( test_object, (creator0) );
        authors.push_front( "creator0" );
        author_keys.push_front( key(creator0) );
      }
    }
  }

  void prepare_voters()
  {
    ACTORS_EXT( test_object,
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

    voters =
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

    voter_keys =
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

  }

  void prepare_10_voters()
  {
    ACTORS_EXT( test_object,
            (aoa00)(aoa01)(aoa02)(aoa03)(aoa04)(aoa05)(aoa06)(aoa07)(aoa08)(aoa09)
          )

    voters =
      {
        "aoa00", "aoa01", "aoa02", "aoa03", "aoa04", "aoa05", "aoa06", "aoa07", "aoa08", "aoa09"
      };

    voter_keys =
      {
        key(aoa00), key(aoa01), key(aoa02), key(aoa03), key(aoa04), key(aoa05), key(aoa06), key(aoa07), key(aoa08), key(aoa09)
      };

  }

  void prepare_funds()
  {
    const auto TESTS_1000 = ASSET( "1000.000 TESTS" );
    const auto TESTS_100 = ASSET( "100.000 TESTS" );

    for( uint32_t i = 0; i < voters.size(); ++i )
    {
      test_object.fund( voters[i], TESTS_1000 );
      test_object.vest( voters[i], voters[i], TESTS_100, voter_keys[i] );
    }
  }

  void prepare_comment( const std::string& permlink, uint32_t creator_number )
  {
    BOOST_REQUIRE_LT( creator_number, author_keys.size() );
    BOOST_REQUIRE_LT( creator_number, authors.size() );

    test_object.post_comment( authors[ creator_number ], permlink, "title", "body", "test", author_keys[ creator_number ] );
  }

  void voting( size_t& vote_counter, uint32_t author_number, const std::string& permlink, const std::vector<uint32_t>& votes_time )
  {
    if( votes_time.empty() )
      return;

    BOOST_REQUIRE_GE( voters.size() + vote_counter, votes_time.size() );

    auto author = authors[ author_number ];

    uint32_t vote_percent = HIVE_1_PERCENT * 90;

    auto comment_id = db.get_comment( author, permlink ).get_id();

    for( auto& time : votes_time )
    {
      if( time )
        test_object.generate_blocks( db.head_block_time() + fc::seconds( time ) );

      test_object.vote( author, permlink, voters[ vote_counter ], vote_percent, voter_keys[ vote_counter ] );

      auto& cvo = db.get< comment_vote_object, by_comment_voter >( boost::make_tuple( comment_id, test_object.get_account_id( voters[ vote_counter ] ) ) );

      cp.curve_items.emplace_back( curve_printer::curve_item{ db.head_block_time(), cvo.weight, 0, voters[ vote_counter ] } );

      ++vote_counter;
    }
  }

  void results_gathering( curation_reward_stat::rewards_stats& early_stats, curation_reward_stat::rewards_stats& mid_stats, curation_reward_stat::rewards_stats& late_stats )
  {
    const auto& dgpo = db.get_dynamic_global_properties();

    for( auto& item : cp.curve_items )
    {
      const auto& acc = db.get_account( item.account );
      item.reward = static_cast<uint32_t>( acc.curation_rewards.value );

      uint64_t _seconds = static_cast<uint64_t>( ( item.time - cp.start_time ).to_seconds() );

      if( _seconds >= dgpo.early_voting_seconds )
      {
        if( _seconds < ( dgpo.early_voting_seconds + dgpo.mid_voting_seconds ) )
          mid_stats.emplace_back( curation_reward_stat{ item.reward } );
        else
          late_stats.emplace_back( curation_reward_stat{ item.reward } );
      }
      else
      {
        early_stats.emplace_back( curation_reward_stat{ item.reward } );
      }
    }
  }

  void make_payment()
  {
    uint64_t _seconds = static_cast<uint64_t>( ( db.head_block_time() - cp.start_time ).to_seconds() );
    if( seven_days > _seconds )
      test_object.generate_blocks( db.head_block_time() + fc::seconds( seven_days - _seconds ) );
  }
};

BOOST_FIXTURE_TEST_SUITE( curation_reward_tests, clean_database_fixture )

BOOST_AUTO_TEST_CASE( basic_test )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: curation rewards after HF25. Reward during whole rewards-time (7 days). Voting in every window." );

    curation_rewards_handler crh( *this, *db );

    crh.prepare_creator( 1/*nr_creators*/ );
    crh.prepare_voters();
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    crh.prepare_funds();
    generate_block();

    uint32_t author_number = 0;
    std::string permlink  = "somethingpermlink";

    crh.prepare_comment( permlink, author_number );
    generate_block();

    crh.cp.start_time = db->head_block_time();

    std::vector<uint32_t> early_votes_time( 25, 12 );
    std::vector<uint32_t> mid_votes_time( 39, 4480 );
    std::vector<uint32_t> late_votes_time( 95, 4480 );

    size_t vote_counter = 0;
    crh.voting( vote_counter, author_number, permlink, early_votes_time );
    crh.voting( vote_counter, author_number, permlink, mid_votes_time );
    crh.voting( vote_counter, author_number, permlink, late_votes_time );
    crh.make_payment();

    curation_reward_stat::rewards_stats early_stats;
    curation_reward_stat::rewards_stats mid_stats;
    curation_reward_stat::rewards_stats late_stats;

    crh.results_gathering( early_stats, mid_stats, late_stats );

    {
      BOOST_TEST_MESSAGE( "Comparison phases: `early` and `mid`" );
      auto cmp = []( const curation_reward_stat& item_a, const curation_reward_stat& item_b )
      {
        return item_a.value == 645 && ( item_a.value / 2 == item_b.value );
      };
      curation_reward_stat::check_phases( early_stats, mid_stats, cmp );
    }
    {
      BOOST_TEST_MESSAGE( "Comparison phases: `mid` and `late`" );
      auto cmp = []( const curation_reward_stat& item_a, const curation_reward_stat& item_b )
      {
        return item_a.value && ( item_a.value / 4 == item_b.value );
      };
      curation_reward_stat::check_phases( mid_stats, late_stats, cmp );
    }

    print_all( std::cout, crh.cp );
    print( std::cout, crh.cp );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( no_votes )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: curation rewards after HF25. Lack of votes." );

    curation_rewards_handler crh( *this, *db );

    crh.prepare_creator( 1/*nr_creators*/ );
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    uint32_t author_number = 0;
    std::string permlink  = "somethingpermlink";

    crh.prepare_comment( permlink, author_number );
    generate_block();

    generate_blocks( db->head_block_time() + fc::seconds( 60*60*24*7/*7 days like in mainnet*/ ) );

    curation_reward_stat::rewards_stats early_stats;
    curation_reward_stat::rewards_stats mid_stats;
    curation_reward_stat::rewards_stats late_stats;

    crh.results_gathering( early_stats, mid_stats, late_stats );

    const auto& creator = db->get_account( crh.authors[ author_number ] );
    BOOST_REQUIRE_EQUAL( creator.posting_rewards.value, 0 );

    auto cmp = []( const curation_reward_stat& item )
    {
      return item.value == 0;
    };
    curation_reward_stat::check_phase( early_stats, cmp );
    curation_reward_stat::check_phase( mid_stats, cmp );
    curation_reward_stat::check_phase( late_stats, cmp );

    print_all( std::cout, crh.cp );
    print( std::cout, crh.cp );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( one_vote_for_comment )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: curation rewards after HF25. Reward during whole rewards-time (7 days). Only 1 vote per 1 comment." );

    curation_rewards_handler crh( *this, *db );

    crh.prepare_creator( 3/*nr_creators*/ );
    crh.prepare_10_voters();
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    crh.prepare_funds();
    generate_block();

    std::string permlink  = "somethingpermlink";

    for( uint32_t i = 0 ; i < 3; ++i )
      crh.prepare_comment( permlink, i/*author_number*/ );
    generate_block();

    crh.cp.start_time = db->head_block_time();

    //Every comment has only 1 vote, but votes are created in different windows.
    size_t vote_counter = 0;

    uint32_t vote_time_early_window = 12;
    uint32_t vote_time_mid_window   = crh.early_window; // crh.early_window + 12
    uint32_t vote_time_late_window  = crh.mid_window;   // crh.early_window + 12 + crh.mid_window

    crh.voting( vote_counter, 0/*author_number*/, permlink, { vote_time_early_window } );
    crh.voting( vote_counter, 1/*author_number*/, permlink, { vote_time_mid_window } );
    crh.voting( vote_counter, 2/*author_number*/, permlink, { vote_time_late_window } );
    crh.make_payment();

    curation_reward_stat::rewards_stats early_stats;
    curation_reward_stat::rewards_stats mid_stats;
    curation_reward_stat::rewards_stats late_stats;

    crh.results_gathering( early_stats, mid_stats, late_stats );
    /*
      Description:
        vc0 - vote for comment_0
        vc1 - vote for comment_1
        vc2 - vote for comment_2

      |*****early_window*****|*****mid_window*****|*****late_window*****|
                vc0
                                      vc1
                                                            vc2

      Results:
          vc0 == vc1 == vc2
    */

    {
      auto cmp = []( const curation_reward_stat& item_a, const curation_reward_stat& item_b )
      {
        return item_a.value == 12463 && item_a.value == item_b.value;
      };
      curation_reward_stat::check_phases( early_stats, mid_stats, cmp );
      curation_reward_stat::check_phases( mid_stats, late_stats, cmp );
    }

    print_all( std::cout, crh.cp );
    print( std::cout, crh.cp );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( two_votes_for_comment )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: curation rewards after HF25. Reward during whole rewards-time (7 days). Only 2 votes per 1 comment." );

    curation_rewards_handler crh( *this, *db );

    crh.prepare_creator( 3/*nr_creators*/ );
    crh.prepare_10_voters();
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    crh.prepare_funds();
    generate_block();

    std::string permlink  = "somethingpermlink";

    for( uint32_t i = 0 ; i < 3; ++i )
      crh.prepare_comment( permlink, i/*author_number*/ );
    generate_block();

    crh.cp.start_time = db->head_block_time();

    /*
      Every comment has only 2 votes in different windows.
      comment_0: early_window,  mid_window
      comment_1: early_window,  late_window
      comment_2: mid_window,    late_window
    */
    size_t vote_counter = 0;

    uint32_t vote_time_early_window = 3600;
    uint32_t vote_time_mid_window   = crh.early_window;       // crh.early_window + 3600
    uint32_t vote_time_late_window  = crh.mid_window + 3600;  // crh.early_window + 3600 + crh.mid_window + 3600

    //Both votes are in the same block.
    crh.voting( vote_counter, 0/*author_number*/, permlink, { vote_time_early_window } );
    crh.voting( vote_counter, 1/*author_number*/, permlink, { 0 } );

    //Both votes are in the same block.
    crh.voting( vote_counter, 0/*author_number*/, permlink, { vote_time_mid_window } );
    crh.voting( vote_counter, 2/*author_number*/, permlink, { 0 } );

    //Both votes are in the same block.
    crh.voting( vote_counter, 1/*author_number*/, permlink, { vote_time_late_window } );
    crh.voting( vote_counter, 2/*author_number*/, permlink, { 0 } );

    crh.make_payment();

    curation_reward_stat::rewards_stats early_stats;
    curation_reward_stat::rewards_stats mid_stats;
    curation_reward_stat::rewards_stats late_stats;

    crh.results_gathering( early_stats, mid_stats, late_stats );
    /*
      Description:
        vc0a - vote(a) for comment_0      vc0b - vote(b) for comment_0
        vc1a - vote(a) for comment_1      vc1b - vote(b) for comment_1
        vc2a - vote(a) for comment_2      vc2b - vote(b) for comment_2

      |*****early_window*****|*****mid_window*****|*****late_window*****|
                vc0a                  vc0b
                vc1a                                        vc1b
                                      vc2a                  vc2b

      Results:
        sum( vc0a + vc0b ) == sum( vc1a + vc1b ) == sum( vc2a + vc2b )

        vc0a < vc1a
        vc1a > vc2a

        vc0b > vc1b
        vc1b < vc2b
    */
    {
      BOOST_REQUIRE_EQUAL( early_stats.size(),  2 );
      BOOST_REQUIRE_EQUAL( mid_stats.size(),    2 );
      BOOST_REQUIRE_EQUAL( late_stats.size(),   2 );

      BOOST_REQUIRE_EQUAL( early_stats[0].value, 8308 );
      BOOST_REQUIRE_EQUAL( early_stats[1].value, 11078 );

      BOOST_REQUIRE_EQUAL( mid_stats[0].value, 4154 );
      BOOST_REQUIRE_EQUAL( mid_stats[1].value, 9970 );

      BOOST_REQUIRE_EQUAL( late_stats[0].value, 1384 );
      BOOST_REQUIRE_EQUAL( late_stats[1].value, 2492 );

      curation_reward_stat::display_stats( early_stats );
      curation_reward_stat::display_stats( mid_stats );
      curation_reward_stat::display_stats( late_stats );

      auto vc0a = early_stats[ 0 ].value;
      auto vc0b =   mid_stats[ 0 ].value;

      auto vc1a = early_stats[ 1 ].value;
      auto vc1b =  late_stats[ 0 ].value;

      auto vc2a =   mid_stats[ 1 ].value;
      auto vc2b =  late_stats[ 1 ].value;

      BOOST_REQUIRE_EQUAL( vc0a + vc0b, vc1a + vc1b );
      BOOST_REQUIRE_EQUAL( vc1a + vc1b, vc2a + vc2b );

      BOOST_REQUIRE_LT( vc0a, vc1a );
      BOOST_REQUIRE_GT( vc1a, vc2a );

      BOOST_REQUIRE_GT( vc0b, vc1b );
      BOOST_REQUIRE_LT( vc1b, vc2b );
    }

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()
