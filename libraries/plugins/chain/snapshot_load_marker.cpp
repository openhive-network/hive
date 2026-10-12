#include <hive/plugins/chain/snapshot_load_marker.hpp>

#include <fc/exception/exception.hpp>
#include <fc/io/json.hpp>
#include <fc/variant_object.hpp>

#include <boost/filesystem/operations.hpp>

#include <fstream>
#include <iterator>

namespace hive { namespace plugins { namespace chain {

namespace bfs = boost::filesystem;

namespace {

const char* const LOADING = "loading";
const char* const REPLAYING = "replaying";

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
  bfs::rename( temp_path, _path );
}

void snapshot_load_marker::clear() const
{
  bfs::remove( _path );
}

} } } // hive::plugins::chain
