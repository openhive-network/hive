#include "hived_fixture.hpp"

#include <hive/manifest/plugins.hpp>

#include <hive/plugins/condenser_api/condenser_api_plugin.hpp>
#include <hive/plugins/witness/witness_plugin.hpp>

#include <hive/utilities/logging_config.hpp>
#include <hive/utilities/options_description_ex.hpp>

#include <boost/test/unit_test.hpp>

namespace bpo = boost::program_options;

namespace hive { namespace chain {

hived_fixture::hived_fixture( bool remove_db_files /*= true*/, bool disable_p2p /*= true*/)
  : _disable_p2p( disable_p2p ), _remove_db_files( remove_db_files )
{}

hived_fixture::~hived_fixture() 
{
  try {
    if( !std::uncaught_exceptions() )
    {
      // If we're exiting nominally, check that skip flags have been restored.
      // Note that you can't do it after appbase finish -
      // the memory will be released by then.
      BOOST_CHECK_EQUAL( db->get_node_skip_flags(), database::skip_nothing );
    }

    // Try to finish appbase nominally.
    theApp.generate_interrupt_request();
    theApp.finish(); // performs plugin shutdown too
    
    // Possible exception during test is NOT a reason to stop. Let BOOST handle the rest.
    return;
  } FC_CAPTURE_AND_LOG( () )

  // Exception during appbase finish IS the reason to stop the tests.
  exit(1);
}

void hived_fixture::set_logging_config( const fc::optional< fc::logging_config > common_logging_config )
{
  // If the config is filled by another hived_fixture...
  if( common_logging_config )
  {
    // And postponed_init has not been called yet...
    FC_ASSERT( _logging_config.valid() == false );
    // Set common value to skip this phase during postponed init (see postponed_init_impl).
    _logging_config = common_logging_config;
  }
}

void hived_fixture::postponed_init_impl( bool remove_db_files,
  const config_arg_override_t& config_arg_overrides )
{
  try
  {
    bpo::variables_map option_overrides;

    _data_dir = common_init( theApp, remove_db_files, _data_dir, [&](appbase::application& app, int argc, char** argv)
    {
      
      // Global value should always default to true.
      account_name_type::set_verify( true );

      // Setup logging options.
      app.add_logging_program_options();

      // Register every plugin existing in repository as hived does.
      hive::plugins::register_plugins( app );

      config_arg_override_t default_overrides = {
        // turn off RC stats reports by default - specific tests/fixtures can turn it on later
        { "rc-stats-report-type", { "NONE" } }
      };

      if( not config_arg_overrides.empty() )
        std::copy_if( config_arg_overrides.begin(),
                      config_arg_overrides.end(),
                      std::back_inserter( default_overrides ),
                      [&]( const auto& override ) {
                        auto error_str = "Unit test nodes must not listen to other ones!";
                        BOOST_REQUIRE_MESSAGE( override.first != "p2p-endpoint", error_str );
                        BOOST_REQUIRE_MESSAGE( override.first != "p2p-seed-node", error_str );
                        BOOST_REQUIRE_MESSAGE( override.first != "p2p-parameters", error_str );
                        return true;
                      }
                    );
      {
        // In order to create boost::variables_map with overrides we need to:
        // 1. Create the options descriptions (definitions), so that they will be recognized later.
        bpo::options_description cfg_descriptions, cli_descriptions;
        // In case some plugin option is provided we need to know every plugin option descriptions.
        app.set_plugin_options( &cli_descriptions, &cfg_descriptions );
        for (const auto& opt : cli_descriptions.options())
        {
          if ( cfg_descriptions.find_nothrow( opt->long_name(), false /*approx*/ ) == nullptr )
            cfg_descriptions.add(opt);
        }
        using multi_line_t = config_arg_override_t::value_type::second_type;
        // For non-plugin overrides add default string descriptions.
        for( const auto& [name, _value] : default_overrides )
        {
          if( cfg_descriptions.find_nothrow( name.c_str(), false /*approx*/ ) == nullptr )
          {
            cfg_descriptions.add_options()
              ( name.c_str(), bpo::value< multi_line_t >() );
          }
        }
        // 2. Add the options actual "parsed" values.
        bpo::parsed_options the_options( &cfg_descriptions );
        for( const auto& [name, value] : default_overrides )
        {
          the_options.options.emplace_back( name, value );
        }
        // 3. Use the only way to add options to variables_map.
        bpo::store( the_options, option_overrides );
      }

      // Initialize appbase. The only plugin initialized explicitly as template parameter here
      // is needed in all fixtures. Note that more plugins may be initialized using provided
      // option_overrides/config_arg_overrides.
      app.initialize< 
        hive::plugins::debug_node::debug_node_plugin
      >( argc, argv, option_overrides );

      db_plugin = app.find_plugin< hive::plugins::debug_node::debug_node_plugin >();
      BOOST_REQUIRE( db_plugin );
      db_plugin->logging = false;

      _chain = &( app.get_plugin< hive::plugins::chain::chain_plugin >() );
      if( _disable_p2p )
        _chain->disable_p2p(); // We don't want p2p plugin connections at all.
      _block_reader = &( _chain->block_reader() );
      db = &( _chain->db() );
      BOOST_REQUIRE( db );
      db->_log_hardforks = false;

      thread_pool = &_chain->get_thread_pool();
      BOOST_REQUIRE( thread_pool );

      // Skip this phase when common logging config was provided by another fixture 
      // (see set_logging_config).
      if( not _logging_config )
      {
        // Load configuration file into logging config structure, used to create loggers & appenders.
        // Store the structure for further examination (in tests).
        _logging_config = app.load_logging_config();
      }

      ah_plugin_type* ah_plugin = app.find_plugin< ah_plugin_type >();
      if( ah_plugin != nullptr )
      {
        ah_plugin->set_destroy_database_on_startup();
        ah_plugin->set_destroy_database_on_shutdown();
      }

      app.startup();
    } );

    FC_ASSERT( _data_dir != fc::path() );

  } catch ( const fc::exception& e )
  {
    edump( (e.to_detail_string()) );
    throw;
  }
}

const hive::chain::block_read_i& hived_fixture::get_block_reader() const
{
  FC_ASSERT( _block_reader != nullptr );
  return *_block_reader;
}

hive::plugins::chain::chain_plugin& hived_fixture::get_chain_plugin() const
{
  FC_ASSERT( _chain != nullptr );
  return *_chain;
}

json_rpc_database_fixture::json_rpc_database_fixture()
{
  try {

  configuration_data.set_initial_asset_supply( INITIAL_TEST_SUPPLY, HBD_INITIAL_TEST_SUPPLY );

  hive::plugins::condenser_api::condenser_api_plugin* denser_api_plugin = nullptr;
  postponed_init(
    {
      config_line_t( { "plugin",
        { HIVE_ACCOUNT_HISTORY_ROCKSDB_PLUGIN_NAME,
          HIVE_WITNESS_PLUGIN_NAME,
          HIVE_JSON_RPC_PLUGIN_NAME,
          HIVE_BLOCK_API_PLUGIN_NAME,
          HIVE_DATABASE_API_PLUGIN_NAME,
          HIVE_CONDENSER_API_PLUGIN_NAME } }
      ),
      config_line_t( { "shared-file-size",
        { std::to_string( 1024 * 1024 * shared_file_size_in_mb_64 ) } }
      )
    },
    &ah_plugin,
    &rpc_plugin,
    &denser_api_plugin
  );

  init_account_pub_key = init_account_priv_key.get_public_key();

  generate_block();
  db->set_hardfork( HIVE_BLOCKCHAIN_VERSION.minor_v() );
  generate_block();
  db->_log_hardforks = true;

  vest( HIVE_INIT_MINER_NAME, ASSET( "10.000 TESTS" ) );

  // Fill up the rest of the required miners
  for( int i = HIVE_NUM_INIT_MINERS; i < HIVE_MAX_WITNESSES; i++ )
  {
    account_create( HIVE_INIT_MINER_NAME + fc::to_string( i ), init_account_pub_key );
    fund( HIVE_INIT_MINER_NAME + fc::to_string( i ), HIVE_MIN_PRODUCER_REWARD );
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

json_rpc_database_fixture::~json_rpc_database_fixture() {}

fc::variant json_rpc_database_fixture::get_answer( std::string& request )
{
  return fc::json::from_string( rpc_plugin->call( request ), fc::json::format_validation_mode::full );
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

void json_rpc_database_fixture::review_answer( fc::variant& answer, int64_t code, bool is_warning, bool is_fail,
  fc::optional< fc::variant > id, const char* message )
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
    if( message != nullptr )
      BOOST_CHECK_EQUAL( error[ "message" ].as_string(), message );
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

  fc::variants request_array = fc::json::from_string( request, fc::json::format_validation_mode::full ).get_array();
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

fc::variant json_rpc_database_fixture::make_request( std::string& request, int64_t code, bool is_warning, bool is_fail,
  const char* message )
{
  fc::variant answer = get_answer( request );
  BOOST_REQUIRE( answer.is_object() );
  fc::optional< fc::variant > id;

  try
  {
    id = fc::json::from_string( request, fc::json::format_validation_mode::full ).get_object()[ "id" ];
  }
  catch( ... ) {}

  review_answer( answer, code, is_warning, is_fail, id, message );

  return answer;
}

void json_rpc_database_fixture::make_positive_request( std::string& request )
{
  make_request( request, 0/*code*/, false/*is_warning*/, false/*is_fail*/);
}

bool hived_fixture::push_block( const std::shared_ptr<full_block_type>& b, uint32_t skip_flags /* = 0 */ )
{
  return test::_push_block( get_chain_plugin(), b, skip_flags );
}

namespace test {

bool _push_block(hive::plugins::chain::chain_plugin& chain, const block_header& header, 
                 const std::vector<std::shared_ptr<full_transaction_type>>& full_transactions, 
                 const fc::ecc::private_key& signer,
                 uint32_t skip_flags /* = 0 */)
{
  std::shared_ptr<full_block_type> full_block( hive::chain::full_block_type::create_from_block_header_and_transactions( header, full_transactions, &signer ) );
  existing_block_flow_control block_ctrl( full_block );
  return chain.push_block( block_ctrl, skip_flags );
}

bool _push_block( hive::plugins::chain::chain_plugin& chain, const std::shared_ptr<full_block_type>& b, uint32_t skip_flags /* = 0 */ )
{
  existing_block_flow_control block_ctrl( b );
  return chain.push_block( block_ctrl, skip_flags);
}

} // namespace test

} } // hive::chain
