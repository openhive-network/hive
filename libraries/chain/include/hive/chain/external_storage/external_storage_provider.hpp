#pragma once

#include <hive/chain/external_storage/types.hpp>

#include<memory>

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
    virtual void remove( ColumnTypes column_type, const Slice& key ) = 0;

    virtual void put_entity( ColumnTypes column_type, const Slice& key, const WideColumns& wide_columns ) = 0;
    virtual bool get_entity( ColumnTypes column_type, const Slice& key, PinnableWideColumns& wide_columns ) = 0;

    virtual void compaction() = 0;
};

}}
