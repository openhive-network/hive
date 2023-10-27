#include <beekeeper_wasm/beekeeper_wasm_app.hpp>

#include <fc/log/logger_config.hpp>

#include <boost/filesystem.hpp>

#include <iostream>

namespace beekeeper {

namespace bfs = boost::filesystem;

beekeeper_wasm_app::beekeeper_wasm_app()
{
}

beekeeper_wasm_app::~beekeeper_wasm_app()
{
  if( enable_logs )
    fc::configure_logging( fc::logging_config() );//destroy explicitly created appenders
}

void beekeeper_wasm_app::set_program_options()
{
  options.add_options()
    ("enable-logs", boost::program_options::value<bool>()->default_value( true ), "Whether logs can be written. By default logs are enabled" )
    ;

  beekeeper_app_init::set_program_options();
}

init_data beekeeper_wasm_app::initialize( int argc, char** argv )
{
  bpo::store( bpo::parse_command_line( argc, argv, options ), args );
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

void beekeeper_wasm_app::setup_logger( const boost::program_options::variables_map& args )
{
  FC_ASSERT( args.count("enable-logs") );

  enable_logs = args.at( "enable-logs" ).as<bool>();

  if( enable_logs )
    fc::configure_logging( fc::logging_config::default_config( "stdout" ) );
  else
    fc::configure_logging( fc::logging_config() );
}

std::shared_ptr<beekeeper::beekeeper_wallet_manager> beekeeper_wasm_app::create_wallet( const boost::filesystem::path& cmd_wallet_dir, uint64_t cmd_unlock_timeout, uint32_t cmd_session_limit )
{
  return std::make_shared<beekeeper::beekeeper_wallet_manager>( std::make_shared<session_manager_base>(), std::make_shared<beekeeper_instance_base>( cmd_wallet_dir ),
                                                                      cmd_wallet_dir, cmd_unlock_timeout, cmd_session_limit );
}

init_data beekeeper_wasm_app::init( int argc, char** argv )
{
  return run( argc, argv );
}

}