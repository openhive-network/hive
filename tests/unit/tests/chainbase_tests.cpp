#include <boost/test/unit_test.hpp>

#include <chainbase/chainbase.hpp>
#include <chainbase/container_layout.hpp>
#include <chainbase/huge_pages.hpp>
#include <chainbase/index_key_snapshot.hpp>

#include <hive/utilities/tempdir.hpp>

#include <fc/filesystem.hpp>
#include <fc/io/raw.hpp>
#include <fc/log/logger_config.hpp>
#include <fc/reflect/reflect.hpp>

#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>

#include <algorithm>
#include <array>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
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

struct keyed_item
{
  uint32_t id = 0;
  std::array< char, 4 > name = {};
  uint32_t group = 0;
  uint64_t score = 0;
  uint64_t payload = 0;

  uint64_t get_score() const { return score; }
  const std::array< char, 4 >& get_name() const { return name; }
};

struct name_hash
{
  size_t operator()( const std::array< char, 4 >& name ) const { return std::hash< std::string_view >()( std::string_view( name.data(), name.size() ) ); }
};

namespace bmi = boost::multi_index;

using keyed_item_index = bmi::multi_index_container<
  keyed_item,
  bmi::indexed_by<
    bmi::ordered_unique< bmi::member< keyed_item, uint32_t, &keyed_item::id > >,
    bmi::hashed_unique< bmi::const_mem_fun< keyed_item, const std::array< char, 4 >&, &keyed_item::get_name >, name_hash >,
    bmi::hashed_non_unique< bmi::member< keyed_item, uint32_t, &keyed_item::group > >,
    bmi::ordered_non_unique< bmi::const_mem_fun< keyed_item, uint64_t, &keyed_item::get_score > >,
    bmi::ordered_non_unique< bmi::composite_key< keyed_item,
      bmi::member< keyed_item, uint32_t, &keyed_item::group >,
      bmi::const_mem_fun< keyed_item, uint64_t, &keyed_item::get_score > > >,
    bmi::ordered_unique< bmi::composite_key< keyed_item,
      bmi::const_mem_fun< keyed_item, const std::array< char, 4 >&, &keyed_item::get_name >,
      bmi::member< keyed_item, uint32_t, &keyed_item::id > > >
  >
>;

struct string_keyed_item
{
  uint32_t id = 0;
  std::string name;
};

using string_keyed_item_index = bmi::multi_index_container<
  string_keyed_item,
  bmi::indexed_by<
    bmi::ordered_unique< bmi::member< string_keyed_item, uint32_t, &string_keyed_item::id > >,
    bmi::ordered_unique< bmi::member< string_keyed_item, std::string, &string_keyed_item::name > >
  >
>;

class layout_item : public chainbase::object< 0, layout_item >
{
  CHAINBASE_OBJECT( layout_item );

public:
  CHAINBASE_DEFAULT_CONSTRUCTOR( layout_item )

  uint64_t key = 0;
};

using layout_item_index = bmi::multi_index_container<
  layout_item,
  bmi::indexed_by<
    bmi::ordered_unique< bmi::tag< chainbase::by_id >, bmi::const_mem_fun< layout_item, layout_item::id_type, &layout_item::get_id > >
  >,
  chainbase::multi_index_allocator< layout_item >
>;

class other_layout_item : public chainbase::object< 1, other_layout_item >
{
  CHAINBASE_OBJECT( other_layout_item );

public:
  CHAINBASE_DEFAULT_CONSTRUCTOR( other_layout_item )
};

using other_layout_item_index = bmi::multi_index_container<
  other_layout_item,
  bmi::indexed_by<
    bmi::ordered_unique< bmi::tag< chainbase::by_id >, bmi::const_mem_fun< other_layout_item, other_layout_item::id_type, &other_layout_item::get_id > >
  >,
  chainbase::multi_index_allocator< other_layout_item >
>;

struct keyed_hash
{
  size_t operator()( uint64_t key ) const { return std::hash< uint64_t >()( key ^ seed ); }
  uint64_t seed = 0;
};

// the same objects in a container laid out differently, stored under the same name
using hashed_layout_item_index = bmi::multi_index_container<
  layout_item,
  bmi::indexed_by<
    bmi::ordered_unique< bmi::tag< chainbase::by_id >, bmi::const_mem_fun< layout_item, layout_item::id_type, &layout_item::get_id > >,
    bmi::hashed_unique< bmi::member< layout_item, uint64_t, &layout_item::key >, keyed_hash >
  >,
  chainbase::multi_index_allocator< layout_item >
>;

// the same objects in a container of another type with the same size, stored under the same name
using non_unique_layout_item_index = bmi::multi_index_container<
  layout_item,
  bmi::indexed_by<
    bmi::ordered_non_unique< bmi::tag< chainbase::by_id >, bmi::const_mem_fun< layout_item, layout_item::id_type, &layout_item::get_id > >
  >,
  chainbase::multi_index_allocator< layout_item >
>;

using archived_keys = bmi::multi_index_container<
  uint64_t,
  bmi::indexed_by< bmi::ordered_unique< bmi::identity< uint64_t > > >,
  boost::interprocess::allocator< uint64_t, boost::interprocess::managed_mapped_file::segment_manager >
>;

using hashed_archived_keys = bmi::multi_index_container<
  uint64_t,
  bmi::indexed_by< bmi::hashed_unique< bmi::identity< uint64_t >, keyed_hash > >,
  boost::interprocess::allocator< uint64_t, boost::interprocess::managed_mapped_file::segment_manager >
>;

const auto replay_needed = []( const std::logic_error& e ) { return std::string( e.what() ).find( "A replay (or a snapshot load) is needed" ) != std::string::npos; };

template< typename Index >
void create_state_with_index( const chainbase::bfs::path& state_dir )
{
  chainbase::database db;
  db.open( state_dir, 0, STATE_FILE_SIZE );
  db.add_index< Index >();
  db.close();
}

/// What a first open leaves when it is interrupted after adding the index of layout_item and recording RecordedContainer
/// as the layout of the index of other_layout_item, before constructing that index.
template< typename RecordedContainer >
void create_interrupted_state( const chainbase::bfs::path& state_dir )
{
  chainbase::database db;
  db.open( state_dir, 0, STATE_FILE_SIZE );
  db.add_index< layout_item_index >();
  chainbase::record_container_layout< RecordedContainer >( *db.get_segment_manager(), boost::core::demangle( typeid( other_layout_item ).name() ) );
  db.close();
}

std::string read_file( const chainbase::bfs::path& path )
{
  std::ifstream file( path.string(), std::ios::binary );
  return std::string( std::istreambuf_iterator< char >( file ), std::istreambuf_iterator< char >() );
}

const auto creation_interrupted = []( const std::logic_error& e )
{
  return replay_needed( e ) && std::string( e.what() ).find( "was not completely created: its first open did not complete (it was interrupted or failed)" ) != std::string::npos;
};

const auto lock_left_held = []( const std::logic_error& e )
{
  return replay_needed( e ) && std::string( e.what() ).find( "was left held by a process that was killed" ) != std::string::npos;
};

const auto other_index_is_missing = []( const std::logic_error& e )
{
  const std::string what = e.what();
  return replay_needed( e ) && what.find( "The index " + boost::core::demangle( typeid( other_layout_item ).name() ) + " is not stored" ) != std::string::npos &&
    what.find( "differs" ) == std::string::npos;
};

}

FC_REFLECT( layout_item, (id)(key) )
FC_REFLECT( other_layout_item, (id) )

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

BOOST_AUTO_TEST_CASE( failed_pool_block_allocation_names_its_size )
{
  fc::temp_directory dir( hive::utilities::temp_directory_path() );
  chainbase::database db;
  db.open( shared_memory_dir( dir ), 0, STATE_FILE_SIZE );
  auto* segment_manager = db.get_segment_manager();

  // leave the free memory in 1 KiB holes
  constexpr size_t hole_size = 1024;
  std::vector< void* > chunks;
  while( void* chunk = segment_manager->allocate( hole_size, std::nothrow ) )
    chunks.push_back( chunk );
  std::vector< void* > kept;
  for( size_t i = 0; i < chunks.size(); ++i )
  {
    if( i % 2 == 0 )
      kept.push_back( chunks[ i ] );
    else
      segment_manager->deallocate( chunks[ i ] );
  }
  const size_t largest = db.get_largest_free_block();
  BOOST_REQUIRE_GE( largest, hole_size );
  BOOST_REQUIRE_LT( largest, 4 * hole_size );
  BOOST_REQUIRE_GT( db.get_free_memory(), 64 * hole_size );
  BOOST_CHECK( db.has_contiguous_free_memory( largest ) );
  BOOST_CHECK( !db.has_contiguous_free_memory( largest + 1 ) );

  // one block of the pool holds 64 items of 64 bytes, so it fits in no hole
  chainbase::pool_allocator_t< std::array< char, 64 >, 64, false > pool( segment_manager );
  try
  {
    pool.allocate();
    BOOST_FAIL( "the pool block was allocated" );
  }
  catch( const boost::interprocess::bad_alloc& e )
  {
    BOOST_CHECK( dynamic_cast< const chainbase::segment_allocation_error* >( &e ) != nullptr );
    const std::string message = e.what();
    BOOST_TEST_MESSAGE( message );
    BOOST_CHECK( message.find( "Could not allocate " ) == 0 );
    BOOST_CHECK( message.find( "a block of 64 objects of" ) != std::string::npos );
    BOOST_CHECK( message.find( "the largest free block holds " + std::to_string( largest ) + "." ) != std::string::npos );
  }
  BOOST_CHECK_EQUAL( db.get_largest_free_block(), largest );

  for( void* chunk : kept )
    segment_manager->deallocate( chunk );
  BOOST_CHECK_NO_THROW( pool.deallocate( pool.allocate() ) );
  db.close();
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

BOOST_AUTO_TEST_CASE( index_key_snapshot_copies_every_kind_of_index_key )
{
  using keys_snapshot = chainbase::index_keys::index_key_snapshot< keyed_item, keyed_item_index >;
  static_assert( keys_snapshot::supported, "keys of ordered and hashed, unique and non-unique indexes are copied" );
  static_assert( std::is_same_v< keys_snapshot::type,
    std::tuple< uint32_t, std::array< char, 4 >, uint32_t, uint64_t, std::tuple< uint32_t, uint64_t >, std::tuple< std::array< char, 4 >, uint32_t > > >,
    "keys are copied by value, composite keys as tuples of their members" );
  static_assert( !chainbase::index_keys::index_key_snapshot< string_keyed_item, string_keyed_item_index >::supported,
    "a container with a key owning allocated memory is re-indexed on every modify" );

  keyed_item_index items;
  const auto& item = *items.insert( keyed_item{ 1, { 'a', 'b', 'c', 0 }, 7, 9, 0 } ).first;
  const auto keys = keys_snapshot::take( items, item );
  BOOST_CHECK( keys == keys_snapshot::type( 1, item.name, 7, 9, std::make_tuple( 7u, uint64_t( 9 ) ), std::make_tuple( item.name, 1u ) ) );

  items.modify( items.begin(), []( keyed_item& i ){ ++i.payload; } );
  BOOST_CHECK( keys_snapshot::take( items, item ) == keys );
  items.modify( items.begin(), []( keyed_item& i ){ ++i.score; } );
  BOOST_CHECK( keys_snapshot::take( items, item ) != keys );
  BOOST_CHECK( std::get< 4 >( keys_snapshot::take( items, item ) ) == std::make_tuple( 7u, uint64_t( 10 ) ) );
}

BOOST_AUTO_TEST_CASE( index_is_reopened_by_the_build_that_laid_it_out )
{
  fc::temp_directory dir( hive::utilities::temp_directory_path() );
  const auto state_dir = shared_memory_dir( dir );
  create_state_with_index< layout_item_index >( state_dir );

  chainbase::database db;
  db.open( state_dir, 0, STATE_FILE_SIZE );
  BOOST_CHECK_NO_THROW( db.add_index< layout_item_index >() );
  db.close();

  chainbase::database reader;
  reader.open( state_dir, chainbase::database::read_only_storage );
  BOOST_CHECK_NO_THROW( reader.add_index< layout_item_index >() );
  reader.close();
}

BOOST_AUTO_TEST_CASE( index_laid_out_differently_is_refused )
{
  fc::temp_directory dir( hive::utilities::temp_directory_path() );
  const auto state_dir = shared_memory_dir( dir );
  create_state_with_index< layout_item_index >( state_dir );
  // The stored container really has another size, which Boost.Interprocess asserts against in a typed find (the stored
  // bytes have to be a multiple of the type's size). With Boost asserts enabled (a Debug build), this aborts unless the
  // recorded layout is read before the container is looked up as hashed_layout_item_index.
  static_assert( sizeof( chainbase::generic_index< layout_item_index > ) % sizeof( chainbase::generic_index< hashed_layout_item_index > ) != 0 );

  chainbase::database db;
  db.open( state_dir, 0, STATE_FILE_SIZE );
  BOOST_CHECK_EXCEPTION( db.add_index< hashed_layout_item_index >(), chainbase::container_layout_mismatch, replay_needed );
  db.close();

  chainbase::database reader;
  reader.open( state_dir, chainbase::database::read_only_storage );
  BOOST_CHECK_EXCEPTION( reader.add_index< hashed_layout_item_index >(), chainbase::container_layout_mismatch, replay_needed );
  reader.close();
}

BOOST_AUTO_TEST_CASE( index_of_another_type_with_the_same_size_is_refused )
{
  static_assert( sizeof( chainbase::generic_index< layout_item_index > ) == sizeof( chainbase::generic_index< non_unique_layout_item_index > ) );
  fc::temp_directory dir( hive::utilities::temp_directory_path() );
  const auto state_dir = shared_memory_dir( dir );
  create_state_with_index< layout_item_index >( state_dir );

  chainbase::database db;
  db.open( state_dir, 0, STATE_FILE_SIZE );
  BOOST_CHECK_EXCEPTION( db.add_index< non_unique_layout_item_index >(), chainbase::container_layout_mismatch, replay_needed );
  db.close();
}

BOOST_AUTO_TEST_CASE( index_of_another_size_with_the_same_type_name_is_refused )
{
  fc::temp_directory dir( hive::utilities::temp_directory_path() );
  const auto state_dir = shared_memory_dir( dir );
  create_state_with_index< layout_item_index >( state_dir );

  chainbase::database db;
  db.open( state_dir, 0, STATE_FILE_SIZE );
  // what a build leaves whose container has the same type name, but a member of another size (a seeded hasher)
  const std::string index_name = boost::core::demangle( typeid( layout_item ).name() );
  auto* stored = db.get_segment_manager()->find< chainbase::container_layout >( ( index_name + "::layout" ).c_str() ).first;
  BOOST_REQUIRE( stored != nullptr );
  static_assert( sizeof( chainbase::container_layout ) == 2 * sizeof( uint64_t ) );
  uint64_t fields[ 2 ];
  std::memcpy( fields, stored, sizeof( fields ) );
  BOOST_REQUIRE_EQUAL( fields[ 0 ], sizeof( chainbase::generic_index< layout_item_index > ) );
  fields[ 0 ] += sizeof( uint64_t );
  std::memcpy( static_cast< void* >( stored ), fields, sizeof( fields ) );

  BOOST_CHECK_EXCEPTION( db.add_index< layout_item_index >(), chainbase::container_layout_mismatch, replay_needed );
  db.close();
}

BOOST_AUTO_TEST_CASE( index_without_recorded_layout_is_refused )
{
  fc::temp_directory dir( hive::utilities::temp_directory_path() );
  const auto state_dir = shared_memory_dir( dir );
  create_state_with_index< layout_item_index >( state_dir );

  chainbase::database db;
  db.open( state_dir, 0, STATE_FILE_SIZE );
  // what a build that did not record layouts leaves
  const std::string index_name = boost::core::demangle( typeid( layout_item ).name() );
  BOOST_REQUIRE( db.get_segment_manager()->destroy< chainbase::container_layout >( ( index_name + "::layout" ).c_str() ) );
  BOOST_CHECK_EXCEPTION( db.add_index< layout_item_index >(), std::logic_error, replay_needed );
  db.close();
}

BOOST_AUTO_TEST_CASE( index_is_created_when_only_its_layout_is_recorded )
{
  fc::temp_directory dir( hive::utilities::temp_directory_path() );
  const auto state_dir = shared_memory_dir( dir );
  {
    // what a first open interrupted between recording the layout and constructing the index leaves
    chainbase::database db;
    db.open( state_dir, 0, STATE_FILE_SIZE );
    chainbase::record_container_layout< chainbase::generic_index< layout_item_index > >( *db.get_segment_manager(),
      boost::core::demangle( typeid( layout_item ).name() ) );
    db.close();
  }

  chainbase::database db;
  db.open( state_dir, 0, STATE_FILE_SIZE );
  BOOST_CHECK_NO_THROW( db.add_index< layout_item_index >() );
  db.close();

  chainbase::database reader;
  reader.open( state_dir, chainbase::database::read_only_storage );
  BOOST_CHECK_NO_THROW( reader.add_index< layout_item_index >() );
  reader.close();
}

BOOST_AUTO_TEST_CASE( interrupted_first_open_is_refused )
{
  fc::temp_directory dir( hive::utilities::temp_directory_path() );
  const auto state_dir = shared_memory_dir( dir );
  create_interrupted_state< chainbase::generic_index< other_layout_item_index > >( state_dir );

  chainbase::database db;
  db.open( state_dir, 0, STATE_FILE_SIZE );
  BOOST_REQUIRE_NO_THROW( db.add_index< layout_item_index >() );
  BOOST_CHECK_EXCEPTION( db.add_index< other_layout_item_index >(), chainbase::index_set_mismatch, other_index_is_missing );
  db.close();
}

BOOST_AUTO_TEST_CASE( state_whose_creation_did_not_complete_is_refused )
{
  // as an open is left when its process is killed before any index is stored (its environment holds no version or
  // plugins either), while indexes are stored, and once all of them are stored
  for( int stored_indexes : { 0, 1, 2 } )
  {
    BOOST_TEST_MESSAGE( "indexes stored: " << stored_indexes );
    fc::temp_directory dir( hive::utilities::temp_directory_path() );
    const auto state_dir = shared_memory_dir( dir );
    const auto state_file = state_dir / "shared_memory.bin";
    {
      chainbase::database creator;
      creator.open( state_dir, chainbase::database::track_creation, STATE_FILE_SIZE );
      if( stored_indexes > 0 )
        creator.add_index< layout_item_index >();
      if( stored_indexes > 1 )
        creator.add_index< other_layout_item_index >();
      creator.close();
    }
    const std::string created = read_file( state_file );

    const std::string version = R"({"version":{"node_type":"testnet"}})";
    const helpers::environment_extension_resources environment_extension( version, { "chain" }, []( const std::string& ) {} );
    // also by an open that would grow the file first
    for( uint32_t flags : { 0u, uint32_t( chainbase::database::track_creation ) } )
    {
      chainbase::database db;
      BOOST_CHECK_EXCEPTION( db.open( state_dir, flags, 2 * STATE_FILE_SIZE, nullptr, &environment_extension ), chainbase::incomplete_state, creation_interrupted );
    }
    chainbase::database reader;
    BOOST_CHECK_EXCEPTION( reader.open( state_dir, chainbase::database::read_only_storage ), chainbase::incomplete_state, creation_interrupted );
    BOOST_CHECK( read_file( state_file ) == created );
  }
}

BOOST_AUTO_TEST_CASE( state_whose_creation_completed_opens )
{
  fc::temp_directory dir( hive::utilities::temp_directory_path() );
  const auto state_dir = shared_memory_dir( dir );
  {
    chainbase::database creator;
    creator.open( state_dir, chainbase::database::track_creation, STATE_FILE_SIZE );
    creator.add_index< layout_item_index >();
    // a creation that grows the state is not refused by its own mark
    BOOST_REQUIRE_NO_THROW( creator.resize( 2 * STATE_FILE_SIZE ) );
    creator.complete_creation();
    creator.close();
  }

  for( uint32_t flags : { 0u, uint32_t( chainbase::database::track_creation ), uint32_t( chainbase::database::read_only_storage ) } )
  {
    chainbase::database db;
    BOOST_REQUIRE_NO_THROW( db.open( state_dir, flags, STATE_FILE_SIZE ) );
    BOOST_CHECK_NO_THROW( db.add_index< layout_item_index >() );
    BOOST_CHECK_NO_THROW( db.complete_creation() );
    db.close();
  }
}

BOOST_AUTO_TEST_CASE( state_locked_by_a_killed_process_is_refused )
{
  fc::temp_directory dir( hive::utilities::temp_directory_path() );
  const auto state_dir = shared_memory_dir( dir );
  create_state_with_index< layout_item_index >( state_dir );

  const pid_t pid = ::fork();
  BOOST_REQUIRE( pid >= 0 );
  if( pid == 0 )
  {
    // the test framework's handler would report the signal and carry on running the tests in this process
    ::signal( SIGALRM, SIG_DFL );
    ::alarm( 60 );
    try
    {
      chainbase::database db;
      db.open( state_dir, 0, STATE_FILE_SIZE );
      // dies holding the segment's lock, as a process killed inside an index construction does
      auto die = []() { ::_exit( 0 ); };
      db.get_segment_manager()->atomic_func( die );
    }
    catch( ... )
    {
    }
    ::_exit( 2 );
  }
  int status = 0;
  BOOST_REQUIRE_EQUAL( ::waitpid( pid, &status, 0 ), pid );
  BOOST_REQUIRE( WIFEXITED( status ) && WEXITSTATUS( status ) == 0 );

  const auto state_file = state_dir / "shared_memory.bin";
  const std::string left = read_file( state_file );

  // refused before the file is grown, and the same way on every later open, leaving the file as it is
  chainbase::database db;
  BOOST_CHECK_EXCEPTION( db.open( state_dir, 0, 2 * STATE_FILE_SIZE ), chainbase::incomplete_state, lock_left_held );
  chainbase::database reader;
  BOOST_CHECK_EXCEPTION( reader.open( state_dir, chainbase::database::read_only_storage ), chainbase::incomplete_state, lock_left_held );
  BOOST_CHECK_EXCEPTION( db.open( state_dir, 0, STATE_FILE_SIZE ), chainbase::incomplete_state, lock_left_held );
  BOOST_CHECK( read_file( state_file ) == left );
}

BOOST_AUTO_TEST_CASE( layout_recorded_for_a_missing_index_is_reported_as_missing )
{
  fc::temp_directory dir( hive::utilities::temp_directory_path() );
  const auto state_dir = shared_memory_dir( dir );
  // the layout is not the one of the running build, but there is no container to read through it
  create_interrupted_state< chainbase::generic_index< layout_item_index > >( state_dir );

  chainbase::database db;
  db.open( state_dir, 0, STATE_FILE_SIZE );
  BOOST_REQUIRE_NO_THROW( db.add_index< layout_item_index >() );
  BOOST_CHECK_EXCEPTION( db.add_index< other_layout_item_index >(), chainbase::index_set_mismatch, other_index_is_missing );
  db.close();
}

BOOST_AUTO_TEST_CASE( named_container_layout_is_verified )
{
  fc::temp_directory dir( hive::utilities::temp_directory_path() );
  chainbase::database db;
  db.open( shared_memory_dir( dir ), 0, STATE_FILE_SIZE );
  auto& segment_manager = *db.get_segment_manager();

  // neither the container nor its layout is stored yet
  BOOST_CHECK( !chainbase::verify_container_layout< archived_keys >( segment_manager, "archive" ) );

  // a layout recorded without its container, even another one, does not stand for a stored container
  chainbase::record_container_layout< hashed_archived_keys >( segment_manager, "archive" );
  BOOST_CHECK( !chainbase::verify_container_layout< archived_keys >( segment_manager, "archive" ) );
  BOOST_REQUIRE( segment_manager.destroy< chainbase::container_layout >( "archive::layout" ) );

  // a container stored without its layout, as a build that did not record layouts leaves it
  segment_manager.construct< archived_keys >( "archive" )( archived_keys::allocator_type( &segment_manager ) );
  BOOST_CHECK_EXCEPTION( chainbase::verify_container_layout< archived_keys >( segment_manager, "archive" ), chainbase::container_layout_mismatch, replay_needed );

  chainbase::record_container_layout< archived_keys >( segment_manager, "archive" );
  BOOST_CHECK( chainbase::verify_container_layout< archived_keys >( segment_manager, "archive" ) );
  BOOST_CHECK_EXCEPTION( chainbase::verify_container_layout< hashed_archived_keys >( segment_manager, "archive" ), chainbase::container_layout_mismatch, replay_needed );
  BOOST_CHECK( !( chainbase::container_layout::of< archived_keys >() == chainbase::container_layout::of< hashed_archived_keys >() ) );

  // the types of the members of a wrapper are part of its layout
  BOOST_CHECK( !( chainbase::container_layout::of< layout_item, archived_keys >() == chainbase::container_layout::of< layout_item, hashed_archived_keys >() ) );
  BOOST_CHECK( !( chainbase::container_layout::of< layout_item, archived_keys >() == chainbase::container_layout::of< layout_item >() ) );
  db.close();
}

BOOST_AUTO_TEST_SUITE_END()
