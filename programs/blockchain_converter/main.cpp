#include <boost/exception/diagnostic_information.hpp>
#include <boost/program_options.hpp>

#include <appbase/application.hpp>

#include <string>
#include <exception>
#include <iostream>

#include <hive/utilities/git_revision.hpp>
#include <hive/protocol/config.hpp>

#include <fc/exception/exception.hpp>

#include "block_log_conversion_plugin.hpp"
#include "node_based_conversion_plugin.hpp"

namespace bpo = boost::program_options;

using hive::utilities::git_revision_sha;
using hive::protocol::version;

namespace hcplugins = hive::converter::plugins;

using hcplugins::node_based_conversion::node_based_conversion_plugin;
using hcplugins::block_log_conversion::block_log_conversion_plugin;

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
    auto& bc_converter_app = appbase::app();

    // Setup converter options
    bpo::options_description logging_opts{"Logging options"};
      logging_opts.add_options()
      ("log-per-block,l", bpo::value< uint32_t >()->default_value( 0 ), "displays blocks in JSON format every n blocks")
      ("log-specific,s", bpo::value< uint32_t >()->default_value( 0 ), "displays only block with specified number");
    bpo::options_description conversion_opts{"Conversion Options"};
      conversion_opts.add_options()
      ("chain-id,C", bpo::value< std::string >()->required(), "new chain ID")
      ("private-key,K", bpo::value< std::string >()->required(), "private key with which all transactions and blocks will be signed ")
      ("owner-key,O", bpo::value< std::string >()->default_value( "" ), "owner key of the second authority")
      ("active-key,A", bpo::value< std::string >()->default_value( "" ), "active key of the second authority")
      ("posting-key,P", bpo::value< std::string >()->default_value( "" ), "posting key of the second authority")
      ("use-same-key,U", "use given private key as the owner, active and posting keys if not specified")
      ("resume-block,R", bpo::value< uint32_t >()->default_value( 0 ), "resume conversion from the given block number")
      ("stop-block,S", bpo::value< uint32_t >()->default_value( 0 ), "stop conversion at the given block number");
      bpo::options_description source_opts{"Source options"};
        source_opts.add_options()
      ("input,i", bpo::value< std::string >(), "input source (depending on plugin enabled - block log path or hive API endpoint)")
      ("output,o", bpo::value< std::string >(), "output source (depending on plugin enabled - block log path or hive API endpoint)" );
      bpo::options_description cli_options{};
        cli_options.add( source_opts ).add( logging_opts );

    bc_converter_app.add_program_options( conversion_opts, cli_options );

    bc_converter_app.register_plugin< node_based_conversion_plugin >();
    bc_converter_app.register_plugin< block_log_conversion_plugin >();

    bc_converter_app.set_default_plugins< block_log_conversion_plugin >();

    bc_converter_app.set_version_string( version_string() );
    bc_converter_app.set_app_name( "blockchain_converter" );

    if( !bc_converter_app.initialize( argc, argv ) )
      return -1;

    bc_converter_app.startup();
    bc_converter_app.exec();

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
