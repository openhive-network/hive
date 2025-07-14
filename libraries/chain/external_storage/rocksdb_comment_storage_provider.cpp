
#include <hive/chain/external_storage/rocksdb_comment_storage_provider.hpp>

namespace hive { namespace chain {

rocksdb_comment_storage_provider::rocksdb_comment_storage_provider( const bfs::path& blockchain_storage_path, const bfs::path& storage_path, appbase::application& app )
  : rocksdb_base_storage_provider( blockchain_storage_path, storage_path, app )
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

  return columnDefs;
}

void rocksdb_comment_storage_provider::save( ColumnTypes column_type, const Slice& key, const Slice& value )
{
  rocksdb_storage_provider::save( column_type, key, value );
}

bool rocksdb_comment_storage_provider::read( ColumnTypes column_type, const Slice& key, PinnableSlice& value )
{
  return rocksdb_storage_provider::read( column_type, key, value );
}

uint32_t rocksdb_comment_storage_provider::get_lib() const
{
  return rocksdb_storage_provider::get_lib();
}

}}
