#include "hived_fixture.hpp"

#include <hive/manifest/plugins.hpp>

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

} } // hive::chain
