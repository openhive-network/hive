#include <boost/test/unit_test.hpp>

#include <chainbase/chainbase.hpp>

#include <hive/utilities/tempdir.hpp>

#include <fc/filesystem.hpp>

#include <algorithm>
#include <cerrno>
#include <fstream>
#include <string>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

namespace {

constexpr size_t STATE_FILE_SIZE = 1024 * 1024;

chainbase::bfs::path shared_memory_dir( const fc::temp_directory& dir )
{
  return chainbase::bfs::absolute( static_cast< const chainbase::bfs::path& >( dir.path() ) );
}

std::vector< std::string > file_names( const chainbase::bfs::path& dir )
{
  std::vector< std::string > names;
  for( const auto& entry : chainbase::bfs::directory_iterator( dir ) )
    names.push_back( entry.path().filename().string() );
  return names;
}

auto refused_with( const std::string& message )
{
  return [message]( const std::runtime_error& e ) { return std::string( e.what() ).find( message ) != std::string::npos; };
}

bool has_file( const chainbase::bfs::path& dir, const std::string& name )
{
  const auto names = file_names( dir );
  return std::find( names.begin(), names.end(), name ) != names.end();
}

pid_t dead_pid()
{
  const pid_t pid = ::fork();
  if( pid == 0 )
    ::_exit( 0 );
  BOOST_REQUIRE( pid > 0 );
  BOOST_REQUIRE( ::waitpid( pid, nullptr, 0 ) == pid );
  return pid;
}

std::string new_state_file_name( pid_t pid )
{
  return "shared_memory.bin." + std::to_string( pid ) + ".new";
}

struct wipeable_database : chainbase::database
{
  using chainbase::database::wipe;
};

}

BOOST_AUTO_TEST_SUITE( chainbase_tests )

BOOST_AUTO_TEST_CASE( refused_open_releases_the_state_file )
{
  fc::temp_directory dir( hive::utilities::temp_directory_path() );
  const auto state_dir = shared_memory_dir( dir );
  // a segment without the environment chainbase stores in every file it creates
  chainbase::bip::managed_mapped_file( chainbase::bip::create_only, ( state_dir / "shared_memory.bin" ).string().c_str(), STATE_FILE_SIZE );
  const auto refused_for_environment = refused_with( "Unable to find environment data" );

  chainbase::database first;
  BOOST_CHECK_EXCEPTION( first.open( state_dir, 0, STATE_FILE_SIZE ), std::runtime_error, refused_for_environment );

  chainbase::database writer;
  BOOST_CHECK_EXCEPTION( writer.open( state_dir, 0, STATE_FILE_SIZE ), std::runtime_error, refused_for_environment );
  chainbase::database reader;
  BOOST_CHECK_EXCEPTION( reader.open( state_dir, chainbase::database::read_only_storage ), std::runtime_error, refused_for_environment );
}

BOOST_AUTO_TEST_CASE( created_state_file_is_locked_from_the_start )
{
  fc::temp_directory dir( hive::utilities::temp_directory_path() );
  const auto state_dir = shared_memory_dir( dir );

  chainbase::database creator;
  creator.open( state_dir, 0, STATE_FILE_SIZE );
  BOOST_CHECK( file_names( state_dir ) == std::vector< std::string >{ "shared_memory.bin" } );

  chainbase::database other;
  BOOST_CHECK_EXCEPTION( other.open( state_dir, 0, STATE_FILE_SIZE ), std::runtime_error, refused_with( "could not gain write access" ) );

  creator.close();
  BOOST_CHECK_NO_THROW( other.open( state_dir, 0, STATE_FILE_SIZE ) );
  other.close();
}

BOOST_AUTO_TEST_CASE( state_file_is_created_in_place_without_hard_links )
{
  fc::temp_directory dir( hive::utilities::temp_directory_path() );
  const auto state_dir = shared_memory_dir( dir );

  chainbase::database creator;
  int link_attempts = 0;
  creator.set_link_file_function( [&link_attempts]( const chainbase::bfs::path&, const chainbase::bfs::path& ) { ++link_attempts; return EPERM; } );
  creator.open( state_dir, 0, STATE_FILE_SIZE );
  BOOST_CHECK_EQUAL( link_attempts, 1 );
  BOOST_CHECK( file_names( state_dir ) == std::vector< std::string >{ "shared_memory.bin" } );

  chainbase::database other;
  BOOST_CHECK_EXCEPTION( other.open( state_dir, 0, STATE_FILE_SIZE ), std::runtime_error, refused_with( "could not gain write access" ) );

  creator.close();
  // opened as a complete state file, with its environment
  BOOST_CHECK_NO_THROW( other.open( state_dir, 0, STATE_FILE_SIZE ) );
  other.close();
}

BOOST_AUTO_TEST_CASE( stray_new_state_files_are_removed )
{
  fc::temp_directory dir( hive::utilities::temp_directory_path() );
  const auto state_dir = shared_memory_dir( dir );
  const auto stray = new_state_file_name( dead_pid() );
  // a creation in progress in another live process
  const auto in_progress = new_state_file_name( ::getppid() );
  std::ofstream( ( state_dir / stray ).string() ).put( 'x' );
  std::ofstream( ( state_dir / in_progress ).string() ).put( 'x' );

  wipeable_database db;
  db.open( state_dir, 0, STATE_FILE_SIZE );
  BOOST_CHECK( !has_file( state_dir, stray ) );
  BOOST_CHECK( has_file( state_dir, in_progress ) );
  db.close();

  std::ofstream( ( state_dir / stray ).string() ).put( 'x' );
  db.wipe( state_dir );
  BOOST_CHECK( file_names( state_dir ) == std::vector< std::string >{ in_progress } );
}

BOOST_AUTO_TEST_SUITE_END()
