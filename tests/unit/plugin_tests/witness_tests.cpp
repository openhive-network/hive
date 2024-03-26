#ifdef IS_TEST_NET
#include <boost/test/unit_test.hpp>

#include <hive/chain/comment_object.hpp>
#include <hive/chain/full_transaction.hpp>

#include <hive/protocol/hive_operations.hpp>
#include <hive/protocol/transaction_util.hpp>

#include <hive/plugins/chain/chain_plugin.hpp>
#include <hive/plugins/witness/witness_plugin.hpp>
#include <hive/plugins/colony/colony_plugin.hpp>

#include <boost/scope_exit.hpp>

#include "../db_fixture/hived_fixture.hpp"

using namespace hive::chain;
using namespace hive::protocol;

struct witness_fixture : public hived_fixture
{
  witness_fixture() : hived_fixture( true, false ) {}
  virtual ~witness_fixture() {}

  void initialize( const config_arg_override_t& extra_config_arg_overrides = config_arg_override_t(),
    const std::string& shared_file_size = "1G" )
  {
    theApp.init_signals_handler();

    configuration_data.set_initial_asset_supply(
      200'000'000'000ul, 1'000'000'000ul, 100'000'000'000ul,
      price( VEST_asset( 1'800 ), HIVE_asset( 1'000 ) )
    );
    genesis_time = fc::time_point::now() + fc::seconds( 1 );
    // genesis slightly in the future
    configuration_data.set_hardfork_schedule( genesis_time, { { HIVE_NUM_HARDFORKS, 1 } } );

    config_arg_override_t config_arg_overrides = {
      config_line_t( { "plugin", { HIVE_WITNESS_PLUGIN_NAME } } ),
      config_line_t( { "shared-file-size", { shared_file_size } } ),
      config_line_t( { "witness", { "\"initminer\"" } } ),
      config_line_t( { "private-key", { init_account_priv_key.key_to_wif() } } )
    };
    config_arg_overrides.insert( config_arg_overrides.end(),
      extra_config_arg_overrides.begin(), extra_config_arg_overrides.end() );
    postponed_init( config_arg_overrides );

    init_account_pub_key = init_account_priv_key.get_public_key();
  }

  uint32_t get_block_num() const
  {
    uint32_t num = 0;
    db->with_read_lock( [&]() { num = db->head_block_num(); } );
    return num;
  }

  template< typename ACTION >
  uint32_t wait_for_block_change( uint32_t block_num, ACTION&& action )
  {
    bool stop = false;
    do
    {
      fc::usleep( fc::seconds( 1 ) );
      db->with_read_lock( [&]()
      {
        uint32_t new_block = db->head_block_num();
        if( new_block > block_num )
        {
          block_num = new_block;
          action();
          stop = true;
        }
      } );
    }
    while( !stop );
    return block_num;
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
    schedule_transaction( tx );
  }

  void schedule_transaction( const signed_transaction& tx ) const
  {
    full_transaction_ptr _tx = full_transaction_type::create_from_signed_transaction( tx, serialization_type::hf26, false );
    _tx->sign_transaction( { init_account_priv_key }, db->get_chain_id(), fc::ecc::fc_canonical, serialization_type::hf26 );
    get_chain_plugin().accept_transaction( _tx, hive::plugins::chain::chain_plugin::lock_type::fc );
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

  fc::time_point_sec get_genesis_time() const { return genesis_time; }

private:
  fc::time_point_sec genesis_time;
};

BOOST_FIXTURE_TEST_SUITE( witness_tests, witness_fixture )

#define CATCH( thread_name )                                                  \
catch( const fc::exception& ex )                                              \
{                                                                             \
  elog( "Unhandled fc::exception thrown from '" thread_name "' thread: ${r}", \
    ( "r", ex.to_detail_string() ) );                                         \
  BOOST_CHECK( false && "Unhandled fc::exception" );                          \
}                                                                             \
catch( ... )                                                                  \
{                                                                             \
  elog( "Unhandled exception thrown from '" thread_name "' thread" );         \
  BOOST_CHECK( false && "Unhandled exception" );                              \
}

BOOST_AUTO_TEST_CASE( witness_basic_test )
{
  try
  {
    initialize();

    fc::thread api_thread;
    api_thread.async( [&]()
    { 
      BOOST_SCOPE_EXIT( this_ ) { this_->theApp.kill( true ); } BOOST_SCOPE_EXIT_END
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
      }
      CATCH( "API" )
    } );

    theApp.wait();
    ilog( "Test done" );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( multiple_feeding_threads_test )
{
  try
  {
    configuration_data.min_root_comment_interval = fc::seconds( 3 * HIVE_BLOCK_INTERVAL );
    initialize();

    enum { ALICE, BOB, CAROL, DAN, FEEDER_COUNT };
    std::atomic<uint32_t> active_feeders( 0 );
    fc::thread sync_thread;
    sync_thread.async( [&]()
    {
      BOOST_SCOPE_EXIT( this_ ) { this_->theApp.kill( true ); } BOOST_SCOPE_EXIT_END
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
      }
      CATCH( "DAN" )
      --active_feeders;
      ilog( "'DAN' thread finished" );
    } );

    theApp.wait();
    ilog( "Test done" );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( colony_basic_test )
{
  try
  {
    configuration_data.min_root_comment_interval = fc::seconds( 3 );
    const uint32_t COLONY_START = 16;

    initialize( {
      config_line_t( { "plugin", { HIVE_COLONY_PLUGIN_NAME } } ),
      config_line_t( { "colony-sign-with", { init_account_priv_key.key_to_wif() } } ),
      config_line_t( { "colony-start-at-block", { std::to_string( COLONY_START ) } } ),
      config_line_t( { "colony-transactions-per-block", { "5000" } } ),
      config_line_t( { "colony-no-broadcast", { "1" } } ),
      config_line_t( { "colony-article", { R"~({"min":100,"max":5000,"weight":16,"exponent":4})~" } } ),
      config_line_t( { "colony-reply", { R"~({"min":30,"max":1000,"weight":110,"exponent":5})~" } } ),
      config_line_t( { "colony-vote", { R"~({"weight":2070})~" } } ),
      config_line_t( { "colony-transfer", { R"~({"min":0,"max":350,"weight":87,"exponent":4})~" } } ),
      config_line_t( { "colony-custom", { R"~({"min":20,"max":400,"weight":6006,"exponent":1})~" } } )
    }, "8G" );

    fc::thread api_thread;
    api_thread.async( [&]()
    {
      BOOST_SCOPE_EXIT( this_ ) { this_->theApp.kill( true ); } BOOST_SCOPE_EXIT_END
      try
      {
        ilog( "Wait for first block after genesis" );
        fc::sleep_until( get_genesis_time() + HIVE_BLOCK_INTERVAL );
        ilog( "All hardforks should have been applied" );
        BOOST_REQUIRE( db->has_hardfork( HIVE_NUM_HARDFORKS ) );
        db->_log_hardforks = true;

        ilog( "Starting 'API' thread that will be sending transactions" );

        // now we have up to block 'colony_start' to add users and some initial comments
        const int ACCOUNTS = 20000;
        signed_transaction tx; // building multiop transaction to speed up the process
        uint32_t block_num = 0;
        db->with_read_lock( [&]()
        {
          tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
          tx.set_reference_block( db->head_block_id() );
          block_num = db->head_block_num();
        } );

        // start with setting block size to 2MB (otherwise it will change to default 128kB on first schedule)
        {
          witness_set_properties_operation witness_props;
          witness_props.owner = "initminer";
          witness_props.props[ "key" ] = fc::raw::pack_to_vector( init_account_pub_key );
          witness_props.props[ "maximum_block_size" ] = fc::raw::pack_to_vector( HIVE_MAX_BLOCK_SIZE );
          tx.operations.emplace_back( witness_props );
          schedule_transaction( tx );
          tx.clear();
        }

        for( size_t i = 0; i < ACCOUNTS; ++i )
        {
          auto account_name = "account" + std::to_string( i );

          account_create_operation create;
          create.new_account_name = account_name;
          create.creator = HIVE_INIT_MINER_NAME;
          create.fee = db->get_witness_schedule_object().median_props.account_creation_fee;
          create.owner = authority( 1, init_account_pub_key, 1 );
          create.active = create.owner;
          create.posting = create.owner;
          create.memo_key = init_account_pub_key;
          tx.operations.emplace_back( create );

          transfer_to_vesting_operation vest;
          vest.from = HIVE_INIT_MINER_NAME;
          vest.to = account_name;
          vest.amount = ASSET( "1000.000 TESTS" );
          tx.operations.emplace_back( vest );

          transfer_operation fund;
          fund.from = HIVE_INIT_MINER_NAME;
          fund.to = account_name;
          fund.amount = ASSET( "10.000 TBD" );
          tx.operations.emplace_back( fund );

          schedule_transaction( tx );
          tx.clear();

          comment_operation comment;
          comment.parent_permlink = "hello";
          comment.author = account_name;
          comment.permlink = "hello";
          comment.title = "hello";
          comment.body = "Hi. I'm " + std::string( account_name );
          tx.operations.emplace_back( comment );
          schedule_transaction( comment );
          tx.clear();

          if( i % 2000 == 0 )
          {
            ilog( "Adding users... ${i}", ( i ) );
            block_num = wait_for_block_change( block_num, [](){} );
          }
        }

        block_num = get_block_num();
        BOOST_REQUIRE_LT( block_num, COLONY_START ); // check that we've managed to prepare before activation of colony
        ilog( "Sleeping until block #${b}", ( "b", COLONY_START ) );
        fc::sleep_until( get_genesis_time() + COLONY_START * HIVE_BLOCK_INTERVAL );
        block_num = get_block_num();

        const auto& block_reader = get_chain_plugin().block_reader();

        const uint32_t FULL_RATE_BLOCKS = ( COLONY_START + 20 + 10 ) / 21 * 21 - 3;
          // there needs to be 2 blocks of margin before next schedule
        uint32_t start = block_num + 5; // 5 blocks of margin
        do
        {
          block_num = wait_for_block_change( block_num, [&]()
          {
            auto block = block_reader.head_block();
            uint32_t tx_count = block->get_full_transactions().size();
            ilog( "Tx count for block #${b} is ${tx_count}", ( "b", block->get_block_num() )( tx_count ) );
            if( block->get_block_num() >= start )
            {
              BOOST_CHECK_LT( tx_count, 5200 );
              BOOST_CHECK_GT( tx_count, 4800 );
            }
          } );
        }
        while( block_num < FULL_RATE_BLOCKS );

        // reduce block size to 1MB so colony is forced to adjust production rate
        {
          witness_set_properties_operation witness_props;
          witness_props.owner = "initminer";
          witness_props.props[ "key" ] = fc::raw::pack_to_vector( init_account_pub_key );
          witness_props.props[ "maximum_block_size" ] = fc::raw::pack_to_vector( HIVE_MAX_BLOCK_SIZE / 2 );
          tx.operations.emplace_back( witness_props );
          schedule_transaction( tx );
          tx.clear();
          ilog( "Changing block size to 1MB - colony production rate should be reduced" );
        }

        // rate adjustment should happen next block after start of schedule (should be future
        // schedule, but there is currently a bug where such change activates when future schedule
        // is formed, not when it is activated) - increase number of blocks produced accordingly
        // in case that bug is fixed;
        // the question is, how do we actually test it? it shows in block stats, but that's it;
        // above all 5000 produced transactions should fit in each block, after change in block
        // size first block should have many pending transactions to apply, however next and
        // further blocks there should be just around 200 pending transactions after each block
        // despite only ~3800 transactions fitting in smaller blocks
        const uint32_t REDUCED_RATE_BLOCKS = ( FULL_RATE_BLOCKS + 3 + 20 + 10 ) / 21 * 21 - 3;
          // there needs to be 2 blocks of margin before next schedule
        start = block_num + 5; // 5 blocks of margin
        do
        {
          block_num = wait_for_block_change( block_num, [&]()
          {
            auto block = block_reader.head_block();
            uint32_t tx_count = block->get_full_transactions().size();
            ilog( "Tx count for block #${b} is ${tx_count}", ( "b", block->get_block_num() )( tx_count ) );
            if( block->get_block_num() >= start )
            {
              BOOST_CHECK_LT( tx_count, 4000 );
              BOOST_CHECK_GT( tx_count, 3600 );
            }
          } );
        }
        while( block_num < REDUCED_RATE_BLOCKS );

        // reduce block size to 64kB so colony is forced to drastically adjust production rate,
        // even stop producing for a while; block stats should show a lot of pending transactions
        // and no new incoming until almost all the pending are exhausted
        {
          witness_set_properties_operation witness_props;
          witness_props.owner = "initminer";
          witness_props.props[ "key" ] = fc::raw::pack_to_vector( init_account_pub_key );
          witness_props.props[ "maximum_block_size" ] = fc::raw::pack_to_vector( HIVE_MIN_BLOCK_SIZE_LIMIT );
          tx.operations.emplace_back( witness_props );
          schedule_transaction( tx );
          tx.clear();
          ilog( "Changing block size to 64kB - colony production rate should be drastically reduced" );
        }

        const uint32_t MINIMAL_RATE_BLOCKS = ( REDUCED_RATE_BLOCKS + 3 + 20 + 10 ) / 21 * 21 - 3;
          // there needs to be 2 blocks of margin before next schedule
        start = block_num + 5; // 5 blocks of margin
        do
        {
          block_num = wait_for_block_change( block_num, [&]()
          {
            auto block = block_reader.head_block();
            uint32_t tx_count = block->get_full_transactions().size();
            ilog( "Tx count for block #${b} is ${tx_count}", ( "b", block->get_block_num() )( tx_count ) );
            if( block->get_block_num() >= start )
            {
              BOOST_CHECK_LT( tx_count, 300 );
              BOOST_CHECK_GT( tx_count, 200 );
            }
          } );
        }
        while( block_num < MINIMAL_RATE_BLOCKS );

        // increase block size back to 2MB, so colony can ramp up production again
        {
          witness_set_properties_operation witness_props;
          witness_props.owner = "initminer";
          witness_props.props[ "key" ] = fc::raw::pack_to_vector( init_account_pub_key );
          witness_props.props[ "maximum_block_size" ] = fc::raw::pack_to_vector( HIVE_MAX_BLOCK_SIZE );
          tx.operations.emplace_back( witness_props );
          // we need to change at least expiration time, or this transaction would be a duplicate
          // of the one we made previously when setting 2MB blocks
          tx.set_expiration( tx.expiration + HIVE_BLOCK_INTERVAL );
          schedule_transaction( tx );
          tx.clear();
          ilog( "Changing block size back to 2MB - colony production rate should return to full" );
        }

        const uint32_t FULL2_RATE_BLOCKS = ( MINIMAL_RATE_BLOCKS + 3 + 20 + 10 ) / 21 * 21;
        start = block_num + 5; // 5 blocks of margin
        do
        {
          block_num = wait_for_block_change( block_num, [&]()
          {
            auto block = block_reader.head_block();
            uint32_t tx_count = block->get_full_transactions().size();
            ilog( "Tx count for block #${b} is ${tx_count}", ( "b", block->get_block_num() )( tx_count ) );
            if( block->get_block_num() >= start )
            {
              BOOST_CHECK_LT( tx_count, 5200 );
              BOOST_CHECK_GT( tx_count, 4800 );
            }
          } );
        }
        while( block_num < FULL2_RATE_BLOCKS );

        ilog( "'API' thread finished" );
      }
      CATCH( "API" )
    } );

    theApp.wait();
    ilog( "Test done" );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()
#endif
