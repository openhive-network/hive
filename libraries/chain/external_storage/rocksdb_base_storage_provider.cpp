
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

void rocksdb_base_storage_provider::openDb( uint32_t expected_lib )
{
  rocksdb_storage_provider::openDb( expected_lib );
}

void rocksdb_base_storage_provider::shutdownDb()
{
  rocksdb_storage_provider::shutdownDb();
}

void rocksdb_base_storage_provider::wipeDb()
{
  rocksdb_storage_provider::wipeDb();
}

void rocksdb_base_storage_provider::flushDb()
{
  rocksdb_storage_provider::flushDb();
}

void rocksdb_base_storage_provider::update_lib( uint32_t lib )
{
  rocksdb_storage_provider::update_lib( lib );
}

uint32_t rocksdb_base_storage_provider::get_lib() const
{
  return rocksdb_storage_provider::get_lib();
}

}}
