
#include <hive/chain/external_storage/rocksdb_account_storage_provider.hpp>
#include <hive/chain/external_storage/account_rocksdb_objects.hpp>
#include <hive/chain/external_storage/rocksdb_column_family_iterator.hpp>
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
    byTxIdColumn.options.comparator = by_Hash_Comparator();
  }

  {
    columnDefs.emplace_back("account_authority", ColumnFamilyOptions());
    auto& byTxIdColumn = columnDefs.back();
    byTxIdColumn.options.comparator = by_Hash_Comparator();
  }

  {
    columnDefs.emplace_back("account", ColumnFamilyOptions());
    auto& byTxIdColumn = columnDefs.back();
    byTxIdColumn.options.comparator = by_Hash_Comparator();
  }

  {
    columnDefs.emplace_back("account_by_id", ColumnFamilyOptions());
    auto& byTxIdColumn = columnDefs.back();
    byTxIdColumn.options.comparator = by_id_Comparator();
  }

  {
    columnDefs.emplace_back("account_by_next_vesting_withdrawal", ColumnFamilyOptions());
    auto& byTxIdColumn = columnDefs.back();
    byTxIdColumn.options.comparator = by_time_account_name_pair_Comparator();
  }

  {
    columnDefs.emplace_back("account_by_delayed_voting", ColumnFamilyOptions());
    auto& byTxIdColumn = columnDefs.back();
    byTxIdColumn.options.comparator = by_time_account_id_pair_Comparator();
  }

  {
    columnDefs.emplace_back("account_by_governance_vote_expiration_ts", ColumnFamilyOptions());
    auto& byTxIdColumn = columnDefs.back();
    byTxIdColumn.options.comparator = by_time_account_id_pair_Comparator();
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

std::unique_ptr<::rocksdb::Iterator> rocksdb_account_storage_provider::create_column_family_iterator( ColumnTypes column_type )
{
  auto _column_handle = getColumnHandle( column_type );
  FC_ASSERT( _column_handle );

  rocksdb::ReadOptions read_options;
  return std::unique_ptr<::rocksdb::Iterator>( getStorage()->NewIterator( read_options, _column_handle ) );
}

}}
