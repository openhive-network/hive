#include "hived_fixture.hpp"

#include <hive/manifest/plugins.hpp>

#include <hive/plugins/condenser_api/condenser_api_plugin.hpp>
#include <hive/plugins/witness/witness_plugin.hpp>

#include <hive/utilities/logging_config.hpp>
#include <hive/utilities/options_description_ex.hpp>

#include <boost/test/unit_test.hpp>

namespace bpo = boost::program_options;

namespace hive { namespace chain {

hived_fixture::hived_fixture() {}

hived_fixture::~hived_fixture()
{
  appbase::reset(); // Also performs plugin shutdown.
}

void hived_fixture::postponed_init_impl( const config_arg_override_t& config_arg_overrides )
{
  try
  {
    bpo::variables_map option_overrides;

    _data_dir = common_init( [&]( appbase::application& app, int argc, char** argv )
    {
      // Setup logging options.
      app.add_logging_program_options();

      // Register every plugin existing in repository as hived does.
      hive::plugins::register_plugins();

      if( not config_arg_overrides.empty() )
      {
        // In order to create boost::variables_map with overrides we need to:
        // 1. Create the options descriptions (definitions), so that they will be recognized later.
        bpo::options_description descriptions, dummy;
        // In case some plugin option is provided we need to know every plugin option descriptions.
        app.set_plugin_options( &dummy, &descriptions );
        using multi_line_t = config_arg_override_t::value_type::second_type;
        // For non-plugin overrides add default string descriptions.
        for( const auto& override : config_arg_overrides )
        {
          // override.first contains name of the option, e.g. "log-appender"
          if( descriptions.find_nothrow( override.first.c_str(), false /*approx*/ ) == nullptr )
          {
            descriptions.add_options()
              ( override.first.c_str(), bpo::value< multi_line_t >() );
          }
        }
        // 2. Add the options actual "parsed" values.
        bpo::parsed_options the_options( &descriptions );
        for( const auto& override : config_arg_overrides )
        {
          bpo::option opt( override.first, override.second );
          the_options.options.push_back( opt );
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

      db = &app.get_plugin< hive::plugins::chain::chain_plugin >().db();
      BOOST_REQUIRE( db );

      // Load configuration file into logging config structure, used to create loggers & appenders.
      // Store the structure for further examination (in tests).
      _logging_config = app.load_logging_config();
    } );

    BOOST_ASSERT( _data_dir );

  } catch ( const fc::exception& e )
  {
    edump( (e.to_detail_string()) );
    throw;
  }
}

json_rpc_database_fixture::json_rpc_database_fixture()
{
  try {

  hive::plugins::condenser_api::condenser_api_plugin* denser_api_plugin = nullptr;
  postponed_init(
    { config_line_t( { "plugin",
      { HIVE_ACCOUNT_HISTORY_ROCKSDB_PLUGIN_NAME,
        HIVE_WITNESS_PLUGIN_NAME,
        HIVE_JSON_RPC_PLUGIN_NAME,
        HIVE_BLOCK_API_PLUGIN_NAME,
        HIVE_DATABASE_API_PLUGIN_NAME,
        HIVE_CONDENSER_API_PLUGIN_NAME } } ) },
    &ah_plugin,
    &rpc_plugin,
    &denser_api_plugin
  );

  denser_api_plugin->plugin_startup();
  rpc_plugin->finalize_startup();

  ah_plugin->set_destroy_database_on_startup();
  ah_plugin->set_destroy_database_on_shutdown();
  ah_plugin->plugin_startup();

  init_account_pub_key = init_account_priv_key.get_public_key();

  open_database( get_data_dir() );

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
  if( !std::uncaught_exceptions() )
  {
    BOOST_CHECK( db->get_node_properties().skip_flags == database::skip_nothing );
  }

  if( ah_plugin )
    ah_plugin->plugin_shutdown();
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

fc::variant json_rpc_database_fixture::make_request( std::string& request, int64_t code, bool is_warning, bool is_fail,
  const char* message )
{
  fc::variant answer = get_answer( request );
  BOOST_REQUIRE( answer.is_object() );
  fc::optional< fc::variant > id;

  try
  {
    id = fc::json::from_string( request ).get_object()[ "id" ];
  }
  catch( ... ) {}

  review_answer( answer, code, is_warning, is_fail, id, message );

  return answer;
}

void json_rpc_database_fixture::make_positive_request( std::string& request )
{
  make_request( request, 0/*code*/, false/*is_warning*/, false/*is_fail*/);
}

} } // hive::chain
