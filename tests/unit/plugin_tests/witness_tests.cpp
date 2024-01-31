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
  witness_fixture() : hived_fixture( true, false ) {}
  virtual ~witness_fixture() {}

  void initialize()
  {
    theApp.init_signals_handler();

    configuration_data.set_initial_asset_supply(
      200'000'000'000ul, 1'000'000'000ul, 100'000'000'000ul,
      price( VEST_asset( 1'800 ), HIVE_asset( 1'000 ) )
    );
    genesis_time = fc::time_point::now() + fc::seconds( 1 );
    // genesis slightly in the future
    configuration_data.set_hardfork_schedule( genesis_time, { { HIVE_NUM_HARDFORKS, 1 } } );

    postponed_init(
      {
        config_line_t( { "plugin", { HIVE_WITNESS_PLUGIN_NAME } } ),
        config_line_t( { "shared-file-size", { "1G" } } ),
        config_line_t( { "witness", { "\"initminer\"" } } ),
        config_line_t( { "private-key", { init_account_priv_key.key_to_wif() } } )
      }
    );

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

  fc::time_point_sec get_genesis_time() const { return genesis_time; }

private:
  fc::time_point_sec genesis_time;
};

BOOST_FIXTURE_TEST_SUITE( witness_tests, witness_fixture )

BOOST_AUTO_TEST_CASE( witness_basic_test )
{
  using namespace hive::plugins::witness;

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
      catch( ... )
      {
        elog( "Unhandled exception thrown from 'API' thread" );
      }
    } );

    theApp.wait();
    ilog( "Test done" );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()
#endif
