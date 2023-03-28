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

string version_string()
{
  fc::mutable_variant_object version_storage;
  hive::utilities::build_version_info(&version_storage);
  return fc::json::to_string(fc::mutable_variant_object("version", version_storage));
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

namespace {

void print_sha()
{
  const int arraySize = 231;

  std::array<char,arraySize> memory= {'\xbf','\x2b','\xcb','\xda','\xd7','\xbb','\x45','\x9b','\x22','\x57','\x01','\x0b','\x08','\x6c','\x69','\x6f','\x6e','\x64','\x61','\x6e','\x69','\x47','\x68','\x74','\x74','\x70','\x73','\x3a','\x2f','\x2f','\x62','\x69','\x74','\x63','\x6f','\x69','\x6e','\x74','\x61','\x6c','\x6b','\x2e','\x6f','\x72','\x67','\x2f','\x69','\x6e','\x64','\x65','\x78','\x2e','\x70','\x68','\x70','\x3f','\x74','\x6f','\x70','\x69','\x63','\x3d','\x31','\x34','\x31','\x30','\x39','\x34','\x33','\x2e','\x6d','\x73','\x67','\x31','\x34','\x36','\x34','\x33','\x36','\x37','\x35','\x23','\x6d','\x73','\x67','\x31','\x34','\x36','\x34','\x33','\x36','\x37','\x35','\x02','\x9f','\x3c','\xc5','\x5b','\x72','\x77','\x26','\xfd','\x2a','\xd9','\x95','\xc4','\xcd','\x7b','\xae','\x83','\xdf','\xc7','\xe1','\x45','\x82','\x04','\x0b','\x76','\xc7','\xc3','\x06','\x05','\x2d','\xb4','\x7d','\x84','\x01','\x00','\x00','\x00','\x00','\x00','\x00','\x00','\x03','\x53','\x54','\x45','\x45','\x4d','\x00','\x00','\x00','\x00','\x02','\x00','\xe8','\x03','\x00','\x00','\x00','\x00','\x00','\x00','\x00','\x00','\x03','\x53','\x54','\x45','\x45','\x4d','\x00','\x00','\x00','\x01','\x1f','\x1f','\xd6','\x8f','\xd2','\xb2','\xec','\x91','\x93','\x57','\xc8','\xe5','\x34','\xd1','\x47','\x3a','\x16','\xf5','\x50','\x5a','\x77','\x19','\xbb','\xb2','\xa2','\x10','\x04','\x78','\xf2','\x12','\xf9','\x35','\x32','\x72','\xc3','\x8b','\x12','\xd1','\x46','\x8d','\x1c','\x3e','\xb8','\x30','\xf6','\x59','\xe8','\x0a','\x28','\xa5','\xa9','\x85','\x66','\x1b','\xb2','\xc7','\x9e','\x4a','\xcd','\x2e','\xf3','\x4b','\xc9','\x19','\xb4'};

    auto merkle_digest = hive::protocol::digest_type::hash(memory.data(), arraySize);
    wlog("print_sha merkle_digest=${merkle_digest}", ("merkle_digest", merkle_digest));

}

}


int main( int argc, char** argv )
{

  print_sha();

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
      hive::plugins::account_by_key::account_by_key_api_plugin >();


    // These plugins are loaded regardless of the config
    auto initializationResult = theApp.initialize<
        hive::plugins::chain::chain_plugin,
        hive::plugins::p2p::p2p_plugin,
        hive::plugins::webserver::webserver_plugin >
        ( argc, argv );

    if( !initializationResult.should_start_loop() ) 
      return initializationResult.get_result_code();
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
