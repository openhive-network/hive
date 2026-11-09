
#include <hive/chain/external_storage/types.hpp>
#include <hive/chain/external_storage/rocksdb_comment_storage_provider.hpp>

#include <rocksdb/filter_policy.h>
#include <rocksdb/table.h>

namespace hive { namespace chain {

rocksdb_comment_storage_provider::rocksdb_comment_storage_provider( const bfs::path& blockchain_storage_path, const bfs::path& storage_path, appbase::application& app, size_t block_cache_size )
  : rocksdb_storage_provider( blockchain_storage_path, storage_path, app, "COMMENT" ),
    _blockCache( ::rocksdb::NewLRUCache( block_cache_size ) )
{

}

ColumnFamilyOptions rocksdb_comment_storage_provider::makeColumnOptions() const
{
  // The archive is read by point lookups of comments that mostly are not there yet (or at all),
  // so a bloom filter lets most reads skip every level. Index and filter blocks live in the
  // shared block cache with high priority (L0 ones pinned) rather than in each table reader:
  // max_open_files is bounded, and blocks held by a table reader are lost when it is evicted
  // from the table cache, while cached ones survive the reopen. Filters are not required to
  // read existing SST files; files written before they were enabled get them on compaction.
  ::rocksdb::BlockBasedTableOptions tableOptions;
  tableOptions.filter_policy.reset( ::rocksdb::NewBloomFilterPolicy( 10 ) );
  tableOptions.block_cache = _blockCache;
  tableOptions.cache_index_and_filter_blocks = true;
  tableOptions.cache_index_and_filter_blocks_with_high_priority = true;
  tableOptions.pin_l0_filter_and_index_blocks_in_cache = true;

  ColumnFamilyOptions options;
  options.table_factory.reset( ::rocksdb::NewBlockBasedTableFactory( tableOptions ) );
  return options;
}

rocksdb_storage_provider::ColumnDefinitions rocksdb_comment_storage_provider::prepareColumnDefinitions( bool addDefaultColumn )
{
  ColumnDefinitions columnDefs;

  if(addDefaultColumn)
    columnDefs.emplace_back("default", makeColumnOptions());

  columnDefs.emplace_back("account_permlink_hash", makeColumnOptions());
  auto& byTxIdColumn = columnDefs.back();
  byTxIdColumn.options.comparator = by_Hash_Comparator();

  return columnDefs;
}

WriteBatch& rocksdb_comment_storage_provider::getWriteBuffer()
{
  return _writeBuffer;
}

void rocksdb_comment_storage_provider::save( const Slice& key, const Slice& value )
{
  rocksdb_storage_provider::save( key, value );
}

bool rocksdb_comment_storage_provider::read( const Slice& key, PinnableSlice& value )
{
  return rocksdb_storage_provider::read( key, value );
}

void rocksdb_comment_storage_provider::multi_read( const std::vector<Slice>& keys, std::vector<PinnableSlice>& values, std::vector<::rocksdb::Status>& statuses )
{
  rocksdb_storage_provider::multi_read( keys, values, statuses );
}

}}
