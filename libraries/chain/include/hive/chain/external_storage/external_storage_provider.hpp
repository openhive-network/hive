#pragma once

#include <hive/chain/external_storage/types.hpp>

#include <memory>
#include <vector>

#include <rocksdb/db.h>
#include <rocksdb/slice.h>

namespace hive { namespace chain {

using ::rocksdb::DB;
using ::rocksdb::Slice;
using ::rocksdb::PinnableSlice;
using ::rocksdb::ColumnFamilyHandle;
using ::rocksdb::WideColumns;
using ::rocksdb::PinnableWideColumns;


class external_storage_provider
{
  public:

    using ptr = std::shared_ptr<external_storage_provider>;

    virtual void save( ColumnTypes column_type, const Slice& key, const Slice& value ) = 0;
    virtual bool read( ColumnTypes column_type, const Slice& key, PinnableSlice& value ) = 0;
    /// Batch read multiple keys from different columns in a single call.
    /// Returns a vector of bools indicating which reads succeeded.
    /// Default implementation falls back to sequential reads.
    virtual std::vector<bool> multi_read( const std::vector<ColumnTypes>& column_types,
                                          const std::vector<Slice>& keys,
                                          std::vector<std::string>& values )
    {
      const size_t n = column_types.size();
      values.resize( n );
      std::vector<bool> results( n );
      for( size_t i = 0; i < n; ++i )
      {
        PinnableSlice ps;
        results[i] = read( column_types[i], keys[i], ps );
        if( results[i] )
          values[i].assign( ps.data(), ps.size() );
      }
      return results;
    }
    virtual void remove( ColumnTypes column_type, const Slice& key ) = 0;

    virtual void put_entity( ColumnTypes column_type, const Slice& key, const WideColumns& wide_columns ) = 0;
    virtual bool get_entity( ColumnTypes column_type, const Slice& key, PinnableWideColumns& wide_columns ) = 0;

    virtual void compaction() = 0;
};

}}
