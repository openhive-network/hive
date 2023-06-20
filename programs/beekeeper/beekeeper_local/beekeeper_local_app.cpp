#include <beekeeper_local/beekeeper_local_app.hpp>

#include <boost/filesystem.hpp>

namespace beekeeper {

namespace bfs = boost::filesystem;

beekeeper_local_app::beekeeper_local_app()
{
}

beekeeper_local_app::~beekeeper_local_app()
{
}

std::pair<bool, bool> beekeeper_local_app::initialize( int argc, char** argv )
{
  bpo::store( bpo::parse_command_line( argc, argv, options ), args );
  auto _initialization = initialize_program_options();
  if( !_initialization.first )
    return { false, true };

  hive::utilities::notifications::dynamic_notify( notification_handler, "starting with token: " + _initialization.second );

  return { true, false };
}

bool beekeeper_local_app::start()
{
  std::cout << "Start in progress. Nothing to do\n";
  return true;
}

const boost::program_options::variables_map& beekeeper_local_app::get_args() const
{
  return args;
}

bfs::path beekeeper_local_app::get_data_dir() const
{
  return bfs::current_path();
}

void beekeeper_local_app::setup_notifications( const boost::program_options::variables_map& args )
{
  notification_handler.setup( hive::utilities::notifications::setup_notifications( args ) );
}

}