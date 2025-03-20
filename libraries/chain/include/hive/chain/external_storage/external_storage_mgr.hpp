#pragma once

#include<hive/chain/database.hpp>

#include<memory>

#include <rocksdb/slice.h>

namespace hive { namespace chain {

using ::rocksdb::Slice;

class external_storage_mgr
{
  public:

    using ptr = std::shared_ptr<external_storage_mgr>;

    virtual void save( const Slice& key, const Slice& value, const uint32_t& column_number ) = 0;
    virtual void read( const Slice& key, std::string& value, const uint32_t& column_number ) = 0;
};

}}
