#include <hive/plugins/chain/snapshot_load_marker.hpp>

#include <fc/exception/exception.hpp>
#include <fc/io/json.hpp>
#include <fc/variant_object.hpp>

#include <boost/filesystem/operations.hpp>

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <fstream>
#include <iterator>

namespace hive { namespace plugins { namespace chain {

namespace bfs = boost::filesystem;

namespace {

const char* const LOADING = "loading";
const char* const REPLAYING = "replaying";

/// Flushes a file, or a directory's entries, to the disk.
void sync_to_disk( const bfs::path& path )
{
  const int fd = ::open( path.string().c_str(), O_RDONLY | O_CLOEXEC );
  FC_ASSERT( fd >= 0, "Cannot open ${p}: ${e}", ( "p", path.string() )( "e", std::strerror( errno ) ) );
  const int result = ::fsync( fd );
  const int error = errno;
  ::close( fd );
  FC_ASSERT( result == 0, "Cannot flush ${p} to the disk: ${e}", ( "p", path.string() )( "e", std::strerror( error ) ) );
}

} // namespace

snapshot_load_marker::snapshot_load_marker( const bfs::path& shared_memory_dir )
  : _path( shared_memory_dir / FILE_NAME )
{
}

std::optional< snapshot_load_marker::pending_load > snapshot_load_marker::read() const
{
  if( !bfs::exists( _path ) )
    return std::nullopt;

  std::ifstream file( _path.string() );
  const std::string content( ( std::istreambuf_iterator< char >( file ) ), std::istreambuf_iterator< char >() );
  try
  {
    const fc::variant_object marker = fc::json::from_string( content, fc::json::format_validation_mode::full ).get_object();
    pending_load result;
    result.snapshot = marker[ "snapshot" ].as_string();
    const std::string stage = marker[ "stage" ].as_string();
    FC_ASSERT( stage == LOADING || stage == REPLAYING, "Unknown stage `${stage}'", ( stage ) );
    result.stage = stage == REPLAYING ? load_stage::replaying : load_stage::loading;
    return result;
  }
  catch( const fc::exception& e )
  {
    wlog( "Snapshot load marker ${p} cannot be read, so it is taken for an unfinished load: ${e}", ( "p", _path.string() )( "e", e.to_string() ) );
    return pending_load{};
  }
}

void snapshot_load_marker::write( load_stage stage, const std::string& snapshot ) const
{
  const fc::variant_object marker = fc::mutable_variant_object( "stage", stage == load_stage::replaying ? REPLAYING : LOADING )( "snapshot", snapshot );
  const bfs::path temp_path = _path.string() + ".tmp";
  {
    std::ofstream file( temp_path.string(), std::ios::trunc );
    file << fc::json::to_string( marker ) << '\n';
    file.flush();
    FC_ASSERT( file.good(), "Cannot write snapshot load marker ${p}", ( "p", temp_path.string() ) );
  }
  // Without both a power loss could leave no marker, or an empty one, next to a partial state.
  sync_to_disk( temp_path );
  bfs::rename( temp_path, _path );
  sync_to_disk( _path.parent_path() );
}

void snapshot_load_marker::clear() const
{
  bfs::remove( _path );
}

} } } // hive::plugins::chain
