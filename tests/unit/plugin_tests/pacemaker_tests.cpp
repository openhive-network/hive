#ifdef IS_TEST_NET
#include <boost/test/unit_test.hpp>

#include <hive/chain/block_log.hpp>
#include <hive/chain/full_transaction.hpp>

#include <hive/plugins/pacemaker/pacemaker_plugin.hpp>

#include "../db_fixture/hived_fixture.hpp"

using namespace hive::chain;
using namespace hive::protocol;

struct pacemaker_fixture : public hived_fixture
{
  pacemaker_fixture() : hived_fixture( true, false ) {}
  virtual ~pacemaker_fixture()
  {
    if( not pacemaker_source.empty() )
      fc::remove_all( pacemaker_source );
    configuration_data = configuration();
  }

  void initialize( std::function< void( hived_fixture& builder ) > build_block_log,
    int32_t genesis_delay = 1,
    const config_arg_override_t& extra_config_args = config_arg_override_t(),
    const std::string& shared_file_size = "1G" )
  {
    theApp.init_signals_handler();

    configuration_data.set_initial_asset_supply(
      200'000'000'000ul, 1'000'000'000ul, 100'000'000'000ul,
      price( VEST_asset( 1'800 ), HIVE_asset( 1'000 ) )
    );
    genesis_time = fc::time_point::now() + fc::seconds( genesis_delay );
    // we need to make genesis time proper multiple of 3 seconds, otherwise only first block will be
    // produced at genesis + 3s, next one will be earlier than genesis + 6s (see database::get_slot_time)
    genesis_time = fc::time_point_sec( ( genesis_time.sec_since_epoch() + 2 ) / HIVE_BLOCK_INTERVAL * HIVE_BLOCK_INTERVAL );

    configuration_data.set_hardfork_schedule( genesis_time, { { HIVE_NUM_HARDFORKS, 1 } } );
    const std::vector< std::string > initial_witnesses = { "wit1", "wit2", "wit3", "wit4", "wit5",
      "wit6", "wit7", "wit8", "wit9", "wita", "witb", "witc", "witd", "wite", "witf", "witg",
      "with", "witi", "witj", "witk" };
    configuration_data.set_init_witnesses( initial_witnesses );

    config_arg_override_t config_args = {
      config_line_t( { "shared-file-size", { shared_file_size } } )
    };

    // build block log with the same config but separate fixture
    fc::path data_dir, block_log_dir, pacemaker_source;
    {
      hived_fixture builder;
      builder.postponed_init( config_args );

      build_block_log( builder );

      data_dir = builder.get_data_dir();
      block_log_dir = data_dir / "blockchain";
    }

    // move produced block log to new place
    pacemaker_source = data_dir / "pacemaker";
    fc::remove_all( pacemaker_source );
    fc::rename( block_log_dir, pacemaker_source );

    config_args.push_back( config_line_t( { "plugin", { HIVE_PACEMAKER_PLUGIN_NAME } } ) );
    config_args.push_back( config_line_t( { "pacemaker-source",
      { ( pacemaker_source / block_log_file_name_info::_legacy_file_name ).string() } } ) );
    config_args.insert( config_args.end(), extra_config_args.begin(), extra_config_args.end() );
    postponed_init( config_args );

    init_account_pub_key = init_account_priv_key.get_public_key();
  }

  fc::time_point_sec get_genesis_time() const { return genesis_time; }

private:
  fc::time_point_sec genesis_time;
  fc::path pacemaker_source;
};

BOOST_FIXTURE_TEST_SUITE( pacemaker_tests, pacemaker_fixture )

BOOST_AUTO_TEST_CASE( pacemaker_basic_test )
{
  try
  {
    bool test_passed = false;
    const int BLOCKS_TO_SYNC = HIVE_MAX_WITNESSES;
    const int BLOCKS_BEFORE_ANCHOR = BLOCKS_TO_SYNC + 3;
    const int ALL_BLOCKS = BLOCKS_BEFORE_ANCHOR + 100;

    hive::protocol::transaction_id_type anchor_id;
    initialize( [&]( hived_fixture& builder )
    {
      builder.generate_blocks( BLOCKS_BEFORE_ANCHOR );

      // push transaction to act as anchor for artificial slowdown
      custom_json_operation op;
      op.required_auths.insert( HIVE_INIT_MINER_NAME );
      op.id = "anchor";
      op.json = "{}";

      hive::protocol::signed_transaction tx;
      tx.set_expiration( builder.db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
      tx.operations.push_back( op );
      auto ftx = builder.push_transaction( tx, init_account_priv_key );
      anchor_id = ftx->get_transaction_id();

      // add some more blocks
      builder.generate_blocks( ALL_BLOCKS - BLOCKS_BEFORE_ANCHOR );
      while( builder.get_last_irreversible_block_num() < BLOCKS_BEFORE_ANCHOR )
        builder.generate_block();
    },
    - BLOCKS_TO_SYNC * HIVE_BLOCK_INTERVAL, // genesis in the past for testing sync mode of pacemaker
    { config_line_t( { "pacemaker-max-offset", { std::to_string( HIVE_BLOCK_INTERVAL * 1000 ) } } ) } );

    // add artificial slowdown to processing of anchor transaction
    db_plugin->debug_update( [&]( database& db )
    {
      ilog( "Slow processing start" );
      fc::usleep( fc::seconds( 2 * HIVE_BLOCK_INTERVAL + 1 ) );
      // slowdown exceeds max offset, so it should cause pacemaker to break processing on next block
      // (note that we need to account for normal interval between blocks)
      ilog( "Slow processing end" );
    }, hive::chain::database::skip_nothing, anchor_id );

    fc::thread api_thread;
    api_thread.async( [&]()
    {
      try
      {
        ilog( "Starting test thread" );

        // wait one interval for "sync" blocks to finish being emitted by pacemaker
        fc::usleep( fc::seconds( HIVE_BLOCK_INTERVAL ) );
        db->with_read_lock( [&]()
        {
          BOOST_REQUIRE_GE( db->head_block_num(), BLOCKS_TO_SYNC );
        } );

        // wait for all blocks to be emitted
        fc::sleep_until( get_genesis_time() + ALL_BLOCKS * HIVE_BLOCK_INTERVAL );

        // test should be canceled before reaching this point
        ilog( "test thread finished incorrectly" );
      }
      catch( const fc::canceled_exception& ex )
      {
        ilog( "test thread canceled properly" );
        test_passed = true;
      }
      catch( const fc::exception& ex )
      {
        elog( "Unhandled fc::exception thrown from 'test' thread: ${r}",
          ( "r", ex.to_detail_string() ) );
        BOOST_CHECK( false && "Unhandled fc::exception" );
      }
      catch( ... )
      {
        elog( "Unhandled exception thrown from 'test' thread" );
        BOOST_CHECK( false && "Unhandled exception" );
      }
      if( !test_passed )
        theApp.generate_interrupt_request();
    } );

    theApp.wait4interrupt_request();
    theApp.quit( true );
    db = nullptr; // prevent fixture destructor from accessing database after it was closed
    ilog( "Test done" );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()
#endif
