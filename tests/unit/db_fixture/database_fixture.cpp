#include <boost/test/unit_test.hpp>
#include <boost/program_options.hpp>

#include <hive/utilities/tempdir.hpp>
#include <hive/utilities/database_configuration.hpp>

#include <hive/chain/history_object.hpp>
#include <hive/chain/hive_objects.hpp>
#include <hive/chain/sps_objects.hpp>
#include <hive/chain/util/delayed_voting.hpp>

#include <hive/plugins/account_history/account_history_plugin.hpp>
#include <hive/plugins/account_history_rocksdb/account_history_rocksdb_plugin.hpp>
#include <hive/plugins/chain/chain_plugin.hpp>
#include <hive/plugins/rc/rc_plugin.hpp>
#include <hive/plugins/webserver/webserver_plugin.hpp>
#include <hive/plugins/witness/witness_plugin.hpp>

#include <hive/plugins/condenser_api/condenser_api_plugin.hpp>

#include <hive/chain/smt_objects/nai_pool_object.hpp>

#include <fc/crypto/digest.hpp>
#include <fc/smart_ref_impl.hpp>

#include <iostream>
#include <iomanip>
#include <sstream>

#include "database_fixture.hpp"

//using namespace hive::chain::test;

uint32_t HIVE_TESTING_GENESIS_TIMESTAMP = 1431700000;

using namespace hive::plugins::webserver;
using namespace hive::plugins::database_api;
using namespace hive::plugins::block_api;
using hive::plugins::condenser_api::condenser_api_plugin;

namespace hive { namespace chain {

typedef hive::plugins::account_history::account_history_plugin ah_plugin;
//typedef hive::plugins::account_history_rocksdb::account_history_rocksdb_plugin ah_plugin;

using std::cout;
using std::cerr;

clean_database_fixture::clean_database_fixture( uint16_t shared_file_size_in_mb, fc::optional<uint32_t> hardfork )
{
  try {
  int argc = boost::unit_test::framework::master_test_suite().argc;
  char** argv = boost::unit_test::framework::master_test_suite().argv;
  for( int i=1; i<argc; i++ )
  {
    const std::string arg = argv[i];
    if( arg == "--record-assert-trip" )
      fc::enable_record_assert_trip = true;
    if( arg == "--show-test-names" )
      std::cout << "running test " << boost::unit_test::framework::current_test_case().p_name << std::endl;
  }

  appbase::app().register_plugin< ah_plugin >();
  db_plugin = &appbase::app().register_plugin< hive::plugins::debug_node::debug_node_plugin >();
  rc_plugin = &appbase::app().register_plugin< hive::plugins::rc::rc_plugin >();
  appbase::app().register_plugin< hive::plugins::witness::witness_plugin >();

  db_plugin->logging = false;
  appbase::app().initialize<
    ah_plugin,
    hive::plugins::debug_node::debug_node_plugin,
    hive::plugins::rc::rc_plugin,
    hive::plugins::witness::witness_plugin
    >( argc, argv );

  hive::plugins::rc::rc_plugin_skip_flags rc_skip;
  rc_skip.skip_reject_not_enough_rc = 1;
  rc_skip.skip_deduct_rc = 0;
  rc_skip.skip_negative_rc_balance = 1;
  rc_skip.skip_reject_unknown_delta_vests = 0;
  rc_plugin->set_rc_plugin_skip_flags( rc_skip );

  db = &appbase::app().get_plugin< hive::plugins::chain::chain_plugin >().db();
  BOOST_REQUIRE( db );

  init_account_pub_key = init_account_priv_key.get_public_key();

  open_database( shared_file_size_in_mb );

  inject_hardfork( hardfork.valid() ? ( *hardfork ) : HIVE_BLOCKCHAIN_VERSION.minor_v() );

  vest( "initminer", 10000 );

  // Fill up the rest of the required miners
  for( int i = HIVE_NUM_INIT_MINERS; i < HIVE_MAX_WITNESSES; i++ )
  {
    account_create( HIVE_INIT_MINER_NAME + fc::to_string( i ), init_account_pub_key );
    fund( HIVE_INIT_MINER_NAME + fc::to_string( i ), HIVE_MIN_PRODUCER_REWARD.amount.value );
    witness_create( HIVE_INIT_MINER_NAME + fc::to_string( i ), init_account_priv_key, "foo.bar", init_account_pub_key, HIVE_MIN_PRODUCER_REWARD.amount );
  }

  validate_database();
  } catch ( const fc::exception& e )
  {
    edump( (e.to_detail_string()) );
    throw;
  }

  return;
}

clean_database_fixture::~clean_database_fixture()
{ try {
  // If we're unwinding due to an exception, don't do any more checks.
  // This way, boost test's last checkpoint tells us approximately where the error was.
  if( !std::uncaught_exception() )
  {
    BOOST_CHECK( db->get_node_properties().skip_flags == database::skip_nothing );
  }

  if( data_dir )
    db->wipe( data_dir->path(), data_dir->path(), true );
  return;
} FC_CAPTURE_AND_LOG( () )
  exit(1);
}

void clean_database_fixture::validate_database()
{
  database_fixture::validate_database();
  rc_plugin->validate_database();
}

void clean_database_fixture::resize_shared_mem( uint64_t size, fc::optional<uint32_t> hardfork )
{
  db->wipe( data_dir->path(), data_dir->path(), true );
  int argc = boost::unit_test::framework::master_test_suite().argc;
  char** argv = boost::unit_test::framework::master_test_suite().argv;
  for( int i=1; i<argc; i++ )
  {
    const std::string arg = argv[i];
    if( arg == "--record-assert-trip" )
      fc::enable_record_assert_trip = true;
    if( arg == "--show-test-names" )
      std::cout << "running test " << boost::unit_test::framework::current_test_case().p_name << std::endl;
  }
  init_account_pub_key = init_account_priv_key.get_public_key();

  {
    hive::chain::open_args args;
    args.data_dir = data_dir->path();
    args.shared_mem_dir = args.data_dir;
    args.initial_supply = INITIAL_TEST_SUPPLY;
    args.hbd_initial_supply = HBD_INITIAL_TEST_SUPPLY;
    args.shared_file_size = size;
    args.database_cfg = hive::utilities::default_database_configuration();
    db->open( args );
  }

  boost::program_options::variables_map options;

  inject_hardfork( hardfork.valid() ? ( *hardfork ) : HIVE_BLOCKCHAIN_VERSION.minor_v() );

  vest( "initminer", 10000 );

  // Fill up the rest of the required miners
  for( int i = HIVE_NUM_INIT_MINERS; i < HIVE_MAX_WITNESSES; i++ )
  {
    account_create( HIVE_INIT_MINER_NAME + fc::to_string( i ), init_account_pub_key );
    fund( HIVE_INIT_MINER_NAME + fc::to_string( i ), HIVE_MIN_PRODUCER_REWARD.amount.value );
    witness_create( HIVE_INIT_MINER_NAME + fc::to_string( i ), init_account_priv_key, "foo.bar", init_account_pub_key, HIVE_MIN_PRODUCER_REWARD.amount );
  }

  validate_database();
}

void clean_database_fixture::inject_hardfork( uint32_t hardfork )
{
  generate_block();
  db->set_hardfork( hardfork );
  generate_block();
}

hardfork_database_fixture::hardfork_database_fixture( uint16_t shared_file_size_in_mb, uint32_t hardfork )
                            : clean_database_fixture( shared_file_size_in_mb, hardfork )
{
}

hardfork_database_fixture::~hardfork_database_fixture()
{
}

genesis_database_fixture::genesis_database_fixture( uint16_t shared_file_size_in_mb )
  : clean_database_fixture( shared_file_size_in_mb, 0 )
{}

genesis_database_fixture::~genesis_database_fixture()
{}

curation_database_fixture::curation_database_fixture( uint16_t shared_file_size_in_mb )
  : clean_database_fixture( ( configuration_data.set_cashout_related_values( //apply HF25 mainnet values
    0, 60 * 60 * 24, 60 * 60 * 24 * 2, 60 * 60 * 24 * 7, 60 * 60 * 12 ), shared_file_size_in_mb ) )
{
}

curation_database_fixture::~curation_database_fixture()
{
  configuration_data.reset();
}

cluster_database_fixture::cluster_database_fixture( uint16_t _shared_file_size_in_mb )
                            : shared_file_size_in_mb( _shared_file_size_in_mb )
{
  configuration_data.set_cashout_related_values( //apply HF25 mainnet values
    0, 60 * 60 * 24, 60 * 60 * 24 * 2, 60 * 60 * 24 * 7, 60 * 60 * 12 );
}

cluster_database_fixture::~cluster_database_fixture()
{
  configuration_data.reset();
}

live_database_fixture::live_database_fixture()
{
  try
  {
    int argc = boost::unit_test::framework::master_test_suite().argc;
    char** argv = boost::unit_test::framework::master_test_suite().argv;

    ilog( "Loading saved chain" );
    _chain_dir = fc::current_path() / "test_blockchain";
    FC_ASSERT( fc::exists( _chain_dir ), "Requires blockchain to test on in ./test_blockchain" );

    appbase::app().register_plugin< ah_plugin >();
    appbase::app().initialize< ah_plugin >( argc, argv );

    db = &appbase::app().get_plugin< hive::plugins::chain::chain_plugin >().db();
    BOOST_REQUIRE( db );

    {
      hive::chain::open_args args;
      args.data_dir = _chain_dir;
      args.shared_mem_dir = args.data_dir;
      args.database_cfg = hive::utilities::default_database_configuration();
      db->open( args );
    }

    validate_database();
    generate_block();

    ilog( "Done loading saved chain" );
  }
  FC_LOG_AND_RETHROW()
}

live_database_fixture::~live_database_fixture()
{
  try
  {
    // If we're unwinding due to an exception, don't do any more checks.
    // This way, boost test's last checkpoint tells us approximately where the error was.
    if( !std::uncaught_exception() )
    {
      BOOST_CHECK( db->get_node_properties().skip_flags == database::skip_nothing );
    }

    db->pop_block();
    db->close();
    return;
  }
  FC_CAPTURE_AND_LOG( () )
  exit(1);
}

fc::ecc::private_key database_fixture::generate_private_key(string seed)
{
  static const fc::ecc::private_key committee = fc::ecc::private_key::regenerate( fc::sha256::hash( string( "init_key" ) ) );
  if( seed == "init_key" )
    return committee;
  return fc::ecc::private_key::regenerate( fc::sha256::hash( seed ) );
}

#ifdef HIVE_ENABLE_SMT
asset_symbol_type database_fixture::get_new_smt_symbol( uint8_t token_decimal_places, chain::database* db )
{
  // The list of available nais is not dependent on SMT desired precision (token_decimal_places).
  static std::vector< asset_symbol_type >::size_type next_nai = 0;
  auto available_nais = db->get< nai_pool_object >().pool();
  FC_ASSERT( available_nais.size() > 0, "No available nai returned by get_nai_pool." );
  const asset_symbol_type& new_nai = available_nais[ next_nai++ % available_nais.size() ];
  // Note that token's precision is needed now, when creating actual symbol.
  return asset_symbol_type::from_nai( new_nai.to_nai(), token_decimal_places );
}
#endif

void database_fixture::open_database( uint16_t shared_file_size_in_mb )
{
  if( !data_dir )
  {
    data_dir = fc::temp_directory( hive::utilities::temp_directory_path() );
    db->_log_hardforks = false;

    idump( (data_dir->path()) );

    hive::chain::open_args args;
    args.data_dir = data_dir->path();
    args.shared_mem_dir = args.data_dir;
    args.initial_supply = INITIAL_TEST_SUPPLY;
    args.hbd_initial_supply = HBD_INITIAL_TEST_SUPPLY;
    args.shared_file_size = 1024 * 1024 * shared_file_size_in_mb; // 8MB(default) or more:  file for testing
    args.database_cfg = hive::utilities::default_database_configuration();
    db->open(args);
  }
  else
  {
    idump( (data_dir->path()) );
  }
}

void database_fixture::generate_block(uint32_t skip, const fc::ecc::private_key& key, int miss_blocks)
{
  skip |= default_skip;
  db_plugin->debug_generate_blocks( hive::utilities::key_to_wif( key ), 1, skip, miss_blocks );
}

void database_fixture::generate_blocks( uint32_t block_count )
{
  auto produced = db_plugin->debug_generate_blocks( debug_key, block_count, default_skip, 0 );
  BOOST_REQUIRE( produced == block_count );
}

void database_fixture::generate_blocks(fc::time_point_sec timestamp, bool miss_intermediate_blocks)
{
  db_plugin->debug_generate_blocks_until( debug_key, timestamp, miss_intermediate_blocks, default_skip );
  BOOST_REQUIRE( ( db->head_block_time() - timestamp ).to_seconds() < HIVE_BLOCK_INTERVAL );
}

void database_fixture::generate_seconds_blocks( uint32_t seconds, bool skip_interm_blocks )
{
  fc::time_point_sec timestamp = db->head_block_time();
  timestamp += fc::seconds(seconds);
  generate_blocks( timestamp, skip_interm_blocks );
}

void database_fixture::generate_days_blocks( uint32_t days, bool skip_interm_blocks )
{
  generate_seconds_blocks( days * 24 * 3600, skip_interm_blocks );
}

fc::string database_fixture::get_current_time_iso_string() const
{
  fc::time_point_sec current_time = db->head_block_time();
  return current_time.to_iso_string();
}

const account_object& database_fixture::account_create(
  const string& name,
  const string& creator,
  const private_key_type& creator_key,
  const share_type& fee,
  const public_key_type& key,
  const public_key_type& post_key,
  const string& json_metadata
  )
{
  try
  {
    auto actual_fee = std::min( fee, db->get_witness_schedule_object().median_props.account_creation_fee.amount );
    auto fee_remainder = fee - actual_fee;

    account_create_operation op;
    op.new_account_name = name;
    op.creator = creator;
    op.fee = asset( actual_fee, HIVE_SYMBOL );
    op.owner = authority( 1, key, 1 );
    op.active = authority( 1, key, 1 );
    op.posting = authority( 1, post_key, 1 );
    op.memo_key = key;
    op.json_metadata = json_metadata;

    trx.operations.push_back( op );

    trx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    sign( trx, creator_key );
    trx.validate();
    db->push_transaction( trx, 0 );
    trx.clear();

    if( fee_remainder > 0 )
    {
      vest( HIVE_INIT_MINER_NAME, name, asset( fee_remainder, HIVE_SYMBOL ) );
    }

    const account_object& acct = db->get_account( name );

    return acct;
  }
  FC_CAPTURE_AND_RETHROW( (name)(creator) )
}

const account_object& database_fixture::account_create(
  const string& name,
  const public_key_type& key,
  const public_key_type& post_key
)
{
  try
  {
    return account_create(
      name,
      HIVE_INIT_MINER_NAME,
      init_account_priv_key,
      std::max( db->get_witness_schedule_object().median_props.account_creation_fee.amount * HIVE_CREATE_ACCOUNT_WITH_HIVE_MODIFIER, share_type( 100 ) ),
      key,
      post_key,
      "" );
  }
  FC_CAPTURE_AND_RETHROW( (name) );
}

const account_object& database_fixture::account_create_default_fee(
  const string& name,
  const public_key_type& key,
  const public_key_type& post_key
)
{
  try
  {
    return account_create(
      name,
      HIVE_INIT_MINER_NAME,
      init_account_priv_key,
      db->get_witness_schedule_object().median_props.account_creation_fee.amount,
      key,
      post_key,
      "" );
  }
  FC_CAPTURE_AND_RETHROW( (name) );
}

const account_object& database_fixture::account_create(
  const string& name,
  const public_key_type& key
)
{
  return account_create( name, key, key );
}

const witness_object& database_fixture::witness_create(
  const string& owner,
  const private_key_type& owner_key,
  const string& url,
  const public_key_type& signing_key,
  const share_type& fee )
{
  try
  {
    witness_update_operation op;
    op.owner = owner;
    op.url = url;
    op.block_signing_key = signing_key;
    op.fee = asset( fee, HIVE_SYMBOL );

    trx.operations.push_back( op );
    trx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    sign( trx, owner_key );
    trx.validate();
    db->push_transaction( trx, 0 );
    trx.clear();

    return db->get_witness( owner );
  }
  FC_CAPTURE_AND_RETHROW( (owner)(url) )
}

void database_fixture::fund(
  const string& account_name,
  const share_type& amount
  )
{
  try
  {
    transfer( HIVE_INIT_MINER_NAME, account_name, asset( amount, HIVE_SYMBOL ) );

  } FC_CAPTURE_AND_RETHROW( (account_name)(amount) )
}

void database_fixture::fund(
  const string& account_name,
  const asset& amount,
  bool update_print_rate
  )
{
  try
  {
    db_plugin->debug_update( [=]( database& db)
    {
      if( amount.symbol.space() == asset_symbol_type::smt_nai_space )
      {
        db.adjust_balance(account_name, amount);
        db.adjust_supply(amount);
        // Note that SMT have no equivalent of HBD, hence no virtual supply, hence no need to update it.
        return;
      }

      const auto& median_feed = db.get_feed_history();
      if( amount.symbol == HBD_SYMBOL )
      {
        if( median_feed.current_median_history.is_null() )
          db.modify( median_feed, [&]( feed_history_object& f )
        {
          f.current_median_history = price( asset( 1, HBD_SYMBOL ), asset( 1, HIVE_SYMBOL ) );
          f.market_median_history = f.current_median_history;
          f.current_min_history = f.current_median_history;
          f.current_max_history = f.current_median_history;
        } );
      }

      db.modify( db.get_account( account_name ), [&]( account_object& a )
      {
        if( amount.symbol == HIVE_SYMBOL )
          a.balance += amount;
        else if( amount.symbol == HBD_SYMBOL )
        {
          a.hbd_balance += amount;
          a.hbd_seconds_last_update = db.head_block_time();
        }
      });

      db.modify( db.get_dynamic_global_properties(), [&]( dynamic_global_property_object& gpo )
      {
        if( amount.symbol == HIVE_SYMBOL )
          gpo.current_supply += amount;
        else if( amount.symbol == HBD_SYMBOL )
        {
          gpo.current_hbd_supply += amount;
          gpo.virtual_supply = gpo.current_supply + gpo.current_hbd_supply * median_feed.current_median_history;
        }
      });

      if( update_print_rate )
        db.update_virtual_supply();
    }, default_skip );
  }
  FC_CAPTURE_AND_RETHROW( (account_name)(amount) )
}

void database_fixture::convert(
  const string& account_name,
  const asset& amount )
{
  try
  {
    if ( amount.symbol == HIVE_SYMBOL )
    {
      db->adjust_balance( account_name, -amount );
      db->adjust_balance( account_name, db->to_hbd( amount ) );
      db->adjust_supply( -amount );
      db->adjust_supply( db->to_hbd( amount ) );
    }
    else if ( amount.symbol == HBD_SYMBOL )
    {
      db->adjust_balance( account_name, -amount );
      db->adjust_balance( account_name, db->to_hive( amount ) );
      db->adjust_supply( -amount );
      db->adjust_supply( db->to_hive( amount ) );
    }
  } FC_CAPTURE_AND_RETHROW( (account_name)(amount) )
}

void database_fixture::transfer(
  const string& from,
  const string& to,
  const asset& amount )
{
  try
  {
    transfer_operation op;
    op.from = from;
    op.to = to;
    op.amount = amount;

    trx.operations.push_back( op );
    trx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    trx.validate();

    if( from == HIVE_INIT_MINER_NAME )
    {
      sign( trx, init_account_priv_key );
    }

    db->push_transaction( trx, ~0 );
    trx.clear();
  } FC_CAPTURE_AND_RETHROW( (from)(to)(amount) )
}

void database_fixture::push_transaction( const operation& op, const fc::ecc::private_key& key )
{
  signed_transaction tx;
  tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
  tx.operations.push_back( op );
  sign( tx, key );
  db->push_transaction( tx, 0 );
}

void database_fixture::vest( const string& from, const string& to, const asset& amount )
{
  try
  {
    FC_ASSERT( amount.symbol == HIVE_SYMBOL, "Can only vest TESTS" );

    transfer_to_vesting_operation op;
    op.from = from;
    op.to = to;
    op.amount = amount;

    trx.operations.push_back( op );
    trx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    trx.validate();

    // This sign() call fixes some tests, like withdraw_vesting_apply, that use this method
    //   with debug_plugin such that trx may be re-applied with less generous skip flags.
    if( from == HIVE_INIT_MINER_NAME )
    {
      sign( trx, init_account_priv_key );
    }

    db->push_transaction( trx, ~0 );
    trx.clear();
  } FC_CAPTURE_AND_RETHROW( (from)(to)(amount) )
}

void database_fixture::vest( const string& from, const share_type& amount )
{
  try
  {
    transfer_to_vesting_operation op;
    op.from = from;
    op.to = "";
    op.amount = asset( amount, HIVE_SYMBOL );

    trx.operations.push_back( op );
    trx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    trx.validate();

    if( from == HIVE_INIT_MINER_NAME )
    {
      sign( trx, init_account_priv_key );
    }

    db->push_transaction( trx, ~0 );
    trx.clear();
  } FC_CAPTURE_AND_RETHROW( (from)(amount) )
}

void database_fixture::vest( const string& from, const string& to, const asset& amount, const fc::ecc::private_key& key )
{
  FC_ASSERT( amount.symbol == HIVE_SYMBOL, "Can only vest TESTS" );

  transfer_to_vesting_operation op;
  op.from = from;
  op.to = to;
  op.amount = amount;

  push_transaction( op, key );
}

void database_fixture::proxy( const string& account, const string& proxy )
{
  try
  {
    account_witness_proxy_operation op;
    op.account = account;
    op.proxy = proxy;
    trx.operations.push_back( op );
    trx.set_expiration(db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION);
    db->push_transaction( trx, ~0 );
    trx.clear();
  } FC_CAPTURE_AND_RETHROW( (account)(proxy) )
}

void database_fixture::set_price_feed( const price& new_price, bool stop_at_update_block )
{
  for( size_t i = 1; i < 8; i++ )
  {
    witness_set_properties_operation op;
    op.owner = HIVE_INIT_MINER_NAME + fc::to_string( i );
    op.props[ "hbd_exchange_rate" ] = fc::raw::pack_to_vector( new_price );
    op.props[ "key" ] = fc::raw::pack_to_vector( init_account_pub_key );

    trx.operations.push_back( op );
    trx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    db->push_transaction( trx, ~0 );
    trx.clear();
  }

  if( stop_at_update_block )
    generate_blocks( HIVE_FEED_INTERVAL_BLOCKS - ( db->head_block_num() % HIVE_FEED_INTERVAL_BLOCKS ) );
  else
    generate_blocks( HIVE_BLOCKS_PER_HOUR );

  BOOST_REQUIRE(
#ifdef IS_TEST_NET
    !db->skip_price_feed_limit_check ||
#endif
    db->get(feed_history_id_type()).current_median_history == new_price
  );
}

void database_fixture::set_witness_props( const flat_map< string, vector< char > >& props )
{
  trx.clear();
  for( size_t i=0; i<HIVE_MAX_WITNESSES; i++ )
  {
    witness_set_properties_operation op;
    op.owner = HIVE_INIT_MINER_NAME + (i == 0 ? "" : fc::to_string( i ));
    op.props = props;
    if( props.find( "key" ) == props.end() )
      op.props["key"] = fc::raw::pack_to_vector( init_account_pub_key );

    trx.operations.push_back( op );
    trx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    db->push_transaction( trx, ~0 );
    trx.clear();
  }

  const witness_schedule_object* wso = &(db->get_witness_schedule_object());
  uint32_t old_next_shuffle = wso->next_shuffle_block_num;

  for( size_t i=0; i<2*HIVE_MAX_WITNESSES+1; i++ )
  {
    generate_block();
    wso = &(db->get_witness_schedule_object());
    if( wso->next_shuffle_block_num != old_next_shuffle )
      return;
  }
  FC_ASSERT( false, "Couldn't apply properties in ${n} blocks", ("n", 2*HIVE_MAX_WITNESSES+1) );
}

account_id_type database_fixture::get_account_id( const string& account_name )const
{
  return db->get_account( account_name ).get_id();
}

asset database_fixture::get_balance( const string& account_name )const
{
  return db->get_account( account_name ).get_balance();
}

asset database_fixture::get_hbd_balance( const string& account_name )const
{
  return db->get_account( account_name ).get_hbd_balance();
}

asset database_fixture::get_savings( const string& account_name )const
{
  return db->get_account( account_name ).get_savings();
}

asset database_fixture::get_hbd_savings( const string& account_name )const
{
  return db->get_account( account_name ).get_hbd_savings();
}

asset database_fixture::get_rewards( const string& account_name )const
{
  return db->get_account( account_name ).get_rewards();
}

asset database_fixture::get_hbd_rewards( const string& account_name )const
{
  return db->get_account( account_name ).get_hbd_rewards();
}

asset database_fixture::get_vesting( const string& account_name )const
{
  return db->get_account( account_name ).get_vesting();
}

asset database_fixture::get_vest_rewards( const string& account_name )const
{
  return db->get_account( account_name ).get_vest_rewards();
}

asset database_fixture::get_vest_rewards_as_hive( const string& account_name )const
{
  return db->get_account( account_name ).get_vest_rewards_as_hive();
}

void database_fixture::post_comment_internal( const std::string& _author, const std::string& _permlink, const std::string& _title, const std::string& _body, const std::string& _parent_permlink, const fc::ecc::private_key& _key )
{
  comment_operation comment;

  comment.author = _author;
  comment.permlink = _permlink;
  comment.title = _title;
  comment.body = _body;
  comment.parent_permlink = _parent_permlink;

  signed_transaction trx;
  trx.operations.push_back( comment );
  trx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
  sign( trx, _key );
  db->push_transaction( trx, 0 );
  trx.signatures.clear();
  trx.operations.clear();
}

void database_fixture::post_comment_with_block_generation( std::string _author, std::string _permlink, std::string _title, std::string _body, std::string _parent_permlink, const fc::ecc::private_key& _key)
{
  generate_blocks( db->head_block_time() + HIVE_MIN_ROOT_COMMENT_INTERVAL + fc::seconds( HIVE_BLOCK_INTERVAL ), true );

  post_comment_internal( _author, _permlink, _title, _body, _parent_permlink, _key );
}

void database_fixture::post_comment( std::string _author, std::string _permlink, std::string _title, std::string _body, std::string _parent_permlink, const fc::ecc::private_key& _key)
{
  post_comment_internal( _author, _permlink, _title, _body, _parent_permlink, _key );
}

void database_fixture::vote( std::string _author, std::string _permlink, std::string _voter, int16_t _weight, const fc::ecc::private_key& _key )
{
  vote_operation vote;

  vote.author = _author;
  vote.permlink = _permlink;
  vote.voter = _voter;
  vote.weight = _weight;
 
  signed_transaction trx;
  trx.operations.push_back( vote );
  trx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
  sign( trx, _key );
  db->push_transaction( trx, 0 );
  trx.signatures.clear();
  trx.operations.clear();
}

void database_fixture::sign(signed_transaction& trx, const fc::ecc::private_key& key)
{
  trx.sign( key, db->get_chain_id(), default_sig_canon );
}

vector< operation > database_fixture::get_last_operations( uint32_t num_ops )
{
  vector< operation > ops;
  const auto& acc_hist_idx = db->get_index< account_history_index >().indices().get< by_id >();
  auto itr = acc_hist_idx.end();

  while( itr != acc_hist_idx.begin() && ops.size() < num_ops )
  {
    itr--;
    const buffer_type& _serialized_op = db->get(itr->op).serialized_op;
    std::vector<char> serialized_op;
    serialized_op.reserve( _serialized_op.size() );
    std::copy( _serialized_op.begin(), _serialized_op.end(), std::back_inserter( serialized_op ) );
    ops.push_back( fc::raw::unpack_from_vector< hive::chain::operation >( serialized_op ) );
  }

  return ops;
}

void database_fixture::validate_database()
{
  try
  {
    db->validate_invariants();
#ifdef HIVE_ENABLE_SMT
    db->validate_smt_invariants();
#endif
  }
  FC_LOG_AND_RETHROW();
}

#ifdef HIVE_ENABLE_SMT

template< typename T >
asset_symbol_type t_smt_database_fixture< T >::create_smt_with_nai( const string& account_name, const fc::ecc::private_key& key,
  uint32_t nai, uint8_t token_decimal_places )
{
  smt_create_operation op;
  signed_transaction tx;
  try
  {
    fund( account_name, 10 * 1000 * 1000 );
    this->generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    convert( account_name, ASSET( "5000.000 TESTS" ) );

    op.symbol = asset_symbol_type::from_nai( nai, token_decimal_places );
    op.precision = op.symbol.decimals();
    op.smt_creation_fee = this->db->get_dynamic_global_properties().smt_creation_fee;
    op.control_account = account_name;

    tx.operations.push_back( op );
    tx.set_expiration( this->db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.sign( key, this->db->get_chain_id(), fc::ecc::bip_0062 );

    this->db->push_transaction( tx, 0 );

    this->generate_block();
  }
  FC_LOG_AND_RETHROW();

  return op.symbol;
}

template< typename T >
asset_symbol_type t_smt_database_fixture< T >::create_smt( const string& account_name, const fc::ecc::private_key& key,
  uint8_t token_decimal_places )
{
  asset_symbol_type symbol;
  try
  {
    auto nai_symbol = this->get_new_smt_symbol( token_decimal_places, this->db );
    symbol = create_smt_with_nai( account_name, key, nai_symbol.to_nai(), token_decimal_places );
  }
  FC_LOG_AND_RETHROW();

  return symbol;
}

void sub_set_create_op( smt_create_operation* op, account_name_type control_acount, chain::database& db )
{
  op->precision = op->symbol.decimals();
  op->smt_creation_fee = db.get_dynamic_global_properties().smt_creation_fee;
  op->control_account = control_acount;
}

void set_create_op( smt_create_operation* op, account_name_type control_account, uint8_t token_decimal_places, chain::database& db )
{
  op->symbol = database_fixture::get_new_smt_symbol( token_decimal_places, &db );
  sub_set_create_op( op, control_account, db );
}

void set_create_op( smt_create_operation* op, account_name_type control_account, uint32_t token_nai, uint8_t token_decimal_places, chain::database& db )
{
  op->symbol.from_nai(token_nai, token_decimal_places);
  sub_set_create_op( op, control_account, db );
}

template< typename T >
std::array<asset_symbol_type, 3> t_smt_database_fixture< T >::create_smt_3(const char* control_account_name, const fc::ecc::private_key& key)
{
  smt_create_operation op0;
  smt_create_operation op1;
  smt_create_operation op2;

  try
  {
    fund( control_account_name, 10 * 1000 * 1000 );
    this->generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    convert( control_account_name, ASSET( "5000.000 TESTS" ) );

    set_create_op( &op0, control_account_name, 0, *this->db );
    set_create_op( &op1, control_account_name, 1, *this->db );
    set_create_op( &op2, control_account_name, 1, *this->db );

    signed_transaction tx;
    tx.operations.push_back( op0 );
    tx.operations.push_back( op1 );
    tx.operations.push_back( op2 );
    tx.set_expiration( this->db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.sign( key, this->db->get_chain_id(), fc::ecc::bip_0062 );
    this->db->push_transaction( tx, 0 );

    this->generate_block();

    std::array<asset_symbol_type, 3> retVal;
    retVal[0] = op0.symbol;
    retVal[1] = op1.symbol;
    retVal[2] = op2.symbol;
    std::sort(retVal.begin(), retVal.end(),
        [](const asset_symbol_type & a, const asset_symbol_type & b) -> bool
    {
      return a.to_nai() < b.to_nai();
    });
    return retVal;
  }
  FC_LOG_AND_RETHROW();
}

void push_invalid_operation(const operation& invalid_op, const fc::ecc::private_key& key, database* db)
{
  signed_transaction tx;
  tx.operations.push_back( invalid_op );
  tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
  tx.sign( key, db->get_chain_id(), fc::ecc::bip_0062 );
  HIVE_REQUIRE_THROW( db->push_transaction( tx, database::skip_transaction_dupe_check ), fc::assert_exception );
}

template< typename T >
void t_smt_database_fixture< T >::create_invalid_smt( const char* control_account_name, const fc::ecc::private_key& key )
{
  // Fail due to precision too big.
  smt_create_operation op_precision;
  HIVE_REQUIRE_THROW( set_create_op( &op_precision, control_account_name, HIVE_ASSET_MAX_DECIMALS + 1, *this->db ), fc::assert_exception );
}

template< typename T >
void t_smt_database_fixture< T >::create_conflicting_smt( const asset_symbol_type existing_smt, const char* control_account_name,
  const fc::ecc::private_key& key )
{
  // Fail due to the same nai & precision.
  smt_create_operation op_same;
  set_create_op( &op_same, control_account_name, existing_smt.to_nai(), existing_smt.decimals(), *this->db );
  push_invalid_operation( op_same, key, this->db );
  // Fail due to the same nai (though different precision).
  smt_create_operation op_same_nai;
  set_create_op( &op_same_nai, control_account_name, existing_smt.to_nai(), existing_smt.decimals() == 0 ? 1 : 0, *this->db );
  push_invalid_operation (op_same_nai, key, this->db );
}

template< typename T >
smt_generation_unit t_smt_database_fixture< T >::get_generation_unit( const units& hive_unit, const units& token_unit )
{
  smt_generation_unit ret;

  ret.hive_unit = hive_unit;
  ret.token_unit = token_unit;

  return ret;
}

template< typename T >
smt_capped_generation_policy t_smt_database_fixture< T >::get_capped_generation_policy
(
  const smt_generation_unit& pre_soft_cap_unit,
  const smt_generation_unit& post_soft_cap_unit,
  uint16_t soft_cap_percent,
  uint32_t min_unit_ratio,
  uint32_t max_unit_ratio
)
{
  smt_capped_generation_policy ret;

  ret.pre_soft_cap_unit = pre_soft_cap_unit;
  ret.post_soft_cap_unit = post_soft_cap_unit;

  ret.soft_cap_percent = soft_cap_percent;

  ret.min_unit_ratio = min_unit_ratio;
  ret.max_unit_ratio = max_unit_ratio;

  return ret;
}

template asset_symbol_type t_smt_database_fixture< clean_database_fixture >::create_smt( const string& account_name, const fc::ecc::private_key& key, uint8_t token_decimal_places );

template asset_symbol_type t_smt_database_fixture< database_fixture >::create_smt( const string& account_name, const fc::ecc::private_key& key, uint8_t token_decimal_places );

template void t_smt_database_fixture< clean_database_fixture >::create_invalid_smt( const char* control_account_name, const fc::ecc::private_key& key );
template void t_smt_database_fixture< clean_database_fixture >::create_conflicting_smt( const asset_symbol_type existing_smt, const char* control_account_name, const fc::ecc::private_key& key );
template std::array<asset_symbol_type, 3> t_smt_database_fixture< clean_database_fixture >::create_smt_3( const char* control_account_name, const fc::ecc::private_key& key );

template smt_generation_unit t_smt_database_fixture< clean_database_fixture >::get_generation_unit( const units& hive_unit, const units& token_unit );
template smt_capped_generation_policy t_smt_database_fixture< clean_database_fixture >::get_capped_generation_policy
(
  const smt_generation_unit& pre_soft_cap_unit,
  const smt_generation_unit& post_soft_cap_unit,
  uint16_t soft_cap_percent,
  uint32_t min_unit_ratio,
  uint32_t max_unit_ratio
);

#endif

void sps_proposal_database_fixture::plugin_prepare()
{
  int argc = boost::unit_test::framework::master_test_suite().argc;
  char** argv = boost::unit_test::framework::master_test_suite().argv;
  for( int i=1; i<argc; i++ )
  {
    const std::string arg = argv[i];
    if( arg == "--record-assert-trip" )
      fc::enable_record_assert_trip = true;
    if( arg == "--show-test-names" )
      std::cout << "running test " << boost::unit_test::framework::current_test_case().p_name << std::endl;
  }

  db_plugin = &appbase::app().register_plugin< hive::plugins::debug_node::debug_node_plugin >();
  init_account_pub_key = init_account_priv_key.get_public_key();

  db_plugin->logging = false;
  appbase::app().initialize<
    hive::plugins::debug_node::debug_node_plugin
  >( argc, argv );

  db = &appbase::app().get_plugin< hive::plugins::chain::chain_plugin >().db();
  BOOST_REQUIRE( db );

  open_database();

  generate_block();
  db->set_hardfork( HIVE_NUM_HARDFORKS );
  generate_block();


  validate_database();
}

int64_t sps_proposal_database_fixture::create_proposal( std::string creator, std::string receiver,
                  time_point_sec start_date, time_point_sec end_date,
                  asset daily_pay, const fc::ecc::private_key& key, bool with_block_generation )
{
  signed_transaction tx;
  create_proposal_operation op;

  op.creator = creator;
  op.receiver = receiver;

  op.start_date = start_date;
  op.end_date = end_date;

  op.daily_pay = daily_pay;

  static uint32_t cnt = 0;
  op.subject = std::to_string( cnt );

  const std::string permlink = "permlink" + std::to_string( cnt );
  if( with_block_generation )
    post_comment_with_block_generation(creator, permlink, "title", "body", "test", key);
  else
    post_comment(creator, permlink, "title", "body", "test", key);

  op.permlink = permlink;

  tx.operations.push_back( op );
  tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
  sign( tx, key );
  db->push_transaction( tx, 0 );
  tx.signatures.clear();
  tx.operations.clear();

  const auto& proposal_idx = db-> template get_index< proposal_index >().indices(). template get< by_proposal_id >();
  auto itr = proposal_idx.end();
  BOOST_REQUIRE( proposal_idx.begin() != itr );
  --itr;
  BOOST_REQUIRE( creator == itr->creator );

  //An unique subject is generated by cnt
  ++cnt;

  return itr->proposal_id;
}

void sps_proposal_database_fixture::update_proposal(uint64_t proposal_id, std::string creator, asset daily_pay, std::string subject, std::string permlink, const fc::ecc::private_key& key, time_point_sec* end_date)
{
  signed_transaction tx;
  update_proposal_operation op;

  op.proposal_id = proposal_id;
  op.creator = creator;
  op.daily_pay = daily_pay;
  op.subject = subject;
  op.permlink = permlink;

  if (end_date != nullptr) {
    update_proposal_end_date ped;
    ped.end_date = *end_date;
    op.extensions.insert(ped);
  }

  tx.operations.push_back( op );
  tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
  sign( tx, key );
  db->push_transaction( tx, 0 );
  tx.signatures.clear();
  tx.operations.clear();
}

void sps_proposal_database_fixture::vote_proposal( std::string voter, const std::vector< int64_t >& id_proposals, bool approve, const fc::ecc::private_key& key )
{
  update_proposal_votes_operation op;

  op.voter = voter;
  op.proposal_ids.insert(id_proposals.cbegin(), id_proposals.cend());
  op.approve = approve;

  signed_transaction tx;
  tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
  tx.operations.push_back( op );
  sign( tx, key );
  db->push_transaction( tx, 0 );
}

bool sps_proposal_database_fixture::exist_proposal( int64_t id )
{
  const auto& proposal_idx = db->get_index< proposal_index >().indices(). template get< by_proposal_id >();
  return proposal_idx.find( id ) != proposal_idx.end();
}

const proposal_object* sps_proposal_database_fixture::find_proposal( int64_t id )
{
  const auto& proposal_idx = db->get_index< proposal_index >().indices(). template get< by_proposal_id >();
  auto found = proposal_idx.find( id );

  if( found != proposal_idx.end() )
    return &(*found);
  else
    return nullptr;
}

void sps_proposal_database_fixture::remove_proposal(account_name_type _deleter, flat_set<int64_t> _proposal_id, const fc::ecc::private_key& _key)
{
  remove_proposal_operation rp;
  rp.proposal_owner = _deleter;
  rp.proposal_ids   = _proposal_id;

  signed_transaction trx;
  trx.operations.push_back( rp );
  trx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
  sign( trx, _key );
  db->push_transaction( trx, 0 );
  trx.signatures.clear();
  trx.operations.clear();
}

bool sps_proposal_database_fixture::find_vote_for_proposal(const std::string& _user, int64_t _proposal_id)
{
  const auto& proposal_vote_idx = db->get_index< proposal_vote_index >().indices(). template get< by_voter_proposal >();
  auto found_vote = proposal_vote_idx.find( boost::make_tuple(_user, _proposal_id ) );
  return found_vote != proposal_vote_idx.end();
}

uint64_t sps_proposal_database_fixture::get_nr_blocks_until_maintenance_block()
{
  auto block_time = db->head_block_time();

  auto next_maintenance_time = db->get_dynamic_global_properties().next_maintenance_time;
  auto ret = ( next_maintenance_time - block_time ).to_seconds() / HIVE_BLOCK_INTERVAL;

  FC_ASSERT( next_maintenance_time >= block_time );

  return ret;
}

uint64_t sps_proposal_database_fixture::get_nr_blocks_until_daily_maintenance_block()
{
  auto block_time = db->head_block_time();

  auto next_maintenance_time = db->get_dynamic_global_properties().next_daily_maintenance_time;
  auto ret = ( next_maintenance_time - block_time ).to_seconds() / HIVE_BLOCK_INTERVAL;

  FC_ASSERT( next_maintenance_time >= block_time );

  return ret;
}

void sps_proposal_database_fixture::witness_vote( account_name_type _voter, account_name_type _witness, const fc::ecc::private_key& _key, bool _approve )
{
  signed_transaction tx;
  account_witness_vote_operation op;
  op.account = _voter;
  op.witness = _witness;
  op.approve = _approve;

  tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
  tx.operations.push_back( op );
  sign( tx, _key );
  db->push_transaction( tx, 0 );
}

void sps_proposal_database_fixture::proxy( account_name_type _account, account_name_type _proxy, const fc::ecc::private_key& _key )
{
  signed_transaction tx;
  account_witness_proxy_operation op;
  op.account = _account;
  op.proxy = _proxy;

  tx.operations.push_back( op );
  tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
  sign( tx, _key );
  db->push_transaction( tx, 0 );
}

void hf23_database_fixture::delegate_vest( const string& delegator, const string& delegatee, const asset& amount, const fc::ecc::private_key& key )
{
  delegate_vesting_shares_operation op;
  op.vesting_shares = amount;
  op.delegator = delegator;
  op.delegatee = delegatee;

  push_transaction( op, key );
}

void delayed_vote_database_fixture::witness_vote( const std::string& account, const std::string& witness, const bool approve, const fc::ecc::private_key& key )
{
  account_witness_vote_operation op;

  op.account = account;
  op.witness = witness;
  op.approve = approve;

  push_transaction( op, key );
}

void delayed_vote_database_fixture::decline_voting_rights( const string& account, const bool decline, const fc::ecc::private_key& key )
{
  decline_voting_rights_operation op;
  op.account = account;
  op.decline = decline;

  push_transaction( op, key );
}

time_point_sec delayed_vote_database_fixture::move_forward_with_update( const fc::microseconds& time, delayed_voting::opt_votes_update_data_items& items )
{
  std::vector<std::pair<account_name_type,votes_update_data> > tmp;
  tmp.reserve( items->size() );

  for(const votes_update_data& var : *items)
    tmp.emplace_back(var.account->name, var);

  items->clear();

  generate_blocks( db->head_block_time() + time );

  for(const auto& var : tmp)
  {
    auto x = var.second;
    x.account = &db->get_account(var.first);
    items->insert(x);
  }

    return db->head_block_time();
};

void delayed_vote_database_fixture::withdraw_vesting( const string& account, const asset& amount, const fc::ecc::private_key& key )
{
  FC_ASSERT( amount.symbol == VESTS_SYMBOL, "Can only withdraw VESTS");

  withdraw_vesting_operation op;
  op.account = account;
  op.vesting_shares = amount;

  push_transaction( op, key );
}

void delayed_vote_database_fixture::proxy( const string& account, const string& proxy, const fc::ecc::private_key& key )
{
  account_witness_proxy_operation op;
  op.account = account;
  op.proxy = proxy;
  trx.operations.push_back( op );

  push_transaction( op, key );
}

share_type delayed_vote_database_fixture::get_votes( const string& witness_name )
{
  const auto& idx = db->get_index< witness_index >().indices().get< by_name >();
  auto found = idx.find( witness_name );

  if( found == idx.end() )
    return 0;
  else
    return found->votes.value;
}

int32_t delayed_vote_database_fixture::get_user_voted_witness_count( const account_name_type& name )
{
  int32_t res = 0;

  const auto& vidx = db->get_index< witness_vote_index >().indices().get<by_account_witness>();
  auto itr = vidx.lower_bound( boost::make_tuple( name, account_name_type() ) );
  while( itr != vidx.end() && itr->account == name )
  {
    ++itr;
    ++res;
  }
  return res;
}

asset delayed_vote_database_fixture::to_vest( const asset& liquid, const bool to_reward_balance )
{
  const auto& cprops = db->get_dynamic_global_properties();
  price vesting_share_price = to_reward_balance ? cprops.get_reward_vesting_share_price() : cprops.get_vesting_share_price();

  return liquid * ( vesting_share_price );
}

template< typename COLLECTION >
fc::optional< size_t > delayed_vote_database_fixture::get_position_in_delayed_voting_array( const COLLECTION& collection, size_t nr_interval, size_t seconds )
{
  if( collection.empty() )
    return fc::optional< size_t >();

  auto time = collection[ 0 ].time + fc::seconds( HIVE_DELAYED_VOTING_INTERVAL_SECONDS * nr_interval ) + fc::seconds( seconds );

  size_t idx = 0;
  for( auto& item : collection )
  {
    auto end_of_day = item.time + fc::seconds( HIVE_DELAYED_VOTING_INTERVAL_SECONDS );

    if( end_of_day > time )
      return idx;

    ++idx;
  }

  return fc::optional< size_t >();
}

template< typename COLLECTION >
bool delayed_vote_database_fixture::check_collection( const COLLECTION& collection, ushare_type idx, const fc::time_point_sec& time, const ushare_type val )
{
  if( idx.value >= collection.size() )
    return false;
  else
  {
    bool check_time = collection[ idx.value ].time == time;
    bool check_val = collection[ idx.value ].val == val;
    return check_time && check_val;
  }
}

template< typename COLLECTION >
bool delayed_vote_database_fixture::check_collection( const COLLECTION& collection, const bool withdraw_executor, const share_type val, const account_object& obj )
{
  auto found = collection->find( { withdraw_executor, val, &obj } );
  if( found == collection->end() )
    return false;

  if( !found->account )
    return false;
  return ( found->withdraw_executor == withdraw_executor ) && ( found->val == val ) && ( found->account->get_id() == obj.get_id() );
}

using dvd_vector = std::vector< delayed_votes_data >;
using bip_dvd_vector = chainbase::t_vector< delayed_votes_data >;

template fc::optional< size_t > delayed_vote_database_fixture::get_position_in_delayed_voting_array< bip_dvd_vector >( const bip_dvd_vector& collection, size_t day, size_t minutes );
template bool delayed_vote_database_fixture::check_collection< dvd_vector >( const dvd_vector& collection, ushare_type idx, const fc::time_point_sec& time, const ushare_type val );
#ifndef ENABLE_STD_ALLOCATOR
template bool delayed_vote_database_fixture::check_collection< bip_dvd_vector >( const bip_dvd_vector& collection, ushare_type idx, const fc::time_point_sec& time, const ushare_type val );
#endif
template bool delayed_vote_database_fixture::check_collection< delayed_voting::opt_votes_update_data_items >( const delayed_voting::opt_votes_update_data_items& collection, const bool withdraw_executor, const share_type val, const account_object& obj );

json_rpc_database_fixture::json_rpc_database_fixture()
{
  try {
  int argc = boost::unit_test::framework::master_test_suite().argc;
  char** argv = boost::unit_test::framework::master_test_suite().argv;
  for( int i=1; i<argc; i++ )
  {
    const std::string arg = argv[i];
    if( arg == "--record-assert-trip" )
      fc::enable_record_assert_trip = true;
    if( arg == "--show-test-names" )
      std::cout << "running test " << boost::unit_test::framework::current_test_case().p_name << std::endl;
  }

  appbase::app().register_plugin< ah_plugin >();
  db_plugin = &appbase::app().register_plugin< hive::plugins::debug_node::debug_node_plugin >();
  appbase::app().register_plugin< hive::plugins::witness::witness_plugin >();
  rpc_plugin = &appbase::app().register_plugin< hive::plugins::json_rpc::json_rpc_plugin >();
  appbase::app().register_plugin< hive::plugins::block_api::block_api_plugin >();
  appbase::app().register_plugin< hive::plugins::database_api::database_api_plugin >();
  appbase::app().register_plugin< hive::plugins::condenser_api::condenser_api_plugin >();

  db_plugin->logging = false;
  appbase::app().initialize<
    ah_plugin,
    hive::plugins::debug_node::debug_node_plugin,
    hive::plugins::json_rpc::json_rpc_plugin,
    hive::plugins::block_api::block_api_plugin,
    hive::plugins::database_api::database_api_plugin,
    hive::plugins::condenser_api::condenser_api_plugin
    >( argc, argv );

  appbase::app().get_plugin< hive::plugins::condenser_api::condenser_api_plugin >().plugin_startup();

  rpc_plugin->finalize_startup();

  db = &appbase::app().get_plugin< hive::plugins::chain::chain_plugin >().db();
  BOOST_REQUIRE( db );

  init_account_pub_key = init_account_priv_key.get_public_key();

  open_database();

  generate_block();
  db->set_hardfork( HIVE_BLOCKCHAIN_VERSION.minor_v() );
  generate_block();

  vest( "initminer", 10000 );

  // Fill up the rest of the required miners
  for( int i = HIVE_NUM_INIT_MINERS; i < HIVE_MAX_WITNESSES; i++ )
  {
    account_create( HIVE_INIT_MINER_NAME + fc::to_string( i ), init_account_pub_key );
    fund( HIVE_INIT_MINER_NAME + fc::to_string( i ), HIVE_MIN_PRODUCER_REWARD.amount.value );
    witness_create( HIVE_INIT_MINER_NAME + fc::to_string( i ), init_account_priv_key, "foo.bar", init_account_pub_key, HIVE_MIN_PRODUCER_REWARD.amount );
  }

  validate_database();
  } catch ( const fc::exception& e )
  {
    edump( (e.to_detail_string()) );
    throw;
  }

  return;
}

json_rpc_database_fixture::~json_rpc_database_fixture()
{
  // If we're unwinding due to an exception, don't do any more checks.
  // This way, boost test's last checkpoint tells us approximately where the error was.
  if( !std::uncaught_exception() )
  {
    BOOST_CHECK( db->get_node_properties().skip_flags == database::skip_nothing );
  }

  if( data_dir )
    db->wipe( data_dir->path(), data_dir->path(), true );
  return;
}

fc::variant json_rpc_database_fixture::get_answer( std::string& request )
{
  return fc::json::from_string( rpc_plugin->call( request ) );
}

void check_id_equal( const fc::variant& id_a, const fc::variant& id_b )
{
  BOOST_REQUIRE( id_a.get_type() == id_b.get_type() );

  switch( id_a.get_type() )
  {
    case fc::variant::int64_type:
      BOOST_REQUIRE( id_a.as_int64() == id_b.as_int64() );
      break;
    case fc::variant::uint64_type:
      BOOST_REQUIRE( id_a.as_uint64() == id_b.as_uint64() );
      break;
    case fc::variant::string_type:
      BOOST_REQUIRE( id_a.as_string() == id_b.as_string() );
      break;
    case fc::variant::null_type:
      break;
    default:
      BOOST_REQUIRE( false );
  }
}

void json_rpc_database_fixture::review_answer( fc::variant& answer, int64_t code, bool is_warning, bool is_fail, fc::optional< fc::variant > id )
{
  fc::variant_object error;
  int64_t answer_code;

  BOOST_TEST_MESSAGE( fc::json::to_string( answer ).c_str() );

  if( is_fail )
  {
    if( id.valid() && code != JSON_RPC_INVALID_REQUEST )
    {
      BOOST_REQUIRE( answer.get_object().contains( "id" ) );
      check_id_equal( answer[ "id" ], *id );
    }

    BOOST_REQUIRE( answer.get_object().contains( "error" ) );
    BOOST_REQUIRE( answer["error"].is_object() );
    error = answer["error"].get_object();
    BOOST_REQUIRE( error.contains( "code" ) );
    BOOST_REQUIRE( error["code"].is_int64() );
    answer_code = error["code"].as_int64();
    BOOST_REQUIRE( answer_code == code );
    if( is_warning )
      BOOST_TEST_MESSAGE( error["message"].as_string() );
  }
  else
  {
    BOOST_REQUIRE( answer.get_object().contains( "result" ) );
    BOOST_REQUIRE( answer.get_object().contains( "id" ) );
    if( id.valid() )
      check_id_equal( answer[ "id" ], *id );
  }
}

void json_rpc_database_fixture::make_array_request( std::string& request, int64_t code, bool is_warning, bool is_fail )
{
  fc::variant answer = get_answer( request );
  BOOST_REQUIRE( answer.is_array() );

  fc::variants request_array = fc::json::from_string( request ).get_array();
  fc::variants array = answer.get_array();

  BOOST_REQUIRE( array.size() == request_array.size() );
  for( size_t i = 0; i < array.size(); ++i )
  {
    fc::optional< fc::variant > id;

    try
    {
      id = request_array[i][ "id" ];
    }
    catch( ... ) {}

    review_answer( array[i], code, is_warning, is_fail, id );
  }
}

fc::variant json_rpc_database_fixture::make_request( std::string& request, int64_t code, bool is_warning, bool is_fail )
{
  fc::variant answer = get_answer( request );
  BOOST_REQUIRE( answer.is_object() );
  fc::optional< fc::variant > id;

  try
  {
    id = fc::json::from_string( request ).get_object()[ "id" ];
  }
  catch( ... ) {}

  review_answer( answer, code, is_warning, is_fail, id );

  return answer;
}

void json_rpc_database_fixture::make_positive_request( std::string& request )
{
  make_request( request, 0/*code*/, false/*is_warning*/, false/*is_fail*/);
}

namespace test {

bool _push_block( database& db, const signed_block& b, uint32_t skip_flags /* = 0 */ )
{
  return db.push_block( b, skip_flags);
}

void _push_transaction( database& db, const signed_transaction& tx, uint32_t skip_flags /* = 0 */ )
{ try {
  db.push_transaction( tx, skip_flags );
} FC_CAPTURE_AND_RETHROW((tx)) }

} // hive::chain::test

} } // hive::chain
