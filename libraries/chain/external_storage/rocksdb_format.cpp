#include <hive/chain/external_storage/rocksdb_format.hpp>

#include <rocksdb/table.h>

namespace hive { namespace chain {

void pin_rocksdb_on_disk_format( ::rocksdb::ColumnFamilyOptions& options )
{
  ::rocksdb::BlockBasedTableOptions table_options;
  if( options.table_factory )
  {
    if( const auto* current = options.table_factory->GetOptions< ::rocksdb::BlockBasedTableOptions >() )
      table_options = *current;
  }
  table_options.format_version = ROCKSDB_PINNED_FORMAT_VERSION;
  options.table_factory.reset( ::rocksdb::NewBlockBasedTableFactory( table_options ) );

  options.compression = ::rocksdb::kSnappyCompression;
  // OptimizeLevelStyleCompaction fills compression_per_level (NoCompression at L0..N-2, LZ4 at
  // the last level), which takes precedence over the base compression field and bypasses the pin.
  // Clear it so kSnappyCompression applies at every level.
  options.compression_per_level.clear();
}

} } // hive::chain
