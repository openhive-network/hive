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

class external_snapshot_storage_provider
{
  public:

    using ptr = std::shared_ptr<external_snapshot_storage_provider>;

    virtual std::unique_ptr<DB>& getStorage() = 0;

    virtual void openDb( bool cleanDatabase ) = 0;
    virtual void shutdownDb( bool removeDB = false ) = 0;
    virtual void wipeDb() = 0;
};


class external_storage_reader_writer
{
  public:

    using ptr = std::shared_ptr<external_storage_reader_writer>;

    virtual void save( ColumnTypes column_type, const Slice& key, const Slice& value ) = 0;
    virtual bool read( ColumnTypes column_type, const Slice& key, PinnableSlice& value ) = 0;
};

class external_storage_provider: public external_storage_reader_writer, public external_snapshot_storage_provider
{
  public:

    virtual void flush() = 0;

    virtual void update_lib( uint32_t ) = 0;
    virtual void update_reindex_point( uint32_t ) = 0;
};

}}
