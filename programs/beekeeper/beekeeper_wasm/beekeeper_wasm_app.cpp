#include <beekeeper_wasm/beekeeper_wasm_app.hpp>

#include <fc/log/logger_config.hpp>

#include <boost/filesystem.hpp>

#include <iostream>

namespace beekeeper {

namespace bfs = boost::filesystem;

beekeeper_wasm_app::beekeeper_wasm_app()
{
  old_log_level = fc::logger::get( "default" ).get_log_level();
}

beekeeper_wasm_app::~beekeeper_wasm_app()
{
  if( enable_logs )
    fc::logger::get( "default" ).set_log_level( old_log_level );
}

void beekeeper_wasm_app::set_program_options()
{
  options_cfg.add_options()
    ("enable-logs", boost::program_options::value<bool>()->default_value( true ), "Whether logs can be written. By default logs are enabled" )
    ;

  beekeeper_app_base::set_program_options();
}

uint32_t beekeeper_wasm_app::initialize( int argc, char** argv )
{
  bpo::store( bpo::parse_command_line( argc, argv, options_cfg ), args );
  return initialize_program_options();
}

void beekeeper_wasm_app::start()
{
  ilog( "Start in progress. Nothing to do" );
}

const boost::program_options::variables_map& beekeeper_wasm_app::get_args() const
{
  return args;
}

bfs::path beekeeper_wasm_app::get_data_dir() const
{
  return bfs::current_path();
}

void beekeeper_wasm_app::setup( const boost::program_options::variables_map& args )
{
  FC_ASSERT( args.count("enable-logs") );

  enable_logs = args.at( "enable-logs" ).as<bool>();

  if( enable_logs )
    fc::configure_logging( fc::logging_config::default_config( "stdout" ) );
  else
    fc::logger::get( "default" ).set_log_level( fc::log_level::off );
}

std::shared_ptr<beekeeper::beekeeper_wallet_manager> beekeeper_wasm_app::create_wallet( const boost::filesystem::path& cmd_wallet_dir, uint64_t cmd_unlock_timeout, uint32_t cmd_session_limit )
{
  return std::make_shared<beekeeper::beekeeper_wallet_manager>( std::make_shared<session_manager_base>(), std::make_shared<beekeeper_instance_base>(),
                                                                      cmd_wallet_dir, cmd_unlock_timeout, cmd_session_limit );
}

uint32_t beekeeper_wasm_app::init( int argc, char** argv )
{
  return run( argc, argv );
}

}