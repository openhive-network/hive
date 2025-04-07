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

class external_comment_storage_provider
{
  public:

    using ptr = std::shared_ptr<external_comment_storage_provider>;

    virtual void save( const Slice& key, const Slice& value ) = 0;
    virtual bool read( const Slice& key, PinnableSlice& value ) = 0;
    virtual void flush() = 0;
};

class external_ah_storage_provider
{
  public:

    using ptr = std::shared_ptr<external_ah_storage_provider>;

    virtual std::unique_ptr<DB>& getStorage() = 0;

    virtual void openDb( bool cleanDatabase ) = 0;
    virtual void shutdownDb( bool removeDB = false ) = 0;

    virtual const std::atomic_uint& get_cached_irreversible_block() const = 0;
    virtual unsigned int get_cached_reindex_point() const = 0;

    virtual uint64_t get_operationSeqId() const = 0;
    virtual void set_operationSeqId( uint64_t value ) = 0;

    virtual uint64_t get_accountHistorySeqId() const = 0;
    virtual void set_accountHistorySeqId( uint64_t value ) = 0;

    virtual unsigned int get_collectedOps() const = 0;
    virtual void set_collectedOps( unsigned int value ) = 0;

    virtual void update_lib( uint32_t ) = 0;
    virtual void update_reindex_point( uint32_t ) = 0;

    virtual void flushStorage() = 0;
    virtual void flushWriteBuffer(DB* storage = nullptr) = 0;

    virtual std::vector<ColumnFamilyHandle*>& getColumnHandles() = 0;

    virtual CachableWriteBatch& getCachableWriteBuffer() = 0;
};

}}
