#pragma once

#include <memory>

namespace rocksdb { class DB; }

namespace hive { namespace chain {

using ::rocksdb::DB;

class external_basic_provider
{
  public:

    using ptr = std::shared_ptr<external_basic_provider>;

    virtual std::unique_ptr<DB>& getStorage() = 0;

    virtual void openDb( uint32_t expected_lib ) = 0;
    virtual void shutdownDb() = 0;
    /// Submit the pending write batch and additionally force every column family out into
    /// immutable files. Only for explicit flush points; see flushStorage.
    virtual void flushDb() = 0;
    /// Submit the pending write batch so it reaches the storage's write-ahead log. Cheap, and
    /// creates no new files. This is the half the per-block path needs (issue #869).
    virtual void flushWriteBuffer() = 0;
    /// Force in-memory tables out into immutable files without touching the write batch.
    virtual void flushStorage() = 0;
    virtual void wipeDb() = 0;
    virtual void finalizeStorage() = 0;

    virtual void update_lib( uint32_t ) = 0;
    virtual uint32_t get_lib() const = 0;
};

}}
