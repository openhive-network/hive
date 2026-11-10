#include <hive/chain/external_storage/rocksdb_trace.hpp>

#include <fc/exception/exception.hpp>
#include <fc/filesystem.hpp>
#include <fc/io/json.hpp>
#include <fc/log/logger.hpp>
#include <fc/variant_object.hpp>

#include <rocksdb/db.h>
#include <rocksdb/env.h>
#include <rocksdb/trace_reader_writer.h>
#include <rocksdb/utilities/checkpoint.h>
#include <rocksdb/version.h>

#include <boost/filesystem/operations.hpp>

#include <memory>

namespace hive { namespace chain {

namespace bfs = boost::filesystem;

namespace {

const char* const CHECKPOINT_DIR_NAME = "checkpoint";
const char* const TRACE_FILE_NAME = "trace";
const char* const INFO_FILE_NAME = "trace_info.json";

}

rocksdb_trace::rocksdb_trace( const rocksdb_trace_config& config, const std::string& db_name )
  : _config( config ), _db_name( db_name ), _output_dir( bfs::absolute( config.dir / db_name ) )
{
  FC_ASSERT( _config.first_block < _config.last_block,
    "RocksDB trace of `${name}` needs a block range whose first block is below its last one, got ${f}-${l}",
    ("name", _db_name)("f", _config.first_block)("l", _config.last_block) );
  FC_ASSERT( !bfs::exists( _output_dir ),
    "RocksDB trace output directory `${d}` already exists. Move it away or choose another trace directory.",
    ("d", _output_dir.string()) );
  ilog( "`${name}` RocksDB database will be traced into `${d}` while the last irreversible block is in ${f}-${l}",
    ("name", _db_name)("d", _output_dir.string())("f", _config.first_block)("l", _config.last_block) );
}

void rocksdb_trace::on_lib( ::rocksdb::DB& db, uint32_t lib )
{
  switch( _state )
  {
  case state::waiting:
    if( lib >= _config.last_block )
    {
      wlog( "`${name}` RocksDB database is past the trace range ${f}-${l} (last irreversible block ${lib}), nothing will be traced",
        ("name", _db_name)("f", _config.first_block)("l", _config.last_block)(lib) );
      _state = state::done;
    }
    else if( lib >= _config.first_block )
    {
      start( db, lib );
    }
    break;
  case state::tracing:
    if( lib >= _config.last_block )
      stop( db, lib, "end of range" );
    break;
  case state::done:
    break;
  }
}

void rocksdb_trace::start( ::rocksdb::DB& db, uint32_t lib )
{
  // Set first, so a failure below is not retried on every following block.
  _state = state::done;

  bfs::create_directories( _output_dir );

  // The checkpoint is the database exactly as the first traced operation finds it: nothing is
  // written between the two, the caller's thread being the only writer.
  ::rocksdb::Checkpoint* checkpoint_ptr = nullptr;
  const auto create_status = ::rocksdb::Checkpoint::Create( &db, &checkpoint_ptr );
  std::unique_ptr< ::rocksdb::Checkpoint > checkpoint( checkpoint_ptr );
  FC_ASSERT( create_status.ok(), "Cannot create a checkpoint of `${name}` RocksDB database: ${e}", ("name", _db_name)("e", create_status.ToString()) );
  const bfs::path checkpoint_dir = _output_dir / CHECKPOINT_DIR_NAME;
  const auto checkpoint_status = checkpoint->CreateCheckpoint( checkpoint_dir.string() );
  FC_ASSERT( checkpoint_status.ok(), "Cannot create a checkpoint of `${name}` RocksDB database in `${d}`: ${e}",
    ("name", _db_name)("d", checkpoint_dir.string())("e", checkpoint_status.ToString()) );

  std::unique_ptr< ::rocksdb::TraceWriter > trace_writer;
  const bfs::path trace_file = _output_dir / TRACE_FILE_NAME;
  const auto writer_status = ::rocksdb::NewFileTraceWriter( db.GetEnv(), ::rocksdb::EnvOptions(), trace_file.string(), &trace_writer );
  FC_ASSERT( writer_status.ok(), "Cannot create RocksDB trace file `${f}`: ${e}", ("f", trace_file.string())("e", writer_status.ToString()) );
  const auto trace_status = db.StartTrace( ::rocksdb::TraceOptions(), std::move( trace_writer ) );
  FC_ASSERT( trace_status.ok(), "Cannot start tracing `${name}` RocksDB database: ${e}", ("name", _db_name)("e", trace_status.ToString()) );

  _state = state::tracing;
  _start_block = lib;
  write_info( nullptr );
  ilog( "Started tracing `${name}` RocksDB database at last irreversible block ${lib} into `${d}`",
    ("name", _db_name)(lib)("d", _output_dir.string()) );
}

void rocksdb_trace::stop( ::rocksdb::DB& db, uint32_t lib, const char* reason )
{
  if( _state != state::tracing )
    return;
  _state = state::done;
  _end_block = lib;

  const auto status = db.EndTrace();
  if( !status.ok() )
  {
    elog( "Cannot end the trace of `${name}` RocksDB database in `${d}`, the trace file may be incomplete: ${e}",
      ("name", _db_name)("d", _output_dir.string())("e", status.ToString()) );
    return;
  }

  try
  {
    write_info( reason );
  }
  catch( const fc::exception& e )
  {
    elog( "Cannot record the end of the trace of `${name}` RocksDB database: ${e}", ("name", _db_name)("e", e.to_detail_string()) );
    return;
  }
  ilog( "Ended tracing `${name}` RocksDB database at last irreversible block ${lib} (${reason}), trace is in `${d}`",
    ("name", _db_name)(lib)(reason)("d", _output_dir.string()) );
}

void rocksdb_trace::write_info( const char* end_reason ) const
{
  fc::mutable_variant_object info;
  info( "database", _db_name )
    ( "first_block", _config.first_block )
    ( "last_block", _config.last_block )
    ( "start_block", _start_block )
    ( "checkpoint", CHECKPOINT_DIR_NAME )
    ( "trace", TRACE_FILE_NAME )
    ( "rocksdb_version", ::rocksdb::GetRocksVersionAsString() )
    ( "ended", end_reason != nullptr );
  if( end_reason != nullptr )
    info( "end_block", _end_block )( "end_reason", end_reason );

  // Replaced in one rename, so a reader never sees a half written file.
  const bfs::path info_file = _output_dir / INFO_FILE_NAME;
  const bfs::path tmp_file = _output_dir / ( std::string( INFO_FILE_NAME ) + ".tmp" );
  fc::json::save_to_file( fc::variant( info ), fc::path( tmp_file ) );
  bfs::rename( tmp_file, info_file );
}

} } // hive::chain
