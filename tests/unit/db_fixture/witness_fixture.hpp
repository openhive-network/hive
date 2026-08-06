#pragma once

/**
  * Fixture running a real hived node (block production included), shared by the tests that need
  * one: witness/colony/queen behaviour, and the account history storage tests that need blocks
  * produced the way a live node produces them.
  */

#include <hive/chain/detail/state/comment_object.hpp>
#include <hive/chain/full_transaction.hpp>

#include <hive/utilities/signal.hpp>
#include <boost/signals2.hpp>
#include <fc/thread/thread.hpp>

#include <hive/protocol/hive_operations.hpp>
#include <hive/protocol/transaction_util.hpp>

#include <hive/plugins/chain/chain_plugin.hpp>
#include <hive/plugins/witness/witness_plugin.hpp>
#include <hive/plugins/colony/colony_plugin.hpp>
#include <hive/plugins/queen/queen_plugin.hpp>

#include <hive/chain/detail/state/witness_objects_multiindex.hpp>
#include <hive/chain/detail/state/comment_object_multiindex.hpp>

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

  void initialize( int genesis_delay = 3, // genesis slightly in the future or past with negative values (increased for CI stability)
    const std::vector< std::string > initial_witnesses = {}, // initial witnesses over 'initminer'
    const std::vector< std::string > represented_witnesses = { "initminer" }, // which witnesses can produce
    const config_arg_override_t& extra_config_args = config_arg_override_t(),
    const std::string& shared_file_size = "1G" )
  {
    theApp.init_signals_handler();

    configuration_data.set_initial_asset_supply(
      HIVE_asset( 200'000'000'000ll ), HBD_asset( 1'000'000'000ll ),
      HIVE_asset( 100'000'000'000ll ), VEST_price( 1'800, 1'000 )
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
  uint32_t wait_for_block_change( uint32_t block_num, ACTION&& action, uint32_t timeout_seconds = 60 )
  {
    bool stop = false;
    uint32_t waited = 0;
    do
    {
      fc::usleep( fc::seconds( 1 ) );
      ++waited;
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
    while( !stop && !theApp.is_interrupt_request() && waited < timeout_seconds );
    BOOST_REQUIRE_MESSAGE( stop || theApp.is_interrupt_request(), "Timed out waiting for block change" );
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
    create.fee = db->get_witness_schedule_object().median_props.account_creation_fee.to_asset();
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
