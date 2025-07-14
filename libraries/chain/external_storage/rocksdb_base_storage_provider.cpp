
#include <hive/chain/external_storage/rocksdb_base_storage_provider.hpp>

namespace hive { namespace chain {

rocksdb_base_storage_provider::rocksdb_base_storage_provider( const bfs::path& blockchain_storage_path, const bfs::path& storage_path, appbase::application& app )
  : rocksdb_storage_provider( blockchain_storage_path, storage_path, app )
{

}

WriteBatch& rocksdb_base_storage_provider::getWriteBuffer()
{
  return _writeBuffer;
}

std::unique_ptr<DB>& rocksdb_base_storage_provider::getStorage()
{
  return rocksdb_storage_provider::getStorage();
}

void rocksdb_base_storage_provider::openDb( bool cleanDatabase )
{
  rocksdb_storage_provider::openDb( cleanDatabase );
}

void rocksdb_base_storage_provider::shutdownDb( bool removeDB )
{
  rocksdb_storage_provider::shutdownDb( removeDB );
}

void rocksdb_base_storage_provider::wipeDb()
{
  rocksdb_storage_provider::wipeDb();
}

void rocksdb_base_storage_provider::save( const Slice& key, const Slice& value )
{
  rocksdb_storage_provider::save( key, value );
}

bool rocksdb_base_storage_provider::read( const Slice& key, PinnableSlice& value )
{
  return rocksdb_storage_provider::read( key, value );
}

void rocksdb_base_storage_provider::flush()
{
  rocksdb_storage_provider::flush();
}

void rocksdb_base_storage_provider::update_lib( uint32_t lib )
{
  rocksdb_storage_provider::update_lib( lib );
}

void rocksdb_base_storage_provider::update_reindex_point( uint32_t rp )
{
  rocksdb_storage_provider::update_reindex_point( rp );
}

}}
