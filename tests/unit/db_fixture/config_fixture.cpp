#include "config_fixture.hpp"

#include <hive/plugins/witness/witness_plugin.hpp>

#include <hive/utilities/logging_config.hpp>
#include <hive/utilities/options_description_ex.hpp>

#include <boost/test/unit_test.hpp>

namespace bpo = boost::program_options;

namespace hive { namespace chain {

config_fixture::config_fixture()
{
  try
  {
    auto _data_dir = common_init( [&]( appbase::application& app, int argc, char** argv )
    {
      // Setup logging options.
      hive::utilities::options_description_ex options;
      hive::utilities::set_logging_program_options( options );
      app.add_program_options( hive::utilities::options_description_ex(), options );

      // Initialize appbase. Note that no plugin is initialized here.
      app.initialize( argc, argv );

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
