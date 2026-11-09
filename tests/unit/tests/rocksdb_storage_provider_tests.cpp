
#include <boost/test/unit_test.hpp>

#include <hive/chain/database_exceptions.hpp>
#include <hive/chain/external_storage/rocksdb_comment_storage_provider.hpp>
#include <hive/chain/external_storage/rocksdb_format.hpp>
#include <hive/chain/external_storage/types.hpp>

#include <hive/utilities/tempdir.hpp>

#include <appbase/application.hpp>

#include <fc/filesystem.hpp>

#include <rocksdb/advanced_cache.h>
#include <rocksdb/db.h>
#include <rocksdb/filter_policy.h>
#include <rocksdb/table.h>
#include <rocksdb/table_properties.h>

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

std::string comment_key( char seed )
{
  return std::string( 20, seed );
}

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
  fc::temp_directory temp_dir( hive::utilities::temp_directory_path() );
  const bfs::path storage_path = temp_dir.path() / "comments-rocksdb-storage";
  const size_t block_cache_size = 48 * 1024 * 1024;

  appbase::application app;
  inspectable_comment_storage_provider provider( temp_dir.path(), storage_path, app, block_cache_size );
  provider.openDb( 0 );

  const auto& handles = provider.getColumnHandles();
  BOOST_REQUIRE_EQUAL( handles.size(), 2u );

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

    BOOST_REQUIRE( options.table_factory );
    const auto* table_options = options.table_factory->GetOptions< ::rocksdb::BlockBasedTableOptions >();
    BOOST_REQUIRE( table_options != nullptr );
    BOOST_REQUIRE( table_options->filter_policy );
    BOOST_CHECK_EQUAL( std::string( table_options->filter_policy->CompatibilityName() ), "rocksdb.BuiltinBloomFilter" );
    BOOST_CHECK( table_options->cache_index_and_filter_blocks );
    BOOST_CHECK( table_options->pin_l0_filter_and_index_blocks_in_cache );

    BOOST_REQUIRE( table_options->block_cache );
    BOOST_CHECK_EQUAL( table_options->block_cache->GetCapacity(), block_cache_size );
    if( !shared_cache )
      shared_cache = table_options->block_cache;
    BOOST_CHECK( table_options->block_cache == shared_cache );
  }

  provider.shutdownDb();
}

BOOST_AUTO_TEST_CASE( archive_written_with_untuned_options_stays_readable )
{
  fc::temp_directory temp_dir( hive::utilities::temp_directory_path() );
  const bfs::path storage_path = temp_dir.path() / "comments-rocksdb-storage";

  std::map< std::string, std::string > comments;
  for( char seed = 'a'; seed <= 'z'; ++seed )
    comments[ comment_key( seed ) ] = std::string( "comment-" ) + seed;

  // Lay out the archive as the code before table options and column family tuning did: default
  // ColumnFamilyOptions everywhere, so the SST files carry no filter blocks.
  {
    ::rocksdb::Options options;
    options.create_if_missing = true;
    std::unique_ptr< ::rocksdb::DB > db;
    BOOST_REQUIRE( ::rocksdb::DB::Open( options, storage_path.string(), &db ).ok() );

    ::rocksdb::ColumnFamilyOptions hash_options;
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

    BOOST_REQUIRE( db->DestroyColumnFamilyHandle( hash_column ).ok() );
    BOOST_REQUIRE( db->Close().ok() );
  }

  appbase::application app;
  rocksdb_comment_storage_provider provider( temp_dir.path(), storage_path, app );
  provider.openDb( 0 );

  for( const auto& comment : comments )
  {
    PinnableSlice value;
    BOOST_REQUIRE( provider.read( comment.first, value ) );
    BOOST_CHECK_EQUAL( value.ToString(), comment.second );
  }
  PinnableSlice missing;
  BOOST_CHECK( !provider.read( comment_key( '0' ), missing ) );

  provider.shutdownDb();
}

BOOST_AUTO_TEST_CASE( written_tables_use_pinned_on_disk_format )
{
  fc::temp_directory temp_dir( hive::utilities::temp_directory_path() );
  const bfs::path storage_path = temp_dir.path() / "comments-rocksdb-storage";

  {
    appbase::application app;
    rocksdb_comment_storage_provider provider( temp_dir.path(), storage_path, app );
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

  for( auto* cf_handle : cf_handles )
  {
    ::rocksdb::TablePropertiesCollection tables;
    BOOST_REQUIRE( db->GetPropertiesOfAllTables( cf_handle, &tables ).ok() );
    BOOST_REQUIRE_MESSAGE( !tables.empty(), "no SST file in column family " << cf_handle->GetName() );
    for( const auto& [ file, properties ] : tables )
    {
      BOOST_CHECK_MESSAGE( properties->format_version == ROCKSDB_PINNED_FORMAT_VERSION,
        file << " has format_version " << properties->format_version );
      BOOST_CHECK_MESSAGE( properties->compression_name == "Snappy",
        file << " has compression " << properties->compression_name );
    }
  }

  for( auto* cf_handle : cf_handles )
    BOOST_REQUIRE( db->DestroyColumnFamilyHandle( cf_handle ).ok() );
}

BOOST_AUTO_TEST_SUITE_END()
