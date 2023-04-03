#include <appbase/application.hpp>

#include <hive/plugins/webserver/webserver_plugin.hpp>
#include <hive/plugins/clive/clive_plugin.hpp>
#include <hive/plugins/clive_api/clive_api_plugin.hpp>

#include <fc/exception/exception.hpp>
#include <fc/thread/thread.hpp>
#include <fc/interprocess/signals.hpp>
#include <fc/git_revision.hpp>
#include <fc/stacktrace.hpp>

#include <boost/exception/diagnostic_information.hpp>
#include <boost/program_options.hpp>

#include <iostream>
#include <csignal>
#include <vector>

namespace bpo = boost::program_options;

int main( int argc, char** argv )
{
  try
  {
    bpo::options_description options;

    options.add_options()
      ("backtrace", bpo::value< string >()->default_value( "yes" ), "Whether to print backtrace on SIGSEGV" );

    auto& theApp = appbase::app();

    theApp.add_program_options( bpo::options_description(), options );

    appbase::app().register_plugin< hive::plugins::webserver::webserver_plugin >();
    appbase::app().register_plugin< hive::plugins::json_rpc::json_rpc_plugin >();
    appbase::app().register_plugin< hive::plugins::clive::clive_plugin >();
    appbase::app().register_plugin< hive::plugins::clive_api::clive_api_plugin >();

    theApp.set_app_name( "bee_keeper" );

    auto initializationResult = theApp.initialize<
        hive::plugins::clive_api::clive_api_plugin,
        hive::plugins::webserver::webserver_plugin >
        ( argc, argv );

    if( !initializationResult.should_start_loop() ) 
      return initializationResult.get_result_code();
    else hive::notify_hived_status("starting");

    auto& args = theApp.get_args();

    if( args.at( "backtrace" ).as< string >() == "yes" )
    {
      fc::print_stacktrace_on_segfault();
      ilog( "Backtrace on segfault is enabled." );
    }

    theApp.startup();
    theApp.exec();
    std::cout << "exited cleanly\n";
    
    return initializationResult.get_result_code();
  }
  catch ( const boost::exception& e )
  {
    std::cerr << boost::diagnostic_information(e) << "\n";
  }
  catch ( const fc::exception& e )
  {
    std::cerr << e.to_detail_string() << "\n";
  }
  catch ( const std::exception& e )
  {
    std::cerr << e.what() << "\n";
  }
  catch ( ... )
  {
    std::cerr << "unknown exception\n";
  }

  return -1;
}
