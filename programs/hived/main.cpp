#include <appbase/application.hpp>
#include <hive/manifest/plugins.hpp>

#include <hive/protocol/types.hpp>
#include <hive/protocol/version.hpp>

#include <hive/utilities/logging_config.hpp>
#include <hive/utilities/key_conversion.hpp>
#include <hive/utilities/git_revision.hpp>

#include <hive/plugins/account_by_key/account_by_key_plugin.hpp>
#include <hive/plugins/account_by_key_api/account_by_key_api_plugin.hpp>
#include <hive/plugins/chain/chain_plugin.hpp>
//#include <hive/plugins/condenser_api/condenser_api_plugin.hpp>
#include <hive/plugins/p2p/p2p_plugin.hpp>
#include <hive/plugins/webserver/webserver_plugin.hpp>
#include <hive/plugins/witness/witness_plugin.hpp>
#include <hive/plugins/wallet_bridge_api/wallet_bridge_api_plugin.hpp>

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
using hive::protocol::version;
using std::string;

string& version_string()
{
  static string v_str =
    "  \"version\" : { \"hive_blockchain_hard_fork\" : \""   + fc::string( HIVE_BLOCKCHAIN_VERSION )            + "\", " +
    "\"hive_git_revision\" : \""                             + fc::string( hive::utilities::git_revision_sha )  + "\" }";

  return v_str;
}

void info(const hive::protocol::chain_id_type& chainId)
  {
  std::cerr << "------------------------------------------------------\n\n";
#ifdef USE_ALTERNATE_CHAIN_ID
  std::cerr << "            STARTING "
# ifdef IS_TEST_NET
                            "TEST"
# else
                            "MIRROR"
# endif
                            " NETWORK\n\n";
  std::cerr << "------------------------------------------------------\n";
  std::cerr << "initminer private key: " << hive::utilities::key_to_wif(HIVE_INIT_PRIVATE_KEY) << "\n";
#else
  std::cerr << "                @     @@@@@@    ,@@@@@%               \n";
  std::cerr << "               @@@@    (@@@@@*    @@@@@@              \n";
  std::cerr << "             %@@@@@@     @@@@@@    %@@@@@,            \n";
  std::cerr << "            @@@@@@@@@@    @@@@@@     @@@@@@           \n";
  std::cerr << "          ,@@@@@@@@@@@@     @@@@@@    @@@@@@          \n";
  std::cerr << "         @@@@@@@@@@@@@@@&    @@@@@@     @@@@@@        \n";
  std::cerr << "        @@@@@@@@@@@@@@@@@@    .@@@@@%    @@@@@@       \n";
  std::cerr << "      @@@@@@@@@@@@@@@@@@@@@(              .@@@@@%     \n";
  std::cerr << "       @@@@@@@@@@@@@@@@@@@@               @@@@@@      \n";
  std::cerr << "        *@@@@@@@@@@@@@@@@     @@@@@@    @@@@@@.       \n";
  std::cerr << "          @@@@@@@@@@@@@@    &@@@@@.    @@@@@@         \n";
  std::cerr << "           #@@@@@@@@@@     @@@@@@    #@@@@@/          \n";
  std::cerr << "             @@@@@@@@    /@@@@@/    @@@@@@            \n";
  std::cerr << "              @@@@@(    @@@@@@    .@@@@@&             \n";
  std::cerr << "                @@     @@@@@&    @@@@@@               \n\n";
  std::cerr << "                STARTING HIVE NETWORK\n\n";
  std::cerr << "------------------------------------------------------\n";
#endif
  std::cerr << "initminer public key: " << HIVE_INIT_PUBLIC_KEY_STR << "\n";
  std::cerr << "chain id: " << std::string(chainId) << "\n";
  std::cerr << "blockchain version: " << fc::string(HIVE_BLOCKCHAIN_VERSION) << "\n";
  std::cerr << "------------------------------------------------------\n\n";

  std::cerr << "hived git_revision: \"" + fc::string(hive::utilities::git_revision_sha) + "\"\n\n";
  }

int main( int argc, char** argv )
{
  try
  {
    // Setup logging config
    bpo::options_description options;

    hive::utilities::set_logging_program_options( options );
    hive::utilities::notifications::add_program_options(options);
    options.add_options()
      ("backtrace", bpo::value< string >()->default_value( "yes" ), "Whether to print backtrace on SIGSEGV" );

    auto& theApp = appbase::app();

    theApp.add_program_options( bpo::options_description(), options );

    hive::plugins::register_plugins();

    theApp.set_version_string( version_string() );
    theApp.set_app_name( "hived" );

    // These plugins are included in the default config
    theApp.set_default_plugins<
      hive::plugins::witness::witness_plugin,
      hive::plugins::account_by_key::account_by_key_plugin,
      hive::plugins::account_by_key::account_by_key_api_plugin,
      hive::plugins::wallet_bridge_api::wallet_bridge_api_plugin >();


    // These plugins are loaded regardless of the config
    bool initialized = theApp.initialize<
        hive::plugins::chain::chain_plugin,
        hive::plugins::p2p::p2p_plugin,
        hive::plugins::webserver::webserver_plugin >
        ( argc, argv );

    if( !initialized ) return 0;
    else hive::notify_hived_status("starting");

    const auto& chainPlugin = theApp.get_plugin<hive::plugins::chain::chain_plugin>();
    auto chainId = chainPlugin.db().get_chain_id();
    info(chainId);

    auto& args = theApp.get_args();

    try
    {
      fc::optional< fc::logging_config > logging_config = hive::utilities::load_logging_config( args, appbase::app().data_dir() );
      if( logging_config )
        fc::configure_logging( *logging_config );
    }
    catch( const fc::exception& e )
    {
      wlog( "Error parsing logging config. ${e}", ("e", e.to_string()) );
    }

    if( args.at( "backtrace" ).as< string >() == "yes" )
    {
      fc::print_stacktrace_on_segfault();
      ilog( "Backtrace on segfault is enabled." );
    }

    theApp.startup();
    theApp.exec();
    std::cout << "exited cleanly\n";
    return 0;
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
