#pragma once

#include<memory>

#include <rocksdb/slice.h>

namespace hive { namespace chain {

using ::rocksdb::Slice;
using ::rocksdb::PinnableSlice;

class external_comment_storage_provider
{
  public:

    using ptr = std::shared_ptr<external_comment_storage_provider>;

    virtual void save( const Slice& key, const Slice& value ) = 0;
    virtual bool read( const Slice& key, PinnableSlice& value ) = 0;
};

}}
