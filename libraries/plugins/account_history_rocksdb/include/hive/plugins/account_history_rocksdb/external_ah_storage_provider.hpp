#pragma once

#include <hive/chain/external_storage/types.hpp>
#include <hive/chain/external_storage/external_storage_provider.hpp>

#include<hive/chain/database.hpp>

#include<memory>

#include <rocksdb/db.h>
#include <rocksdb/slice.h>

namespace hive { namespace chain {

using ::rocksdb::DB;
using ::rocksdb::Slice;
using ::rocksdb::PinnableSlice;
using ::rocksdb::ColumnFamilyHandle;

class external_ah_storage_provider: public external_snapshot_storage_provider
{
  public:

    using ptr = std::shared_ptr<external_ah_storage_provider>;

    virtual uint64_t get_operationSeqId() const = 0;
    virtual void set_operationSeqId( uint64_t value ) = 0;

    virtual uint64_t get_accountHistorySeqId() const = 0;
    virtual void set_accountHistorySeqId( uint64_t value ) = 0;

    virtual unsigned int get_collectedOps() const = 0;
    virtual void set_collectedOps( unsigned int value ) = 0;

    virtual void flushStorage() = 0;
    virtual void flushWriteBuffer(DB* storage = nullptr) = 0;

    virtual ColumnFamilyHandle* getColumnHandle( Columns column ) = 0;

    virtual CachableWriteBatch& getCachableWriteBuffer() = 0;
};

}}
