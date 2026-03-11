
#include <hive/chain/external_storage/rocksdb_account_storage_provider.hpp>
#include <hive/chain/external_storage/account_rocksdb_objects.hpp>
namespace hive { namespace chain {

rocksdb_account_storage_provider::rocksdb_account_storage_provider( const bfs::path& blockchain_storage_path, const bfs::path& storage_path, appbase::application& app )
  : rocksdb_base_storage_provider( blockchain_storage_path, storage_path, app, "ACCOUNT" )
{

}

rocksdb_storage_provider::ColumnDefinitions rocksdb_account_storage_provider::prepareColumnDefinitions( bool addDefaultColumn )
{
  ColumnDefinitions columnDefs;

  if(addDefaultColumn)
    columnDefs.emplace_back("default", ColumnFamilyOptions());

  // R-ARCH-3: Tune write buffers to absorb write bursts during heavy archival (blocks 55M-70M).
  auto apply_data_column_options = []( ColumnFamilyOptions& opts )
  {
    // R-ARCH-3: Larger write buffers reduce memtable→SST flush frequency,
    // preventing L0 file accumulation and write stalls during archival surges.
    opts.write_buffer_size = 128 * 1024 * 1024;        // 128MB (up from 64MB default)
    opts.max_write_buffer_number = 4;                   // more room before stalls
    opts.min_write_buffer_number_to_merge = 2;          // merge memtables before flushing
    opts.target_file_size_base = 128 * 1024 * 1024;     // match write_buffer_size
  };

  {
    columnDefs.emplace_back("account_metadata", ColumnFamilyOptions());
    auto& byTxIdColumn = columnDefs.back();
    byTxIdColumn.options.comparator = by_account_name_Comparator();
    apply_data_column_options( byTxIdColumn.options );
  }

  {
    columnDefs.emplace_back("account_authority", ColumnFamilyOptions());
    auto& byTxIdColumn = columnDefs.back();
    byTxIdColumn.options.comparator = by_account_name_Comparator();
    apply_data_column_options( byTxIdColumn.options );
  }

  {
    columnDefs.emplace_back("account", ColumnFamilyOptions());
    auto& byTxIdColumn = columnDefs.back();
    byTxIdColumn.options.comparator = by_account_name_Comparator();
    apply_data_column_options( byTxIdColumn.options );
  }

  {
    columnDefs.emplace_back("account_by_id", ColumnFamilyOptions());
    auto& byTxIdColumn = columnDefs.back();
    byTxIdColumn.options.comparator = by_id_Comparator();
    apply_data_column_options( byTxIdColumn.options );
  }

  // Split object columns (recovery + delayed_votes merged into assets)
  {
    columnDefs.emplace_back("assets", ColumnFamilyOptions());
    auto& column = columnDefs.back();
    column.options.comparator = by_id_Comparator();
    apply_data_column_options( column.options );
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

void rocksdb_account_storage_provider::put_entity( ColumnTypes column_type, const Slice& key, const WideColumns& wide_columns )
{
  rocksdb_storage_provider::put_entity( column_type, key, wide_columns );
}

bool rocksdb_account_storage_provider::get_entity( ColumnTypes column_type, const Slice& key, PinnableWideColumns& wide_columns )
{
  return rocksdb_storage_provider::get_entity( column_type, key, wide_columns );
}

void rocksdb_account_storage_provider::compaction()
{
  getStorage()->CompactRange(rocksdb::CompactRangeOptions(), nullptr, nullptr );
}

}}