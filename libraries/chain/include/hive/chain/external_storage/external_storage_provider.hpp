#pragma once

#include<hive/chain/database.hpp>

#include<memory>

#include <rocksdb/slice.h>

namespace hive { namespace chain {

using ::rocksdb::Slice;
using ::rocksdb::PinnableSlice;

class external_storage_provider
{
  public:

    using ptr = std::shared_ptr<external_storage_provider>;

    virtual void save( const Slice& key, const Slice& value, const uint32_t& column_number ) = 0;
    virtual bool read( const Slice& key, PinnableSlice& value, const uint32_t& column_number ) = 0;
    virtual void remove( const Slice& key, const uint32_t& column_number ) = 0;
    virtual void flush() = 0;
};

}}
