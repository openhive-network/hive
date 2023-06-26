#include <beekeeper_wasm_core/beekeeper_wasm_app.hpp>

#include <boost/filesystem.hpp>

namespace beekeeper {

namespace bfs = boost::filesystem;

beekeeper_wasm_app::beekeeper_wasm_app()
{
}

beekeeper_wasm_app::~beekeeper_wasm_app()
{
}

void beekeeper_wasm_app::set_program_options()
{
  beekeeper_app_init::set_program_options();
}

std::pair<bool, bool> beekeeper_wasm_app::initialize( int argc, char** argv )
{
  bpo::store( bpo::parse_command_line( argc, argv, options ), args );
  auto _initialization = initialize_program_options();
  if( !_initialization.first )
    return { false, true };

  return { true, false };
}

bool beekeeper_wasm_app::start()
{
  std::cout << "Start in progress. Nothing to do\n";
  return true;
}

const boost::program_options::variables_map& beekeeper_wasm_app::get_args() const
{
  return args;
}

bfs::path beekeeper_wasm_app::get_data_dir() const
{
  return bfs::current_path();
}

void beekeeper_wasm_app::setup_notifications( const boost::program_options::variables_map& args )
{
}

std::shared_ptr<beekeeper::beekeeper_wallet_manager> beekeeper_wasm_app::create_wallet( const boost::filesystem::path& cmd_wallet_dir, uint64_t cmd_unlock_timeout, uint32_t cmd_session_limit )
{
  return std::make_shared<beekeeper::beekeeper_wallet_manager>( std::make_shared<session_manager_base>(), std::make_shared<beekeeper_instance_base>( cmd_wallet_dir ),
                                                                      cmd_wallet_dir, cmd_unlock_timeout, cmd_session_limit );
}

}