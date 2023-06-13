#include "config_fixture.hpp"

#include <hive/plugins/witness/witness_plugin.hpp>

#include <hive/utilities/logging_config.hpp>
#include <hive/utilities/options_description_ex.hpp>

#include <boost/test/unit_test.hpp>

namespace bpo = boost::program_options;

namespace hive { namespace chain {

config_fixture::config_fixture() {}

void config_fixture::postponed_init( 
  const config_arg_override_t& config_arg_overrides /*= config_arg_override_t()*/ )
{
  try
  {
    bpo::variables_map option_overrides;
    if( not config_arg_overrides.empty() )
    {
      // In order to create boost::variables_map with overrides we need to:
      // 1. Create the options descriptions (definitions), so that they will be recognized later.
      bpo::options_description descriptions;
      using multi_line_t = config_arg_override_t::value_type::second_type;
      for( const auto& override : config_arg_overrides )
      {
        // override.first contains name of the option, e.g. "log-appender"
        descriptions.add_options()
          ( override.first.c_str(), bpo::value< multi_line_t >() );
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

    _data_dir = common_init( [&]( appbase::application& app, int argc, char** argv )
    {
      // Setup logging options.
      app.add_logging_program_options();

      // Initialize appbase. Note that no plugin is initialized here.
      app.initialize( argc, argv, option_overrides );

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

config_fixture::~config_fixture()
{
  // Shutdown any plugin here, when initialized in constructor.
}

} } // hive::chain
