#include <boost/test/unit_test.hpp>

#include "../db_fixture/database_fixture.hpp"

#include <hive/chain/comment_object.hpp>
#include <hive/chain/hive_objects.hpp>

using namespace hive;
using namespace hive::chain;
using namespace hive::protocol;
using fc::string;

struct window_input_data
{
  uint32_t nr_voters  = 0;
  uint32_t interval   = 0;
  uint32_t amount     = 0;
};

struct comment_reward_info
{
  share_type total_reward;
  share_type author_tokens;
  share_type curation_tokens;
};

struct reward_stat
{
  using rewards_stats = std::vector<reward_stat>;

  uint64_t value = 0;

  public:

    static void display_comment_rewards( const comment_reward_info& obj )
    {
      BOOST_TEST_MESSAGE( "calculated total reward : author-tokens : curation-tokens" );
      display_reward( obj.total_reward );
      display_reward( obj.author_tokens );
      display_reward( obj.curation_tokens );
    }

    static void display_reward( const share_type& reward )
    {
      std::string str = std::to_string( reward.value );

      BOOST_TEST_MESSAGE( str.c_str() );
    }

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

    static void check_phase( const rewards_stats& stats, std::function<bool( const reward_stat& item )> cmp )
    {
      display_stats( stats );

      for( auto& item : stats )
      {
        BOOST_REQUIRE( cmp( item ) );
      }
    }

    static void check_phases( const rewards_stats& stats_a, const rewards_stats& stats_b, std::function<bool( const reward_stat& item_a, const reward_stat& item_b )> cmp )
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

  void clear()
  {
    start_time = time_point_sec();
    curve_items.clear();
  }
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
  const uint32_t cashout_time               = 60 * 60 * 24 * 7; // 7 days - HF25 mainnet value

  /*
    early window(24 hours)                  <0;86400)
    mid window(24 hours from 24 hours)      <86400;259200)
    late window(until 7 days from 48 hours) <259200;604800)
      note that late window also contains 12h upvote lockout that we want to avoid since it would make it hard
      to compare vote power and rewards for votes cast within that extra-late window
      therefore votes must be limited not to 604800 cashout but 604800-12*3600=561600
  */
  const int32_t early_window                = 86400;
  const int32_t mid_window                  = 259200;
  const int32_t late_window                 = 561600;

  static const uint32_t default_amount      = 1'000'000;

  std::vector<curve_printer>                curve_printers;

  std::map<uint32_t, std::string>           authors;
  std::map<uint32_t, fc::ecc::private_key>  author_keys;

  std::deque<std::string>                   voters;
  std::deque<fc::ecc::private_key>          voter_keys;

  clean_database_fixture&                   test_object;
  chain::database&                          db;

  std::vector<comment_reward_info>          comment_rewards;

  boost::signals2::connection               _comment_reward_con;

  curation_rewards_handler( clean_database_fixture& _test_object, chain::database& _db )
  : test_object( _test_object ), db( _db )
  {
    create_printer();

    _comment_reward_con = db.add_comment_reward_handler(
      [&]( const comment_reward_notification& note )
      {
        comment_rewards.emplace_back( comment_reward_info{ note.total_reward, note.author_tokens, note.curation_tokens } );
      },
      appbase::app().get_plugin< hive::plugins::debug_node::debug_node_plugin >()
    );
  }

  ~curation_rewards_handler()
  {
    chain::util::disconnect_signal( _comment_reward_con );
  }

  void prepare_author( std::set<uint32_t> _authors )
  {
    BOOST_REQUIRE( !_authors.empty() );

    for( auto author : _authors )
    {
      if( author == 3 )
      {
        ACTORS_EXT( test_object, (author3) );
        authors.insert( std::make_pair( 3, "author3" ) );
        author_keys.insert( std::make_pair( 3, key(author3) ) );
      }
      if( author == 2 )
      {
        ACTORS_EXT( test_object, (author2) );
        authors.insert( std::make_pair( 2, "author2" ) );
        author_keys.insert( std::make_pair( 2, key(author2) ) );
      }
      if( author == 1 )
      {
        ACTORS_EXT( test_object, (author1) );
        authors.insert( std::make_pair( 1, "author1" ) );
        author_keys.insert( std::make_pair( 1, key(author1) ) );
      }
      if( author == 0 )
      {
        ACTORS_EXT( test_object, (author0) );
        authors.insert( std::make_pair( 0, "author0" ) );
        author_keys.insert( std::make_pair( 0, key(author0) ) );
      }
    }
  }

  void prepare_voters_0_80()
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
          )

    auto _voters =
      {
        "aoa00", "aoa01", "aoa02", "aoa03", "aoa04", "aoa05", "aoa06", "aoa07", "aoa08", "aoa09",
        "aoa10", "aoa11", "aoa12", "aoa13", "aoa14", "aoa15", "aoa16", "aoa17", "aoa18", "aoa19",
        "aoa20", "aoa21", "aoa22", "aoa23", "aoa24", "aoa25", "aoa26", "aoa27", "aoa28", "aoa29",
        "aoa30", "aoa31", "aoa32", "aoa33", "aoa34", "aoa35", "aoa36", "aoa37", "aoa38", "aoa39",

        "bob00", "bob01", "bob02", "bob03", "bob04", "bob05", "bob06", "bob07", "bob08", "bob09",
        "bob10", "bob11", "bob12", "bob13", "bob14", "bob15", "bob16", "bob17", "bob18", "bob19",
        "bob20", "bob21", "bob22", "bob23", "bob24", "bob25", "bob26", "bob27", "bob28", "bob29",
        "bob30", "bob31", "bob32", "bob33", "bob34", "bob35", "bob36", "bob37", "bob38", "bob39"
      };
    std::copy( _voters.begin(), _voters.end(), std::back_inserter( voters ) );

    auto _voter_keys =
      {
        key(aoa00), key(aoa01), key(aoa02), key(aoa03), key(aoa04), key(aoa05), key(aoa06), key(aoa07), key(aoa08), key(aoa09),
        key(aoa10), key(aoa11), key(aoa12), key(aoa13), key(aoa14), key(aoa15), key(aoa16), key(aoa17), key(aoa18), key(aoa19),
        key(aoa20), key(aoa21), key(aoa22), key(aoa23), key(aoa24), key(aoa25), key(aoa26), key(aoa27), key(aoa28), key(aoa29),
        key(aoa30), key(aoa31), key(aoa32), key(aoa33), key(aoa34), key(aoa35), key(aoa36), key(aoa37), key(aoa38), key(aoa39),

        key(bob00), key(bob01), key(bob02), key(bob03), key(bob04), key(bob05), key(bob06), key(bob07), key(bob08), key(bob09),
        key(bob10), key(bob11), key(bob12), key(bob13), key(bob14), key(bob15), key(bob16), key(bob17), key(bob18), key(bob19),
        key(bob20), key(bob21), key(bob22), key(bob23), key(bob24), key(bob25), key(bob26), key(bob27), key(bob28), key(bob29),
        key(bob30), key(bob31), key(bob32), key(bob33), key(bob34), key(bob35), key(bob36), key(bob37), key(bob38), key(bob39)
      };
    std::copy( _voter_keys.begin(), _voter_keys.end(), std::back_inserter( voter_keys ) );
  }

  void prepare_voters_80_160()
  {
    ACTORS_EXT( test_object,
            (coc00)(coc01)(coc02)(coc03)(coc04)(coc05)(coc06)(coc07)(coc08)(coc09)
            (coc10)(coc11)(coc12)(coc13)(coc14)(coc15)(coc16)(coc17)(coc18)(coc19)
            (coc20)(coc21)(coc22)(coc23)(coc24)(coc25)(coc26)(coc27)(coc28)(coc29)
            (coc30)(coc31)(coc32)(coc33)(coc34)(coc35)(coc36)(coc37)(coc38)(coc39)

            (dod00)(dod01)(dod02)(dod03)(dod04)(dod05)(dod06)(dod07)(dod08)(dod09)
            (dod10)(dod11)(dod12)(dod13)(dod14)(dod15)(dod16)(dod17)(dod18)(dod19)
            (dod20)(dod21)(dod22)(dod23)(dod24)(dod25)(dod26)(dod27)(dod28)(dod29)
            (dod30)(dod31)(dod32)(dod33)(dod34)(dod35)(dod36)(dod37)(dod38)(dod39)
          )

    auto _voters =
      {
        "coc00", "coc01", "coc02", "coc03", "coc04", "coc05", "coc06", "coc07", "coc08", "coc09",
        "coc10", "coc11", "coc12", "coc13", "coc14", "coc15", "coc16", "coc17", "coc18", "coc19",
        "coc20", "coc21", "coc22", "coc23", "coc24", "coc25", "coc26", "coc27", "coc28", "coc29",
        "coc30", "coc31", "coc32", "coc33", "coc34", "coc35", "coc36", "coc37", "coc38", "coc39",

        "dod00", "dod01", "dod02", "dod03", "dod04", "dod05", "dod06", "dod07", "dod08", "dod09",
        "dod10", "dod11", "dod12", "dod13", "dod14", "dod15", "dod16", "dod17", "dod18", "dod19",
        "dod20", "dod21", "dod22", "dod23", "dod24", "dod25", "dod26", "dod27", "dod28", "dod29",
        "dod30", "dod31", "dod32", "dod33", "dod34", "dod35", "dod36", "dod37", "dod38", "dod39"
      };
    std::copy( _voters.begin(), _voters.end(), std::back_inserter( voters ) );

    auto _voter_keys =
      {
        key(coc00), key(coc01), key(coc02), key(coc03), key(coc04), key(coc05), key(coc06), key(coc07), key(coc08), key(coc09),
        key(coc10), key(coc11), key(coc12), key(coc13), key(coc14), key(coc15), key(coc16), key(coc17), key(coc18), key(coc19),
        key(coc20), key(coc21), key(coc22), key(coc23), key(coc24), key(coc25), key(coc26), key(coc27), key(coc28), key(coc29),
        key(coc30), key(coc31), key(coc32), key(coc33), key(coc34), key(coc35), key(coc36), key(coc37), key(coc38), key(coc39),

        key(dod00), key(dod01), key(dod02), key(dod03), key(dod04), key(dod05), key(dod06), key(dod07), key(dod08), key(dod09),
        key(dod10), key(dod11), key(dod12), key(dod13), key(dod14), key(dod15), key(dod16), key(dod17), key(dod18), key(dod19),
        key(dod20), key(dod21), key(dod22), key(dod23), key(dod24), key(dod25), key(dod26), key(dod27), key(dod28), key(dod29),
        key(dod30), key(dod31), key(dod32), key(dod33), key(dod34), key(dod35), key(dod36), key(dod37), key(dod38), key(dod39)
      };
    std::copy( _voter_keys.begin(), _voter_keys.end(), std::back_inserter( voter_keys ) );

  }

  void prepare_voters()
  {
    prepare_voters_0_80();
    prepare_voters_80_160();
  }

  void prepare_10_voters()
  {
    ACTORS_EXT( test_object,
            (aoa00)(aoa01)(aoa02)(aoa03)(aoa04)(aoa05)(aoa06)(aoa07)(aoa08)(aoa09)
          )

    auto _voters =
      {
        "aoa00", "aoa01", "aoa02", "aoa03", "aoa04", "aoa05", "aoa06", "aoa07", "aoa08", "aoa09"
      };
    std::copy( _voters.begin(), _voters.end(), std::back_inserter( voters ) );

    auto _voter_keys =
      {
        key(aoa00), key(aoa01), key(aoa02), key(aoa03), key(aoa04), key(aoa05), key(aoa06), key(aoa07), key(aoa08), key(aoa09)
      };
    std::copy( _voter_keys.begin(), _voter_keys.end(), std::back_inserter( voter_keys ) );

  }

  void prepare_funds_impl( uint32_t idx, uint32_t amount )
  {
      test_object.fund( voters[idx], asset( amount, HIVE_SYMBOL ) );
      test_object.vest( voters[idx], voters[idx], asset( amount / 10, HIVE_SYMBOL ), voter_keys[idx] );
  }

  void prepare_funds( uint32_t& counter, const window_input_data& window )
  {
    for( uint32_t i = counter; i < counter + window.nr_voters; ++i )
    {
      prepare_funds_impl( i, window.amount );
    }

    counter += window.nr_voters;
  }

  void prepare_funds( const window_input_data& early, const window_input_data& mid, const window_input_data& late )
  {
    BOOST_REQUIRE_GE( voters.size(), early.nr_voters + mid.nr_voters + late.nr_voters );

    uint32_t counter = 0;

    prepare_funds( counter, early );
    prepare_funds( counter, mid );
    prepare_funds( counter, late );
  }

  void prepare_funds( uint32_t amount = curation_rewards_handler::default_amount )
  {
    for( uint32_t i = 0; i < voters.size(); ++i )
    {
      prepare_funds_impl( i, amount );
    }
  }

  void prepare_comment( const std::string& permlink, uint32_t creator_number )
  {
    auto found_keys = author_keys.find( creator_number );
    BOOST_REQUIRE( found_keys != author_keys.end() );

    auto found_author = authors.find( creator_number );
    BOOST_REQUIRE( found_author != authors.end() );

    test_object.post_comment( found_author->second, permlink, "title", "body", "test", found_keys->second );
  }

  void create_printer()
  {
    curve_printers.emplace_back( curve_printer() );
  }

  void clear_printer( uint32_t comment_idx = 0 )
  {
    BOOST_REQUIRE_LT( comment_idx, curve_printers.size() );

    curve_printers[ comment_idx ].clear();
  }

  const curve_printer& get_printer( uint32_t comment_idx = 0 ) const
  {
    BOOST_REQUIRE_LT( comment_idx, curve_printers.size() );
    return curve_printers[ comment_idx ];
  }

  void set_start_time( const time_point_sec& _time, uint32_t comment_idx = 0 )
  {
    BOOST_REQUIRE_LT( comment_idx, curve_printers.size() );

    curve_printers[ comment_idx ].start_time = _time;
  }

  void voting( size_t& vote_counter, uint32_t author_number, const std::string& permlink, const std::vector<uint32_t>& votes_time, uint32_t comment_idx = 0 )
  {
    if( votes_time.empty() )
      return;

    BOOST_REQUIRE_LT( comment_idx, curve_printers.size() );
    BOOST_REQUIRE_GE( voters.size() + vote_counter, votes_time.size() );

    auto found_author = authors.find( author_number );
    BOOST_REQUIRE( found_author != authors.end() );

    auto author = found_author->second;

    uint32_t vote_percent = HIVE_1_PERCENT * 90;

    auto comment_id = db.get_comment( author, permlink ).get_id();

    for( auto& time : votes_time )
    {
      if( time )
        test_object.generate_blocks( db.head_block_time() + fc::seconds( time ) );

      test_object.vote( author, permlink, voters[ vote_counter ], vote_percent, voter_keys[ vote_counter ] );

      auto& cvo = db.get< comment_vote_object, by_comment_voter >( boost::make_tuple( comment_id, test_object.get_account_id( voters[ vote_counter ] ) ) );

      curve_printers[ comment_idx ].curve_items.emplace_back( curve_printer::curve_item{ db.head_block_time(), cvo.get_weight(), 0, voters[ vote_counter ] } );

      ++vote_counter;
    }
  }

  void curation_gathering( reward_stat::rewards_stats& early_stats, reward_stat::rewards_stats& mid_stats, reward_stat::rewards_stats& late_stats, uint32_t comment_idx = 0 )
  {
    BOOST_REQUIRE_LT( comment_idx, curve_printers.size() );

    const auto& dgpo = db.get_dynamic_global_properties();

    for( auto& item : curve_printers[ comment_idx ].curve_items )
    {
      const auto& acc = db.get_account( item.account );
      item.reward = static_cast<uint32_t>( acc.curation_rewards.value );

      uint64_t _seconds = static_cast<uint64_t>( ( item.time - curve_printers[ comment_idx ].start_time ).to_seconds() );

      if( _seconds >= dgpo.early_voting_seconds )
      {
        if( _seconds < ( dgpo.early_voting_seconds + dgpo.mid_voting_seconds ) )
          mid_stats.emplace_back( reward_stat{ item.reward } );
        else
          late_stats.emplace_back( reward_stat{ item.reward } );
      }
      else
      {
        early_stats.emplace_back( reward_stat{ item.reward } );
      }
    }
  }

  void curation_gathering( reward_stat::rewards_stats& early_stats, reward_stat::rewards_stats& late_stats, uint32_t comment_idx = 0 )
  {
    BOOST_REQUIRE_LT( comment_idx, curve_printers.size() );

    const auto& dgpo = db.get_dynamic_global_properties();

    for( auto& item : curve_printers[ comment_idx ].curve_items )
    {
      const auto& acc = db.get_account( item.account );
      item.reward = static_cast<uint32_t>( acc.curation_rewards.value );

      uint64_t _seconds = static_cast<uint64_t>( ( item.time - curve_printers[ comment_idx ].start_time ).to_seconds() );

      if( _seconds >= dgpo.reverse_auction_seconds )
      {
        late_stats.emplace_back( reward_stat{ item.reward } );
      }
      else
      {
        early_stats.emplace_back( reward_stat{ item.reward } );
      }
    }
  }

  void make_payment( uint32_t back_offset_blocks = 0, uint32_t comment_idx = 0 )
  {
    BOOST_REQUIRE_LT( comment_idx, curve_printers.size() );

    auto _time = db.head_block_time();
    uint64_t _seconds = static_cast<uint64_t>( ( _time - curve_printers[ comment_idx ].start_time ).to_seconds() );

    _seconds += back_offset_blocks * HIVE_BLOCK_INTERVAL;

    if( cashout_time > _seconds )
      test_object.generate_blocks( _time + fc::seconds( cashout_time - _seconds ) );
  }

  share_type calculate_reward()
  {
    const auto& props = db.get_dynamic_global_properties();

    int64_t start_inflation_rate = int64_t( HIVE_INFLATION_RATE_START_PERCENT );
    int64_t inflation_rate_adjustment = int64_t( db.head_block_num() / HIVE_INFLATION_NARROWING_PERIOD );
    int64_t inflation_rate_floor = int64_t( HIVE_INFLATION_RATE_STOP_PERCENT );

    // below subtraction cannot underflow int64_t because inflation_rate_adjustment is <2^32
    int64_t current_inflation_rate = std::max( start_inflation_rate - inflation_rate_adjustment, inflation_rate_floor );

    auto new_hive = ( props.virtual_supply.amount * current_inflation_rate ) / ( int64_t( HIVE_100_PERCENT ) * int64_t( HIVE_BLOCKS_PER_YEAR ) );
    return ( new_hive * props.content_reward_percent ) / HIVE_100_PERCENT;
  }

  share_type current_total_reward()
  {
    const auto& reward_idx = db.get_index< reward_fund_index, by_id >();

    auto fund_id = db.get_reward_fund().get_id();

    auto found = reward_idx.find( fund_id );
    if( found == reward_idx.end() )
      return 0;

    return found->reward_balance.amount;
  }

};

BOOST_FIXTURE_TEST_SUITE( curation_reward_tests, curation_database_fixture )

void basic_test_impl(
                      const std::string& test_name,
                      clean_database_fixture& mgr_test,
                      const window_input_data& early,
                      const window_input_data& mid,
                      const window_input_data& late,
                      uint32_t reward, const fc::optional<uint32_t>& offset = fc::optional<uint32_t>() )
{
  curation_rewards_handler crh( mgr_test, *( mgr_test.db ) );

  crh.prepare_author( { 0 } );
  crh.prepare_voters();
  mgr_test.generate_block();

  mgr_test.set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
  mgr_test.generate_block();

  crh.prepare_funds( early, mid, late );
  mgr_test.generate_block();

  uint32_t author_number = 0;
  std::string permlink  = "somethingpermlink";

  crh.prepare_comment( permlink, author_number );
  const auto creation_time = mgr_test.db->head_block_time();
  crh.set_start_time( creation_time );
  mgr_test.generate_block();

  if( offset.valid() )
    mgr_test.generate_blocks( mgr_test.db->head_block_time() + fc::seconds( *offset ) );

  std::vector<uint32_t> early_votes_time( early.nr_voters,  early.interval );
  std::vector<uint32_t> mid_votes_time(   mid.nr_voters,    mid.interval );
  std::vector<uint32_t> late_votes_time(  late.nr_voters,   late.interval );

  size_t vote_counter = 0;
  crh.voting( vote_counter, author_number, permlink, early_votes_time );
  BOOST_REQUIRE(mgr_test.db->head_block_time() < creation_time + fc::seconds(crh.early_window)); //too many early voters?
  if( !mid_votes_time.empty() )
  {
    mgr_test.generate_blocks( creation_time + fc::seconds( crh.early_window ) );
    mid_votes_time.front() = 0;
  }
  crh.voting( vote_counter, author_number, permlink, mid_votes_time );
  BOOST_REQUIRE( mgr_test.db->head_block_time() < creation_time + fc::seconds( crh.mid_window ) ); //too many mid voters?
  if( !late_votes_time.empty() )
  {
    mgr_test.generate_blocks( creation_time + fc::seconds( crh.mid_window ) );
    late_votes_time.front() = 0;
  }
  crh.voting( vote_counter, author_number, permlink, late_votes_time );
  BOOST_REQUIRE( mgr_test.db->head_block_time() < creation_time + fc::seconds( crh.late_window ) ); //too many late voters?
  crh.make_payment();

  reward_stat::rewards_stats early_stats;
  reward_stat::rewards_stats mid_stats;
  reward_stat::rewards_stats late_stats;

  crh.curation_gathering( early_stats, mid_stats, late_stats );
  BOOST_REQUIRE_EQUAL(early_stats.size(), early.nr_voters);
  BOOST_REQUIRE_EQUAL( mid_stats.size(), mid.nr_voters );
  BOOST_REQUIRE_EQUAL( late_stats.size(), late.nr_voters );

  auto cmp = [ reward ]( const reward_stat::rewards_stats& stats_a, const reward_stat::rewards_stats& stats_b, uint32_t factor )
  {
    BOOST_REQUIRE( factor == 2 || factor == 4 );

    auto _cmp = [ reward, factor ]( const reward_stat& item_a, const reward_stat& item_b )
    {
      if( factor == 2 )
        return item_a.value == reward && ( item_a.value / factor == item_b.value );
      else
        return item_a.value && ( item_a.value / factor == item_b.value );
    };
    reward_stat::check_phases( stats_a, stats_b, _cmp );
  };

  {
    BOOST_TEST_MESSAGE( "Comparison phases: `early` and `mid`" );
    cmp( early_stats, mid_stats, 2/*factor*/ );
    if( early_stats.empty() ^ mid_stats.empty() )
    {
      if( !early_stats.empty() )
        BOOST_REQUIRE_EQUAL( early_stats[0].value, reward );
      else
        BOOST_REQUIRE_EQUAL( mid_stats[0].value, reward );
    }
  }
  {
    BOOST_TEST_MESSAGE( "Comparison phases: `mid` and `late`" );
    cmp( mid_stats, late_stats, 4/*factor*/ );
    if( mid_stats.empty() ^ late_stats.empty() )
    {
      if( !mid_stats.empty() )
      {
        if( early_stats.empty() )
          BOOST_REQUIRE_EQUAL( mid_stats[0].value, reward );
        else
          BOOST_REQUIRE_EQUAL( mid_stats[0].value, reward / 2 );
      }
      else
      {
        if( early_stats.empty() && mid_stats.empty() )
          BOOST_REQUIRE_EQUAL( late_stats[0].value, reward );
        else if( mid_stats.empty() )
          BOOST_REQUIRE_EQUAL( late_stats[0].value, reward / 4 );
        else
          BOOST_REQUIRE_EQUAL( late_stats[0].value, reward / 8 );
      }
    }
  }

  print_all( std::cout, crh.get_printer() );
  print( std::cout, crh.get_printer() );

  std::ofstream file( test_name );
  print( file, crh.get_printer() );

  mgr_test.validate_database();
}

BOOST_AUTO_TEST_CASE( basic_test_v0 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: curation rewards after HF25. Reward during whole rewards-time (7 days). Voting in every window." );

    auto _a = curation_rewards_handler::default_amount;

    basic_test_impl( "00.content_format.time:curation_reward.csv",
                      *this,
                      window_input_data{ 44/*nr_voters*/, 12/*interval*/    , _a }/*early*/,
                      window_input_data{ 38/*nr_voters*/, 4480/*interval*/  , _a }/*mid*/,
                      window_input_data{ 77/*nr_voters*/, 3925/*interval*/  , _a }/*late*/, 645/*reward*/ );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( basic_test_v1 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: curation rewards after HF25. Reward during whole rewards-time (7 days). Voting in 2 windows: early, mid." );

    auto _a = curation_rewards_handler::default_amount;

    basic_test_impl( "01.content_format.time:curation_reward.csv",
                      *this,
                      window_input_data{ 44/*nr_voters*/, 12/*interval*/    , _a }/*early*/,
                      window_input_data{ 20/*nr_voters*/, 4479/*interval*/  , _a }/*mid*/,
                      window_input_data()/*late*/, 760/*reward*/ );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( basic_test_v2 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: curation rewards after HF25. Reward during whole rewards-time (7 days). Voting in 2 windows: early, mid." );

    auto _a = curation_rewards_handler::default_amount;

    basic_test_impl( "02.content_format.time:curation_reward.csv",
                      *this,
                      window_input_data{ 69/*nr_voters*/, 12/*interval*/    , _a }/*early*/,
                      window_input_data{ 90/*nr_voters*/, 1930/*interval*/ , _a }/*mid*/,
                      window_input_data()/*late*/, 411/*reward*/ );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( basic_test_v3 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: curation rewards after HF25. Reward during whole rewards-time (7 days). Voting in early window." );

    auto _a = curation_rewards_handler::default_amount;

    basic_test_impl( "03.content_format.time:curation_reward.csv",
                      *this,
                      window_input_data{ 50/*nr_voters*/, 1720/*interval*/, _a }/*early*/,
                      window_input_data()/*mid*/,
                      window_input_data()/*late*/, 805/*reward*/, 12/*offset*/ );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( basic_test_v4 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: curation rewards after HF25. Reward during whole rewards-time (7 days). Voting in mid window." );

    auto _a = curation_rewards_handler::default_amount;

    basic_test_impl( "04.content_format.time:curation_reward.csv",
                      *this,
                      window_input_data()/*early*/,
                      window_input_data{ 50/*nr_voters*/, 300/*interval*/, _a }/*mid*/,
                      window_input_data()/*late*/, 805/*reward*/, 12/*offset*/ );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( basic_test_v5 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: curation rewards after HF25. Reward during whole rewards-time (7 days). Voting in early window." );

    auto _a = curation_rewards_handler::default_amount * 2;

    basic_test_impl( "05.content_format.time:curation_reward.csv",
                      *this,
                      window_input_data{ 50/*nr_voters*/, 1720/*interval*/, _a }/*early*/,
                      window_input_data()/*mid*/,
                      window_input_data()/*late*/, 805/*reward*/, 12/*offset*/ );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( basic_test_v6 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: curation rewards after HF25. Reward during whole rewards-time (7 days). Voting in mid window." );

    auto _a = curation_rewards_handler::default_amount * 2;

    basic_test_impl( "06.content_format.time:curation_reward.csv",
                      *this,
                      window_input_data()/*early*/,
                      window_input_data{ 50/*nr_voters*/, 300/*interval*/, _a }/*mid*/,
                      window_input_data()/*late*/, 805/*reward*/, 12/*offset*/ );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( basic_test_v7 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: curation rewards after HF25. Reward during whole rewards-time (7 days)." );
    BOOST_TEST_MESSAGE( "Totally 50 voters vote on 1 comment" );
    BOOST_TEST_MESSAGE( "1 voter votes in early window, rest of voters vote in mid window." );

    auto _a = curation_rewards_handler::default_amount * 2;

    basic_test_impl( "07.content_format.time:curation_reward.csv",
                      *this,
                      window_input_data{ 1/*nr_voters*/, 12/*interval*/, _a }/*early*/,
                      window_input_data{ 49/*nr_voters*/, 300/*interval*/, _a }/*mid*/,
                      window_input_data()/*late*/, 1579/*reward*/, 12/*offset*/ );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( basic_test_v8 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: curation rewards after HF25. Reward during whole rewards-time (7 days)." );
    BOOST_TEST_MESSAGE( "Totally 50 voters vote on 1 comment" );
    BOOST_TEST_MESSAGE( "5 voters vote in early window, rest of voters vote in mid window." );

    auto _a = curation_rewards_handler::default_amount * 2;

    basic_test_impl( "08.content_format.time:curation_reward.csv",
                      *this,
                      window_input_data{ 5/*nr_voters*/, 12/*interval*/, _a }/*early*/,
                      window_input_data{ 45/*nr_voters*/, 300/*interval*/, _a }/*mid*/,
                      window_input_data()/*late*/, 1464/*reward*/, 12/*offset*/ );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( no_votes )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: curation rewards after HF25. Lack of votes." );

    curation_rewards_handler crh( *this, *db );

    crh.prepare_author( { 0 } );
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    uint32_t author_number = 0;
    std::string permlink  = "somethingpermlink";

    crh.prepare_comment( permlink, author_number );
    generate_block();

    generate_blocks( db->head_block_time() + fc::seconds( 60*60*24*7/*7 days like in mainnet*/ ) );

    reward_stat::rewards_stats early_stats;
    reward_stat::rewards_stats mid_stats;
    reward_stat::rewards_stats late_stats;

    crh.curation_gathering( early_stats, mid_stats, late_stats );

    auto found_author = crh.authors.find( author_number );
    BOOST_REQUIRE( found_author != crh.authors.end() );
    const auto& creator = db->get_account( found_author->second );
    BOOST_REQUIRE_EQUAL( creator.posting_rewards.value, 0 );

    auto cmp = []( const reward_stat& item )
    {
      return item.value == 0;
    };
    reward_stat::check_phase( early_stats, cmp );
    reward_stat::check_phase( mid_stats, cmp );
    reward_stat::check_phase( late_stats, cmp );

    print_all( std::cout, crh.get_printer() );
    print( std::cout, crh.get_printer() );

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

    crh.prepare_author( { 0, 1, 2 } );
    crh.prepare_10_voters();
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    crh.prepare_funds();
    generate_block();

    std::string permlink  = "somethingpermlink";

    for( uint32_t i = 0 ; i < 3; ++i )
      crh.prepare_comment( permlink, i/*author_number*/ );
    crh.set_start_time( db->head_block_time() );
    generate_block();

    //Every comment has only 1 vote, but votes are created in different windows.
    size_t vote_counter = 0;

    uint32_t vote_time_early_window = 12;
    uint32_t vote_time_mid_window   = crh.early_window; // crh.early_window + 12
    uint32_t vote_time_late_window  = crh.mid_window;   // crh.early_window + 12 + crh.mid_window

    crh.voting( vote_counter, 0/*author_number*/, permlink, { vote_time_early_window } );
    crh.voting( vote_counter, 1/*author_number*/, permlink, { vote_time_mid_window } );
    crh.voting( vote_counter, 2/*author_number*/, permlink, { vote_time_late_window } );
    crh.make_payment();

    reward_stat::rewards_stats early_stats;
    reward_stat::rewards_stats mid_stats;
    reward_stat::rewards_stats late_stats;

    crh.curation_gathering( early_stats, mid_stats, late_stats );
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
      auto cmp = []( const reward_stat& item_a, const reward_stat& item_b )
      {
        return item_a.value == 12463 && item_a.value == item_b.value;
      };
      reward_stat::check_phases( early_stats, mid_stats, cmp );
      reward_stat::check_phases( mid_stats, late_stats, cmp );
    }

    print_all( std::cout, crh.get_printer() );
    print( std::cout, crh.get_printer() );

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

    crh.prepare_author( { 0, 1, 2 } );
    crh.prepare_10_voters();
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    crh.prepare_funds();
    generate_block();

    std::string permlink  = "somethingpermlink";

    for( uint32_t i = 0 ; i < 3; ++i )
      crh.prepare_comment( permlink, i/*author_number*/ );
    crh.set_start_time( db->head_block_time() );
    generate_block();

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

    reward_stat::rewards_stats early_stats;
    reward_stat::rewards_stats mid_stats;
    reward_stat::rewards_stats late_stats;

    crh.curation_gathering( early_stats, mid_stats, late_stats );
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

      reward_stat::display_stats( early_stats );
      reward_stat::display_stats( mid_stats );
      reward_stat::display_stats( late_stats );

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


void two_comments_in_the_same_blocks_impl( cluster_database_fixture& cluster, bool restore_author_reward_curve )
{
    auto preparation = []( curation_rewards_handler& crh, cluster_database_fixture::ptr_hardfork_database_fixture& executor )
    {
      crh.prepare_author( { 0, 1 } );
      crh.prepare_voters_0_80();
      crh.create_printer();
      executor->generate_block();

      executor->set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
      executor->generate_block();

      crh.prepare_funds();
      executor->generate_block();

      return crh.calculate_reward();
    };

    auto execution = []( curation_rewards_handler& crh, cluster_database_fixture::ptr_hardfork_database_fixture& executor,
                          share_type& total_rewards_pool_before, share_type& total_rewards_pool_after )
    {
      size_t vote_counter = 0;

      std::string permlink  = "somethingpermlink";

      crh.prepare_comment( permlink, 0/*author_number*/ );
      crh.prepare_comment( permlink, 1/*author_number*/ );
      crh.set_start_time( executor->db->head_block_time() );
      executor->generate_block();

      const uint32_t nr_votes_first = 75;
      const uint32_t nr_votes_second = 1;

      //All votes( for both comments ) are in the same block.
      for( uint32_t i = 0; i < nr_votes_first; ++i )
        crh.voting( vote_counter, 0/*author_number*/, permlink, { 0 } );
      for( uint32_t i = 0; i < nr_votes_second; ++i )
        crh.voting( vote_counter, 1/*author_number*/, permlink, { 0 } );

      crh.make_payment( 1/*back_offset_blocks*/ );
      total_rewards_pool_before = crh.current_total_reward();
      crh.make_payment();
      total_rewards_pool_after = crh.current_total_reward();
    };

    std::vector<comment_reward_info> comment_rewards_hf24;

    auto hf24_content = [&]( cluster_database_fixture::ptr_hardfork_database_fixture& executor )
    {
      //=====hf24=====
      reward_stat::rewards_stats early_stats[2];
      reward_stat::rewards_stats late_stats[2];

      share_type total_rewards_pool_before;
      share_type total_rewards_pool_after;

      curation_rewards_handler crh( *executor, *( executor->db ) );

      share_type reward_per_block = preparation( crh, executor );

      {
        execution( crh, executor, total_rewards_pool_before, total_rewards_pool_after );

        crh.curation_gathering( early_stats[0], late_stats[0] );

        BOOST_REQUIRE_EQUAL( crh.comment_rewards.size(), 2 );
        comment_rewards_hf24 = crh.comment_rewards;
      }
      {
        //Move one element into second collection in order to display in better way.
        if( !early_stats[0].empty() )
        {
          early_stats[1].emplace_back( early_stats[0].back() );
          early_stats[0].pop_back();
        }

        if( !late_stats[0].empty() )
        {
          late_stats[1].emplace_back( late_stats[0].back() );
          late_stats[0].pop_back();
        }
      }
      {
        BOOST_TEST_MESSAGE( "reward_per_block" );
        reward_stat::display_reward( reward_per_block );

        BOOST_TEST_MESSAGE( "*****HF-24*****" );
        BOOST_TEST_MESSAGE( "curation rewards" );
        reward_stat::display_stats( early_stats[0] );
        reward_stat::display_stats( late_stats[0] );

        BOOST_TEST_MESSAGE( "curation rewards(2)" );
        reward_stat::display_stats( early_stats[1] );
        reward_stat::display_stats( late_stats[1] );

        BOOST_TEST_MESSAGE( "current pool of rewards before payment" );
        reward_stat::display_reward( total_rewards_pool_before );
        BOOST_TEST_MESSAGE( "current pool of rewards after payment" );
        reward_stat::display_reward( total_rewards_pool_after );

        BOOST_TEST_MESSAGE( "comment rewards" );
        for( auto& item : crh.comment_rewards )
          reward_stat::display_comment_rewards( item );
      }

      uint32_t _sum_curation_rewards[2]  = { 0, 0 };
      {
        for( uint32_t c = 0; c < 2; ++c )
        {
          for( auto& item : early_stats[c] )
            _sum_curation_rewards[c] += item.value;
        }

        BOOST_TEST_MESSAGE( "*****HF24:SUM CURATION REWARDS*****" );
        BOOST_TEST_MESSAGE( "sum_curation_rewards" );
        BOOST_TEST_MESSAGE( std::to_string( _sum_curation_rewards[0] ).c_str() );
        BOOST_TEST_MESSAGE( "sum_curation_rewards(2)" );
        BOOST_TEST_MESSAGE( std::to_string( _sum_curation_rewards[1] ).c_str() );

        BOOST_REQUIRE_EQUAL( _sum_curation_rewards[0], crh.comment_rewards[0].curation_tokens.value );
        BOOST_REQUIRE_EQUAL( _sum_curation_rewards[1], crh.comment_rewards[1].curation_tokens.value );
      }
      {
        /*
          Not used rewards are returned back after(!) processing all comments, therefore it doesn't matter how much it's returned,
          if all comments are in the same block.
        */
        BOOST_REQUIRE_EQUAL( crh.comment_rewards[0].author_tokens.value, 36951 );
        BOOST_REQUIRE_EQUAL( crh.comment_rewards[1].author_tokens.value, 289 );
      }

      executor->validate_database();
    };

    std::vector<comment_reward_info> comment_rewards_hf25;

    auto hf25_content = [&]( cluster_database_fixture::ptr_hardfork_database_fixture& executor )
    {
      //=====hf25=====
      reward_stat::rewards_stats early_stats[2];
      reward_stat::rewards_stats mid_stats[2];
      reward_stat::rewards_stats late_stats[2];

      share_type total_rewards_pool_before;
      share_type total_rewards_pool_after;

      if( restore_author_reward_curve )
      {
        executor->db_plugin->debug_update( []( database& db )
        {
          db.modify( db.get< reward_fund_object, by_name >( HIVE_POST_REWARD_FUND_NAME ), [&]( reward_fund_object& rfo )
          {
            rfo.author_reward_curve = convergent_linear;
          });
        });
      }

      curation_rewards_handler crh( *executor, *( executor->db ) );

      share_type reward_per_block = preparation( crh, executor );

      {
        //**********************HF 25 is activated**********************
        execution( crh, executor, total_rewards_pool_before, total_rewards_pool_after );

        crh.curation_gathering( early_stats[0], mid_stats[0], late_stats[0] );

        BOOST_REQUIRE_EQUAL( crh.comment_rewards.size(), 2 );
        comment_rewards_hf25 = crh.comment_rewards;
      }

      {
        //Move one element into second collection in order to display in better way.
        if( !early_stats[0].empty() )
        {
          early_stats[1].emplace_back( early_stats[0].back() );
          early_stats[0].pop_back();
        }

        if( !mid_stats[0].empty() )
        {
          mid_stats[1].emplace_back( mid_stats[0].back() );
          mid_stats[0].pop_back();
        }

        if( !late_stats[0].empty() )
        {
          late_stats[1].emplace_back( late_stats[0].back() );
          late_stats[0].pop_back();
        }
      }
      {
        BOOST_TEST_MESSAGE( "reward_per_block" );
        reward_stat::display_reward( reward_per_block );

        BOOST_TEST_MESSAGE( "*****HF-25*****" );
        BOOST_TEST_MESSAGE( "curation rewards" );
        reward_stat::display_stats( early_stats[0] );
        reward_stat::display_stats( mid_stats[0] );
        reward_stat::display_stats( late_stats[0] );

        BOOST_TEST_MESSAGE( "curation rewards(2)" );
        reward_stat::display_stats( early_stats[1] );
        reward_stat::display_stats( mid_stats[1] );
        reward_stat::display_stats( late_stats[1] );

        BOOST_TEST_MESSAGE( "current pool of rewards before payment" );
        reward_stat::display_reward( total_rewards_pool_before );
        BOOST_TEST_MESSAGE( "current pool of rewards after payment" );
        reward_stat::display_reward( total_rewards_pool_after );

        BOOST_TEST_MESSAGE( "current pool of rewards before payment(2)" );
        reward_stat::display_reward( total_rewards_pool_before );
        BOOST_TEST_MESSAGE( "current pool of rewards after payment(2)" );
        reward_stat::display_reward( total_rewards_pool_after );

        BOOST_TEST_MESSAGE( "comment rewards" );
        for( auto& item : crh.comment_rewards )
          reward_stat::display_comment_rewards( item );
      }

      uint32_t _sum_curation_rewards[2]  = { 0, 0 };
      {
        for( uint32_t c = 0; c < 2; ++c )
        {
          for( auto& item : early_stats[c] )
            _sum_curation_rewards[c] += item.value;
        }

        BOOST_TEST_MESSAGE( "*****HF25:SUM CURATION REWARDS*****" );
        BOOST_TEST_MESSAGE( "sum_curation_rewards" );
        BOOST_TEST_MESSAGE( std::to_string( _sum_curation_rewards[0] ).c_str() );
        BOOST_TEST_MESSAGE( "sum_curation_rewards(2)" );
        BOOST_TEST_MESSAGE( std::to_string( _sum_curation_rewards[1] ).c_str() );

        BOOST_REQUIRE_EQUAL( _sum_curation_rewards[0], crh.comment_rewards[0].curation_tokens.value );
        BOOST_REQUIRE_EQUAL( _sum_curation_rewards[1], crh.comment_rewards[1].curation_tokens.value );
      }
      {
        /*
          Not used rewards are returned back after(!) processing all comments, therefore it doesn't matter how much it's returned,
          if all comments are in the same block.
        */
        if( restore_author_reward_curve )
        {
          BOOST_REQUIRE_EQUAL( crh.comment_rewards[0].author_tokens.value, 36951 );
          BOOST_REQUIRE_EQUAL( crh.comment_rewards[1].author_tokens.value, 289 );
        }
        else
        {
          BOOST_REQUIRE_EQUAL( crh.comment_rewards[0].author_tokens.value, 36750 );
          BOOST_REQUIRE_EQUAL( crh.comment_rewards[1].author_tokens.value, 490 );
        }
      }

      executor->validate_database();
    };

    cluster.execute_hardfork<24>( hf24_content );
    cluster.execute_hardfork<25>( hf25_content );

    if( restore_author_reward_curve )
    {
      BOOST_REQUIRE_EQUAL( comment_rewards_hf24[0].author_tokens.value, comment_rewards_hf25[0].author_tokens.value );
      BOOST_REQUIRE_EQUAL( comment_rewards_hf24[1].author_tokens.value, comment_rewards_hf25[1].author_tokens.value );
    }
    else
    {
      BOOST_REQUIRE_GT( comment_rewards_hf24[0].author_tokens.value, comment_rewards_hf25[0].author_tokens.value );
      BOOST_REQUIRE_LT( comment_rewards_hf24[1].author_tokens.value, comment_rewards_hf25[1].author_tokens.value );
    }
}

void two_comments_in_different_blocks_impl( cluster_database_fixture& cluster, bool restore_author_reward_curve )
{
    auto preparation = []( curation_rewards_handler& crh, cluster_database_fixture::ptr_hardfork_database_fixture& executor )
    {
      crh.prepare_author( { 0, 1 } );
      crh.prepare_voters_0_80();
      crh.create_printer();
      executor->generate_block();

      executor->set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
      executor->generate_block();

      crh.prepare_funds();
      executor->generate_block();

      return crh.calculate_reward();
    };

    auto execution = []( curation_rewards_handler& crh, cluster_database_fixture::ptr_hardfork_database_fixture& executor,
                          share_type total_rewards_pool_before[], share_type total_rewards_pool_after[] )
    {
      size_t vote_counter = 0;

      const uint32_t nr_votes_first = 75;
      const uint32_t nr_votes_second = 1;

      std::string permlink  = "somethingpermlink";

      crh.prepare_comment( permlink, 0/*author_number*/ );
      crh.set_start_time( executor->db->head_block_time() );
      executor->generate_block();

      //All votes( for both comments ) are in the same block.
      for( uint32_t i = 0; i < nr_votes_first; ++i )
        crh.voting( vote_counter, 0/*author_number*/, permlink, { 0 } );
      executor->generate_block();

      //More blocks in order to generate some rewards by inflation( especially, when HF is applied )
      const int increasing_rewards_factor = 100;
      executor->generate_blocks( increasing_rewards_factor );

      crh.prepare_comment( permlink, 1/*author_number*/ );
      crh.set_start_time( executor->db->head_block_time(), 1/*comment_idx*/ );
      executor->generate_block();

      //All votes( for both comments ) are in the same block.
      for( uint32_t i = 0; i < nr_votes_second; ++i )
        crh.voting( vote_counter, 1/*author_number*/, permlink, { 0 }, 1/*comment_idx*/ );
      executor->generate_block();

      crh.make_payment( 1/*back_offset_blocks*/ );
      total_rewards_pool_before[0] = crh.current_total_reward();
      crh.make_payment();
      total_rewards_pool_after[0] = crh.current_total_reward();

      executor->generate_blocks( increasing_rewards_factor );

      crh.make_payment( 1/*back_offset_blocks*/, 1/*comment_idx*/ );
      total_rewards_pool_before[1] = crh.current_total_reward();
      crh.make_payment( 0/*back_offset_blocks*/, 1/*comment_idx*/ );
      total_rewards_pool_after[1] = crh.current_total_reward();
    };

    std::vector<comment_reward_info> comment_rewards_hf24;

    auto hf24_content = [&]( cluster_database_fixture::ptr_hardfork_database_fixture& executor )
    {
      //=====hf24=====
      reward_stat::rewards_stats early_stats[2];
      reward_stat::rewards_stats late_stats[2];

      share_type total_rewards_pool_before[2];
      share_type total_rewards_pool_after[2];

      curation_rewards_handler crh( *executor, *( executor->db ) );

      share_type reward_per_block = preparation( crh, executor );

      {
        //**********************HF 24 is activated**********************
        execution( crh, executor, total_rewards_pool_before, total_rewards_pool_after );

        crh.curation_gathering( early_stats[0], late_stats[0] );
        crh.curation_gathering( early_stats[1], late_stats[1], 1/*comment_idx*/ );

        BOOST_REQUIRE_EQUAL( crh.comment_rewards.size(), 2 );
        comment_rewards_hf24 = crh.comment_rewards;
      }

      {
        BOOST_TEST_MESSAGE( "reward_per_block" );
        reward_stat::display_reward( reward_per_block );

        BOOST_TEST_MESSAGE( "*****HF-24*****" );
        BOOST_TEST_MESSAGE( "curation rewards" );
        reward_stat::display_stats( early_stats[0] );
        reward_stat::display_stats( late_stats[0] );

        BOOST_TEST_MESSAGE( "curation rewards(2)" );
        reward_stat::display_stats( early_stats[1] );
        reward_stat::display_stats( late_stats[1] );

        BOOST_TEST_MESSAGE( "current pool of rewards before payment" );
        reward_stat::display_reward( total_rewards_pool_before[0] );
        BOOST_TEST_MESSAGE( "current pool of rewards after payment" );
        reward_stat::display_reward( total_rewards_pool_after[0] );

        BOOST_TEST_MESSAGE( "current pool of rewards before payment(2)" );
        reward_stat::display_reward( total_rewards_pool_before[1] );
        BOOST_TEST_MESSAGE( "current pool of rewards after payment(2)" );
        reward_stat::display_reward( total_rewards_pool_after[1] );

        BOOST_TEST_MESSAGE( "comment rewards" );
        for( auto& item : crh.comment_rewards )
          reward_stat::display_comment_rewards( item );
      }

      uint32_t _sum_curation_rewards[2]  = { 0, 0 };
      {
        for( uint32_t c = 0; c < 2; ++c )
        {
          for( auto& item : early_stats[c] )
            _sum_curation_rewards[c] += item.value;
        }

        BOOST_TEST_MESSAGE( "*****HF24:SUM CURATION REWARDS*****" );
        BOOST_TEST_MESSAGE( "sum_curation_rewards" );
        BOOST_TEST_MESSAGE( std::to_string( _sum_curation_rewards[0] ).c_str() );
        BOOST_TEST_MESSAGE( "sum_curation_rewards(2)" );
        BOOST_TEST_MESSAGE( std::to_string( _sum_curation_rewards[1] ).c_str() );

        BOOST_REQUIRE_EQUAL( _sum_curation_rewards[0], crh.comment_rewards[0].curation_tokens.value );
        BOOST_REQUIRE_EQUAL( _sum_curation_rewards[1], crh.comment_rewards[1].curation_tokens.value );
      }
      {
        /*
          Not used rewards are returned back after processing first comment,
          therefore second reward in HF24 is higher than second reward in HF25.
        */
        BOOST_REQUIRE_EQUAL( crh.comment_rewards[0].author_tokens.value, 40330 );
        BOOST_REQUIRE_EQUAL( crh.comment_rewards[1].author_tokens.value, 179 );
      }

      executor->validate_database();
    };

    std::vector<comment_reward_info> comment_rewards_hf25;

    auto hf25_content = [&]( cluster_database_fixture::ptr_hardfork_database_fixture& executor )
    {
      //=====hf25=====
      reward_stat::rewards_stats early_stats[2];
      reward_stat::rewards_stats mid_stats[2];
      reward_stat::rewards_stats late_stats[2];

      share_type total_rewards_pool_before[2];
      share_type total_rewards_pool_after[2];

      if( restore_author_reward_curve )
      {
        executor->db_plugin->debug_update( []( database& db )
        {
          db.modify( db.get< reward_fund_object, by_name >( HIVE_POST_REWARD_FUND_NAME ), [&]( reward_fund_object& rfo )
          {
            rfo.author_reward_curve = convergent_linear;
          });
        });
      }

      curation_rewards_handler crh( *executor, *( executor->db ) );

      share_type reward_per_block = preparation( crh, executor );

      {
        //**********************HF 25 is activated**********************
        execution( crh, executor, total_rewards_pool_before, total_rewards_pool_after );

        crh.curation_gathering( early_stats[0], mid_stats[0], late_stats[0] );
        crh.curation_gathering( early_stats[1], mid_stats[1], late_stats[1], 1/*comment_idx*/ );

        BOOST_REQUIRE( !crh.comment_rewards.empty() );
        comment_rewards_hf25 = crh.comment_rewards;
      }

      {
        BOOST_TEST_MESSAGE( "reward_per_block" );
        reward_stat::display_reward( reward_per_block );

        BOOST_TEST_MESSAGE( "*****HF-25*****" );
        BOOST_TEST_MESSAGE( "curation rewards" );
        reward_stat::display_stats( early_stats[0] );
        reward_stat::display_stats( mid_stats[0] );
        reward_stat::display_stats( late_stats[0] );

        BOOST_TEST_MESSAGE( "curation rewards(2)" );
        reward_stat::display_stats( early_stats[1] );
        reward_stat::display_stats( mid_stats[1] );
        reward_stat::display_stats( late_stats[1] );

        BOOST_TEST_MESSAGE( "current pool of rewards before payment" );
        reward_stat::display_reward( total_rewards_pool_before[0] );
        BOOST_TEST_MESSAGE( "current pool of rewards after payment" );
        reward_stat::display_reward( total_rewards_pool_after[0] );

        BOOST_TEST_MESSAGE( "current pool of rewards before payment(2)" );
        reward_stat::display_reward( total_rewards_pool_before[1] );
        BOOST_TEST_MESSAGE( "current pool of rewards after payment(2)" );
        reward_stat::display_reward( total_rewards_pool_after[1] );

        BOOST_TEST_MESSAGE( "comment rewards" );
        for( auto& item : crh.comment_rewards )
          reward_stat::display_comment_rewards( item );
      }

      uint32_t _sum_curation_rewards[2]  = { 0, 0 };
      {
        for( uint32_t c = 0; c < 2; ++c )
        {
          for( auto& item : early_stats[c] )
            _sum_curation_rewards[c] += item.value;
        }

        BOOST_TEST_MESSAGE( "*****HF25:SUM CURATION REWARDS*****" );
        BOOST_TEST_MESSAGE( "sum_curation_rewards" );
        BOOST_TEST_MESSAGE( std::to_string( _sum_curation_rewards[0] ).c_str() );
        BOOST_TEST_MESSAGE( "sum_curation_rewards(2)" );
        BOOST_TEST_MESSAGE( std::to_string( _sum_curation_rewards[1] ).c_str() );

        BOOST_REQUIRE_EQUAL( _sum_curation_rewards[0], crh.comment_rewards[0].curation_tokens.value );
        BOOST_REQUIRE_EQUAL( _sum_curation_rewards[1], crh.comment_rewards[1].curation_tokens.value );
      }
      {
        /*
          Not used rewards are returned back after processing first comment,
          therefore second reward in HF24 is higher than second reward in HF25.
        */
        BOOST_REQUIRE_EQUAL( crh.comment_rewards[0].author_tokens.value, 40330 );
        if( restore_author_reward_curve )
        {
          BOOST_REQUIRE_EQUAL( crh.comment_rewards[1].author_tokens.value, 24 );
        }
        else
        {
          BOOST_REQUIRE_EQUAL( crh.comment_rewards[1].author_tokens.value, 41 );
        }
        
      }

      executor->validate_database();
    };

    cluster.execute_hardfork<24>( hf24_content );
    cluster.execute_hardfork<25>( hf25_content );

    BOOST_REQUIRE_EQUAL(  comment_rewards_hf24[0].author_tokens.value, comment_rewards_hf25[0].author_tokens.value );
    BOOST_REQUIRE_GT(     comment_rewards_hf24[1].author_tokens.value, comment_rewards_hf25[1].author_tokens.value );
}

BOOST_FIXTURE_TEST_SUITE( curation_reward_tests2, cluster_database_fixture )

BOOST_AUTO_TEST_CASE( one_vote_per_comment )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: curation rewards before and after HF25. Reward during whole rewards-time (7 days). Only 1 vote per 1 comment." );
    BOOST_TEST_MESSAGE( "HF24: inside `reverse_auction_seconds` window. HF25: inside `early` window." );

    auto preparation = []( curation_rewards_handler& crh, ptr_hardfork_database_fixture& executor )
    {
      crh.prepare_author( { 0 } );
      crh.prepare_10_voters();
      executor->generate_block();

      executor->set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
      executor->generate_block();

      crh.prepare_funds();
      executor->generate_block();

      return crh.calculate_reward();
    };

    auto execution = []( curation_rewards_handler& crh, ptr_hardfork_database_fixture& executor,
                          share_type& total_rewards_pool_before, share_type& total_rewards_pool_after )
    {
      size_t vote_counter = 0;
      uint32_t vote_01_time = 12;

      std::string permlink  = "somethingpermlink";

      crh.prepare_comment( permlink, 0/*author_number*/ );
      crh.set_start_time( executor->db->head_block_time() );
      executor->generate_block();

      crh.voting( vote_counter, 0/*author_number*/, permlink, { vote_01_time } );
      crh.make_payment( 1/*back_offset_blocks*/ );
      total_rewards_pool_before = crh.current_total_reward();
      crh.make_payment();
      total_rewards_pool_after = crh.current_total_reward();
    };

    std::vector<comment_reward_info> comment_rewards_hf24;

    auto hf24_content = [&]( ptr_hardfork_database_fixture& executor )
    {
      reward_stat::rewards_stats early_stats;
      reward_stat::rewards_stats late_stats;
      share_type total_rewards_pool_before;
      share_type total_rewards_pool_after;

      curation_rewards_handler crh( *executor, *( executor->db ) );

      share_type reward_per_block = preparation( crh, executor );

      {
        //**********************HF 24 is activated**********************
        //Only 1 vote in `reverse_auction_seconds` window.
        execution( crh, executor, total_rewards_pool_before, total_rewards_pool_after );

        crh.curation_gathering( early_stats, late_stats );

        BOOST_REQUIRE( !crh.comment_rewards.empty() );
        comment_rewards_hf24 = crh.comment_rewards;
      }

      {
        BOOST_TEST_MESSAGE( "reward_per_block" );
        reward_stat::display_reward( reward_per_block );

        BOOST_TEST_MESSAGE( "*****HF-24*****" );
        BOOST_TEST_MESSAGE( "curation rewards" );
        reward_stat::display_stats( early_stats );
        reward_stat::display_stats( late_stats );

        BOOST_TEST_MESSAGE( "current pool of rewards before payment" );
        reward_stat::display_reward( total_rewards_pool_before );
        BOOST_TEST_MESSAGE( "current pool of rewards after payment" );
        reward_stat::display_reward( total_rewards_pool_after );

        BOOST_TEST_MESSAGE( "comment rewards" );
        for( auto& item : crh.comment_rewards )
          reward_stat::display_comment_rewards( item );
      }
      {
        /*
          early_stats.size() == 1
          late_stats == empty
          early_stats[0] < author_tokens[0]                 ( curation_reward < author_reward )
          reward > early_stats[0] + author_tokens[0]   ( current_reward > curation_reward + author_reward )
          checking exact values
        */
        BOOST_REQUIRE_EQUAL( early_stats.size(), 1 );
        BOOST_REQUIRE( late_stats.empty() );
        BOOST_REQUIRE_LT( early_stats[0].value, crh.comment_rewards[0].author_tokens.value );
        BOOST_REQUIRE_GT( crh.comment_rewards[0].total_reward.value, early_stats[0].value + crh.comment_rewards[0].author_tokens.value );
        BOOST_REQUIRE_EQUAL( early_stats[0].value,   1864 );
        BOOST_REQUIRE_EQUAL( early_stats[0].value,   crh.comment_rewards[0].curation_tokens.value );
        BOOST_REQUIRE_EQUAL( crh.comment_rewards[0].author_tokens.value, 37300 );
      }
      {
        /*
          total_rewards_pool_before + reward_per_block == total_rewards_pool_after + early_stats[0] + author_tokens[0]
        */
        auto _temp_before = total_rewards_pool_before.value + reward_per_block.value;
        auto _temp_after = total_rewards_pool_after.value + early_stats[0].value + crh.comment_rewards[0].author_tokens.value;
        BOOST_REQUIRE_EQUAL( _temp_before, _temp_after );
      }

      executor->validate_database();
    };

    std::vector<comment_reward_info> comment_rewards_hf25;

    auto hf25_content = [&]( ptr_hardfork_database_fixture& executor )
    {
      reward_stat::rewards_stats early_stats;
      reward_stat::rewards_stats mid_stats;
      reward_stat::rewards_stats late_stats;
      share_type total_rewards_pool_before;
      share_type total_rewards_pool_after;

      curation_rewards_handler crh( *executor, *( executor->db ) );

      share_type reward_per_block = preparation( crh, executor );

      {
        //**********************HF 25 is activated**********************
        //Only 1 vote in `early` window.
        execution( crh, executor, total_rewards_pool_before, total_rewards_pool_after );

        crh.curation_gathering( early_stats, mid_stats, late_stats );

        BOOST_REQUIRE( !crh.comment_rewards.empty() );
        comment_rewards_hf25 = crh.comment_rewards;
      }

      {
        BOOST_TEST_MESSAGE( "reward_per_block" );
        reward_stat::display_reward( reward_per_block );

        BOOST_TEST_MESSAGE( "*****HF-25*****" );
        BOOST_TEST_MESSAGE( "curation rewards" );
        reward_stat::display_stats( early_stats );
        reward_stat::display_stats( mid_stats );
        reward_stat::display_stats( late_stats );

        BOOST_TEST_MESSAGE( "current pool of rewards before payment" );
        reward_stat::display_reward( total_rewards_pool_before );
        BOOST_TEST_MESSAGE( "current pool of rewards after payment" );
        reward_stat::display_reward( total_rewards_pool_after );

        BOOST_TEST_MESSAGE( "comment rewards" );
        for( auto& item : crh.comment_rewards )
          reward_stat::display_comment_rewards( item );
      }
      {
        /*
          early_stats.size() == 1
          mid_stats == empty
          late_stats == empty
          early_stats[0] == author_tokens[0]            ( curation_reward == author_reward )
          total_reward[0] = early_stats[0] + author_tokens[0]   ( current_reward == curation_reward + author_reward )
          checking exact values
        */
        BOOST_REQUIRE_EQUAL( early_stats.size(), 1 );
        BOOST_REQUIRE( mid_stats.empty() );
        BOOST_REQUIRE( late_stats.empty() );
        BOOST_REQUIRE_EQUAL( early_stats[0].value, crh.comment_rewards[0].author_tokens.value );
        BOOST_REQUIRE_EQUAL( crh.comment_rewards[0].total_reward.value, early_stats[0].value + crh.comment_rewards[0].author_tokens.value );
        BOOST_REQUIRE_EQUAL( early_stats[0].value,   37300 );
        BOOST_REQUIRE_EQUAL( early_stats[0].value,   crh.comment_rewards[0].curation_tokens.value );
        BOOST_REQUIRE_EQUAL( crh.comment_rewards[0].author_tokens.value, 37300 );
      }
      {
        /*
          total_rewards_pool_before + reward_per_block == total_rewards_pool_after + early_stats[0] + author_tokens[0]
        */
        auto _temp_before = total_rewards_pool_before.value + reward_per_block.value;
        auto _temp_after = total_rewards_pool_after.value + early_stats[0].value + crh.comment_rewards[0].author_tokens.value;
        BOOST_REQUIRE_EQUAL( _temp_before, _temp_after );
      }

      executor->validate_database();
    };

    execute_hardfork<24>( hf24_content );
    execute_hardfork<25>( hf25_content );

    BOOST_REQUIRE_EQUAL( comment_rewards_hf24[0].author_tokens.value, comment_rewards_hf25[0].author_tokens.value );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( one_vote_per_comment_v2 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: curation rewards before and after HF25. Reward during whole rewards-time (7 days). Only 1 vote per 1 comment." );
    BOOST_TEST_MESSAGE( "HF24: exactly when `reverse_auction_seconds` window finishes. HF25: inside `early` window." );

    auto preparation = []( curation_rewards_handler& crh, ptr_hardfork_database_fixture& executor )
    {
      crh.prepare_author( { 0 } );
      crh.prepare_10_voters();
      executor->generate_block();

      executor->set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
      executor->generate_block();

      crh.prepare_funds();
      executor->generate_block();

      return crh.calculate_reward();
    };

    auto execution = []( curation_rewards_handler& crh, ptr_hardfork_database_fixture& executor,
                          share_type& total_rewards_pool_before, share_type& total_rewards_pool_after )
    {
      size_t vote_counter = 0;
      uint32_t vote_01_time = 300/*old value of 'reverse_auction_seconds'*/;

      std::string permlink  = "somethingpermlink";

      crh.prepare_comment( permlink, 0/*author_number*/ );
      crh.set_start_time( executor->db->head_block_time() );
      executor->generate_block();

      crh.voting( vote_counter, 0/*author_number*/, permlink, { vote_01_time } );
      crh.make_payment( 1/*back_offset_blocks*/ );
      total_rewards_pool_before = crh.current_total_reward();
      crh.make_payment();
      total_rewards_pool_after = crh.current_total_reward();
    };

    std::vector<comment_reward_info> comment_rewards_hf24;

    auto hf24_content = [&]( ptr_hardfork_database_fixture& executor )
    {
      reward_stat::rewards_stats early_stats;
      reward_stat::rewards_stats late_stats;
      share_type total_rewards_pool_before;
      share_type total_rewards_pool_after;

      curation_rewards_handler crh( *executor, *( executor->db ) );

      share_type reward_per_block = preparation( crh, executor );

      {
        //**********************HF 24 is activated**********************
        //Only 1 vote in `reverse_auction_seconds` window.
        execution( crh, executor, total_rewards_pool_before, total_rewards_pool_after );

        crh.curation_gathering( early_stats, late_stats );
        BOOST_REQUIRE( !crh.comment_rewards.empty() );
        comment_rewards_hf24 = crh.comment_rewards;
      }

      {
        BOOST_TEST_MESSAGE( "reward_per_block" );
        reward_stat::display_reward( reward_per_block );

        BOOST_TEST_MESSAGE( "*****HF-24*****" );
        BOOST_TEST_MESSAGE( "curation rewards" );
        reward_stat::display_stats( early_stats );
        reward_stat::display_stats( late_stats );

        BOOST_TEST_MESSAGE( "current pool of rewards before payment" );
        reward_stat::display_reward( total_rewards_pool_before );
        BOOST_TEST_MESSAGE( "current pool of rewards after payment" );
        reward_stat::display_reward( total_rewards_pool_after );

        BOOST_TEST_MESSAGE( "comment rewards" );
        for( auto& item : crh.comment_rewards )
          reward_stat::display_comment_rewards( item );
      }
      {
        /*
          early_stats.size() == empty
          late_stats == 1
          late_stats[0] == author_tokens[0]                 ( curation_reward == author_reward )
          crh.comment_rewards[0].total_reward.value == late_stats[0] + author_tokens[0]   ( current_reward == curation_reward + author_reward )
          checking exact values
        */
        BOOST_REQUIRE( early_stats.empty() );
        BOOST_REQUIRE_EQUAL( late_stats.size(), 1 );
        BOOST_REQUIRE_EQUAL( late_stats[0].value, crh.comment_rewards[0].author_tokens.value );
        BOOST_REQUIRE_EQUAL( crh.comment_rewards[0].total_reward.value, late_stats[0].value + crh.comment_rewards[0].author_tokens.value );
        BOOST_REQUIRE_EQUAL( late_stats[0].value,    37300 );
        BOOST_REQUIRE_EQUAL( late_stats[0].value,    crh.comment_rewards[0].curation_tokens.value );
        BOOST_REQUIRE_EQUAL( crh.comment_rewards[0].author_tokens.value, 37300 );
      }
      {
        /*
          total_rewards_pool_before + reward_per_block == total_rewards_pool_after + late_stats[0] + author_tokens[0]
        */
        auto _temp_before = total_rewards_pool_before.value + reward_per_block.value;
        auto _temp_after = total_rewards_pool_after.value + late_stats[0].value + crh.comment_rewards[0].author_tokens.value;
        BOOST_REQUIRE_EQUAL( _temp_before, _temp_after );
      }

      executor->validate_database();
    };

    std::vector<comment_reward_info> comment_rewards_hf25;

    auto hf25_content = [&]( ptr_hardfork_database_fixture& executor )
    {
      reward_stat::rewards_stats early_stats;
      reward_stat::rewards_stats mid_stats;
      reward_stat::rewards_stats late_stats;
      share_type total_rewards_pool_before;
      share_type total_rewards_pool_after;

      curation_rewards_handler crh( *executor, *( executor->db ) );

      share_type reward_per_block = preparation( crh, executor );

      {
        //**********************HF 25 is activated**********************
        //Only 1 vote in `early` window.
        execution( crh, executor, total_rewards_pool_before, total_rewards_pool_after );

        crh.curation_gathering( early_stats, mid_stats, late_stats );
        BOOST_REQUIRE( !crh.comment_rewards.empty() );
        comment_rewards_hf25 = crh.comment_rewards;
      }

      {
        BOOST_TEST_MESSAGE( "reward_per_block" );
        reward_stat::display_reward( reward_per_block );

        BOOST_TEST_MESSAGE( "*****HF-25*****" );
        BOOST_TEST_MESSAGE( "curation rewards" );
        reward_stat::display_stats( early_stats );
        reward_stat::display_stats( mid_stats );
        reward_stat::display_stats( late_stats );

        BOOST_TEST_MESSAGE( "current pool of rewards before payment" );
        reward_stat::display_reward( total_rewards_pool_before );
        BOOST_TEST_MESSAGE( "current pool of rewards after payment" );
        reward_stat::display_reward( total_rewards_pool_after );

        BOOST_TEST_MESSAGE( "comment rewards" );
        for( auto& item : crh.comment_rewards )
          reward_stat::display_comment_rewards( item );
      }
      {
        /*
          early_stats.size() == 1
          mid_stats == empty
          late_stats == empty
          early_stats[0] == author_tokens[0]            ( curation_reward == author_reward )
          crh.comment_rewards[0].total_reward.value = early_stats[0] + author_tokens[0]   ( current_reward == curation_reward + author_reward )
          checking exact values
        */
        BOOST_REQUIRE_EQUAL( early_stats.size(), 1 );
        BOOST_REQUIRE( mid_stats.empty() );
        BOOST_REQUIRE( late_stats.empty() );
        BOOST_REQUIRE_EQUAL( early_stats[0].value, crh.comment_rewards[0].author_tokens.value );
        BOOST_REQUIRE_EQUAL( crh.comment_rewards[0].total_reward.value, early_stats[0].value + crh.comment_rewards[0].author_tokens.value );
        BOOST_REQUIRE_EQUAL( early_stats[0].value,   37300 );
        BOOST_REQUIRE_EQUAL( early_stats[0].value,   crh.comment_rewards[0].curation_tokens.value );
        BOOST_REQUIRE_EQUAL( crh.comment_rewards[0].author_tokens.value, 37300 );
      }
      {
        /*
          total_rewards_pool_before + reward_per_block == total_rewards_pool_after + early_stats[0] + author_tokens[0]
        */
        auto _temp_before = total_rewards_pool_before.value + reward_per_block.value;
        auto _temp_after = total_rewards_pool_after.value + early_stats[0].value + crh.comment_rewards[0].author_tokens.value;
        BOOST_REQUIRE_EQUAL( _temp_before, _temp_after );
      }

      executor->validate_database();
    };

    execute_hardfork<24>( hf24_content );
    execute_hardfork<25>( hf25_content );

    BOOST_REQUIRE_EQUAL( comment_rewards_hf24[0].author_tokens.value, comment_rewards_hf25[0].author_tokens.value );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( five_votes_per_comment )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: curation rewards before and after HF25. Reward during whole rewards-time (7 days). 5 votes per 1 comment." );
    BOOST_TEST_MESSAGE( "HF24: after `reverse_auction_seconds` window. HF25: inside `early` window." );

    auto preparation = []( curation_rewards_handler& crh, ptr_hardfork_database_fixture& executor )
    {
      crh.prepare_author( { 0 } );
      crh.prepare_10_voters();
      executor->generate_block();

      executor->set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
      executor->generate_block();

      crh.prepare_funds();
      executor->generate_block();

      return crh.calculate_reward();
    };

    auto execution = []( curation_rewards_handler& crh, ptr_hardfork_database_fixture& executor,
                          share_type& total_rewards_pool_before, share_type& total_rewards_pool_after )
    {
      size_t vote_counter = 0;
      uint32_t vote_01_time = 300/*old value of 'reverse_auction_seconds'*/ + 3;

      std::string permlink  = "somethingpermlink";

      crh.prepare_comment( permlink, 0/*author_number*/ );
      crh.set_start_time( executor->db->head_block_time() );
      executor->generate_block();

      crh.voting( vote_counter, 0/*author_number*/, permlink, { vote_01_time } );
      crh.voting( vote_counter, 0/*author_number*/, permlink, { 0 } );
      crh.voting( vote_counter, 0/*author_number*/, permlink, { 0 } );
      crh.voting( vote_counter, 0/*author_number*/, permlink, { 0 } );
      crh.voting( vote_counter, 0/*author_number*/, permlink, { 0 } );
      crh.make_payment( 1/*back_offset_blocks*/ );
      total_rewards_pool_before = crh.current_total_reward();
      crh.make_payment();
      total_rewards_pool_after = crh.current_total_reward();
    };

    std::vector<comment_reward_info> comment_rewards_hf24;

    auto hf24_content = [&]( ptr_hardfork_database_fixture& executor )
    {
      reward_stat::rewards_stats early_stats;
      reward_stat::rewards_stats late_stats;
      share_type total_rewards_pool_before;
      share_type total_rewards_pool_after;

      curation_rewards_handler crh( *executor, *( executor->db ) );

      share_type reward_per_block = preparation( crh, executor );

      {
        //**********************HF 24 is activated**********************
        //5 votes in `reverse_auction_seconds` window.
        execution( crh, executor, total_rewards_pool_before, total_rewards_pool_after );

        crh.curation_gathering( early_stats, late_stats );
        BOOST_REQUIRE( !crh.comment_rewards.empty() );
        comment_rewards_hf24 = crh.comment_rewards;
      }

      {
        BOOST_TEST_MESSAGE( "reward_per_block" );
        reward_stat::display_reward( reward_per_block );

        BOOST_TEST_MESSAGE( "*****HF-24*****" );
        BOOST_TEST_MESSAGE( "curation rewards" );
        reward_stat::display_stats( early_stats );
        reward_stat::display_stats( late_stats );

        BOOST_TEST_MESSAGE( "current pool of rewards before payment" );
        reward_stat::display_reward( total_rewards_pool_before );
        BOOST_TEST_MESSAGE( "current pool of rewards after payment" );
        reward_stat::display_reward( total_rewards_pool_after );

        BOOST_TEST_MESSAGE( "comment rewards" );
        for( auto& item : crh.comment_rewards )
          reward_stat::display_comment_rewards( item );
      }

      const uint32_t nr_votes = 5;
      uint32_t _sum_curation_rewards = 0;
      {
        /*
          early_stats.size() == empty
          late_stats == nr_votes
          previous_reward > next_reward
          _sum_curation_rewards == author_tokens[0]                 ( sum_curation_rewards == author_reward )
          crh.comment_rewards[0].total_reward.value == _sum_curation_rewards + author_tokens[0]   ( current_reward == sum_curation_rewards + author_reward )
          checking exact values
        */
        BOOST_REQUIRE( early_stats.empty() );
        BOOST_REQUIRE_EQUAL( late_stats.size(), nr_votes );

        //calculating of auxiliary sum
        for( uint32_t i = 0; i < nr_votes; ++i )
          _sum_curation_rewards += late_stats[i].value;

        BOOST_TEST_MESSAGE( "sum_curation_rewards" );
        BOOST_TEST_MESSAGE( std::to_string( _sum_curation_rewards ).c_str() );

        BOOST_REQUIRE_EQUAL( _sum_curation_rewards, crh.comment_rewards[0].curation_tokens.value );

        for( uint32_t i = 0; i < nr_votes - 1; ++i )
          BOOST_REQUIRE_GT( late_stats[i].value, late_stats[i+1].value );

        BOOST_REQUIRE_EQUAL( _sum_curation_rewards + 2/*rounding*/, crh.comment_rewards[0].author_tokens.value );
        BOOST_REQUIRE_EQUAL( crh.comment_rewards[0].total_reward.value, _sum_curation_rewards + 2/*rounding*/ + crh.comment_rewards[0].author_tokens.value );
        BOOST_REQUIRE_EQUAL( late_stats[0].value,  10529 );
        BOOST_REQUIRE_EQUAL( late_stats[1].value,  8551 );
        BOOST_REQUIRE_EQUAL( late_stats[2].value,  7083 );
        BOOST_REQUIRE_EQUAL( late_stats[3].value,  5963 );
        BOOST_REQUIRE_EQUAL( late_stats[4].value,  5172 );
        BOOST_REQUIRE_EQUAL( crh.comment_rewards[0].author_tokens.value, 37300 );
      }
      {
        /*
          total_rewards_pool_before + reward_per_block == total_rewards_pool_after + _sum + author_tokens[0]
        */
        auto _temp_before = total_rewards_pool_before.value + reward_per_block.value;
        auto _temp_after = total_rewards_pool_after.value + _sum_curation_rewards + crh.comment_rewards[0].author_tokens.value;
        BOOST_REQUIRE_EQUAL( _temp_before, _temp_after );
      }

      executor->validate_database();
    };

    std::vector<comment_reward_info> comment_rewards_hf25;

    auto hf25_content = [&]( ptr_hardfork_database_fixture& executor )
    {
      reward_stat::rewards_stats early_stats;
      reward_stat::rewards_stats mid_stats;
      reward_stat::rewards_stats late_stats;
      share_type total_rewards_pool_before;
      share_type total_rewards_pool_after;

      curation_rewards_handler crh( *executor, *( executor->db ) );

      share_type reward_per_block = preparation( crh, executor );

      {
        //**********************HF 25 is activated**********************
        //5 votes in `reverse_auction_seconds` window.
        execution( crh, executor, total_rewards_pool_before, total_rewards_pool_after );

        crh.curation_gathering( early_stats, mid_stats, late_stats );
        BOOST_REQUIRE( !crh.comment_rewards.empty() );
        comment_rewards_hf25 = crh.comment_rewards;
      }

      {
        BOOST_TEST_MESSAGE( "reward_per_block" );
        reward_stat::display_reward( reward_per_block );

        BOOST_TEST_MESSAGE( "*****HF-25*****" );
        BOOST_TEST_MESSAGE( "curation rewards" );
        reward_stat::display_stats( early_stats );
        reward_stat::display_stats( mid_stats );
        reward_stat::display_stats( late_stats );

        BOOST_TEST_MESSAGE( "current pool of rewards before payment" );
        reward_stat::display_reward( total_rewards_pool_before );
        BOOST_TEST_MESSAGE( "current pool of rewards after payment" );
        reward_stat::display_reward( total_rewards_pool_after );

        BOOST_TEST_MESSAGE( "comment rewards" );
        for( auto& item : crh.comment_rewards )
          reward_stat::display_comment_rewards( item );
      }

      const uint32_t nr_votes = 5;
      uint32_t _sum_curation_rewards = 0;
      {
        /*
          early_stats.size() == nr_votes
          mid_stats == empty
          late_stats == empty
          previous_reward == next_reward
          _sum_curation_rewards == author_tokens[0]                 ( sum_curation_rewards == author_reward )
          crh.comment_rewards[0].total_reward.value = _sum_curation_rewards + author_tokens[0]    ( current_reward == sum_curation_rewards + author_reward )
          checking exact values
        */
        BOOST_REQUIRE_EQUAL( early_stats.size(), nr_votes );
        BOOST_REQUIRE( mid_stats.empty() );
        BOOST_REQUIRE( late_stats.empty() );

        //calculating of auxiliary sum
        for( uint32_t i = 0; i < nr_votes; ++i )
          _sum_curation_rewards += early_stats[i].value;

        BOOST_TEST_MESSAGE( "sum_curation_rewards" );
        BOOST_TEST_MESSAGE( std::to_string( _sum_curation_rewards ).c_str() );

        BOOST_REQUIRE_EQUAL( _sum_curation_rewards, crh.comment_rewards[0].curation_tokens.value );

        for( uint32_t i = 0; i < nr_votes - 1; ++i )
          BOOST_REQUIRE_EQUAL( early_stats[i].value, early_stats[i+1].value );

        BOOST_REQUIRE_EQUAL( _sum_curation_rewards, crh.comment_rewards[0].author_tokens.value );
        BOOST_REQUIRE_EQUAL( crh.comment_rewards[0].total_reward.value, _sum_curation_rewards + crh.comment_rewards[0].author_tokens.value );

        for( uint32_t i = 0; i < nr_votes; ++i )
          BOOST_REQUIRE_EQUAL( early_stats[i].value, 7460 );
        BOOST_REQUIRE_EQUAL( crh.comment_rewards[0].author_tokens.value, 37300 );
      }
      {
        /*
          total_rewards_pool_before + reward_per_block == total_rewards_pool_after + _sum + author_tokens[0]
        */
        auto _temp_before = total_rewards_pool_before.value + reward_per_block.value;
        auto _temp_after = total_rewards_pool_after.value + _sum_curation_rewards + crh.comment_rewards[0].author_tokens.value;
        BOOST_REQUIRE_EQUAL( _temp_before, _temp_after );
      }

      executor->validate_database();
    };

    execute_hardfork<24>( hf24_content );
    execute_hardfork<25>( hf25_content );

    BOOST_REQUIRE_EQUAL( comment_rewards_hf24[0].author_tokens.value, comment_rewards_hf25[0].author_tokens.value );

  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( two_comments_in_the_same_blocks )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: curation rewards before and after HF25. Reward during whole rewards-time (7 days). 2 comments in every hardfork." );
    BOOST_TEST_MESSAGE( "HF24: Both comments in `reverse_auction_seconds` window in the same block. First comment with many votes, second comment with 1 vote." );
    BOOST_TEST_MESSAGE( "HF25: Both comments in `early_window` in the same block. First comment with many votes, second comment with 1 vote." );

    /*
      Changes in HF25: 
        1)  curation_reward_curve:  convergent_square_root  ->  linear
        2)  author_reward_curve:    convergent_linear       ->  linear

      This test checks only the change number `1`, so it's possible to check how this change influences on author rewards,
      taking into consideration HF24 vs HF25.
    */
    two_comments_in_the_same_blocks_impl( *this, true/*restore_author_reward_curve*/ );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( two_comments_in_the_same_blocks_v2 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: curation rewards before and after HF25. Reward during whole rewards-time (7 days). 2 comments in every hardfork." );
    BOOST_TEST_MESSAGE( "HF24: Both comments in `reverse_auction_seconds` window in the same block. First comment with many votes, second comment with 1 vote." );
    BOOST_TEST_MESSAGE( "HF25: Both comments in `early_window` in the same block. First comment with many votes, second comment with 1 vote." );

    two_comments_in_the_same_blocks_impl( *this, false/*restore_author_reward_curve*/ );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( two_comments_in_different_blocks )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: curation rewards before and after HF25. Reward during whole rewards-time (7 days). 2 comments in every hardfork." );
    BOOST_TEST_MESSAGE( "HF24: Both comments in `reverse_auction_seconds` window in different blocks. First comment with many votes, second comment with 1 vote." );
    BOOST_TEST_MESSAGE( "HF25: Both comments in `early_window` in different blocks. First comment with many votes, second comment with 1 vote." );

    /*
      Changes in HF25: 
        1)  curation_reward_curve:  convergent_square_root  ->  linear
        2)  author_reward_curve:    convergent_linear       ->  linear

      This test checks only the change number `1`, so it's possible to check how this change influences on author rewards,
      taking into consideration HF24 vs HF25.
    */
    two_comments_in_different_blocks_impl( *this, true/*restore_author_reward_curve*/ );

  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( two_comments_in_different_blocks_v2 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: curation rewards before and after HF25. Reward during whole rewards-time (7 days). 2 comments in every hardfork." );
    BOOST_TEST_MESSAGE( "HF24: Both comments in `reverse_auction_seconds` window in different blocks. First comment with many votes, second comment with 1 vote." );
    BOOST_TEST_MESSAGE( "HF25: Both comments in `early_window` in different blocks. First comment with many votes, second comment with 1 vote." );

    two_comments_in_different_blocks_impl( *this, false/*restore_author_reward_curve*/ );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()
