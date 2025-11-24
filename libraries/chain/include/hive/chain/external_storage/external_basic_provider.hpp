#pragma once

#include <rocksdb/db.h>

namespace hive { namespace chain {

using ::rocksdb::DB;

class external_basic_provider
{
  public:

    using ptr = std::shared_ptr<external_basic_provider>;

    virtual std::unique_ptr<DB>& getStorage() = 0;

    virtual void openDb( uint32_t expected_lib ) = 0;
    virtual void shutdownDb() = 0;
    virtual void flushDb() = 0;
    virtual void flushWriteBuffer( DB* storageDB = nullptr ) = 0;
    virtual void wipeDb() = 0;

    virtual void update_lib( uint32_t ) = 0;
    virtual uint32_t get_lib() const = 0;
};

}}
