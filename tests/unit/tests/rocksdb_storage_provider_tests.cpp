
#include <boost/test/unit_test.hpp>

#include <hive/chain/database_exceptions.hpp>
#include <hive/chain/external_storage/rocksdb_comment_storage_provider.hpp>

#include <hive/utilities/tempdir.hpp>

#include <appbase/application.hpp>

#include <fc/filesystem.hpp>

#include <rocksdb/db.h>

#include <memory>

using namespace hive::chain;

BOOST_AUTO_TEST_SUITE( rocksdb_storage_provider_tests )

BOOST_AUTO_TEST_CASE( wipe_destroys_existing_database )
{
  fc::temp_directory temp_dir( hive::utilities::temp_directory_path() );
  const bfs::path storage_path = temp_dir.path() / "comments-rocksdb-storage";

  {
    ::rocksdb::Options options;
    options.create_if_missing = true;
    ::rocksdb::DB* db = nullptr;
    BOOST_REQUIRE( ::rocksdb::DB::Open( options, storage_path.string(), &db ).ok() );
    delete db;
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
  ::rocksdb::DB* raw_db = nullptr;
  BOOST_REQUIRE( ::rocksdb::DB::Open( options, storage_path.string(), &raw_db ).ok() );
  std::unique_ptr< ::rocksdb::DB > open_db( raw_db );

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

BOOST_AUTO_TEST_SUITE_END()
