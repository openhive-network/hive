#include <boost/exception/diagnostic_information.hpp>
#include <boost/program_options.hpp>
#include <boost/scope_exit.hpp>

#include <appbase/application.hpp>

#include <string>
#include <exception>
#include <iostream>

#include <hive/utilities/git_revision.hpp>
#include <hive/protocol/config.hpp>

#include <fc/exception/exception.hpp>

#include "plugins/block_log_conversion/block_log_conversion_plugin.hpp"
#include "plugins/node_based_conversion/node_based_conversion_plugin.hpp"
#include "plugins/iceberg_generate/iceberg_generate_plugin.hpp"

namespace bpo = boost::program_options;

using hive::utilities::git_revision_sha;
using hive::protocol::version;

namespace hcplugins = hive::converter::plugins;

using hcplugins::node_based_conversion::node_based_conversion_plugin;
using hcplugins::block_log_conversion::block_log_conversion_plugin;
using hcplugins::iceberg_generate::iceberg_generate_plugin;

std::string& version_string()
{
  static std::string v_str =
    "  \"version\" : { \"hive_blockchain_hard_fork\" : \""   + std::string( HIVE_BLOCKCHAIN_VERSION )            + "\", " +
    "\"hive_git_revision\" : \""                             + std::string( git_revision_sha )  + "\" }";

  return v_str;
}

int main( int argc, char** argv )
{
  try
  {
    appbase::application bc_converter_app;
    bc_converter_app.init_signals_handler();

    BOOST_SCOPE_EXIT(&bc_converter_app)
    {
      if( bc_converter_app.quit() )
        ilog("exited cleanly");
    } BOOST_SCOPE_EXIT_END

    // Setup converter options
    bpo::options_description logging_opts{"Logging options"};
      logging_opts.add_options()
      ("log-per-block,l", bpo::value< uint32_t >()->default_value( 0 ), "Displays blocks in JSON format every n blocks")
      ("log-specific,s", bpo::value< uint32_t >()->default_value( 0 ), "Displays only block with specified number");
    bpo::options_description conversion_opts{"Conversion Options"};
      conversion_opts.add_options()
      ("chain-id,C", bpo::value< std::string >()->required(), "New chain ID")
      ("private-key,K", bpo::value< std::string >()->required(), "Private key with which all transactions and blocks will be signed ")
      ("owner-key,O", bpo::value< std::string >()->default_value( "" ), "Owner key of the second authority")
      ("active-key,A", bpo::value< std::string >()->default_value( "" ), "Active key of the second authority")
      ("posting-key,P", bpo::value< std::string >()->default_value( "" ), "Posting key of the second authority")
      ("use-same-key,U", "Use given private key as the owner, active and posting keys if not specified")
      ("resume-block,R", bpo::value< uint32_t >()->default_value( 0 ), "Resume conversion from the given block number")
      ("stop-block,S", bpo::value< uint32_t >()->default_value( 0 ), "Stop conversion at the given block number")
      ("jobs,j", bpo::value< size_t >()->default_value( 1 ), "Allow N jobs at once to sign transactions");
      bpo::options_description source_opts{"Source options"};
        source_opts.add_options()
      ("input,i", bpo::value< std::vector< std::string > >(),
        "Input source (depending on plugin enabled - block log path or hive API endpoint). For block log path we have 2 options: [DIR]/block_log or [DIR]/block_log_part.0001 depending on used type of block_log")
      ("output,o", bpo::value< std::vector< std::string > >(), "Output source (depending on plugin enabled - block log path or hive API endpoints)");
      bpo::options_description cli_options{};
        cli_options.add( source_opts ).add( logging_opts );

    bc_converter_app.add_program_options( conversion_opts, cli_options );

    bc_converter_app.register_plugin< node_based_conversion_plugin >();
    bc_converter_app.register_plugin< block_log_conversion_plugin >();
    bc_converter_app.register_plugin< iceberg_generate_plugin >();

    bc_converter_app.set_version_string( version_string() );
    bc_converter_app.set_app_name( "blockchain_converter" );

    auto initResult = bc_converter_app.initialize( argc, argv );
    if( !initResult.should_start_loop() )
      return initResult.get_result_code();

    bc_converter_app.startup();
    bc_converter_app.wait();

    return 0;
  }
  catch ( const boost::exception& e )
  {
    elog( boost::diagnostic_information(e) );
  }
  catch ( const fc::exception& e )
  {
    elog( e.to_detail_string() );
  }
  catch ( const std::exception& e )
  {
    elog( e.what() );
  }
  catch ( ... )
  {
    elog( "unknown exception" );
  }

  return -1;
}
