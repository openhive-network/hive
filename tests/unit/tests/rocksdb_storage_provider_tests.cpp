
#include <boost/test/unit_test.hpp>

#include <hive/chain/database_exceptions.hpp>
#include <hive/chain/external_storage/rocksdb_comment_storage_provider.hpp>
#include <hive/chain/external_storage/rocksdb_format.hpp>
#include <hive/chain/external_storage/types.hpp>

#include <hive/utilities/tempdir.hpp>

#include <appbase/application.hpp>

#include <fc/filesystem.hpp>
#include <fc/io/json.hpp>

#include <rocksdb/advanced_cache.h>
#include <rocksdb/db.h>
#include <rocksdb/filter_policy.h>
#include <rocksdb/table.h>
#include <rocksdb/table_properties.h>
#include <rocksdb/trace_reader_writer.h>
#include <rocksdb/utilities/replayer.h>

#include <map>
#include <memory>

using namespace hive::chain;

namespace {

/// Exposes the column handles so the effective options of each column family can be read back.
class inspectable_comment_storage_provider : public rocksdb_comment_storage_provider
{
  public:
    using rocksdb_comment_storage_provider::rocksdb_comment_storage_provider;
    using rocksdb_storage_provider::getColumnHandles;
};

/// compression_name table property of an SST file written with Snappy: from format_version 7 it
/// names the compression manager and, in hex, the compression types its blocks are stored with -
/// Snappy, or none when no block compressed well enough to be stored compressed.
const std::string SNAPPY_COMPRESSION_NAME = "BuiltinV2;01;";
const std::string NOTHING_COMPRESSED_NAME = "BuiltinV2;;";

/// format_version of the SST files hived wrote before it moved to ROCKSDB_FORMAT_VERSION.
constexpr uint32_t OLDER_FORMAT_VERSION = 6;

std::string comment_key( char seed )
{
  return std::string( 20, seed );
}

/// Checks that every SST file of the column family has the current on-disk format, and a
/// partitioned index exactly when @p partitioned. Returns how many of them hold Snappy compressed blocks.
size_t check_tables_of_column_family( ::rocksdb::DB& db, ::rocksdb::ColumnFamilyHandle* cf_handle, bool partitioned )
{
  size_t snappy_compressed = 0;
  ::rocksdb::TablePropertiesCollection tables;
  BOOST_REQUIRE( db.GetPropertiesOfAllTables( cf_handle, &tables ).ok() );
  BOOST_REQUIRE_MESSAGE( !tables.empty(), "no SST file in column family " << cf_handle->GetName() );
  for( const auto& [ file, properties ] : tables )
  {
    BOOST_CHECK_MESSAGE( properties->format_version == ROCKSDB_FORMAT_VERSION,
      file << " has format_version " << properties->format_version );
    BOOST_CHECK_MESSAGE( properties->compression_name == SNAPPY_COMPRESSION_NAME ||
      properties->compression_name == NOTHING_COMPRESSED_NAME,
      file << " has compression " << properties->compression_name );
    if( properties->compression_name == SNAPPY_COMPRESSION_NAME )
      ++snappy_compressed;
    BOOST_CHECK_MESSAGE( properties->filter_size > 0, file << " has no filter" );
    BOOST_CHECK_MESSAGE( ( properties->index_partitions > 0 ) == partitioned,
      file << " has " << properties->index_partitions << " index partitions" );
  }
  return snappy_compressed;
}

/// Lays out an archive as an older build did, with the given options for both column families,
/// then checks that the current provider (with the given block cache) opens it and reads every
/// comment back, and that a compaction rewrites the files in the current format and layout.
void check_archive_written_by_older_build_stays_readable( const ::rocksdb::ColumnFamilyOptions& older_options,
  size_t block_cache_size = rocksdb_comment_storage_provider::DEFAULT_BLOCK_CACHE_SIZE )
{
  fc::temp_directory temp_dir( hive::utilities::temp_directory_path() );
  const bfs::path storage_path = temp_dir.path() / "comments-rocksdb-storage";

  std::map< std::string, std::string > comments;
  for( char seed = 'a'; seed <= 'z'; ++seed )
    comments[ comment_key( seed ) ] = std::string( "comment-" ) + seed;

  {
    ::rocksdb::Options options( ::rocksdb::DBOptions(), older_options );
    options.create_if_missing = true;
    std::unique_ptr< ::rocksdb::DB > db;
    BOOST_REQUIRE( ::rocksdb::DB::Open( options, storage_path.string(), &db ).ok() );

    ::rocksdb::ColumnFamilyOptions hash_options = older_options;
    hash_options.comparator = by_Hash_Comparator();
    ::rocksdb::ColumnFamilyHandle* hash_column = nullptr;
    BOOST_REQUIRE( db->CreateColumnFamily( hash_options, "account_permlink_hash", &hash_column ).ok() );

    ::rocksdb::WriteBatch batch;
    BOOST_REQUIRE( batch.Put( Slice( "STORE_MAJOR_VERSION" ), PrimitiveTypeSlice< uint32_t >( STORE_MAJOR_VERSION ) ).ok() );
    BOOST_REQUIRE( batch.Put( Slice( "STORE_MINOR_VERSION" ), PrimitiveTypeSlice< uint32_t >( STORE_MINOR_VERSION ) ).ok() );
    for( const auto& comment : comments )
      BOOST_REQUIRE( batch.Put( comment.first, comment.second ).ok() );
    BOOST_REQUIRE( db->Write( ::rocksdb::WriteOptions(), &batch ).ok() );
    BOOST_REQUIRE( db->Flush( ::rocksdb::FlushOptions() ).ok() );

    ::rocksdb::TablePropertiesCollection tables;
    BOOST_REQUIRE( db->GetPropertiesOfAllTables( &tables ).ok() );
    BOOST_REQUIRE( !tables.empty() );
    for( const auto& [ file, properties ] : tables )
      BOOST_REQUIRE_MESSAGE( properties->format_version == OLDER_FORMAT_VERSION,
        file << " has format_version " << properties->format_version );

    BOOST_REQUIRE( db->DestroyColumnFamilyHandle( hash_column ).ok() );
    BOOST_REQUIRE( db->Close().ok() );
  }

  appbase::application app;
  inspectable_comment_storage_provider provider( temp_dir.path(), storage_path, app, block_cache_size );
  provider.openDb( 0 );

  for( const auto& comment : comments )
  {
    PinnableSlice value;
    BOOST_REQUIRE( provider.read( comment.first, value ) );
    BOOST_CHECK_EQUAL( value.ToString(), comment.second );
  }
  PinnableSlice missing;
  BOOST_CHECK( !provider.read( comment_key( '0' ), missing ) );

  ::rocksdb::DB& db = *provider.getStorage();
  ::rocksdb::ColumnFamilyHandle* comment_column = provider.getColumnHandles()[ CommentsColumns::COMMENT ];
  ::rocksdb::CompactRangeOptions compact_options;
  compact_options.bottommost_level_compaction = ::rocksdb::BottommostLevelCompaction::kForce;
  BOOST_REQUIRE( db.CompactRange( compact_options, comment_column, nullptr, nullptr ).ok() );
  BOOST_CHECK_GT( check_tables_of_column_family( db, comment_column,
    block_cache_size < rocksdb_comment_storage_provider::PARTITIONED_METADATA_CACHE_LIMIT ), 0u );

  for( const auto& comment : comments )
  {
    PinnableSlice value;
    BOOST_REQUIRE( provider.read( comment.first, value ) );
    BOOST_CHECK_EQUAL( value.ToString(), comment.second );
  }

  provider.shutdownDb();
}

/// Column family options of the archive as written before hived wrote format_version 7, without
/// table options of its own (before table options and column family tuning).
::rocksdb::ColumnFamilyOptions older_untuned_layout()
{
  ::rocksdb::BlockBasedTableOptions table_options;
  table_options.format_version = OLDER_FORMAT_VERSION;
  ::rocksdb::ColumnFamilyOptions options;
  options.table_factory.reset( ::rocksdb::NewBlockBasedTableFactory( table_options ) );
  options.compression = ::rocksdb::kSnappyCompression;
  return options;
}

/// Column family options of the archive as written with full filters and a binary search index
/// (before hive#1026, and from PARTITIONED_METADATA_CACHE_LIMIT up) or with partitioned ones.
::rocksdb::ColumnFamilyOptions older_table_layout( bool partitioned )
{
  ::rocksdb::BlockBasedTableOptions table_options;
  table_options.format_version = OLDER_FORMAT_VERSION;
  table_options.filter_policy.reset( ::rocksdb::NewBloomFilterPolicy( 10 ) );
  table_options.cache_index_and_filter_blocks = true;
  table_options.pin_l0_filter_and_index_blocks_in_cache = true;
  if( partitioned )
  {
    table_options.index_type = ::rocksdb::BlockBasedTableOptions::kTwoLevelIndexSearch;
    table_options.partition_filters = true;
  }
  ::rocksdb::ColumnFamilyOptions options;
  options.table_factory.reset( ::rocksdb::NewBlockBasedTableFactory( table_options ) );
  options.compression = ::rocksdb::kSnappyCompression;
  return options;
}

/// Opens an archive with a block cache of the given size and checks the effective options of
/// both column families: the shared cache, the bloom filter, and partitioned index and filters
/// exactly when @p partitioned.
void check_opened_archive_options( size_t block_cache_size, bool partitioned )
{
  fc::temp_directory temp_dir( hive::utilities::temp_directory_path() );
  const bfs::path storage_path = temp_dir.path() / "comments-rocksdb-storage";

  appbase::application app;
  inspectable_comment_storage_provider provider( temp_dir.path(), storage_path, app, block_cache_size );
  provider.openDb( 0 );

  const auto& handles = provider.getColumnHandles();
  BOOST_REQUIRE_EQUAL( handles.size(), 2u );

  BOOST_CHECK_EQUAL( provider.getStorage()->GetDBOptions().max_total_wal_size, 512u * 1024 * 1024 );

  std::shared_ptr< ::rocksdb::Cache > shared_cache;
  for( auto* handle : handles )
  {
    BOOST_TEST_MESSAGE( "Column family: " << handle->GetName() );
    const ::rocksdb::Options options = provider.getStorage()->GetOptions( handle );

    BOOST_CHECK_EQUAL( options.write_buffer_size, 32u * 1024 * 1024 );
    BOOST_CHECK_EQUAL( options.max_write_buffer_number, 6 );
    BOOST_CHECK_EQUAL( options.min_write_buffer_number_to_merge, 2 );
    BOOST_CHECK_EQUAL( options.level0_file_num_compaction_trigger, 2 );
    BOOST_CHECK_EQUAL( options.target_file_size_base, 64u * 1024 * 1024 );
    BOOST_CHECK_GE( options.max_bytes_for_level_base, 256u * 1024 * 1024 );
    BOOST_CHECK( options.compaction_style == ::rocksdb::kCompactionStyleLevel );
    BOOST_CHECK( options.compression == ::rocksdb::kSnappyCompression );
    BOOST_CHECK( options.compression_per_level.empty() );

    BOOST_REQUIRE( options.table_factory );
    const auto* table_options = options.table_factory->GetOptions< ::rocksdb::BlockBasedTableOptions >();
    BOOST_REQUIRE( table_options != nullptr );
    BOOST_CHECK_EQUAL( table_options->format_version, ROCKSDB_FORMAT_VERSION );
    BOOST_REQUIRE( table_options->filter_policy );
    BOOST_CHECK_EQUAL( std::string( table_options->filter_policy->CompatibilityName() ), "rocksdb.BuiltinBloomFilter" );
    BOOST_CHECK( table_options->cache_index_and_filter_blocks );
    BOOST_CHECK( table_options->cache_index_and_filter_blocks_with_high_priority );
    BOOST_CHECK( table_options->pin_l0_filter_and_index_blocks_in_cache );
    if( partitioned )
    {
      BOOST_CHECK( table_options->index_type == ::rocksdb::BlockBasedTableOptions::kTwoLevelIndexSearch );
      BOOST_CHECK( table_options->partition_filters );
      BOOST_CHECK_EQUAL( table_options->metadata_block_size, 16384u );
      BOOST_CHECK( table_options->pin_top_level_index_and_filter );
    }
    else
    {
      BOOST_CHECK( table_options->index_type == ::rocksdb::BlockBasedTableOptions::kBinarySearch );
      BOOST_CHECK( !table_options->partition_filters );
    }

    BOOST_REQUIRE( table_options->block_cache );
    BOOST_CHECK_EQUAL( table_options->block_cache->GetCapacity(), block_cache_size );
    if( !shared_cache )
      shared_cache = table_options->block_cache;
    BOOST_CHECK( table_options->block_cache == shared_cache );
  }

  provider.shutdownDb();
}

/// Writes comments through a provider with a block cache of the given size, then checks with
/// plain RocksDB that every SST file has the current on-disk format, and a partitioned index
/// exactly when @p partitioned.
void check_written_tables( size_t block_cache_size, bool partitioned )
{
  fc::temp_directory temp_dir( hive::utilities::temp_directory_path() );
  const bfs::path storage_path = temp_dir.path() / "comments-rocksdb-storage";

  {
    appbase::application app;
    rocksdb_comment_storage_provider provider( temp_dir.path(), storage_path, app, block_cache_size );
    provider.openDb( 0 );
    for( uint32_t i = 0; i < 100; ++i )
    {
      const std::string key = "comment-key-" + std::to_string( i );
      const std::string value( 512, static_cast< char >( 'a' + i % 26 ) );
      provider.save( Slice( key ), Slice( value ) );
    }
    provider.shutdownDb();
  }

  std::vector< std::string > cf_names;
  BOOST_REQUIRE( ::rocksdb::DB::ListColumnFamilies( ::rocksdb::DBOptions(), storage_path.string(), &cf_names ).ok() );
  BOOST_REQUIRE_EQUAL( cf_names.size(), 2u );

  std::vector< ::rocksdb::ColumnFamilyDescriptor > cf_descriptors;
  for( const auto& cf_name : cf_names )
  {
    cf_descriptors.emplace_back( cf_name, ::rocksdb::ColumnFamilyOptions() );
    if( cf_name == "account_permlink_hash" )
      cf_descriptors.back().options.comparator = by_Hash_Comparator();
  }

  std::vector< ::rocksdb::ColumnFamilyHandle* > cf_handles;
  std::unique_ptr< ::rocksdb::DB > db;
  BOOST_REQUIRE( ::rocksdb::DB::OpenForReadOnly( ::rocksdb::DBOptions(), storage_path.string(), cf_descriptors, &cf_handles, &db ).ok() );

  // the comments (repeated characters) compress, unlike the hashes of account_permlink_hash
  size_t snappy_compressed = 0;
  for( auto* cf_handle : cf_handles )
    snappy_compressed += check_tables_of_column_family( *db, cf_handle, partitioned );
  BOOST_CHECK_GT( snappy_compressed, 0u );

  for( auto* cf_handle : cf_handles )
    BOOST_REQUIRE( db->DestroyColumnFamilyHandle( cf_handle ).ok() );
}

std::string block_comment_key( uint32_t block_num )
{
  std::string key = "comment-of-block-" + std::to_string( block_num );
  key.resize( 20, '.' );
  return key;
}

/// Moves the archive through the given irreversible blocks as rocksdb_comment_archive does
/// (update_lib, then the comments archived in that block), archiving one comment per block and
/// reading the one of the block before.
void archive_blocks( rocksdb_comment_storage_provider& provider, uint32_t first_block, uint32_t last_block )
{
  for( uint32_t block_num = first_block; block_num <= last_block; ++block_num )
  {
    provider.update_lib( block_num );
    provider.save( block_comment_key( block_num ), "comment-body-" + std::to_string( block_num ) );
    provider.flushWriteBuffer();
    PinnableSlice previous;
    BOOST_REQUIRE( provider.read( block_comment_key( block_num - 1 ), previous ) || block_num == 1 );
  }
}

fc::variant_object read_trace_info( const bfs::path& trace_output )
{
  return fc::json::from_file( fc::path( trace_output / "trace_info.json" ), fc::json::format_validation_mode::full ).get_object();
}

/// Opens a copy of the archive (as written by rocksdb_comment_storage_provider) with plain RocksDB.
struct opened_archive_copy
{
  std::unique_ptr< ::rocksdb::DB > db;
  std::vector< ::rocksdb::ColumnFamilyHandle* > handles;

  explicit opened_archive_copy( const bfs::path& path )
  {
    std::vector< ::rocksdb::ColumnFamilyDescriptor > cf_descriptors;
    cf_descriptors.emplace_back( ::rocksdb::kDefaultColumnFamilyName, ::rocksdb::ColumnFamilyOptions() );
    cf_descriptors.emplace_back( "account_permlink_hash", ::rocksdb::ColumnFamilyOptions() );
    cf_descriptors.back().options.comparator = by_Hash_Comparator();
    BOOST_REQUIRE( ::rocksdb::DB::Open( ::rocksdb::DBOptions(), path.string(), cf_descriptors, &handles, &db ).ok() );
  }

  ~opened_archive_copy()
  {
    for( auto* handle : handles )
      db->DestroyColumnFamilyHandle( handle );
  }

  bool has_comment( const std::string& key )
  {
    // rocksdb_storage_provider::save writes through column handle CommentsColumns::COMMENT (0), the default one
    std::string value;
    return db->Get( ::rocksdb::ReadOptions(), handles[ CommentsColumns::COMMENT ], key, &value ).ok();
  }

  void replay( const bfs::path& trace_file )
  {
    std::unique_ptr< ::rocksdb::TraceReader > reader;
    BOOST_REQUIRE( ::rocksdb::NewFileTraceReader( db->GetEnv(), ::rocksdb::EnvOptions(), trace_file.string(), &reader ).ok() );
    std::unique_ptr< ::rocksdb::Replayer > replayer;
    BOOST_REQUIRE( db->NewDefaultReplayer( handles, std::move( reader ), &replayer ).ok() );
    BOOST_REQUIRE( replayer->Prepare().ok() );
    size_t failed = 0;
    const auto status = replayer->Replay( ::rocksdb::ReplayOptions( 1, 1e9 ),
      [&failed]( ::rocksdb::Status s, std::unique_ptr< ::rocksdb::TraceRecordResult >&& )
      {
        // a Get of a missing comment ends NotFound, as it did when traced
        if( !s.ok() && !s.IsNotFound() )
          ++failed;
      } );
    BOOST_REQUIRE_MESSAGE( status.ok(), status.ToString() );
    BOOST_CHECK_EQUAL( failed, 0u );
  }
};
}

BOOST_AUTO_TEST_SUITE( rocksdb_storage_provider_tests )

BOOST_AUTO_TEST_CASE( wipe_destroys_existing_database )
{
  fc::temp_directory temp_dir( hive::utilities::temp_directory_path() );
  const bfs::path storage_path = temp_dir.path() / "comments-rocksdb-storage";

  {
    ::rocksdb::Options options;
    options.create_if_missing = true;
    std::unique_ptr< ::rocksdb::DB > db;
    BOOST_REQUIRE( ::rocksdb::DB::Open( options, storage_path.string(), &db ).ok() );
  }
  BOOST_REQUIRE( bfs::exists( storage_path / "CURRENT" ) );

  appbase::application app;
  rocksdb_comment_storage_provider provider( temp_dir.path(), storage_path, app );
  provider.wipeDb();

  BOOST_REQUIRE( !bfs::exists( storage_path ) );
}

BOOST_AUTO_TEST_CASE( wipe_of_missing_or_empty_storage_succeeds )
{
  fc::temp_directory temp_dir( hive::utilities::temp_directory_path() );
  const bfs::path storage_path = temp_dir.path() / "comments-rocksdb-storage";

  appbase::application app;
  // The provider creates the (empty) storage directory on construction.
  rocksdb_comment_storage_provider provider( temp_dir.path(), storage_path, app );
  BOOST_REQUIRE( bfs::is_directory( storage_path ) );
  BOOST_REQUIRE_NO_THROW( provider.wipeDb() );

  BOOST_REQUIRE( !bfs::exists( storage_path ) );
  BOOST_REQUIRE_NO_THROW( provider.wipeDb() );
}

BOOST_AUTO_TEST_CASE( failed_wipe_raises_and_keeps_database )
{
  fc::temp_directory temp_dir( hive::utilities::temp_directory_path() );
  const bfs::path storage_path = temp_dir.path() / "comments-rocksdb-storage";

  // A database held open by this process keeps its LOCK file locked, so DestroyDB fails
  // regardless of filesystem permissions (also when running as root).
  ::rocksdb::Options options;
  options.create_if_missing = true;
  std::unique_ptr< ::rocksdb::DB > open_db;
  BOOST_REQUIRE( ::rocksdb::DB::Open( options, storage_path.string(), &open_db ).ok() );

  appbase::application app;
  rocksdb_comment_storage_provider provider( temp_dir.path(), storage_path, app );

  try
  {
    provider.wipeDb();
    BOOST_FAIL( "wipeDb() should have failed on a locked database" );
  }
  catch( const plugin_exception& e )
  {
    const std::string message = e.to_detail_string();
    BOOST_TEST_MESSAGE( message );
    BOOST_REQUIRE( message.find( storage_path.string() ) != std::string::npos );
    BOOST_REQUIRE( message.find( "lock" ) != std::string::npos );
  }

  BOOST_REQUIRE( bfs::exists( storage_path / "CURRENT" ) );

  open_db.reset();
  BOOST_REQUIRE_NO_THROW( provider.wipeDb() );
  BOOST_REQUIRE( !bfs::exists( storage_path ) );
}

BOOST_AUTO_TEST_CASE( opened_archive_has_tuned_column_families )
{
  // The shipped block cache keeps full filters and a binary search index.
  check_opened_archive_options( rocksdb_comment_storage_provider::DEFAULT_BLOCK_CACHE_SIZE, false );
}

BOOST_AUTO_TEST_CASE( opened_archive_partitions_index_and_filters_below_cache_limit )
{
  check_opened_archive_options( rocksdb_comment_storage_provider::PARTITIONED_METADATA_CACHE_LIMIT - 1, true );
  check_opened_archive_options( 48 * 1024 * 1024, true );
  check_opened_archive_options( rocksdb_comment_storage_provider::PARTITIONED_METADATA_CACHE_LIMIT, false );
}

BOOST_AUTO_TEST_CASE( archive_written_with_untuned_options_stays_readable )
{
  // As the code before table options and column family tuning: SST files without filter blocks.
  check_archive_written_by_older_build_stays_readable( older_untuned_layout() );
}

BOOST_AUTO_TEST_CASE( archive_written_with_full_filters_stays_readable )
{
  // As the code before partitioned index/filters, and from the cache limit up: a binary search
  // index and one full bloom filter block per SST file.
  check_archive_written_by_older_build_stays_readable( older_table_layout( false ) );
  check_archive_written_by_older_build_stays_readable( older_table_layout( false ), 48 * 1024 * 1024 );
}

BOOST_AUTO_TEST_CASE( archive_written_with_partitioned_index_and_filters_stays_readable )
{
  // As hive#1026 wrote it at every cache size, and below the cache limit since.
  check_archive_written_by_older_build_stays_readable( older_table_layout( true ) );
  check_archive_written_by_older_build_stays_readable( older_table_layout( true ), 48 * 1024 * 1024 );
}

BOOST_AUTO_TEST_CASE( written_tables_use_current_on_disk_format )
{
  check_written_tables( rocksdb_comment_storage_provider::DEFAULT_BLOCK_CACHE_SIZE, false );
  check_written_tables( 48 * 1024 * 1024, true );
}

BOOST_AUTO_TEST_CASE( trace_covers_block_range_and_replays_on_its_checkpoint )
{
  fc::temp_directory temp_dir( hive::utilities::temp_directory_path() );
  const bfs::path storage_path = temp_dir.path() / "comments-rocksdb-storage";
  const bfs::path trace_output = temp_dir.path() / "traces" / "COMMENT";

  rocksdb_trace_config trace_config;
  trace_config.dir = temp_dir.path() / "traces";
  trace_config.first_block = 5;
  trace_config.last_block = 10;

  appbase::application app;
  rocksdb_comment_storage_provider provider( temp_dir.path(), storage_path, app );
  provider.enableTrace( trace_config );
  provider.openDb( 0 );

  archive_blocks( provider, 1, 4 );
  BOOST_REQUIRE( !bfs::exists( trace_output ) );

  archive_blocks( provider, 5, 7 );
  BOOST_REQUIRE( bfs::is_directory( trace_output / "checkpoint" ) );
  BOOST_REQUIRE( bfs::exists( trace_output / "trace" ) );
  BOOST_CHECK( !read_trace_info( trace_output )[ "ended" ].as_bool() );

  archive_blocks( provider, 8, 12 );
  const auto info = read_trace_info( trace_output );
  BOOST_CHECK( info[ "ended" ].as_bool() );
  BOOST_CHECK_EQUAL( info[ "database" ].as_string(), "COMMENT" );
  BOOST_CHECK_EQUAL( info[ "start_block" ].as_uint64(), 5u );
  BOOST_CHECK_EQUAL( info[ "end_block" ].as_uint64(), 10u );
  BOOST_CHECK_EQUAL( info[ "end_reason" ].as_string(), "end of range" );
  BOOST_CHECK_GT( bfs::file_size( trace_output / "trace" ), 0u );

  provider.shutdownDb();

  // The checkpoint is the archive before block 5 became irreversible; replaying the trace on it
  // adds exactly what was written while tracing (blocks 5-9; block 10 came after the trace ended).
  opened_archive_copy archive( trace_output / "checkpoint" );
  BOOST_CHECK( archive.has_comment( block_comment_key( 4 ) ) );
  for( uint32_t block_num = 5; block_num <= 10; ++block_num )
    BOOST_CHECK( !archive.has_comment( block_comment_key( block_num ) ) );

  archive.replay( trace_output / "trace" );

  for( uint32_t block_num = 5; block_num <= 9; ++block_num )
    BOOST_CHECK_MESSAGE( archive.has_comment( block_comment_key( block_num ) ), "comment of block " << block_num << " not replayed" );
  BOOST_CHECK( !archive.has_comment( block_comment_key( 10 ) ) );
}

BOOST_AUTO_TEST_CASE( trace_ends_cleanly_when_database_closes_inside_range )
{
  fc::temp_directory temp_dir( hive::utilities::temp_directory_path() );
  const bfs::path storage_path = temp_dir.path() / "comments-rocksdb-storage";
  const bfs::path trace_output = temp_dir.path() / "traces" / "COMMENT";

  rocksdb_trace_config trace_config;
  trace_config.dir = temp_dir.path() / "traces";
  trace_config.first_block = 3;
  trace_config.last_block = 1000;

  appbase::application app;
  rocksdb_comment_storage_provider provider( temp_dir.path(), storage_path, app );
  provider.enableTrace( trace_config );
  provider.openDb( 0 );
  archive_blocks( provider, 1, 6 );
  provider.shutdownDb();

  const auto info = read_trace_info( trace_output );
  BOOST_CHECK( info[ "ended" ].as_bool() );
  BOOST_CHECK_EQUAL( info[ "start_block" ].as_uint64(), 3u );
  BOOST_CHECK_EQUAL( info[ "end_block" ].as_uint64(), 6u );
  BOOST_CHECK_EQUAL( info[ "end_reason" ].as_string(), "database closed" );

  opened_archive_copy archive( trace_output / "checkpoint" );
  archive.replay( trace_output / "trace" );
  for( uint32_t block_num = 3; block_num <= 6; ++block_num )
    BOOST_CHECK_MESSAGE( archive.has_comment( block_comment_key( block_num ) ), "comment of block " << block_num << " not replayed" );
}

BOOST_AUTO_TEST_CASE( trace_refuses_existing_output_and_traces_nothing_past_its_range )
{
  fc::temp_directory temp_dir( hive::utilities::temp_directory_path() );
  const bfs::path storage_path = temp_dir.path() / "comments-rocksdb-storage";

  rocksdb_trace_config trace_config;
  trace_config.dir = temp_dir.path() / "traces";
  trace_config.first_block = 2;
  trace_config.last_block = 4;

  appbase::application app;
  {
    rocksdb_comment_storage_provider provider( temp_dir.path(), storage_path, app );
    provider.enableTrace( trace_config );
    provider.openDb( 0 );
    // The node resumes past the range: the first LIB it reports is already beyond it.
    provider.update_lib( 7 );
    provider.flushWriteBuffer();
    provider.shutdownDb();
  }
  BOOST_CHECK( !bfs::exists( trace_config.dir / "COMMENT" ) );

  bfs::create_directories( trace_config.dir / "COMMENT" );
  rocksdb_comment_storage_provider provider( temp_dir.path(), storage_path, app );
  BOOST_CHECK_THROW( provider.enableTrace( trace_config ), fc::exception );
}

BOOST_AUTO_TEST_SUITE_END()
