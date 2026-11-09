#pragma once

#include<memory>

#include <rocksdb/slice.h>
#include <rocksdb/status.h>

#include <vector>

namespace hive { namespace chain {

using ::rocksdb::Slice;
using ::rocksdb::PinnableSlice;

class external_comment_storage_provider
{
  public:

    using ptr = std::shared_ptr<external_comment_storage_provider>;

    virtual void save( const Slice& key, const Slice& value ) = 0;
    virtual bool read( const Slice& key, PinnableSlice& value ) = 0;
    /// Reads all keys in one batch; values[i] holds the value of keys[i] when statuses[i].ok().
    virtual void multi_read( const std::vector<Slice>& keys, std::vector<PinnableSlice>& values, std::vector<::rocksdb::Status>& statuses ) = 0;
};

}}
