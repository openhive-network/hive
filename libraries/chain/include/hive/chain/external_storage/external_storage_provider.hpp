#pragma once

#include<hive/chain/database.hpp>

#include<memory>

#include <rocksdb/db.h>
#include <rocksdb/slice.h>

namespace hive { namespace chain {

using ::rocksdb::DB;
using ::rocksdb::Slice;
using ::rocksdb::PinnableSlice;

class external_storage_provider
{
  public:

    using ptr = std::shared_ptr<external_storage_provider>;

    virtual std::unique_ptr<DB>& get_storage() = 0;

    virtual void save( const Slice& key, const Slice& value ) = 0;
    virtual bool read( const Slice& key, PinnableSlice& value ) = 0;
    virtual void flush() = 0;
};

}}
