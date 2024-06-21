#ifdef IS_TEST_NET
#include <boost/test/unit_test.hpp>

#include <hive/chain/comment_object.hpp>
#include <hive/chain/full_transaction.hpp>

#include <hive/protocol/hive_operations.hpp>
#include <hive/protocol/transaction_util.hpp>

#include <hive/plugins/chain/chain_plugin.hpp>
#include <hive/plugins/witness/witness_plugin.hpp>

#include <boost/scope_exit.hpp>

#include "../db_fixture/hived_fixture.hpp"

using namespace hive::chain;
using namespace hive::protocol;

struct witness_fixture : public hived_fixture
{
  witness_fixture( bool remove_db = true ) : hived_fixture( remove_db, false ) {}
  virtual ~witness_fixture() {}

  void initialize( int genesis_delay = 1, // genesis slightly in the future (or past with negative values)
    const std::vector< std::string > initial_witnesses = {}, // initial witnesses over 'initminer'
    const std::vector< std::string > represented_witnesses = { "initminer" } ) // which witnesses can produce
  {
    theApp.init_signals_handler();

    configuration_data.set_initial_asset_supply(
      200'000'000'000ul, 1'000'000'000ul, 100'000'000'000ul,
      price( VEST_asset( 1'800 ), HIVE_asset( 1'000 ) )
    );
    if( genesis_time == fc::time_point_sec() )
    {
      genesis_time = fc::time_point::now() + fc::seconds( genesis_delay );
      // we need to make genesis time proper multiple of 3 seconds, otherwise only first block will be
      // produced at genesis + 3s, next one will be earlier than genesis + 6s (see database::get_slot_time)
      genesis_time = fc::time_point_sec( ( genesis_time.sec_since_epoch() + 2 ) / HIVE_BLOCK_INTERVAL * HIVE_BLOCK_INTERVAL );
    }

    configuration_data.set_hardfork_schedule( genesis_time, { { HIVE_NUM_HARDFORKS, 1 } } );
    if( not initial_witnesses.empty() )
      configuration_data.set_init_witnesses( initial_witnesses );

    config_arg_override_t config_args = {
      config_line_t( { "plugin", { HIVE_WITNESS_PLUGIN_NAME } } ),
      config_line_t( { "shared-file-size", { "1G" } } ),
      config_line_t( { "private-key", { init_account_priv_key.key_to_wif() } } )
    };
    for( auto& name : represented_witnesses )
      config_args.emplace_back( config_line_t( "witness", { "\"" + name + "\"" } ) );

    postponed_init( config_args, &witness_plugin );

    init_account_pub_key = init_account_priv_key.get_public_key();
  }

  uint32_t get_block_num() const
  {
    uint32_t num = 0;
    db->with_read_lock( [&]() { num = db->head_block_num(); } );
    return num;
  }

  void schedule_transaction( const operation& op ) const
  {
    signed_transaction tx;
    db->with_read_lock( [&]()
    {
      tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
      tx.set_reference_block( db->head_block_id() );
    } );
    tx.operations.emplace_back( op );
    full_transaction_ptr _tx = full_transaction_type::create_from_signed_transaction( tx, serialization_type::hf26, false );
    tx.clear();
    _tx->sign_transaction( { init_account_priv_key }, db->get_chain_id(), serialization_type::hf26 );
    get_chain_plugin().accept_transaction( _tx, hive::plugins::chain::chain_plugin::lock_type::fc );
  }

  void schedule_blocks( uint32_t count ) const
  {
    db_plugin->debug_generate_blocks( init_account_priv_key.key_to_wif(), count, default_skip, 0, false );
  }

  void schedule_block() const
  {
    schedule_blocks( 1 );
  }

  void schedule_account_create( const account_name_type& name ) const
  {
    account_create_operation create;
    create.new_account_name = name;
    create.creator = HIVE_INIT_MINER_NAME;
    create.fee = db->get_witness_schedule_object().median_props.account_creation_fee;
    create.owner = authority( 1, init_account_pub_key, 1 );
    create.active = create.owner;
    create.posting = create.owner;
    create.memo_key = init_account_pub_key;
    schedule_transaction( create );
  }

  void schedule_vest( const account_name_type& to, const asset& amount ) const
  {
    transfer_to_vesting_operation vest;
    vest.from = HIVE_INIT_MINER_NAME;
    vest.to = to;
    vest.amount = amount;
    schedule_transaction( vest );
  }

  void schedule_fund( const account_name_type& to, const asset& amount ) const
  {
    transfer_operation fund;
    fund.from = HIVE_INIT_MINER_NAME;
    fund.to = to;
    fund.amount = amount;
    schedule_transaction( fund );
  }

  void schedule_transfer( const account_name_type& from, const account_name_type& to,
    const asset& amount, const std::string& memo ) const
  {
    transfer_operation transfer;
    transfer.from = from;
    transfer.to = to;
    transfer.amount = amount;
    transfer.memo = memo;
    schedule_transaction( transfer );
  }

  void schedule_vote( const account_name_type& voter, const account_name_type& author,
    const std::string& permlink ) const
  {
    vote_operation vote;
    vote.voter = voter;
    vote.author = author;
    vote.permlink = permlink;
    vote.weight = HIVE_100_PERCENT;
    schedule_transaction( vote );
  }

  void set_genesis_time( fc::time_point_sec time ) { genesis_time = time; }
  fc::time_point_sec get_genesis_time() const { return genesis_time; }

  hive::plugins::witness::witness_plugin& get_witness_plugin() const { return *witness_plugin; }

private:
  fc::time_point_sec genesis_time;
  hive::plugins::witness::witness_plugin* witness_plugin = nullptr;
};

struct restart_witness_fixture : public witness_fixture
{
  restart_witness_fixture() : witness_fixture( false ) {}
  virtual ~restart_witness_fixture() {}
};

BOOST_FIXTURE_TEST_SUITE( witness_tests, witness_fixture )

#define CATCH( thread_name )                                                  \
catch( const fc::exception& ex )                                              \
{                                                                             \
  elog( "Unhandled fc::exception thrown from '" thread_name "' thread: ${r}", \
    ( "r", ex.to_detail_string() ) );                                         \
}                                                                             \
catch( ... )                                                                  \
{                                                                             \
  elog( "Unhandled exception thrown from '" thread_name "' thread" );         \
}

BOOST_AUTO_TEST_CASE( witness_basic_test )
{
  using namespace hive::plugins::witness;

  try
  {
    initialize();
    bool test_passed = false;

    fc::thread api_thread;
    api_thread.async( [&]()
    { 
      BOOST_SCOPE_EXIT( this_ ) { this_->theApp.generate_interrupt_request(); } BOOST_SCOPE_EXIT_END
      try
      {
        ilog( "Wait for first block after genesis" );
        fc::sleep_until( get_genesis_time() + HIVE_BLOCK_INTERVAL );
        ilog( "All hardforks should have been applied" );
        BOOST_REQUIRE( db->has_hardfork( HIVE_NUM_HARDFORKS ) );
        db->_log_hardforks = true;

        ilog( "Starting 'API' thread that will be sending transactions" );

        uint32_t current_block_num = get_block_num();
        uint32_t saved_block_num = current_block_num;

        schedule_account_create( "alice" );
        schedule_vest( "alice", ASSET( "1000.000 TESTS" ) );
        schedule_fund( "alice", ASSET( "1.000 TBD" ) );

        schedule_account_create( "bob" );
        schedule_vest( "bob", ASSET( "1000.000 TESTS" ) );
        schedule_fund( "bob", ASSET( "1.000 TBD" ) );

        schedule_account_create( "carol" );
        schedule_vest( "carol", ASSET( "1000.000 TESTS" ) );
        schedule_fund( "carol", ASSET( "1.000 TBD" ) );

        HIVE_REQUIRE_ASSERT( schedule_account_create( "no one" ), "false" ); // invalid name

        ilog( "waiting for the block to consume all account preparation transactions" );
        fc::usleep( fc::seconds( HIVE_BLOCK_INTERVAL ) );
        current_block_num = get_block_num();
        BOOST_REQUIRE_GT( current_block_num, saved_block_num ); // at least one block should have been generated
        saved_block_num = current_block_num;

        schedule_transfer( "alice", "bob", ASSET( "0.100 TBD" ), "" );

        fc::usleep( fc::seconds( HIVE_BLOCK_INTERVAL ) );
        current_block_num = get_block_num();
        BOOST_REQUIRE_GT( get_block_num(), saved_block_num );
        saved_block_num = current_block_num;

        schedule_transfer( "bob", "carol", ASSET( "1.100 TBD" ), "all in" );

        fc::usleep( fc::seconds( HIVE_BLOCK_INTERVAL ) );
        current_block_num = get_block_num();
        BOOST_REQUIRE_GT( get_block_num(), saved_block_num );
        saved_block_num = current_block_num;

        db->with_read_lock( [&]()
        {
          BOOST_REQUIRE_EQUAL( get_hbd_balance( "carol" ).amount.value, 2100 );
        } );

        ilog( "'API' thread finished" );
        test_passed = true;
      }
      CATCH( "API" )
    } );

    theApp.wait4interrupt_request();
    theApp.quit( true );
    db = nullptr; // prevent fixture destructor from accessing database after it was closed
    BOOST_REQUIRE( test_passed );
    ilog( "Test done" );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( multiple_feeding_threads_test )
{
  using namespace hive::plugins::witness;

  try
  {
    configuration_data.min_root_comment_interval = fc::seconds( 3 * HIVE_BLOCK_INTERVAL );
    initialize();

    enum { ALICE, BOB, CAROL, DAN, FEEDER_COUNT };
    bool test_passed[ FEEDER_COUNT + 1 ] = {};
    std::atomic<uint32_t> active_feeders( 0 );
    fc::thread sync_thread;
    sync_thread.async( [&]()
    {
      BOOST_SCOPE_EXIT( this_ ) { this_->theApp.generate_interrupt_request(); } BOOST_SCOPE_EXIT_END
      try
      {
        ilog( "Wait for first block after genesis" );
        fc::sleep_until( get_genesis_time() + HIVE_BLOCK_INTERVAL );
        ilog( "All hardforks should have been applied" );
        BOOST_REQUIRE( db->has_hardfork( HIVE_NUM_HARDFORKS ) );
        db->_log_hardforks = true;

        schedule_account_create( "alice" );
        schedule_vest( "alice", ASSET( "1000.000 TESTS" ) );
        schedule_fund( "alice", ASSET( "1000.000 TBD" ) );

        schedule_account_create( "bob" );
        schedule_vest( "bob", ASSET( "1000.000 TESTS" ) );
        schedule_fund( "bob", ASSET( "1000.000 TBD" ) );

        schedule_account_create( "carol" );
        schedule_vest( "carol", ASSET( "1000.000 TESTS" ) );
        schedule_fund( "carol", ASSET( "1000.000 TBD" ) );

        schedule_account_create( "dan" );
        schedule_vest( "dan", ASSET( "1000.000 TESTS" ) );
        schedule_fund( "dan", ASSET( "1000.000 TBD" ) );

        comment_operation comment;
        comment.parent_permlink = "announcement";
        comment.author = HIVE_INIT_MINER_NAME;
        comment.permlink = "testnet";
        comment.title = "testnet rules";
        comment.body = "Welcome to testnet. All of you got some funds to play with. You can also send "
          "comments and vote. Just no #nsfw and please behave. Have fun.";
        schedule_transaction( comment );

        ilog( "waiting for the block to consume all account preparation transactions" );
        fc::usleep( fc::seconds( HIVE_BLOCK_INTERVAL ) );
        test_passed[ FEEDER_COUNT ] = true;
      }
      CATCH( "SYNC" )

      active_feeders.store( FEEDER_COUNT );
      ilog( "Waiting for feeder threads to finish work" );
      while( active_feeders.load() > 0 )
        fc::usleep( fc::milliseconds( 200 ) );
      ilog( "All threads finished." );
    } );

    fc::thread api_thread[ FEEDER_COUNT ];

    api_thread[ ALICE ].async( [&]()
    {
      try
      {
        while( active_feeders.load() == 0 )
          fc::usleep( fc::milliseconds( 200 ) );
        ilog( "Starting 'API' thread that will be sending transactions from 'alice'" );

        comment_operation comment;
        comment.parent_permlink = "introduction";
        comment.author = "alice";
        comment.permlink = "hello";
        comment.title = "hello";
        comment.body = "Hi. I'm Alice and I'm going to blog about the most awesomest gal in the world that is ME xoxo";
        ilog( "sending alice/hello comment" );
        schedule_transaction( comment );

        ilog( "sending alice -> dan transfer" );
        schedule_transfer( "alice", "dan", ASSET( "12.000 TBD" ), "Banana Latte, 3$ tip" );

        fc::usleep( fc::seconds( 5 * HIVE_BLOCK_INTERVAL ) );
        comment.parent_permlink = "funnydog";
        comment.permlink = "cat";
        comment.title = "cute cat";
        comment.body = "Hi. It is me again. When I went for my Banana Latte this morning, I've met a cute cat. "
          "Not as cute as me though. It allowed me to pet it (obviously). It must have been one of the neighbors' cat "
          "since it had nice clean fur. I wouldn't touch it, if it was a stray cat, I don't want to get cuties, yuck. "
          "Aanyway, there would not be any stray cats, since I live in nice neighborhood. "
          ":peace: and :heart: #cute #cat #latte";
        ilog( "sending alice/cat comment" );
        schedule_transaction( comment );

        fc::usleep( fc::seconds( 6 * HIVE_BLOCK_INTERVAL ) );
        comment.parent_permlink = "highlife";
        comment.permlink = "shopping";
        comment.title = "new dress";
        comment.body = "Hi. I'm thinking of buying a new dress. I'm going to the party tonight and I need to look "
          "fabulous. None of my old dresses will do. Good thing I'm rich so I can go shoppiiing! whenever I want :dollar:. "
          "Should I take a black mini or maybe sexy long red dress with a side cut? I think I'll go with red one, since "
          "it both covers and reveals my beautiful legs. A girl gotta be shrouded in mystery a bit. If you want to see "
          "how I look in it sign up to my OnlyFans :kiss: :wink:";
        ilog( "sending alice/shopping comment" );
        schedule_transaction( comment );

        fc::usleep( fc::seconds( HIVE_BLOCK_INTERVAL ) );
        ilog( "sending alice -> dan transfer" );
        schedule_transfer( "alice", "dan", ASSET( "999.990 TBD" ), "Pull&Bear red cut long #43980982343" );

        ilog( "'alice' logging out" );
        test_passed[ ALICE ] = true;
      }
      CATCH( "ALICE" )
      --active_feeders;
      ilog( "'ALICE' thread finished" );
    } );

    api_thread[ BOB ].async( [&]()
    {
      try
      {
        while( active_feeders.load() == 0 )
          fc::usleep( fc::milliseconds( 200 ) );
        ilog( "Starting 'API' thread that will be sending transactions from 'bob'" );

        account_id_type alice_id, carol_id;
        db->with_read_lock( [&]()
        {
          alice_id = db->get_account( "alice" ).get_id();
          carol_id = db->get_account( "carol" ).get_id();
        } );

        const auto& comment_idx = db->get_index< comment_index, by_id >();
        comment_id_type last_comment;

        for( int i = 0; i < 18 * HIVE_BLOCK_INTERVAL; ++i )
        {
          bool send = false;
          account_name_type author;
          std::string permlink;
          db->with_read_lock( [&]()
          {
            auto commentI = comment_idx.lower_bound( last_comment );
            ++commentI;
            if( commentI != comment_idx.end() )
            {
              last_comment = commentI->get_id();
              const auto& comment_cashout = *db->find_comment_cashout( last_comment );
              auto author_id = comment_cashout.get_author_id();
              author = db->get_account( author_id ).get_name();
              permlink = comment_cashout.get_permlink();
              send = author_id == alice_id || author_id == carol_id;
            }
          } );
          if( send )
          {
            ilog( "sending bob vote on ${a}/${p}", ( "a", author )( "p", permlink ) );
            schedule_vote( "bob", author, permlink );
            ilog( "sending bob -> ${a} transfer", ( "a", author ) );
            schedule_transfer( "bob", author, ASSET( "10.000 TBD" ), "Hello pretty" );
          }
          fc::usleep( fc::seconds( 1 ) );
        }
        ilog( "'bob' logging out" );
        test_passed[ BOB ] = true;
      }
      CATCH( "BOB" )
      --active_feeders;
      ilog( "'BOB' thread finished" );
    } );

    api_thread[ CAROL ].async( [&]()
    {
      try
      {
        while( active_feeders.load() == 0 )
          fc::usleep( fc::milliseconds( 200 ) );
        ilog( "Starting 'API' thread that will be sending transactions from 'carol'" );

        comment_operation comment;
        comment.parent_permlink = "introduction";
        comment.author = "carol";
        comment.permlink = "about";
        comment.title = "About me";
        comment.body = "Good day everyone.\n"
          "![rose](https://cdn.globalrose.com/assets/img/prod/choose-your-quantity-of-roses-globalrose.png)\n"
          "I was introduced to this whole blogging thing by my son, who went to study law in the capital. "
          "I'm still not sure what I'll be writing about. Please be easy on me. I hope we'll get along.";
        ilog( "sending carol/about" );
        schedule_transaction( comment );

        fc::usleep( fc::seconds( 2 * HIVE_BLOCK_INTERVAL ) );
        comment.parent_permlink = "help";
        comment.permlink = "cat";
        comment.title = "Poor Nyanta";
        comment.body = "Oh noes! My son's cat escaped. I let it lay near the balkony where it likes to sleep "
          "in the sun and went to make breakfast. When I returned, I've noticed the window was opened and "
          "Nyanta was gone. Poor thing! How can it survive in the harsh world outside? Maybe someone saw it? "
          "It is a big long [British Shorthair](https://en.wikipedia.org/wiki/British_Shorthair) mixed with "
          "[Maine Coon](https://en.wikipedia.org/wiki/Maincoon). It is mostly dark gray with white muzzle and "
          "ears.";
        ilog( "sending carol/cat (failure - too early)" );
        HIVE_REQUIRE_ASSERT( schedule_transaction( comment ),
          "( _now - auth.last_root_post ) > HIVE_MIN_ROOT_COMMENT_INTERVAL" );
        fc::usleep( fc::seconds( 2 * HIVE_BLOCK_INTERVAL ) );
        ilog( "sending carol/cat" );
        schedule_transaction( comment );

        // wait long enough for alice/shopping comment to show up and a bit more
        fc::usleep( fc::seconds( 8 * HIVE_BLOCK_INTERVAL ) );
        while( db->find_comment( "alice", std::string( "shopping" ) ) == nullptr )
          fc::usleep( fc::seconds( 1 ) );
        fc::usleep( fc::seconds( HIVE_BLOCK_INTERVAL ) );

        comment.parent_author = "alice";
        comment.parent_permlink = "shopping";
        comment.permlink = "harlot";
        comment.title = "Are you not ashamed of yourself?";
        comment.body = "Young lady! When I saw you I almost called the police. You should not be walking down "
          "the street dressed up like that! Leave that kind of outfit for your husband's eyes only in the privacy "
          "of your bedroom. Only \"women of paid affection\" show that much skin to the strangers. Shame on you!";
        ilog( "sending carol/harlot" );
        schedule_transaction( comment );

        ilog( "'carol' logging out" );
        test_passed[ CAROL ] = true;
      }
      CATCH( "CAROL" )
      --active_feeders;
      ilog( "'CAROL' thread finished" );
    } );

    api_thread[ DAN ].async( [&]()
    {
      try
      {
        while( active_feeders.load() == 0 )
          fc::usleep( fc::milliseconds( 200 ) );
        ilog( "Starting 'API' thread that will be sending transactions from 'dan'" );

        comment_operation comment;
        comment.parent_permlink = "offer";
        comment.author = "dan";
        comment.permlink = "cafeteria";
        comment.title = "Welcome to Your Morning Coffee";
        comment.body = "Good morning! Welcome to \"Your Morning Coffee\".\n"
          "Get your daily dose of sugar and caffeine in form of Banana Latte, Snowy Peak Mocca or famous "
          "Tripple Espresso! We also got American Style Apple Pie, Maple Syrup Pancakes and all sort of icecream. "
          "Stop by during your morning jog to replenish those calories. Fat tissue requires nutrition!";
        ilog( "sending dan/cafeteria comment" );
        schedule_transaction( comment );

        fc::usleep( fc::seconds( 5 * HIVE_BLOCK_INTERVAL ) );

        comment.parent_permlink = "offer";
        comment.permlink = "clothes";
        comment.title = "Welcome to Your Fancy Look";
        comment.body = "Good morning! Welcome to \"Your Fancy Look\".\n"
          "We have all kinds of latest fashion clothing made in the finest child labor camps in Bangladesh "
          "and Vietnam. Apply for our \"No Benefits Loyalty Card\" to get exclusive offers and sign up for "
          "\"Just Spam Newsletter\" for daily gossip about unknown celebrities. Make sure to read our \"No "
          "Returns Return Policy\". Fill up your coffers with clothes from famous NoName, NoBrand and other "
          "cheap manufacturers sold at a premium price. The fastest way to part with your money. Visit us today.";
        ilog( "sending dan/clothes comment" );
        schedule_transaction( comment );

        ilog( "'dan' logging out" );
        test_passed[ DAN ] = true;
      }
      CATCH( "DAN" )
      --active_feeders;
      ilog( "'DAN' thread finished" );
    } );

    theApp.wait4interrupt_request();
    theApp.quit( true );
    db = nullptr; // prevent fixture destructor from accessing database after it was closed
    BOOST_REQUIRE( test_passed[ FEEDER_COUNT ] );
    BOOST_REQUIRE( test_passed[ ALICE ] );
    BOOST_REQUIRE( test_passed[ BOB ] );
    BOOST_REQUIRE( test_passed[ CAROL ] );
    BOOST_REQUIRE( test_passed[ DAN ] );
    ilog( "Test done" );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( start_before_genesis_test )
{
  using namespace hive::plugins::witness;

  try
  {
    initialize( 3, { "wit1", "wit2", "wit3", "wit4", "wit5", "wit6", "wit7", "wit8", "wit9", "wita",
      "witb", "witc", "witd", "wite", "witf", "witg", "with", "witi", "witj", "witk" } );
    bool test_passed = false;

    fc::thread api_thread;
    api_thread.async( [&]()
    {
      BOOST_SCOPE_EXIT( this_ ) { this_->theApp.generate_interrupt_request(); } BOOST_SCOPE_EXIT_END
      try
      {
        ilog( "Wait for first block after genesis" );
        fc::sleep_until( get_genesis_time() + HIVE_BLOCK_INTERVAL );
        ilog( "All hardforks should have been applied" );
        BOOST_REQUIRE( db->has_hardfork( HIVE_NUM_HARDFORKS ) );
        db->_log_hardforks = true;

        const auto& block_header = get_block_reader().head_block()->get_block_header();
        ilog( "Block #${n}, ts: ${t}, witness: ${w}", ( "n", block_header.block_num() )
          ( "t", block_header.timestamp )( "w", block_header.witness ) );
        BOOST_REQUIRE_EQUAL( block_header.witness, "initminer" ); // initminer produces all blocks for first two schedules
        BOOST_REQUIRE( block_header.timestamp == get_genesis_time() + HIVE_BLOCK_INTERVAL );

        ilog( "'API' thread finished" );
        test_passed = true;
      }
      CATCH( "API" )
    } );

    theApp.wait4interrupt_request();
    theApp.quit( true );
    db = nullptr; // prevent fixture destructor from accessing database after it was closed
    BOOST_REQUIRE( test_passed );
    ilog( "Test done" );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( missing_blocks_test )
{
  using namespace hive::plugins::witness;

  try
  {
    initialize( -HIVE_MAX_WITNESSES * 2 * HIVE_BLOCK_INTERVAL,
      { "wit1", "wit2", "wit3", "wit4", "wit5", "wit6", "wit7", "wit8", "wit9", "wita",
      "witb", "witc", "witd", "wite", "witf", "witg", "with", "witi", "witj", "witk" },
      { "wit1", "wit3", "wit5", "wit7", "wit9", "witb", "witd", "witf", "with", "witj" }
    ); // representing every other witness
    bool test_passed = false;
    fc::logger::get( "user" ).set_log_level( fc::log_level::info ); // suppress fast confirm broadcast messages

    // produce first two schedules of blocks (initminer) so we can get to actual test;
    // note that genesis time was set in the past so we don't have to wait;
    // also note that we are not representing 'initminer' to avoid witness plugin thinking it is
    // its turn to produce during call below
    schedule_blocks( HIVE_MAX_WITNESSES * 2 );
    BOOST_REQUIRE( db->has_hardfork( HIVE_NUM_HARDFORKS ) );
    db->_log_hardforks = true;

    fc::thread api_thread;
    api_thread.async( [&]()
    {
      BOOST_SCOPE_EXIT( this_ ) { this_->theApp.generate_interrupt_request(); } BOOST_SCOPE_EXIT_END
      try
      {
        uint32_t block_num = HIVE_MAX_WITNESSES * 2;
        BOOST_REQUIRE_EQUAL( block_num, db->head_block_num() );
        fc::time_point_sec next_block_time = get_genesis_time() + ( block_num + 1 ) * HIVE_BLOCK_INTERVAL;
        fc::sleep_until( next_block_time );

        for( int i = 1; i <= HIVE_MAX_WITNESSES; ++i )
        {
          const auto& block_header = get_block_reader().head_block()->get_block_header();
          if( block_num == block_header.block_num() )
          {
            ilog( "Missed block #${n}, ts: ${t}", ( "n", block_num + 1 )( "t", next_block_time ) );
          }
          else
          {
            block_num = block_header.block_num();
            ilog( "Block #${n}, ts: ${t}, witness: ${w}", ( "n", block_num )
              ( "t", block_header.timestamp )( "w", block_header.witness ) );
            BOOST_REQUIRE( block_header.timestamp == next_block_time );
          }
          next_block_time += HIVE_BLOCK_INTERVAL;
          fc::sleep_until( next_block_time );
        }
        BOOST_REQUIRE_EQUAL( block_num, HIVE_MAX_WITNESSES * 3 - 11 ); // we should see 11 missed blocks

        ilog( "'API' thread finished" );
        test_passed = true;
      }
      CATCH( "API" )
    } );

    theApp.wait4interrupt_request();
    theApp.quit( true );
    db = nullptr; // prevent fixture destructor from accessing database after it was closed
    BOOST_REQUIRE( test_passed );
    ilog( "Test done" );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( supplemented_blocks_test )
{
  using namespace hive::plugins::witness;
  // ABW: same as missing_blocks_test except blocks that witness plugin could not produce due to
  // not being marked as representing scheduled witnesses are generated artificially with debug plugin;
  // the main purpose of this test is to check if such thing works; for witness plugin it should
  // look as if those blocks were created outside of node and passed to it through p2p

  try
  {
    initialize( -HIVE_MAX_WITNESSES * 2 * HIVE_BLOCK_INTERVAL,
      { "wit1", "wit2", "wit3", "wit4", "wit5", "wit6", "wit7", "wit8", "wit9", "wita",
      "witb", "witc", "witd", "wite", "witf", "witg", "with", "witi", "witj", "witk" },
      { "wit1", "wit3", "wit5", "wit7", "wit9", "witb", "witd", "witf", "with", "witj" }
    ); // representing every other witness
    bool test_passed = false;
    fc::logger::get( "user" ).set_log_level( fc::log_level::info ); // suppress fast confirm broadcast messages

    // produce first two schedules of blocks (initminer) so we can get to actual test;
    // note that genesis time was set in the past so we don't have to wait;
    // also note that we are not representing 'initminer' to avoid witness plugin thinking it is
    // its turn to produce during call below
    schedule_blocks( HIVE_MAX_WITNESSES * 2 );
    BOOST_REQUIRE( db->has_hardfork( HIVE_NUM_HARDFORKS ) );
    db->_log_hardforks = true;

    fc::thread api_thread;
    api_thread.async( [&]()
    {
      BOOST_SCOPE_EXIT( this_ ) { this_->theApp.generate_interrupt_request(); } BOOST_SCOPE_EXIT_END
      try
      {
        uint32_t block_num = HIVE_MAX_WITNESSES * 2;
        BOOST_REQUIRE_EQUAL( block_num, db->head_block_num() );
        fc::time_point_sec next_block_time = get_genesis_time() + ( block_num + 1 ) * HIVE_BLOCK_INTERVAL;
        fc::sleep_until( next_block_time );

        for( int i = 1; i <= HIVE_MAX_WITNESSES; ++i )
        {
          const auto* block_header = &get_block_reader().head_block()->get_block_header();
          if( block_num == block_header->block_num() )
          {
            ilog( "Supplementing block with debug plugin" );
            schedule_block();
            block_header = &get_block_reader().head_block()->get_block_header();
          }
          block_num = block_header->block_num();
          ilog( "Block #${n}, ts: ${t}, witness: ${w}", ( "n", block_num )
            ( "t", block_header->timestamp )( "w", block_header->witness ) );
          BOOST_REQUIRE( block_header->timestamp == next_block_time );
          next_block_time += HIVE_BLOCK_INTERVAL;
          fc::sleep_until( next_block_time );
        }
        BOOST_REQUIRE_EQUAL( block_num, HIVE_MAX_WITNESSES * 3 ); // all missed blocks should be supplemented

        ilog( "'API' thread finished" );
        test_passed = true;
      }
      CATCH( "API" )
    } );

    theApp.wait4interrupt_request();
    theApp.quit( true );
    db = nullptr; // prevent fixture destructor from accessing database after it was closed
    BOOST_REQUIRE( test_passed );
    ilog( "Test done" );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_FIXTURE_TEST_CASE( not_synced_start_test, restart_witness_fixture )
{
  using namespace hive::plugins::witness;

  bool test_passed = false;

  try
  {
    witness_fixture preparation;
    preparation.initialize( -HIVE_MAX_WITNESSES * 3 * HIVE_BLOCK_INTERVAL,
      { "wit1", "wit2", "wit3", "wit4", "wit5", "wit6", "wit7", "wit8", "wit9", "wita",
      "witb", "witc", "witd", "wite", "witf", "witg", "with", "witi", "witj", "witk" }
    );

    set_data_dir( preparation.theApp.data_dir().c_str() );
    set_genesis_time( preparation.get_genesis_time() );

    preparation.generate_blocks( HIVE_MAX_WITNESSES * 3 ); // last 15 is reversible
    preparation.theApp.generate_interrupt_request();
    preparation.theApp.wait4interrupt_request();
    preparation.theApp.quit( true );
    preparation.db = nullptr; // prevent fixture destructor from accessing database after it was closed
  }
  FC_LOG_AND_RETHROW()

  try
  {

    initialize( 0, // already set
      { "wit1", "wit2", "wit3", "wit4", "wit5", "wit6", "wit7", "wit8", "wit9", "wita",
      "witb", "witc", "witd", "wite", "witf", "witg", "with", "witi", "witj", "witk" },
      { "with" } ); // represent only 'with'
    fc::logger::get( "user" ).set_log_level( fc::log_level::info ); // suppress fast confirm broadcast messages
    db->_log_hardforks = true;

    fc::thread api_thread;
    api_thread.async( [&]()
    {
      BOOST_SCOPE_EXIT( this_ ) { this_->theApp.generate_interrupt_request(); } BOOST_SCOPE_EXIT_END
      try
      {
        // we filled three full schedules of blocks but last 15 should be reversible, which means
        // we are that many behind at the moment; witness should think it is out of sync and not
        // try to produce
        uint32_t block_num = db->head_block_num();
        schedule_blocks( HIVE_MAX_WITNESSES * 3 - block_num );
        block_num = db->head_block_num();
        BOOST_REQUIRE_EQUAL( HIVE_MAX_WITNESSES * 3, block_num );
        // now witness should turn on production, but wait for the turn of 'with'
        bool already_produced = false;
        fc::time_point_sec next_block_time = get_genesis_time() + ( block_num + 1 ) * HIVE_BLOCK_INTERVAL;
        bool should_produce_next = db->get_scheduled_witness( 1 ) == "with";
        fc::sleep_until( next_block_time );

        for( int i = 1; i <= HIVE_MAX_WITNESSES; ++i )
        {
          const auto* block_header = &get_block_reader().head_block()->get_block_header();
          if( block_num == block_header->block_num() )
          {
            ilog( "Supplementing block with debug plugin" );
            BOOST_REQUIRE( not should_produce_next );
            schedule_block();
            block_header = &get_block_reader().head_block()->get_block_header();
            BOOST_REQUIRE_NE( block_header->witness, "with" );
          }
          else
          {
            ilog( "Witness produced first block" );
            BOOST_REQUIRE( should_produce_next );
            BOOST_REQUIRE_EQUAL( block_header->witness, "with" );
            BOOST_REQUIRE( not already_produced );
            already_produced = true;
          }
          block_num = block_header->block_num();
          ilog( "Block #${n}, ts: ${t}, witness: ${w}", ( "n", block_num )
            ( "t", block_header->timestamp )( "w", block_header->witness ) );
          BOOST_REQUIRE( block_header->timestamp == next_block_time );
          next_block_time += HIVE_BLOCK_INTERVAL;
          should_produce_next = db->get_scheduled_witness( 1 ) == "with";
          fc::sleep_until( next_block_time );
        }
        BOOST_REQUIRE( already_produced );

        ilog( "'API' thread finished" );
        test_passed = true;
      }
      CATCH( "API" )
    } );

    theApp.wait4interrupt_request();
    theApp.quit( true );
    db = nullptr; // prevent fixture destructor from accessing database after it was closed
    BOOST_REQUIRE( test_passed );
    ilog( "Test done" );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( block_conflict_test )
{
  using namespace hive::plugins::witness;
  // ABW: in this test we are trying to emulate situation when there is already block waiting
  // in writer queue to be processed, but it arrived so late that witness plugin already
  // requested to produce its alternative; preferably we want the external block to be processed
  // before witness request; we are achieving it by putting really slow transaction inside that
  // block which emulates writer queue congestion

  try
  {
    initialize( -HIVE_MAX_WITNESSES * 2 * HIVE_BLOCK_INTERVAL,
      { "wit1", "wit2", "wit3", "wit4", "wit5", "wit6", "wit7", "wit8", "wit9", "wita",
      "witb", "witc", "witd", "wite", "witf", "witg", "with", "witi", "witj", "witk" },
      { "with" }
    ); // represent any witness (but not 'initminer' to prevent accidental production at the start)
    bool test_passed = false;
    fc::logger::get( "user" ).set_log_level( fc::log_level::info ); // suppress fast confirm broadcast messages

    // produce first two schedules of blocks (initminer) so we can get to actual test
    // note that genesis time was set in the past so we don't have to wait
    schedule_blocks( HIVE_MAX_WITNESSES * 2 );
    BOOST_REQUIRE( db->has_hardfork( HIVE_NUM_HARDFORKS ) );
    db->_log_hardforks = true;
    // set to represent third witness from new schedule
    std::string chosen_witness = db->get_scheduled_witness( 3 );
    ilog( "Chosen witness is ${w}", ( "w", chosen_witness ) );
    get_witness_plugin().set_witnesses( { chosen_witness } );

    fc::thread api_thread;
    api_thread.async( [&]()
    {
      BOOST_SCOPE_EXIT( this_ ) { this_->theApp.generate_interrupt_request(); } BOOST_SCOPE_EXIT_END
      try
      {
        ilog( "Starting test thread" );
        const signed_block_header* block_header = nullptr;
        uint32_t block_num = HIVE_MAX_WITNESSES * 2;
        BOOST_REQUIRE_EQUAL( block_num, db->head_block_num() );
        fc::time_point_sec next_block_time = get_genesis_time() + ( block_num + 1 ) * HIVE_BLOCK_INTERVAL;
        fc::sleep_until( next_block_time );

        // first block of third schedule
        {
          block_header = &get_block_reader().head_block()->get_block_header();
          BOOST_REQUIRE_EQUAL( block_num, block_header->block_num() );
          ilog( "Supplementing block with debug plugin" );
          schedule_block();
          block_header = &get_block_reader().head_block()->get_block_header();
          block_num = block_header->block_num();
          ilog( "Block #${n}, ts: ${t}, witness: ${w}", ( "n", block_num )
            ( "t", block_header->timestamp )( "w", block_header->witness ) );
          next_block_time += HIVE_BLOCK_INTERVAL;
        }
        fc::sleep_until( next_block_time );
        // second block of third schedule - the slow block
        {
          block_header = &get_block_reader().head_block()->get_block_header();
          BOOST_REQUIRE_EQUAL( block_num, block_header->block_num() );
          ilog( "Time to produce slow block" );
          // we are first waiting 0.5s and then we generate marker transaction that takes 1s
          // to process; it means that witness should notice the delay in block N and prepare
          // to take over in case it reaches next timestamp with block N still missing;
          // in the meantime we make the slow transaction (ends 1.5s before next timestamp)
          // and create a block N (production repeats transaction that ends 0.5s before next
          // timestamp, and starts applying that block which will end 0.5s after next
          // timestamp); since witness will request to produce block N alternative 0.4s before
          // next timestamp, we'll have the request of new block waiting for the slow block to
          // finish processing; only after slow transaction is processed for the third time
          // during block reapplication, the block number changes, which would be indication
          // for witness to produce block N+1, but the witness already made its request to
          // produce alternative for block N; fortunately none of the properties of the block
          // are carried by block flow control, so by the time the alternative for block N starts
          // being produced, the actual block N is already visible in state, so in reality block
          // N+1 is going to be produced; if we didn't know that we are at the start of schedule
          // there would be a chance for that to happen on schedule switch, in which case it would
          // be likely that FC_ASSERT( scheduled_witness == witness_owner ); fired, resulting in
          // the block production failure
          fc::usleep( fc::milliseconds( 500 ) );
          ilog( "Adding slow transaction to pending" );
          db_plugin->debug_update( [&]( database& db )
          {
            ilog( "Slow transaction start - tx status is ${s}", ( "s", (int)db.get_tx_status() ) );
            fc::usleep( fc::seconds( 1 ) );
            ilog( "Slow transaction end" );
          } );
          // while it is not exactly the thing we want, since there will be two competing
          // block generations in the queue, it achieves desired result - alternative would
          // require us to have full separate database, in other words, separate node
          ilog( "Producing and reapplying slow block" );
          schedule_block();
          // ABW: there is a very small but nonzero chance that the read below is going to happen
          // after block N+1 is produced by chosen_witness, and the test will fail as a result;
          // will need to be addressed if that actually happens
          block_header = &get_block_reader().head_block()->get_block_header();
          block_num = block_header->block_num();
          ilog( "Block #${n}, ts: ${t}, witness: ${w}", ( "n", block_num )
            ( "t", block_header->timestamp )( "w", block_header->witness ) );
        }
        ilog( "Performing end checks" );
        // the slow block should "win" over witness block
        BOOST_REQUIRE_NE( block_header->witness, chosen_witness );
        // wait for third block of third schedule
        fc::usleep( fc::seconds( 1 ) );
        block_header = &get_block_reader().head_block()->get_block_header();
        // the block should be normal next block, even though it was requested as alternative for missed
        BOOST_REQUIRE_EQUAL( block_header->block_num(), block_num + 1 );
        BOOST_REQUIRE_EQUAL( block_header->witness, chosen_witness );

        ilog( "'API' thread finished" );
        test_passed = true;
      }
      CATCH( "API" )
    } );

    theApp.wait4interrupt_request();
    theApp.quit( true );
    db = nullptr; // prevent fixture destructor from accessing database after it was closed
    BOOST_REQUIRE( test_passed );
    ilog( "Test done" );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( block_lock_test )
{
  using namespace hive::plugins::witness;
  // ABW: same as block_conflict_test, except this test specifically shows how use of read lock
  // in case of missing block in queue congestion environment can lead to witness losing its
  // block

  try
  {
    initialize( -HIVE_MAX_WITNESSES * 2 * HIVE_BLOCK_INTERVAL,
      { "wit1", "wit2", "wit3", "wit4", "wit5", "wit6", "wit7", "wit8", "wit9", "wita",
      "witb", "witc", "witd", "wite", "witf", "witg", "with", "witi", "witj", "witk" },
      { "with" }
    ); // representing any witness (but not 'initminer' to prevent accidental production at the start)
    bool test_passed = false;
    fc::logger::get( "user" ).set_log_level( fc::log_level::info ); // suppress fast confirm broadcast messages

    // produce first two schedules of blocks (initminer) so we can get to actual test
    // note that genesis time was set in the past so we don't have to wait
    schedule_blocks( HIVE_MAX_WITNESSES * 2 );
    BOOST_REQUIRE( db->has_hardfork( HIVE_NUM_HARDFORKS ) );
    db->_log_hardforks = true;
    // set to represent third witness from new schedule
    std::string chosen_witness = db->get_scheduled_witness( 3 );
    ilog( "Chosen witness is ${w}", ( "w", chosen_witness ) );
    get_witness_plugin().set_witnesses( { chosen_witness } );

    fc::thread api_thread;
    api_thread.async( [&]()
    {
      BOOST_SCOPE_EXIT( this_ ) { this_->theApp.generate_interrupt_request(); } BOOST_SCOPE_EXIT_END
      try
      {
        ilog( "Starting test thread" );
        const signed_block_header* block_header = nullptr;
        uint32_t block_num = HIVE_MAX_WITNESSES * 2;
        BOOST_REQUIRE_EQUAL( block_num, db->head_block_num() );
        fc::time_point_sec next_block_time = get_genesis_time() + ( block_num + 1 ) * HIVE_BLOCK_INTERVAL;
        fc::sleep_until( next_block_time );

        // first block of third schedule
        {
          block_header = &get_block_reader().head_block()->get_block_header();
          BOOST_REQUIRE_EQUAL( block_num, block_header->block_num() );
          ilog( "Supplementing block with debug plugin" );
          schedule_block();
          block_header = &get_block_reader().head_block()->get_block_header();
          block_num = block_header->block_num();
          ilog( "Block #${n}, ts: ${t}, witness: ${w}", ( "n", block_num )
            ( "t", block_header->timestamp )( "w", block_header->witness ) );
          next_block_time += HIVE_BLOCK_INTERVAL;
          ilog( "Adding slow transaction to pending" );
          // generate marker transaction that takes 1.5s to process
          // it will be waiting as pending for next block to pick it up
          db_plugin->debug_update( [&]( database& db )
          {
            ilog( "Slow transaction start - tx status is ${s}", ( "s", (int)db.get_tx_status() ) );
            fc::usleep( fc::milliseconds( 1500 ) );
            ilog( "Slow transaction end" );
          } );
        }
        fc::sleep_until( next_block_time );
        // second block of third schedule - the slow block
        {
          block_header = &get_block_reader().head_block()->get_block_header();
          BOOST_REQUIRE_EQUAL( block_num, block_header->block_num() );
          // we should already have pending marker transaction that takes 1.5s to process;
          // shortly after we start producing slow block, witness should notice the delay in
          // block N and try to prepare to take over in case it reaches next timestamp with
          // block N still missing; it won't be able to acquire lock though, since block will
          // take long time to process; the slow block should end just at the next timestamp,
          // so if witness works correctly, it should still manage to produce its block
          ilog( "Producing and reapplying slow block" );
          schedule_block();
          // ABW: there is a very small but nonzero chance that the read below is going to happen
          // after block N+1 is produced by chosen_witness, and the test will fail as a result;
          // will need to be addressed if that actually happens
          block_header = &get_block_reader().head_block()->get_block_header();
          block_num = block_header->block_num();
          ilog( "Block #${n}, ts: ${t}, witness: ${w}", ( "n", block_num )
            ( "t", block_header->timestamp )( "w", block_header->witness ) );
        }
        ilog( "Performing end checks" );
        // the slow block should "win" over witness block
        BOOST_REQUIRE_NE( block_header->witness, chosen_witness );
        // wait for third block of third schedule
        fc::usleep( fc::seconds( 1 ) );
        block_header = &get_block_reader().head_block()->get_block_header();
        // chosen_witness should manage to produce its block
        BOOST_REQUIRE_EQUAL( block_header->block_num(), block_num + 1 );
        BOOST_REQUIRE_EQUAL( block_header->witness, chosen_witness );

        ilog( "'API' thread finished" );
        test_passed = true;
      }
      CATCH( "API" )
    } );

    theApp.wait4interrupt_request();
    theApp.quit( true );
    db = nullptr; // prevent fixture destructor from accessing database after it was closed
    BOOST_REQUIRE( test_passed );
    ilog( "Test done" );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( block_lag_test )
{
  using namespace hive::plugins::witness;
  // ABW: same as block_lock_test, but this test emulates situation where processing of block
  // previous to the one witness wanted to produce takes a lot of time (we can't achieve that
  // actually, since we can only slow down marker transaction, however it makes no difference,
  // since witness can't ask for state data while the block is being processed and it does
  // not matter if it can't ask before or after block number changes)

  try
  {
    initialize( -HIVE_MAX_WITNESSES * 2 * HIVE_BLOCK_INTERVAL,
      { "wit1", "wit2", "wit3", "wit4", "wit5", "wit6", "wit7", "wit8", "wit9", "wita",
      "witb", "witc", "witd", "wite", "witf", "witg", "with", "witi", "witj", "witk" },
      { "with" }
    ); // represent any witness (but not 'initminer' to prevent accidental production at the start)
    bool test_passed = false;
    fc::logger::get( "user" ).set_log_level( fc::log_level::info ); // suppress fast confirm broadcast messages

    // produce first two schedules of blocks (initminer) so we can get to actual test
    // note that genesis time was set in the past so we don't have to wait
    schedule_blocks( HIVE_MAX_WITNESSES * 2 );
    BOOST_REQUIRE( db->has_hardfork( HIVE_NUM_HARDFORKS ) );
    db->_log_hardforks = true;
    // set to represent third witness from new schedule
    std::string chosen_witness = db->get_scheduled_witness( 3 );
    ilog( "Chosen witness is ${w}", ( "w", chosen_witness ) );
    get_witness_plugin().set_witnesses( { chosen_witness } );

    fc::thread api_thread;
    api_thread.async( [&]()
    {
      BOOST_SCOPE_EXIT( this_ ) { this_->theApp.generate_interrupt_request(); } BOOST_SCOPE_EXIT_END
      try
      {
        ilog( "Starting test thread" );
        const signed_block_header* block_header = nullptr;
        uint32_t block_num = HIVE_MAX_WITNESSES * 2;
        BOOST_REQUIRE_EQUAL( block_num, db->head_block_num() );
        fc::time_point_sec next_block_time = get_genesis_time() + ( block_num + 1 ) * HIVE_BLOCK_INTERVAL;
        fc::sleep_until( next_block_time );

        // first block of third schedule
        {
          block_header = &get_block_reader().head_block()->get_block_header();
          BOOST_REQUIRE_EQUAL( block_num, block_header->block_num() );
          ilog( "Supplementing block with debug plugin" );
          schedule_block();
          block_header = &get_block_reader().head_block()->get_block_header();
          block_num = block_header->block_num();
          ilog( "Block #${n}, ts: ${t}, witness: ${w}", ( "n", block_num )
            ( "t", block_header->timestamp )( "w", block_header->witness ) );
          next_block_time += HIVE_BLOCK_INTERVAL;
          ilog( "Adding marker transaction to pending" );
          // generate marker transaction that takes 10s to process during block reapplication;
          // it will be waiting as pending for next block to pick it up
          db_plugin->debug_update( [&]( database& db )
          {
            if( db.get_tx_status() == database::TX_STATUS_P2P_BLOCK )
            {
              ilog( "Slow processing start" );
              fc::usleep( fc::seconds( 10 ) );
              ilog( "Slow processing end" );
            }
          } );
        }
        fc::sleep_until( next_block_time );
        // second block of third schedule - the slow block
        {
          block_header = &get_block_reader().head_block()->get_block_header();
          BOOST_REQUIRE_EQUAL( block_num, block_header->block_num() );
          // we should already have pending marker transaction;
          // shortly after we start producing slow block, witness should notice the delay in
          // block N and try to prepare to take over in case it reaches next timestamp with
          // block N still missing; it won't be able to acquire lock for a long time, so
          // its turn will pass while it was waiting; it should notice that in maybe_produce_block
          // call next to the one when it detected lag for the first time; there should be 3 missed
          // blocks - the one that chosen_witness were to produce and two next
          ilog( "Producing and reapplying slow block" );
          // in order to be able to see which block (timestamp) is when blocks will start
          // to be produced again, we are setting witness plugin to represent all witnesses
          // from this point
          get_witness_plugin().set_witnesses( { "initminer", "wit1", "wit2", "wit3", "wit4",
            "wit5", "wit6", "wit7", "wit8", "wit9", "wita", "witb", "witc", "witd", "wite",
            "witf", "witg", "with", "witi", "witj", "witk" } );
          schedule_block();
          // ABW: there is a very small but nonzero chance that the read below is going to happen
          // after block N+1 is produced by chosen_witness, and the test will fail as a result;
          // will need to be addressed if that actually happens
          block_header = &get_block_reader().head_block()->get_block_header();
          block_num = block_header->block_num();
          ilog( "Block #${n}, ts: ${t}, witness: ${w}", ( "n", block_num )
            ( "t", block_header->timestamp )( "w", block_header->witness ) );
        }
        ilog( "Performing end checks" );
        // block_num and next_block_time is a number and ts of slow block
        BOOST_REQUIRE_NE( block_header->witness, chosen_witness );
        fc::usleep( fc::seconds( HIVE_BLOCK_INTERVAL ) );
        // we should now have a first block after slow one, which should be 4 intervals further
        // (3 missing blocks)
        block_header = &get_block_reader().head_block()->get_block_header();
        BOOST_REQUIRE_EQUAL( block_header->block_num(), block_num + 1 );
        BOOST_REQUIRE( block_header->timestamp == next_block_time + 4 * HIVE_BLOCK_INTERVAL );

        ilog( "'API' thread finished" );
        test_passed = true;
      }
      CATCH( "API" )
    } );

    theApp.wait4interrupt_request();
    theApp.quit( true );
    db = nullptr; // prevent fixture destructor from accessing database after it was closed
    BOOST_REQUIRE( test_passed );
    ilog( "Test done" );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()
#endif
