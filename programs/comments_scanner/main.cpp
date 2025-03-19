#include "hive/chain/external_storage/comment_rocksdb_objects.hpp"
#include "hive/chain/irreversible_block_data.hpp"
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
#include <hive/plugins/state_snapshot/state_snapshot_plugin.hpp>

#include <hive/chain/comment_object.hpp>
#include <hive/chain/external_storage/comment_rocksdb_objects.hpp>

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

#include <fstream>

namespace bpo = boost::program_options;
namespace bfs = boost::filesystem;

using std::string;

string version_string()
{
  fc::mutable_variant_object version_storage;
  hive::utilities::build_version_info(&version_storage);
  return fc::json::to_string(fc::mutable_variant_object("version", version_storage));
}

struct scanner
{
  hive::plugins::chain::chain_plugin& chain;
  bfs::path path;

  hive::chain::database& db;

  scanner( hive::plugins::chain::chain_plugin& chain, const bfs::path& path )
        : chain( chain ), path( path ), db( chain.db() )
  {
  }

  template<typename index_type>
  void scan_any_objects( const std::string& file_name, const std::string& name )
  {
    std::ofstream _file;
    _file.open( path / file_name );

    const auto& _idx = db.get_index< index_type, hive::chain::by_id >();

    auto _itr = _idx.begin();

    uint32_t _cnt = 0;

    while( _itr != _idx.end() )
    {
      _file << _itr->get_author_and_permlink_hash().str() << std::endl;
      ++_itr;
      ++_cnt;
    }

    _file.close();

    dlog("************** ${_cnt} ${name} **************", (_cnt)(name));
  }

  void scan_comment_objects( const std::string& file_name, const std::string& name )
  {
    scan_any_objects<hive::chain::comment_index>( file_name, name );
  }

  void scan_volatile_comment_objects( const std::string& file_name, const std::string& name )
  {
    scan_any_objects<hive::chain::volatile_comment_index>( file_name, name );
  }
};

int main( int argc, char** argv )
{
  try
  {
    appbase::application theApp;
    bool _started_loop = false;

    theApp.init_signals_handler();

    BOOST_SCOPE_EXIT(&theApp, &_started_loop)
    {
      if( theApp.quit() && _started_loop )
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
      hive::plugins::account_by_key::account_by_key_api_plugin,
      hive::plugins::state_snapshot::state_snapshot_plugin >();

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

    _started_loop = true;

    theApp.load_logging_config();

    try
    {
      auto& chainPlugin = theApp.get_plugin<hive::plugins::chain::chain_plugin>();
      theApp.finish_request = [&chainPlugin](){ chainPlugin.finish_request(); };

      theApp.startup();

      scanner _scanner( chainPlugin, theApp.data_dir() );
      _scanner.scan_comment_objects( "00.comment_object.txt", "comment_object" );
      _scanner.scan_volatile_comment_objects( "01.volatile_comment_object.txt", "volatile_comment_object" );
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

    return initializationResult.get_result_code();
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
