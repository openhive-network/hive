
#include <hive/chain/external_storage/rocksdb_account_storage_provider.hpp>
#include <hive/chain/external_storage/account_rocksdb_objects.hpp>
namespace hive { namespace chain {

rocksdb_account_storage_provider::rocksdb_account_storage_provider( const bfs::path& blockchain_storage_path, const bfs::path& storage_path, appbase::application& app )
  : rocksdb_base_storage_provider( blockchain_storage_path, storage_path, app )
{

}

rocksdb_storage_provider::ColumnDefinitions rocksdb_account_storage_provider::prepareColumnDefinitions( bool addDefaultColumn )
{
  ColumnDefinitions columnDefs;

  if(addDefaultColumn)
    columnDefs.emplace_back("default", ColumnFamilyOptions());

  {
    columnDefs.emplace_back("account_metadata", ColumnFamilyOptions());
    auto& byTxIdColumn = columnDefs.back();
    byTxIdColumn.options.comparator = by_account_name_Comparator();
  }

  {
    columnDefs.emplace_back("account_authority", ColumnFamilyOptions());
    auto& byTxIdColumn = columnDefs.back();
    byTxIdColumn.options.comparator = by_account_name_Comparator();
  }

  {
    columnDefs.emplace_back("account", ColumnFamilyOptions());
    auto& byTxIdColumn = columnDefs.back();
    byTxIdColumn.options.comparator = by_account_name_Comparator();
  }

  {
    columnDefs.emplace_back("account_by_id", ColumnFamilyOptions());
    auto& byTxIdColumn = columnDefs.back();
    byTxIdColumn.options.comparator = by_id_Comparator();
  }

  return columnDefs;
}

void rocksdb_account_storage_provider::save( ColumnTypes column_type, const Slice& key, const Slice& value )
{
  rocksdb_storage_provider::save( column_type, key, value );
}

bool rocksdb_account_storage_provider::read( ColumnTypes column_type, const Slice& key, PinnableSlice& value )
{
  return rocksdb_storage_provider::read( column_type, key, value );
}

void rocksdb_account_storage_provider::remove( ColumnTypes column_type, const Slice& key )
{
  rocksdb_storage_provider::remove( column_type, key );
}

void rocksdb_account_storage_provider::compaction()
{
  getStorage()->CompactRange(rocksdb::CompactRangeOptions(), nullptr, nullptr );
}

}}