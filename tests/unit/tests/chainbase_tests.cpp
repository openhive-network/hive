#include <boost/test/unit_test.hpp>

#include <chainbase/chainbase.hpp>

#include <hive/utilities/tempdir.hpp>

#include <fc/filesystem.hpp>

#include <string>
#include <vector>

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

BOOST_AUTO_TEST_SUITE_END()
