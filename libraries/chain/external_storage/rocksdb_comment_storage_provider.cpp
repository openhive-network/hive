
#include <hive/chain/external_storage/rocksdb_comment_storage_provider.hpp>

namespace hive { namespace chain {

rocksdb_comment_storage_provider::rocksdb_comment_storage_provider( const bfs::path& blockchain_storage_path, const bfs::path& storage_path, appbase::application& app )
  : rocksdb_storage_provider( blockchain_storage_path, storage_path, app )
{

}

rocksdb_storage_provider::ColumnDefinitions rocksdb_comment_storage_provider::prepareColumnDefinitions( bool addDefaultColumn )
{
  ColumnDefinitions columnDefs;

  if(addDefaultColumn)
    columnDefs.emplace_back("default", ColumnFamilyOptions());

  columnDefs.emplace_back("account_permlink_hash", ColumnFamilyOptions());
  auto& byTxIdColumn = columnDefs.back();
  byTxIdColumn.options.comparator = by_Hash_Comparator();

  byTxIdColumn.options.bottommost_compression = rocksdb::CompressionType::kSnappyCompression;

  return columnDefs;
}

WriteBatch& rocksdb_comment_storage_provider::getWriteBuffer()
{
  return _writeBuffer;
}

std::unique_ptr<DB>& rocksdb_comment_storage_provider::getStorage()
{
  return rocksdb_storage_provider::getStorage();
}

void rocksdb_comment_storage_provider::openDb( bool cleanDatabase )
{
  rocksdb_storage_provider::openDb( cleanDatabase );
}

void rocksdb_comment_storage_provider::shutdownDb( bool removeDB )
{
  rocksdb_storage_provider::shutdownDb( removeDB );
}

void rocksdb_comment_storage_provider::wipeDb()
{
  rocksdb_storage_provider::wipeDb();
}

void rocksdb_comment_storage_provider::save( const Slice& key, const Slice& value )
{
  rocksdb_storage_provider::save( key, value );
}

bool rocksdb_comment_storage_provider::read( const Slice& key, PinnableSlice& value )
{
  return rocksdb_storage_provider::read( key, value );
}

void rocksdb_comment_storage_provider::flush()
{
  rocksdb_storage_provider::flush();
}

void rocksdb_comment_storage_provider::update_lib( uint32_t lib )
{
  try
  {
    rocksdb_storage_provider::update_lib( lib );
  }
  catch(...)
  {
    try
    {
      std::rethrow_exception( std::current_exception() );
    }
    catch( const std::exception& e )
    {
      elog("exception: ${what}", ("what", e.what()));
    }
  }
}

void rocksdb_comment_storage_provider::update_reindex_point( uint32_t rp )
{
  rocksdb_storage_provider::update_reindex_point( rp );
}

}}
