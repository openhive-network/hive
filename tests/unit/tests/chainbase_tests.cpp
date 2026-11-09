#include <boost/test/unit_test.hpp>

#include <chainbase/chainbase.hpp>
#include <chainbase/huge_pages.hpp>

#include <hive/utilities/tempdir.hpp>

#include <fc/filesystem.hpp>
#include <fc/log/logger_config.hpp>

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
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

void write_file( const chainbase::bfs::path& path, std::time_t modified )
{
  std::ofstream( path.string() ).put( 'x' );
  chainbase::bfs::last_write_time( path, modified );
}

int link_file( const chainbase::bfs::path& existing, const chainbase::bfs::path& new_name )
{
  return ::link( existing.string().c_str(), new_name.string().c_str() ) == 0 ? 0 : errno;
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
  const std::time_t now = std::time( nullptr );
  const std::string stray = "shared_memory.bin.0123456789abcdef0123456789abcdef.new";
  // a creation in progress in another PID namespace, whose PID looks dead here
  const auto in_progress = new_state_file_name( dead_pid() );
  write_file( state_dir / stray, now - 120 );
  write_file( state_dir / in_progress, now );

  wipeable_database db;
  db.open( state_dir, 0, STATE_FILE_SIZE );
  BOOST_CHECK( !has_file( state_dir, stray ) );
  BOOST_CHECK( has_file( state_dir, in_progress ) );
  db.close();

  write_file( state_dir / stray, now - 120 );
  db.wipe( state_dir );
  BOOST_CHECK( file_names( state_dir ) == std::vector< std::string >{ in_progress } );
}

BOOST_AUTO_TEST_CASE( concurrent_creations_of_one_process_do_not_share_a_new_state_file )
{
  fc::temp_directory dir( hive::utilities::temp_directory_path() );
  const auto state_dir = shared_memory_dir( dir );
  const auto refused_for_lock = refused_with( "could not gain write access" );

  // Both creators run in this process, so they have the same PID; the second one runs to completion while the first
  // is about to link its file in place.
  chainbase::database first;
  chainbase::database second;
  chainbase::bfs::path first_new_path;
  chainbase::bfs::path second_new_path;
  bool first_new_path_intact = false;
  second.set_link_file_function( [&]( const chainbase::bfs::path& existing, const chainbase::bfs::path& new_name )
  {
    second_new_path = existing;
    first_new_path_intact = chainbase::bfs::exists( first_new_path );
    return link_file( existing, new_name );
  } );
  bool second_created = false;
  first.set_link_file_function( [&]( const chainbase::bfs::path& existing, const chainbase::bfs::path& new_name )
  {
    if( first_new_path.empty() )
    {
      first_new_path = existing;
      second.open( state_dir, 0, STATE_FILE_SIZE );
      second_created = true;
    }
    return link_file( existing, new_name );
  } );
  BOOST_CHECK_EXCEPTION( first.open( state_dir, 0, STATE_FILE_SIZE ), std::runtime_error, refused_for_lock );

  BOOST_REQUIRE( second_created );
  BOOST_CHECK( first_new_path != second_new_path );
  BOOST_CHECK( first_new_path_intact );
  BOOST_CHECK( file_names( state_dir ) == std::vector< std::string >{ "shared_memory.bin" } );
  second.close();
  // the state file second created, with its environment
  BOOST_CHECK_NO_THROW( first.open( state_dir, 0, STATE_FILE_SIZE ) );
  first.close();
}

BOOST_AUTO_TEST_CASE( state_file_creation_is_repeated_when_its_new_state_file_is_removed )
{
  fc::temp_directory dir( hive::utilities::temp_directory_path() );
  const auto state_dir = shared_memory_dir( dir );

  chainbase::database creator;
  std::vector< chainbase::bfs::path > new_paths;
  creator.set_link_file_function( [&new_paths]( const chainbase::bfs::path& existing, const chainbase::bfs::path& new_name )
  {
    new_paths.push_back( existing );
    if( new_paths.size() == 1 )
      chainbase::bfs::remove( existing );
    return link_file( existing, new_name );
  } );
  creator.open( state_dir, 0, STATE_FILE_SIZE );
  BOOST_CHECK_EQUAL( new_paths.size(), 2u );
  BOOST_CHECK( file_names( state_dir ) == std::vector< std::string >{ "shared_memory.bin" } );

  chainbase::database other;
  BOOST_CHECK_EXCEPTION( other.open( state_dir, 0, STATE_FILE_SIZE ), std::runtime_error, refused_with( "could not gain write access" ) );
  creator.close();
}

BOOST_AUTO_TEST_CASE( resize_keeps_the_state_file_in_the_page_cache )
{
  fc::temp_directory dir( hive::utilities::temp_directory_path() );
  const auto state_dir = shared_memory_dir( dir );
  {
    chainbase::database creator;
    creator.open( state_dir, 0, STATE_FILE_SIZE );
    creator.close();
  }

  chainbase::database db;
  std::vector< chainbase::bfs::path > evicted;
  db.set_evict_page_cache_function( [&evicted]( const chainbase::bfs::path& state_file ) { evicted.push_back( state_file ); } );
  db.open( state_dir, 0, STATE_FILE_SIZE );
  BOOST_REQUIRE_EQUAL( evicted.size(), 1u );
  BOOST_CHECK( evicted.front() == chainbase::bfs::absolute( state_dir / "shared_memory.bin" ) );

  db.resize( 2 * STATE_FILE_SIZE );
  db.resize( 3 * STATE_FILE_SIZE );
  BOOST_CHECK_EQUAL( db.get_max_memory(), 3 * STATE_FILE_SIZE );
  BOOST_CHECK_EQUAL( evicted.size(), 1u );
  db.close();
}

BOOST_AUTO_TEST_CASE( failed_resize_aborts_with_its_cause )
{
  fc::temp_directory dir( hive::utilities::temp_directory_path() );
  const auto state_dir = shared_memory_dir( dir );

  int output[ 2 ];
  BOOST_REQUIRE_EQUAL( ::pipe( output ), 0 );
  const pid_t pid = ::fork();
  BOOST_REQUIRE( pid >= 0 );
  if( pid == 0 )
  {
    // the test framework's handlers would report the signals and carry on running the tests in this process
    ::signal( SIGABRT, SIG_DFL );
    ::signal( SIGALRM, SIG_DFL );
    // bounded, so that a hang fails the test instead of stalling it
    ::alarm( 60 );
    ::dup2( output[ 1 ], STDERR_FILENO );
    ::close( output[ 0 ] );
    ::close( output[ 1 ] );
    try
    {
      fc::configure_logging( fc::logging_config::default_config() );
      chainbase::database db;
      db.open( state_dir, 0, STATE_FILE_SIZE );
      // more than any filesystem has free
      db.resize( size_t( 1 ) << 62 );
    }
    catch( ... )
    {
      ::_exit( 2 );
    }
    ::_exit( 0 );
  }
  ::close( output[ 1 ] );
  std::string logged;
  char buffer[ 4096 ];
  for( ssize_t n; ( n = ::read( output[ 0 ], buffer, sizeof( buffer ) ) ) != 0; )
  {
    if( n < 0 && errno == EINTR )
      continue;
    BOOST_REQUIRE( n > 0 );
    logged.append( buffer, n );
  }
  ::close( output[ 0 ] );
  int status = 0;
  BOOST_REQUIRE_EQUAL( ::waitpid( pid, &status, 0 ), pid );

  BOOST_TEST_MESSAGE( logged );
  BOOST_CHECK( WIFSIGNALED( status ) && WTERMSIG( status ) == SIGABRT );
  BOOST_CHECK( logged.find( "FATAL: could not map the shared memory file" ) != std::string::npos );
  BOOST_CHECK( logged.find( "Free space available is less than declared size" ) != std::string::npos );
}

BOOST_AUTO_TEST_CASE( huge_pages_are_requested_for_every_mapping_of_the_state_file )
{
  fc::temp_directory dir( hive::utilities::temp_directory_path() );
  const auto state_dir = shared_memory_dir( dir );

  chainbase::database db;
  std::vector< std::pair< void*, size_t > > advised;
  // refused as by a kernel without transparent huge pages, which must not stop the node
  db.set_advise_huge_pages_function( [&advised]( void* address, size_t size ) { advised.emplace_back( address, size ); return EINVAL; } );
  db.open( state_dir, 0, STATE_FILE_SIZE );
  BOOST_REQUIRE_EQUAL( advised.size(), 1u );
  BOOST_CHECK( advised.back().first != nullptr );
  BOOST_CHECK_EQUAL( advised.back().second, STATE_FILE_SIZE );

  db.resize( 2 * STATE_FILE_SIZE );
  db.resize( 3 * STATE_FILE_SIZE );
  BOOST_REQUIRE_EQUAL( advised.size(), 3u );
  BOOST_CHECK_EQUAL( advised[ 1 ].second, 2 * STATE_FILE_SIZE );
  BOOST_CHECK_EQUAL( advised[ 2 ].second, 3 * STATE_FILE_SIZE );
  BOOST_CHECK_EQUAL( db.get_max_memory(), 3 * STATE_FILE_SIZE );
  db.close();

  chainbase::database reopened;
  reopened.set_advise_huge_pages_function( [&advised]( void* address, size_t size ) { advised.emplace_back( address, size ); return 0; } );
  reopened.open( state_dir, 0, STATE_FILE_SIZE );
  BOOST_REQUIRE_EQUAL( advised.size(), 4u );
  BOOST_CHECK_EQUAL( advised.back().second, 3 * STATE_FILE_SIZE );
  reopened.close();
}

BOOST_AUTO_TEST_CASE( tmpfs_mount_without_huge_pages_is_found_in_mountinfo )
{
  const std::string mountinfo =
    "22 1 8:1 / / rw,relatime shared:1 - ext4 /dev/sda1 rw,errors=remount-ro\n"
    "25 22 0:23 / /dev rw,nosuid,relatime shared:2 - devtmpfs udev rw,size=8000000k,mode=755\n"
    "26 25 0:24 / /dev/shm rw,nosuid,nodev shared:3 - tmpfs tmpfs rw,inode64\n"
    "40 22 0:40 / /mnt/huge rw,relatime shared:20 - tmpfs tmpfs rw,size=1048576k,inode64,huge=within_size\n"
    "41 22 0:41 / /mnt/advise rw,relatime - tmpfs tmpfs rw,huge=advise\n"
    "42 22 0:42 / /mnt/never rw,relatime - tmpfs tmpfs rw,huge=never\n"
    "43 40 0:43 / /mnt/huge/nested rw,relatime - tmpfs tmpfs rw,inode64\n"
    "44 42 8:1 /data/state /mnt/never/bound rw master:1 - ext4 /dev/sda1 rw\n"
    "45 22 0:45 / /mnt/with\\040space rw - tmpfs tmpfs rw\n"
    "46 22 0:46 / /mnt/stacked rw - tmpfs tmpfs rw\n"
    "47 46 0:47 / /mnt/stacked rw - tmpfs tmpfs rw,huge=always\n";
  const auto mount_of = [&mountinfo]( const std::string& path )
  {
    std::istringstream input( mountinfo );
    return chainbase::tmpfs_mount_without_huge_pages( input, chainbase::bfs::path( path ) );
  };
  const auto none = std::optional< std::string >();

  BOOST_CHECK( mount_of( "/var/lib/hived" ) == none );
  BOOST_CHECK( mount_of( "/dev/shm/hived" ) == std::optional< std::string >( "/dev/shm" ) );
  BOOST_CHECK( mount_of( "/dev/shm" ) == std::optional< std::string >( "/dev/shm" ) );
  BOOST_CHECK( mount_of( "/dev/shmx/hived" ) == none );
  BOOST_CHECK( mount_of( "/mnt/huge/state" ) == none );
  BOOST_CHECK( mount_of( "/mnt/advise/state" ) == none );
  BOOST_CHECK( mount_of( "/mnt/never/state" ) == std::optional< std::string >( "/mnt/never" ) );
  BOOST_CHECK( mount_of( "/mnt/huge/nested/state" ) == std::optional< std::string >( "/mnt/huge/nested" ) );
  BOOST_CHECK( mount_of( "/mnt/never/bound/state" ) == none );
  BOOST_CHECK( mount_of( "/mnt/with space/state" ) == std::optional< std::string >( "/mnt/with space" ) );
  BOOST_CHECK( mount_of( "/mnt/stacked/state" ) == none );

  std::istringstream empty;
  BOOST_CHECK( chainbase::tmpfs_mount_without_huge_pages( empty, "/dev/shm/hived" ) == none );
}

BOOST_AUTO_TEST_SUITE_END()
