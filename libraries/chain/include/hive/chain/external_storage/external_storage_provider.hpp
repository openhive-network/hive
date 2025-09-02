#pragma once

#include <hive/chain/external_storage/types.hpp>

#include<hive/chain/database.hpp>

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

    virtual void openDb( uint32_t expected_lib ) = 0;
    virtual void shutdownDb() = 0;
    virtual void wipeDb() = 0;
};

class external_comment_storage_provider: public external_snapshot_storage_provider
{
  public:

    using ptr = std::shared_ptr<external_comment_storage_provider>;

    virtual void save( const Slice& key, const Slice& value ) = 0;
    virtual bool read( const Slice& key, PinnableSlice& value ) = 0;
    virtual void flush() = 0;

    virtual void update_lib( uint32_t ) = 0;
};

}}
