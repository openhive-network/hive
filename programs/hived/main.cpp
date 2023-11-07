#include <appbase/application.hpp>
#include <hive/manifest/plugins.hpp>

#include <hive/protocol/types.hpp>
#include <hive/protocol/version.hpp>

#include <hive/utilities/logging_config.hpp>
#include <hive/utilities/git_revision.hpp>
#include <hive/utilities/options_description_ex.hpp>

#include <hive/plugins/account_by_key/account_by_key_plugin.hpp>
#include <hive/plugins/account_by_key_api/account_by_key_api_plugin.hpp>
#include <hive/plugins/chain/chain_plugin.hpp>
//#include <hive/plugins/condenser_api/condenser_api_plugin.hpp>
#include <hive/plugins/p2p/p2p_plugin.hpp>
#include <hive/plugins/webserver/webserver_plugin.hpp>
#include <hive/plugins/witness/witness_plugin.hpp>

#include <fc/exception/exception.hpp>
#include <fc/interprocess/signals.hpp>
#include <fc/git_revision.hpp>
#include <fc/stacktrace.hpp>

#include <boost/exception/diagnostic_information.hpp>
#include <boost/program_options.hpp>
#include <boost/scope_exit.hpp>

#include <iostream>
#include <csignal>
#include <vector>

namespace bpo = boost::program_options;
using hive::protocol::version;
using std::string;

string version_string()
{
  fc::mutable_variant_object version_storage;
  hive::utilities::build_version_info(&version_storage);
  return fc::json::to_string(fc::mutable_variant_object("version", version_storage));
}

void info(const hive::protocol::chain_id_type& chainId)
  {
  ilog("------------------------------------------------------\n");
#ifdef USE_ALTERNATE_CHAIN_ID
# ifdef IS_TEST_NET
  ilog("            STARTING TEST NETWORK\n");
# else
  ilog("            STARTING MIRROR NETWORK\n");
# endif
  ilog("------------------------------------------------------");
  ilog("initminer private key: ${key}", ("key", HIVE_INIT_PRIVATE_KEY.key_to_wif() ) );
#else
  ilog("                @     @@@@@@    ,@@@@@%               ");
  ilog("               @@@@    (@@@@@*    @@@@@@              ");
  ilog("             %@@@@@@     @@@@@@    %@@@@@,            ");
  ilog("            @@@@@@@@@@    @@@@@@     @@@@@@           ");
  ilog("          ,@@@@@@@@@@@@     @@@@@@    @@@@@@          ");
  ilog("         @@@@@@@@@@@@@@@&    @@@@@@     @@@@@@        ");
  ilog("        @@@@@@@@@@@@@@@@@@    .@@@@@%    @@@@@@       ");
  ilog("      @@@@@@@@@@@@@@@@@@@@@(              .@@@@@%     ");
  ilog("       @@@@@@@@@@@@@@@@@@@@               @@@@@@      ");
  ilog("        *@@@@@@@@@@@@@@@@     @@@@@@    @@@@@@.       ");
  ilog("          @@@@@@@@@@@@@@    &@@@@@.    @@@@@@         ");
  ilog("           #@@@@@@@@@@     @@@@@@    #@@@@@/          ");
  ilog("             @@@@@@@@    /@@@@@/    @@@@@@            ");
  ilog("              @@@@@(    @@@@@@    .@@@@@&             ");
  ilog("                @@     @@@@@&    @@@@@@               \n");
  ilog("                STARTING HIVE NETWORK\n");
  ilog("------------------------------------------------------");
#endif
  ilog("initminer public key: ${key}", ("key", HIVE_INIT_PUBLIC_KEY_STR) );
  ilog("chain id: ${chain_id}", ("chain_id", std::string(chainId) ) );
  ilog("blockchain version: ${version}", ("version", fc::string(HIVE_BLOCKCHAIN_VERSION) ) );
  ilog("------------------------------------------------------\n");

  ilog("hived git_revision: \"${revision}\"\n", ("revision", fc::string(hive::utilities::git_revision_sha) ) );
  }

int main( int argc, char** argv )
{
  try
  {
    appbase::application theApp;

    theApp.init_signals_handler();

    BOOST_SCOPE_EXIT(&theApp)
    {
      auto _is_thread_closed = theApp.is_thread_closed();
      if( !_is_thread_closed )
        kill(getpid(), SIGINT);

      theApp.wait();

      if( _is_thread_closed )
        ilog("exited cleanly");

    } BOOST_SCOPE_EXIT_END

    // Setup logging config
    theApp.add_logging_program_options();

    hive::utilities::options_description_ex options;
    options.add_options()
      ("backtrace", bpo::value< string >()->default_value( "yes" ), "Whether to print backtrace on SIGSEGV" );
    theApp.add_program_options( hive::utilities::options_description_ex(), options );

    if( theApp.is_interrupt_request() ) return 0;

    hive::plugins::register_plugins( theApp );

    if( theApp.is_interrupt_request() ) return 0;

    theApp.set_version_string( version_string() );
    theApp.set_app_name( "hived" );

    if( theApp.is_interrupt_request() ) return 0;

    // These plugins are included in the default config
    theApp.set_default_plugins<
      hive::plugins::witness::witness_plugin,
      hive::plugins::account_by_key::account_by_key_plugin,
      hive::plugins::account_by_key::account_by_key_api_plugin >();

    if( theApp.is_interrupt_request() ) return 0;

    // These plugins are loaded regardless of the config
    auto initializationResult = theApp.initialize<
        hive::plugins::chain::chain_plugin,
        hive::plugins::p2p::p2p_plugin,
        hive::plugins::webserver::webserver_plugin >
        ( argc, argv );

    if( theApp.is_interrupt_request() ) return 0;

    if( !initializationResult.should_start_loop() ) 
      return initializationResult.get_result_code();
    else theApp.notify_status("starting");

    theApp.load_logging_config();

    try
    {
      const auto& chainPlugin = theApp.get_plugin<hive::plugins::chain::chain_plugin>();
      auto chainId = chainPlugin.db().get_chain_id();
      info(chainId);
    }
    catch(const std::exception& e)
    {
      if( theApp.is_interrupt_request() )
      {
        ilog("Error ${error} occured, but it's skipped because the application is closing",("error", e.what()));
      }
      else
        throw;
    }

    if( theApp.get_args().at( "backtrace" ).as< string >() == "yes" )
    {
      fc::print_stacktrace_on_segfault();
    }

    if( theApp.is_interrupt_request() ) return 0;

    theApp.startup();

    if( theApp.is_interrupt_request() ) return 0;

    theApp.wait();

    return initializationResult.get_result_code();
  }
  catch ( const boost::exception& e )
  {
    ilog( boost::diagnostic_information(e) );
  }
  catch ( const fc::exception& e )
  {
    ilog( e.to_detail_string() );
  }
  catch ( const std::exception& e )
  {
    ilog( e.what() );
  }
  catch ( ... )
  {
    ilog( "unknown exception" );
  }

  return -1;
}
