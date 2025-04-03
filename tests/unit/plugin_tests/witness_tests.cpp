#ifdef IS_TEST_NET
#include <boost/test/unit_test.hpp>

#include <hive/chain/comment_object.hpp>
#include <hive/chain/full_transaction.hpp>

#include <hive/utilities/signal.hpp>

#include <hive/protocol/hive_operations.hpp>
#include <hive/protocol/transaction_util.hpp>

#include <hive/plugins/chain/chain_plugin.hpp>
#include <hive/plugins/witness/witness_plugin.hpp>
#include <hive/plugins/colony/colony_plugin.hpp>
#include <hive/plugins/queen/queen_plugin.hpp>

#include <fc/log/appender.hpp>

#include <boost/scope_exit.hpp>

#include "../db_fixture/hived_fixture.hpp"

using namespace hive::chain;
using namespace hive::protocol;

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

struct witness_fixture : public hived_fixture
{
  witness_fixture( bool remove_db = true ) : hived_fixture( remove_db, false ) {}
  virtual ~witness_fixture() { configuration_data = configuration(); }

  void initialize( int genesis_delay = 1, // genesis slightly in the future (or past with negative values)
    const std::vector< std::string > initial_witnesses = {}, // initial witnesses over 'initminer'
    const std::vector< std::string > represented_witnesses = { "initminer" }, // which witnesses can produce
    const config_arg_override_t& extra_config_args = config_arg_override_t(),
    const std::string& shared_file_size = "1G" )
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
    configuration_data.set_init_witnesses( initial_witnesses );

    std::string _p2p_parameters_value =
    R"({
      "listen_endpoint": "0.0.0.0:0",
      "accept_incoming_connections": false,
      "wait_if_endpoint_is_busy": true,
      "private_key": "0000000000000000000000000000000000000000000000000000000000000000",
      "desired_number_of_connections": 20,
      "maximum_number_of_connections": 200,
      "peer_connection_retry_timeout": 30,
      "peer_inactivity_timeout": 5,
      "peer_advertising_disabled": false,
      "maximum_number_of_blocks_to_handle_at_one_time": 200,
      "maximum_number_of_sync_blocks_to_prefetch": 20000,
      "maximum_blocks_per_peer_during_syncing": 200,"active_ignored_request_timeout_microseconds":6000000
    }
    )";

    config_arg_override_t config_args = {
      config_line_t( { "shared-file-size", { shared_file_size } } ),
      config_line_t( { "private-key", { init_account_priv_key.key_to_wif() } } ),
      config_line_t( { "p2p-parameters", { _p2p_parameters_value } } )
    };
    for( auto& name : represented_witnesses )
      config_args.emplace_back( config_line_t( "witness", { "\"" + name + "\"" } ) );
    config_args.insert( config_args.end(), extra_config_args.begin(), extra_config_args.end() );

    postponed_init( config_args );

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
    while( !stop && !theApp.is_interrupt_request() );
    return block_num;
  }

  full_transaction_ptr build_transaction( const signed_transaction& tx ) const
  {
    full_transaction_ptr _tx = full_transaction_type::create_from_signed_transaction( tx, serialization_type::hf26, false );
    _tx->sign_transaction( { init_account_priv_key }, db->get_chain_id(), serialization_type::hf26 );
    return _tx;
  }

  full_transaction_ptr build_transaction( const operation& op, size_t expiration ) const
  {
    signed_transaction tx;
    db->with_read_lock( [&]()
    {
      tx.set_expiration( db->head_block_time() + expiration );
      tx.set_reference_block( db->head_block_id() );
    } );
    tx.operations.emplace_back( op );
    return build_transaction( tx );
  }

  void schedule_transaction( const full_transaction_ptr& tx ) const
  {
    if( theApp.is_interrupt_request() )
      return;
    get_chain_plugin().accept_transaction( tx, hive::plugins::chain::chain_plugin::lock_type::fc );
  }

  void schedule_transaction( const signed_transaction& tx ) const
  {
    if( theApp.is_interrupt_request() )
      return;
    schedule_transaction( build_transaction( tx ) );
  }

  void schedule_transaction( const operation& op ) const
  {
    if( theApp.is_interrupt_request() )
      return;
    schedule_transaction( build_transaction( op, default_expiration ) );
  }

  void schedule_transaction( const operation& op, size_t expiration ) const
  {
    if( theApp.is_interrupt_request() )
      return;
    schedule_transaction( build_transaction( op, expiration ) );
  }

  void schedule_blocks( uint32_t count ) const
  {
    if( theApp.is_interrupt_request() )
      return;
    db_plugin->debug_generate_blocks( init_account_priv_key, count, default_skip, 0, false );
  }

  void schedule_block() const
  {
    if( theApp.is_interrupt_request() )
      return;
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

  full_transaction_ptr build_transfer( const account_name_type& from, const account_name_type& to,
    const asset& amount, const std::string& memo, size_t expiration ) const
  {
    transfer_operation transfer;
    transfer.from = from;
    transfer.to = to;
    transfer.amount = amount;
    transfer.memo = memo;
    return build_transaction( transfer, expiration );
  }

  void schedule_transfer( const account_name_type& from, const account_name_type& to,
    const asset& amount, const std::string& memo ) const
  {
    schedule_transaction( build_transfer( from, to, amount, memo, default_expiration ) );
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

  void set_default_expiration( size_t expiration ) { default_expiration = expiration; }
  size_t get_default_expiration() const { return default_expiration; }

private:
  fc::time_point_sec genesis_time;
  size_t default_expiration = HIVE_MAX_TIME_UNTIL_EXPIRATION;

public:

  void witness_basic()
  {
    BOOST_SCOPE_EXIT( this_ )
    {
      this_->theApp.generate_interrupt_request();
      this_->theApp.wait4interrupt_request();
      this_->theApp.quit( true );
    } BOOST_SCOPE_EXIT_END

    initialize();
    bool test_passed = false;

    fc::thread api_thread;
    auto _future = api_thread.async( [&]()
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

        HIVE_REQUIRE_ASSERT( schedule_account_create( "no one" ), "validity_check_result != account_name_validity::invalid_sequence" ); // invalid name

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
        test_passed = !theApp.is_interrupt_request();
      }
      CATCH( "API" )
    } );

    _future.wait();
    theApp.wait4interrupt_request();
    theApp.quit( true );
    db = nullptr; // prevent fixture destructor from accessing database after it was closed
    BOOST_REQUIRE( test_passed );
    ilog( "Test done" );
  }
};

struct restart_witness_fixture : public witness_fixture
{
  restart_witness_fixture() : witness_fixture( false ) {}
  virtual ~restart_witness_fixture() {}
};

BOOST_FIXTURE_TEST_SUITE( witness_tests, witness_fixture )

BOOST_AUTO_TEST_CASE( witness_basic_test )
{
  try
  {
    witness_basic();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( witness_basic_with_runtime_expiration_00_test )
{
  try
  {
    set_default_expiration( HIVE_MAX_TIME_UNTIL_SIGNATURE_EXPIRATION );
    witness_basic();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( witness_basic_with_runtime_expiration_01_test )
{
  try
  {
    BOOST_SCOPE_EXIT( this_ )
    {
      this_->theApp.generate_interrupt_request();
      this_->theApp.wait4interrupt_request();
      this_->theApp.quit( true );
    } BOOST_SCOPE_EXIT_END

    set_default_expiration( HIVE_MAX_TIME_UNTIL_SIGNATURE_EXPIRATION );

    initialize();
    bool test_passed = false;

    fc::thread api_thread;
    auto _future = api_thread.async( [&]()
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

        ilog( "waiting for the block to consume all account preparation transactions" );
        fc::usleep( fc::seconds( HIVE_BLOCK_INTERVAL ) );
        current_block_num = get_block_num();
        BOOST_REQUIRE_GT( current_block_num, saved_block_num ); // at least one block should have been generated
        saved_block_num = current_block_num;

        schedule_transaction( build_transfer( "alice", "bob", ASSET( "0.100 TBD" ), "", HIVE_MAX_TIME_UNTIL_EXPIRATION ) );
        schedule_transaction( build_transfer( "alice", "bob", ASSET( "0.100 TBD" ), "", HIVE_MAX_TIME_UNTIL_SIGNATURE_EXPIRATION ) );

        db->with_read_lock( [&]()
        {
          BOOST_REQUIRE_EQUAL( get_hbd_balance( "bob" ).amount.value, 1200 );
        } );

        ilog( "'API' thread finished" );
        test_passed = !theApp.is_interrupt_request();
      }
      CATCH( "API" )
    } );

    _future.wait();
    theApp.wait4interrupt_request();
    theApp.quit( true );
    db = nullptr; // prevent fixture destructor from accessing database after it was closed
    BOOST_REQUIRE( test_passed );
    ilog( "Test done" );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( witness_basic_with_runtime_expiration_02_test )
{
  try
  {
    BOOST_SCOPE_EXIT( this_ )
    {
      this_->theApp.generate_interrupt_request();
      this_->theApp.wait4interrupt_request();
      this_->theApp.quit( true );
    } BOOST_SCOPE_EXIT_END

    set_default_expiration( HIVE_MAX_TIME_UNTIL_SIGNATURE_EXPIRATION );

    initialize();
    bool test_passed = false;

    fc::thread api_thread;
    auto _future = api_thread.async( [&]()
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

        ilog( "waiting for the block to consume all account preparation transactions" );
        fc::usleep( fc::seconds( HIVE_BLOCK_INTERVAL ) );
        current_block_num = get_block_num();
        BOOST_REQUIRE_GT( current_block_num, saved_block_num ); // at least one block should have been generated
        saved_block_num = current_block_num;

        auto _scheduling =[this]( size_t expiration, bool pass = true, bool msg_duplicate = true )
        {
          auto _passed_op = [this, &expiration]()
          {
            schedule_transaction( build_transfer( "alice", "bob", ASSET( "0.001 TBD" ), "", expiration ) );
          };

          auto _failed_op = [&_passed_op, &msg_duplicate]()
          {
            if( msg_duplicate )
            {
              HIVE_REQUIRE_ASSERT( _passed_op(), "!is_known_transaction( trx_id )" );
            }
            else
            {
              HIVE_REQUIRE_ASSERT( _passed_op(),
                "trx.expiration <= now + HIVE_MAX_TIME_UNTIL_SIGNATURE_EXPIRATION" );
            }
          };

          if( pass )
            _passed_op();
          else
            _failed_op();

          _failed_op();
        };

        {
          _scheduling( HIVE_MAX_TIME_UNTIL_EXPIRATION );
          db->with_read_lock( [&]()
          {
            BOOST_REQUIRE_EQUAL( get_hbd_balance( "bob" ).amount.value, 1001 );
          } );
        }

        {
          _scheduling( HIVE_MAX_TIME_UNTIL_SIGNATURE_EXPIRATION );
          db->with_read_lock( [&]()
          {
            BOOST_REQUIRE_EQUAL( get_hbd_balance( "bob" ).amount.value, 1002 );
          } );
        }

        {
          _scheduling( HIVE_MAX_TIME_UNTIL_SIGNATURE_EXPIRATION / 2 );
          db->with_read_lock( [&]()
          {
            BOOST_REQUIRE_EQUAL( get_hbd_balance( "bob" ).amount.value, 1003 );
          } );
        }

        {
          _scheduling( HIVE_MAX_TIME_UNTIL_SIGNATURE_EXPIRATION + 1, false/*pass*/, false/*msg_duplicate*/ );
          db->with_read_lock( [&]()
          {
            BOOST_REQUIRE_EQUAL( get_hbd_balance( "bob" ).amount.value, 1003 );
          } );
        }

        ilog( "'API' thread finished" );
        test_passed = !theApp.is_interrupt_request();
      }
      CATCH( "API" )
    } );

    _future.wait();
    theApp.wait4interrupt_request();
    theApp.quit( true );
    db = nullptr; // prevent fixture destructor from accessing database after it was closed
    BOOST_REQUIRE( test_passed );
    ilog( "Test done" );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( witness_basic_with_runtime_expiration_03_test )
{
  try
  {
    BOOST_SCOPE_EXIT( this_ )
    {
      this_->theApp.generate_interrupt_request();
      this_->theApp.wait4interrupt_request();
      this_->theApp.quit( true );
    } BOOST_SCOPE_EXIT_END

    set_default_expiration( HIVE_MAX_TIME_UNTIL_SIGNATURE_EXPIRATION );

    initialize();
    bool test_passed = false;

    fc::thread api_thread;
    auto _future = api_thread.async( [&]()
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

        ilog( "waiting for the block to consume all account preparation transactions" );
        fc::usleep( fc::seconds( HIVE_BLOCK_INTERVAL ) );
        current_block_num = get_block_num();
        BOOST_REQUIRE_GT( current_block_num, saved_block_num ); // at least one block should have been generated
        saved_block_num = current_block_num;

        {
          const size_t _max_trxs = 100;
          const size_t _start = HIVE_MAX_TIME_UNTIL_EXPIRATION - 50;

          for( size_t i = 0; i < _max_trxs; ++i )
          {
            schedule_transaction( build_transfer( "alice", "bob", ASSET( "0.001 TBD" ), "", _start + i ) );
          }

          schedule_block();

          db->with_read_lock( [&]()
          {
            BOOST_REQUIRE_EQUAL( get_hbd_balance( "bob" ).amount.value, 1000 + _max_trxs );
          } );
        }
        {
          auto _trx_00 = build_transfer( "alice", "bob", ASSET( "0.001 TBD" ), "",
            HIVE_MAX_TIME_UNTIL_SIGNATURE_EXPIRATION - 12 * HIVE_BLOCK_INTERVAL );

          auto _trx_01 = build_transfer( "alice", "bob", ASSET( "0.001 TBD" ), "",
            HIVE_MAX_TIME_UNTIL_SIGNATURE_EXPIRATION - 8 * HIVE_BLOCK_INTERVAL );

          auto _trx_01_a = build_transfer( "alice", "bob", ASSET( "0.002 TBD" ), "",
            HIVE_MAX_TIME_UNTIL_SIGNATURE_EXPIRATION - 8 * HIVE_BLOCK_INTERVAL );

          auto _trx_02 = build_transfer( "alice", "bob", ASSET( "0.001 TBD" ), "",
            HIVE_MAX_TIME_UNTIL_SIGNATURE_EXPIRATION - 4 * HIVE_BLOCK_INTERVAL );

          schedule_blocks( HIVE_MAX_TIME_UNTIL_SIGNATURE_EXPIRATION / HIVE_BLOCK_INTERVAL - 12 );

          HIVE_REQUIRE_ASSERT( schedule_transaction( _trx_00 ),
            "now < full_transaction->get_runtime_expiration()" );

          schedule_blocks( 3 );

          schedule_transaction( _trx_01 );

          schedule_block();

          HIVE_REQUIRE_ASSERT( schedule_transaction( _trx_01 ), "!is_known_transaction( trx_id )" );
          HIVE_REQUIRE_ASSERT( schedule_transaction( _trx_01_a ),
            "now < full_transaction->get_runtime_expiration()" );

          schedule_transaction( _trx_02 );

          schedule_block();

          HIVE_REQUIRE_ASSERT( schedule_transaction( _trx_02 ), "!is_known_transaction( trx_id )" );

          db->with_read_lock( [&]()
          {
            BOOST_REQUIRE_EQUAL( get_hbd_balance( "bob" ).amount.value, 1100 + 2 );
          } );

        }

        ilog( "'API' thread finished" );
        test_passed = !theApp.is_interrupt_request();
      }
      CATCH( "API" )
    } );

    _future.wait();
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
  try
  {
    BOOST_SCOPE_EXIT( this_ )
    {
      this_->theApp.generate_interrupt_request();
      this_->theApp.wait4interrupt_request();
      this_->theApp.quit( true );
    } BOOST_SCOPE_EXIT_END

    configuration_data.min_root_comment_interval = fc::seconds( 3 * HIVE_BLOCK_INTERVAL );
    initialize();

    enum { ALICE, BOB, CAROL, DAN, FEEDER_COUNT };
    bool test_passed[ FEEDER_COUNT + 1 ] = {};
    std::atomic<uint32_t> active_feeders( 0 );
    fc::thread sync_thread;
    auto _future_00 = sync_thread.async( [&]()
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
      while( active_feeders.load() > 0 && !theApp.is_interrupt_request() )
        fc::usleep( fc::milliseconds( 200 ) );
      ilog( "All threads finished." );
    } );

    fc::thread api_thread[ FEEDER_COUNT ];

    auto _future_01 = api_thread[ ALICE ].async( [&]()
    {
      try
      {
        while( active_feeders.load() == 0 && !theApp.is_interrupt_request() )
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

    auto _future_02 = api_thread[ BOB ].async( [&]()
    {
      try
      {
        while( active_feeders.load() == 0 && !theApp.is_interrupt_request() )
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

    auto _future_03 = api_thread[ CAROL ].async( [&]()
    {
      try
      {
        while( active_feeders.load() == 0 && !theApp.is_interrupt_request() )
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
        while( !db->find_comment( "alice", std::string( "shopping" ) ) && !theApp.is_interrupt_request() )
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

    auto _future_04 = api_thread[ DAN ].async( [&]()
    {
      try
      {
        while( active_feeders.load() == 0 && !theApp.is_interrupt_request() )
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

    _future_00.wait();
    _future_01.wait();
    _future_02.wait();
    _future_03.wait();
    _future_04.wait();
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
  try
  {
    BOOST_SCOPE_EXIT( this_ )
    {
      this_->theApp.generate_interrupt_request();
      this_->theApp.wait4interrupt_request();
      this_->theApp.quit( true );
    } BOOST_SCOPE_EXIT_END

    initialize( 3, { "wit1", "wit2", "wit3", "wit4", "wit5", "wit6", "wit7", "wit8", "wit9", "wita",
      "witb", "witc", "witd", "wite", "witf", "witg", "with", "witi", "witj", "witk" } );
    bool test_passed = false;

    fc::thread api_thread;
    auto _future = api_thread.async( [&]()
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
        test_passed = !theApp.is_interrupt_request();
      }
      CATCH( "API" )
    } );

    _future.wait();
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
  try
  {
    BOOST_SCOPE_EXIT( this_ )
    {
      this_->theApp.generate_interrupt_request();
      this_->theApp.wait4interrupt_request();
      this_->theApp.quit( true );
    } BOOST_SCOPE_EXIT_END

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
    auto _future = api_thread.async( [&]()
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
        test_passed = !theApp.is_interrupt_request();
      }
      CATCH( "API" )
    } );

    _future.wait();
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
  // ABW: same as missing_blocks_test except blocks that witness plugin could not produce due to
  // not being marked as representing scheduled witnesses are generated artificially with debug plugin;
  // the main purpose of this test is to check if such thing works; for witness plugin it should
  // look as if those blocks were created outside of node and passed to it through p2p

  try
  {
    BOOST_SCOPE_EXIT( this_ )
    {
      this_->theApp.generate_interrupt_request();
      this_->theApp.wait4interrupt_request();
      this_->theApp.quit( true );
    } BOOST_SCOPE_EXIT_END

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
    auto _future = api_thread.async( [&]()
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
        test_passed = !theApp.is_interrupt_request();
      }
      CATCH( "API" )
    } );

    _future.wait();
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
  bool test_passed = false;

  try
  {
    witness_fixture preparation;

    BOOST_SCOPE_EXIT( &preparation )
    {
      preparation.theApp.generate_interrupt_request();
      preparation.theApp.wait4interrupt_request();
      preparation.theApp.quit( true );
    } BOOST_SCOPE_EXIT_END

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
    BOOST_SCOPE_EXIT( this_ )
    {
      this_->theApp.generate_interrupt_request();
      this_->theApp.wait4interrupt_request();
      this_->theApp.quit( true );
    } BOOST_SCOPE_EXIT_END

    initialize( 0, // already set
      { "wit1", "wit2", "wit3", "wit4", "wit5", "wit6", "wit7", "wit8", "wit9", "wita",
      "witb", "witc", "witd", "wite", "witf", "witg", "with", "witi", "witj", "witk" },
      { "with" } ); // represent only 'with'
    fc::logger::get( "user" ).set_log_level( fc::log_level::info ); // suppress fast confirm broadcast messages
    db->_log_hardforks = true;

    fc::thread api_thread;
    auto _future = api_thread.async( [&]()
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
        test_passed = !theApp.is_interrupt_request();
      }
      CATCH( "API" )
    } );

    _future.wait();
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
  // ABW: in this test we are trying to emulate situation when there is already block waiting
  // in writer queue to be processed, but it arrived so late that witness plugin already
  // requested to produce its alternative; preferably we want the external block to be processed
  // before witness request; we are achieving it by putting really slow transaction inside that
  // block which emulates writer queue congestion

  try
  {
    BOOST_SCOPE_EXIT( this_ )
    {
      this_->theApp.generate_interrupt_request();
      this_->theApp.wait4interrupt_request();
      this_->theApp.quit( true );
    } BOOST_SCOPE_EXIT_END

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
    witness_plugin->set_witnesses( { chosen_witness } );

    fc::thread api_thread;
    auto _future = api_thread.async( [&]()
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
        test_passed = !theApp.is_interrupt_request();
      }
      CATCH( "API" )
    } );

    _future.wait();
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
  // ABW: same as block_conflict_test, except this test specifically shows how use of read lock
  // in case of missing block in queue congestion environment can lead to witness losing its
  // block

  try
  {
    BOOST_SCOPE_EXIT( this_ )
    {
      this_->theApp.generate_interrupt_request();
      this_->theApp.wait4interrupt_request();
      this_->theApp.quit( true );
    } BOOST_SCOPE_EXIT_END

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
    witness_plugin->set_witnesses( { chosen_witness } );

    fc::thread api_thread;
    auto _future = api_thread.async( [&]()
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
        test_passed = !theApp.is_interrupt_request();
      }
      CATCH( "API" )
    } );

    _future.wait();
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
  // ABW: same as block_lock_test, but this test emulates situation where processing of block
  // previous to the one witness wanted to produce takes a lot of time (we can't achieve that
  // actually, since we can only slow down marker transaction, however it makes no difference,
  // since witness can't ask for state data while the block is being processed and it does
  // not matter if it can't ask before or after block number changes)

  try
  {
    BOOST_SCOPE_EXIT( this_ )
    {
      this_->theApp.generate_interrupt_request();
      this_->theApp.wait4interrupt_request();
      this_->theApp.quit( true );
    } BOOST_SCOPE_EXIT_END

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
    witness_plugin->set_witnesses( { chosen_witness } );

    fc::thread api_thread;
    auto _future = api_thread.async( [&]()
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
          witness_plugin->set_witnesses( { "initminer", "wit1", "wit2", "wit3", "wit4",
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
        test_passed = !theApp.is_interrupt_request();
      }
      CATCH( "API" )
    } );

    _future.wait();
    theApp.wait4interrupt_request();
    theApp.quit( true );
    db = nullptr; // prevent fixture destructor from accessing database after it was closed
    BOOST_REQUIRE( test_passed );
    ilog( "Test done" );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( slow_obi_test )
{
  // the test emulates situation from flood testing (before it was corrected) when sending out
  // fast confirm (OBI) transactions took more than 50ms which allowed witness thread to return
  // to block production before data on next production slot was updated

  try
  {
    BOOST_SCOPE_EXIT( this_ )
    {
      this_->theApp.generate_interrupt_request();
      this_->theApp.wait4interrupt_request();
      this_->theApp.quit( true );
    } BOOST_SCOPE_EXIT_END

    initialize();
    bool test_passed = false;
    fc::logger::get( "user" ).set_log_level( fc::log_level::info ); // suppress fast confirm broadcast messages

    fc::thread api_thread;
    auto _future = api_thread.async( [&]()
    {
      struct tcatcher : public fc::appender
      {
        virtual void log( const fc::log_message& m )
        {
          if( m.get_context().get_log_level() == fc::log_level::error )
          {
            const char* PROBLEM_MSG = "Got exception while generating block";
            BOOST_CHECK_NE( std::memcmp( m.get_message().c_str(), PROBLEM_MSG, std::strlen( PROBLEM_MSG ) ), 0 );
          }
        }
      };
      //the only way to see if we run into problem checked by this test is to observe elog messages
      BOOST_REQUIRE( fc::logger::get( DEFAULT_LOGGER ).is_enabled( fc::log_level::error ) );
      auto catcher = fc::shared_ptr<tcatcher>( new tcatcher() );
      fc::logger::get( DEFAULT_LOGGER ).add_appender( catcher );

      boost::signals2::connection _finish_push_block_conn;

      BOOST_SCOPE_EXIT( this_, _finish_push_block_conn, catcher )
      {
        fc::logger::get( DEFAULT_LOGGER ).remove_appender( catcher );
        hive::utilities::disconnect_signal( _finish_push_block_conn );
        this_->theApp.generate_interrupt_request();
      } BOOST_SCOPE_EXIT_END

      try
      {
        ilog( "Starting test thread" );

        // hook to end of block processing with high priority
        db->add_finish_push_block_handler( [&]( const hive::chain::block_notification& note )
        {
          ilog( "start sleeping to trigger problem" );
          // sleep long enough to trigger problem - why 200 when we were talking about 50ms? because
          // original problem showed during heavy load - we need to compensate for that as well
          // (witness plugin wants to reschedule itself every 200ms but not for time shorter than
          // 50ms)
          fc::usleep( fc::milliseconds( 200 ) );
          ilog( "problem should already occur (unless it was fixed)" );
        }, get_chain_plugin(), -1 );

        // produce couple blocks
        fc::time_point_sec stop_time = db->head_block_time() + 4 * HIVE_BLOCK_INTERVAL;
        fc::sleep_until( stop_time );

        ilog( "'API' thread finished" );
        test_passed = !theApp.is_interrupt_request();
      }
      CATCH( "API" )
    } );

    _future.wait();
    theApp.wait4interrupt_request();
    theApp.quit( true );
    db = nullptr; // prevent fixture destructor from accessing database after it was closed
    BOOST_REQUIRE( test_passed );
    ilog( "Test done" );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( colony_basic_test )
{
  try
  {
    BOOST_SCOPE_EXIT( this_ )
    {
      this_->theApp.generate_interrupt_request();
      this_->theApp.wait4interrupt_request();
      this_->theApp.quit( true );
    } BOOST_SCOPE_EXIT_END

    configuration_data.min_root_comment_interval = fc::seconds( 3 );
    const uint32_t COLONY_START = 42; // at the start of third schedule
    bool test_passed = false;

    initialize( 1, {}, { "initminer" }, {
      config_line_t( { "plugin", { HIVE_COLONY_PLUGIN_NAME } } ),
      config_line_t( { "colony-sign-with", { init_account_priv_key.key_to_wif() } } ),
      config_line_t( { "colony-start-at-block", { std::to_string( COLONY_START ) } } ),
      config_line_t( { "colony-transactions-per-block", { "5000" } } ),
      config_line_t( { "colony-no-broadcast", { "1" } } ),
      config_line_t( { "colony-article", { R"~({"min":100,"max":5000,"weight":16,"exponent":4})~" } } ),
      config_line_t( { "colony-reply", { R"~({"min":30,"max":1000,"weight":110,"exponent":5})~" } } ),
      config_line_t( { "colony-vote", { R"~({"weight":2070})~" } } ),
      config_line_t( { "colony-transfer", { R"~({"min":0,"max":350,"weight":87,"exponent":4})~" } } ),
      config_line_t( { "colony-custom", { R"~({"min":20,"max":400,"weight":6006,"exponent":1})~" } } ),
      config_line_t( { "max-mempool-size", { "0" } } ),
    }, "1G" );

    fc::thread api_thread;
    auto _future = api_thread.async( [&]()
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

        // now we have up to block 'colony_start' to add users and some initial comments
        const int ACCOUNTS = 20000;
        signed_transaction tx; // building multiop transaction to speed up the process
        uint32_t block_num = 0;
        db->with_read_lock( [&]()
        {
          tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION / 2 );
          tx.set_reference_block( db->head_block_id() );
          block_num = db->head_block_num();
        } );

        auto set_block_size = [&]( uint32_t value )
        {
          witness_set_properties_operation witness_props;
          witness_props.owner = "initminer";
          witness_props.props[ "key" ] = fc::raw::pack_to_vector( init_account_pub_key );
          witness_props.props[ "maximum_block_size" ] = fc::raw::pack_to_vector( value );
          tx.operations.emplace_back( witness_props );
          tx.set_expiration( tx.expiration + HIVE_BLOCK_INTERVAL ); // shifting expiration to avoid tx duplicates
          schedule_transaction( tx );
          tx.clear();
          ilog( "#${b} sending block size change (${v}) to witness (activates in future schedule)",
            ( "b", block_num )( "v", value ) );
        };

        // start with setting block size to 2MB (it will temporarily change to default 128kB on first schedule)
        set_block_size( HIVE_MAX_BLOCK_SIZE );

        for( size_t i = 0; i < ACCOUNTS && !theApp.is_interrupt_request(); ++i )
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
        BOOST_REQUIRE_LT( block_num, COLONY_START - 2 ); // check that we've managed to prepare before activation of colony (plus margin)
        ilog( "Sleeping until block #${b}", ( "b", COLONY_START - 2 ) );
        fc::sleep_until( get_genesis_time() + ( COLONY_START - 2 ) * HIVE_BLOCK_INTERVAL );
        block_num = get_block_num();
        ilog( "#${b} initial block size should soon set to 2MB", ( "b", block_num ) );

        // reduce block size to 1MB so colony is forced to adjust production rate (in future schedule);
        // note that we need the margin (2 blocks) before colony starts, because otherwise we'd
        // not be able to be sure the following setting of block size will actually fit in nearest block
        set_block_size( HIVE_MAX_BLOCK_SIZE / 2 );

        const auto& block_reader = get_chain_plugin().block_reader();

        const uint32_t FULL_RATE_BLOCKS = ( COLONY_START + 20 + 10 ) / 21 * 21 - 3;
          // there needs to be 2 blocks of margin before next schedule
        uint32_t start = COLONY_START + 5; // 5 blocks of margin
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
        while( block_num < FULL_RATE_BLOCKS && !theApp.is_interrupt_request() );

        ilog( "#${b} block size should soon change to 1MB - colony production rate should be reduced", ( "b", block_num ) );

        // reduce block size to 64kB so colony is forced to drastically adjust production rate
        // (in future schedule), even stop producing for a while; block stats should show a lot
        // of pending transactions and no new incoming until almost all the pending are exhausted
        set_block_size( HIVE_MIN_BLOCK_SIZE_LIMIT );

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
        while( block_num < REDUCED_RATE_BLOCKS && !theApp.is_interrupt_request() );

        ilog( "#${b} block size should soon change to 64kB - colony production rate should be drastically reduced", ( "b", block_num ) );

        // increase block size back to 2MB, so colony can ramp up production again (in future schedule)
        set_block_size( HIVE_MAX_BLOCK_SIZE );

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
              BOOST_CHECK_LT( tx_count, 285 );
              BOOST_CHECK_GT( tx_count, 185 );
            }
          } );
        }
        while( block_num < MINIMAL_RATE_BLOCKS && !theApp.is_interrupt_request() );

        ilog( "#${b} block size should soon change back to 2MB - colony production rate should return to full", ( "b", block_num ) );

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
        while( block_num < FULL2_RATE_BLOCKS && !theApp.is_interrupt_request() );

        ilog( "'API' thread finished" );
        test_passed = !theApp.is_interrupt_request();
      }
      CATCH( "API" )
    } );

    _future.wait();
    theApp.wait4interrupt_request();
    theApp.quit( true );
    db = nullptr; // prevent fixture destructor from accessing database after it was closed
    BOOST_REQUIRE( test_passed );
    ilog( "Test done" );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( colony_no_workers_test )
{
  // this test checks reaction of colony with delayed start on lack of suitable worker accounts
  bool test_passed = false;
  BOOST_SCOPE_EXIT( &test_passed ) { BOOST_REQUIRE( test_passed ); } BOOST_SCOPE_EXIT_END

  try
  {
    BOOST_SCOPE_EXIT( this_ )
    {
      this_->theApp.generate_interrupt_request();
      this_->theApp.wait4interrupt_request();
      this_->theApp.quit( true );
    } BOOST_SCOPE_EXIT_END

    configuration_data.min_root_comment_interval = fc::seconds( 3 );
    const uint32_t COLONY_START = 5;

    std::string not_used_key = fc::ecc::private_key::regenerate( fc::sha256::hash( std::string( "not used key" ) ) ).key_to_wif();
    // we are using some key that is not associated with any account; note that if we left "nothing" as
    // a "key to use", 'temp' account would be a suitable worker
    initialize( 1, {}, { "initminer" }, {
      config_line_t( { "plugin", { HIVE_COLONY_PLUGIN_NAME } } ),
      config_line_t( { "colony-sign-with", { not_used_key } } ),
      config_line_t( { "colony-start-at-block", { std::to_string( COLONY_START ) } } ) } );

    fc::thread api_thread;
    api_thread.async( [&]()
    {
      try
      {
        ilog( "Wait for first block after genesis" );
        fc::sleep_until( get_genesis_time() + HIVE_BLOCK_INTERVAL );
        ilog( "All hardforks should have been applied" );
        BOOST_REQUIRE( db->has_hardfork( HIVE_NUM_HARDFORKS ) );
        db->_log_hardforks = true;

        ilog( "Waiting for 'colony' to activate" );
        fc::sleep_until( get_genesis_time() + COLONY_START * HIVE_BLOCK_INTERVAL - 1 );
        uint32_t block_num = db->head_block_num();
        BOOST_REQUIRE_EQUAL( block_num, COLONY_START - 1 );
        // we are now 1 second before timestamp of block that will activate 'colony' in its
        // post_apply_block notification
        // 'colony' should stop the application which means no new blocks should be created;
        // also this thread should be terminated with cancel exeption before it finishes

        fc::usleep( fc::seconds( 2 ) );
        BOOST_REQUIRE_EQUAL( db, nullptr );

        ilog( "'API' thread finished incorrectly" );
      }
      catch( const fc::canceled_exception& )
      {
        ilog( "'API' thread canceled correctly" );
        test_passed = true;
      }
      CATCH( "API" )
    } );

    theApp.wait4interrupt_request();
    // the block that activated 'colony' should finish properly
    BOOST_REQUIRE_EQUAL( db->head_block_num(), COLONY_START );
    theApp.quit( true );
    db = nullptr; // prevent fixture destructor from accessing database after it was closed
    ilog( "Test done" );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( basic_queen_test )
{ try {
  // NOTE: unlike other tests here, queen tests can be run under debugger, but there is some
  // strange effect at the end (logging thread not shut down) - continue and it will finish ok
  bool test_passed = false;

  BOOST_SCOPE_EXIT( this_ )
  {
    this_->theApp.generate_interrupt_request();
    this_->theApp.wait4interrupt_request();
    this_->theApp.quit( true );
  } BOOST_SCOPE_EXIT_END

  initialize( 1,
    { "wit1", "wit2", "wit3", "wit4", "wit5", "wit6", "wit7", "wit8", "wit9", "wita",
    "witb", "witc", "witd", "wite", "witf", "witg", "with", "witi", "witj", "witk" },
    {}, // queen should ignore configured witnesses and only care about signing key
    {
      config_line_t( { "plugin", { HIVE_QUEEN_PLUGIN_NAME } } ),
      config_line_t( { "queen-block-size", { "1000" } } ),
      config_line_t( { "queen-tx-count", { "3" } } ),
    }
  );

  ilog( "forcing block production" );
  generate_block(); // add block to trigger hardforks

  fc::thread api_thread;
  auto _future = api_thread.async( [&]()
  {
    BOOST_SCOPE_EXIT( this_ ) { this_->theApp.generate_interrupt_request(); } BOOST_SCOPE_EXIT_END
    try
    {
      ilog( "All hardforks should have been applied" );
      BOOST_REQUIRE( db->has_hardfork( HIVE_NUM_HARDFORKS ) );
      BOOST_REQUIRE_EQUAL( get_block_num(), 1 );
      db->_log_hardforks = true;

      std::string name = "wit?";
      for( int i = 1; i <= 9; ++i )
      {
        name[3] = '0' + i;
        ilog( "sending funds to ${name}", ( name ) );
        schedule_fund( name, ASSET( "10.000 TESTS" ) );
      }

      BOOST_REQUIRE_EQUAL( get_block_num(), 4 ); // 1(0), 2(3), 3(3), 4(3)

      for( int i = 0; i <= 10; ++i )
      {
        name[3] = 'a' + i;
        ilog( "sending funds to ${name}", ( name ) );
        schedule_fund( name, ASSET( "10.000 TESTS" ) );
        if( i % 4 == 0 )
        {
          ilog( "forcing block production" );
          schedule_block(); // you can generate blocks with debug plugin to trigger early generation
        }
      }

      BOOST_REQUIRE_EQUAL( get_block_num(), 9 ); // 1(0), 2(3), 3(3), 4(3), 5(1), 6(3), 7(1), 8(3), 9(1)
      BOOST_REQUIRE_GT( db->_pending_tx_size, 0 ); // 2 last transactions are pending

      ilog( "sending funds to witl (failure)" );
      BOOST_REQUIRE_THROW( schedule_fund( "witl", ASSET( "10.000 TESTS" ) ), fc::exception );
      // failed transaction does not trigger block production
      BOOST_REQUIRE_EQUAL( get_block_num(), 9 );

      ilog( "forcing block production" );
      schedule_block();
      BOOST_REQUIRE_EQUAL( get_block_num(), 10 );
      BOOST_REQUIRE_EQUAL( db->_pending_tx_size, 0 );

      // trigger block with big transaction (block size check)
      {
        comment_operation comment;
        comment.parent_permlink = "bigtx";
        comment.author = "wit1";
        comment.permlink = "bigtx";
        comment.title = "Big transaction";
        comment.body = "Lorem ipsum dolor sit amet, consectetur adipiscing elit.Aliquam commodo "
          "vitae elit sodales vulputate. Curabitur sed massa mauris. Integer cursus orci non pulvinar "
          "interdum. Mauris dictum sit amet massa eget pellentesque.Aenean consequat quam sodales "
          "porta sollicitudin. Nullam dui ipsum, vulputate vel nunc sodales, scelerisque porta augue. "
          "Aenean ac porta ante.Donec dapibus orci justo, ut pharetra arcu pulvinar ornare. "
          "Vestibulum ac nisl semper, venenatis nisi vitae, aliquam ipsum.Aenean ut enim eget dui "
          "maximus scelerisque. Suspendisse ut nibh ultrices, hendrerit dolor a, vulputate ante.\n\n"
          "Suspendisse a finibus sapien, ac consequat neque. Donec eget tempus neque, id interdum "
          "metus. In id porta mi. Duis cursus risus est, at iaculis orci aliquam eu.Ut sapien est, "
          "suscipit a massa sed, finibus ultrices odio. Maecenas urna nulla, facilisis vel mi vel, "
          "faucibus porta erat. Suspendisse tempus turpis in mauris interdum lobortis. Mauris eros "
          "nunc, porta sed turpis sed, euismod semper libero.";
        ilog( "sending big comment" );
        schedule_transaction( comment );

        BOOST_REQUIRE_EQUAL( get_block_num(), 11 );
      }

      test_passed = !theApp.is_interrupt_request();
    }
    CATCH( "API" )
  } );

  _future.wait();
  theApp.wait4interrupt_request();
  theApp.quit( true );
  db = nullptr; // prevent fixture destructor from accessing database after it was closed
  BOOST_REQUIRE( test_passed );
  ilog( "Test done" );
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( colony_queen_test )
{
  try
  {
    configuration_data.min_root_comment_interval = fc::seconds( 3 );
    const uint32_t COLONY_START = 42; // at the start of third schedule
    bool test_passed = false;

    BOOST_SCOPE_EXIT( this_ )
    {
      this_->theApp.generate_interrupt_request();
      this_->theApp.wait4interrupt_request();
      this_->theApp.quit( true );
    } BOOST_SCOPE_EXIT_END

    initialize( 1, {}, {}, {
      config_line_t( { "plugin", { HIVE_COLONY_PLUGIN_NAME } } ),
      config_line_t( { "colony-sign-with", { init_account_priv_key.key_to_wif() } } ),
      config_line_t( { "colony-start-at-block", { std::to_string( COLONY_START ) } } ),
      config_line_t( { "colony-transactions-per-block", { "0" } } ), // unlimited
      config_line_t( { "colony-no-broadcast", { "1" } } ),
      config_line_t( { "colony-article", { R"~({"min":100,"max":5000,"weight":16,"exponent":4})~" } } ),
      config_line_t( { "colony-reply", { R"~({"min":30,"max":1000,"weight":110,"exponent":5})~" } } ),
      config_line_t( { "colony-vote", { R"~({"weight":2070})~" } } ),
      config_line_t( { "colony-transfer", { R"~({"min":0,"max":350,"weight":87,"exponent":4})~" } } ),
      config_line_t( { "colony-custom", { R"~({"min":20,"max":400,"weight":6006,"exponent":1})~" } } ),
      config_line_t( { "plugin", { HIVE_QUEEN_PLUGIN_NAME } } ),
      config_line_t( { "queen-block-size", { "1500000" } } )
    }, "1G" );

    generate_block(); // add block to trigger hardforks

    fc::thread api_thread;
    auto _future = api_thread.async( [&]()
    {
      BOOST_SCOPE_EXIT( this_ ) { this_->theApp.generate_interrupt_request(); } BOOST_SCOPE_EXIT_END
      try
      {
        ilog( "All hardforks should have been applied" );
        BOOST_REQUIRE( db->has_hardfork( HIVE_NUM_HARDFORKS ) );
        BOOST_REQUIRE_EQUAL( get_block_num(), 1 );
        db->_log_hardforks = true;

        ilog( "Starting 'API' thread that will be sending transactions" );

        // now we have up to block 'colony_start' to add users and some initial comments
        const int ACCOUNTS = 20000;
        signed_transaction tx; // building multiop transaction to speed up the process
        uint32_t block_num = 1;

        tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION / 2 );
        tx.set_reference_block( db->head_block_id() );

        auto set_block_size = [&]( uint32_t value )
        {
          witness_set_properties_operation witness_props;
          witness_props.owner = "initminer";
          witness_props.props[ "key" ] = fc::raw::pack_to_vector( init_account_pub_key );
          witness_props.props[ "maximum_block_size" ] = fc::raw::pack_to_vector( value );
          tx.operations.emplace_back( witness_props );
          tx.set_expiration( tx.expiration + HIVE_BLOCK_INTERVAL ); // shifting expiration to avoid tx duplicates
          schedule_transaction( tx );
          tx.clear();
          ilog( "#${b} sending block size change (${v}) to witness (activates in future schedule)",
            ( "b", block_num )( "v", value ) );
        };

        // since colony and queen rarely leave any room for actions outside write lock
        // we have to read block number without lock - the actual value does not matter
        // and the actual head block is likely to change before we manage to do something
        // anyway
        auto wait_for_block_change_nolock = [&]( uint32_t block_num )
        {
          do
          {
            fc::usleep( fc::milliseconds( 50 ) );
            uint32_t new_block = db->head_block_num();
            if( new_block > block_num )
              return new_block;
          }
          while( !theApp.is_interrupt_request() );
          return 0u;
        };

        // start with setting block size to 2MB (it will temporarily change to default 128kB on first schedule)
        set_block_size( HIVE_MAX_BLOCK_SIZE );

        for( size_t i = 0; i < ACCOUNTS && !theApp.is_interrupt_request(); ++i )
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
            ilog( "Adding users... ${i}", ( i ) );
        }

        block_num = get_block_num();
        BOOST_REQUIRE_LT( block_num, COLONY_START - 2 ); // check that we've managed to prepare before activation of colony (plus margin)
        schedule_blocks( COLONY_START - 2 - block_num );
        block_num = get_block_num();
        ilog( "#${b} initial block size should soon set to 2MB", ( "b", block_num ) );

        // reduce block size to 1MB so queen has to lower its trigger point
        set_block_size( HIVE_MAX_BLOCK_SIZE / 2 );
        // push past colony starting point - from now on it will be driving queen to produce blocks
        schedule_blocks( 2 );

        const uint32_t FULL_RATE_BLOCKS = ( COLONY_START + 20 + 10 ) / 21 * 21 - 3;
          // there needs to be 2 blocks of margin before next schedule
        block_num = wait_for_block_change_nolock( FULL_RATE_BLOCKS );
        ilog( "#${b} block size should soon change to 1MB", ( "b", block_num ) );

        // reduce block size to 64kB to drastically lower queen's trigger point (in future schedule)
        set_block_size( HIVE_MIN_BLOCK_SIZE_LIMIT );

        const uint32_t REDUCED_RATE_BLOCKS = ( FULL_RATE_BLOCKS + 3 + 20 + 10 ) / 21 * 21 - 3;
          // there needs to be 2 blocks of margin before next schedule
        block_num = wait_for_block_change_nolock( REDUCED_RATE_BLOCKS );
        ilog( "#${b} block size should soon change to 64kB", ( "b", block_num ) );

        // increase block size back to 2MB, so queen actually triggers with its target block size as stated in config
        set_block_size( HIVE_MAX_BLOCK_SIZE );

        const uint32_t MINIMAL_RATE_BLOCKS = ( REDUCED_RATE_BLOCKS + 3 + 20 + 10 ) / 21 * 21 - 3;
          // there needs to be 2 blocks of margin before next schedule
        block_num = wait_for_block_change_nolock( MINIMAL_RATE_BLOCKS );
        ilog( "#${b} block size should soon change back to 2MB", ( "b", block_num ) );

        const uint32_t FULL2_RATE_BLOCKS = ( MINIMAL_RATE_BLOCKS + 3 + 20 + 10 ) / 21 * 21;
        block_num = wait_for_block_change_nolock( FULL2_RATE_BLOCKS );

        ilog( "'API' thread finished" );
        test_passed = !theApp.is_interrupt_request();
      }
      CATCH( "API" )
    } );

    _future.wait();
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
