#pragma once

#include <hive/chain/external_storage/rocksdb_storage_provider.hpp>
#include <hive/plugins/account_history_rocksdb/external_ah_storage_provider.hpp>

#include <rocksdb/slice.h>

namespace hive { namespace chain {

using ::rocksdb::ColumnFamilyHandle;
using ::rocksdb::WriteBatch;

class rocksdb_ah_storage_provider: public rocksdb_storage_provider, public external_ah_storage_provider
{
  private:

    /// IDs to be assigned to object.id field.
    uint64_t                         _operationSeqId = 0;
    uint64_t                         _accountHistorySeqId = 0;

    unsigned int                     _collectedOps = 0;

    CachableWriteBatch _writeBuffer;

    void storeSequenceIds();

    void loadSeqIdentifiers(DB* storageDb) override;

    WriteBatch& getWriteBuffer() override;

  protected:

    ColumnDefinitions prepareColumnDefinitions(bool addDefaultColumn) override;

    void beforeFlushWriteBuffer() override;
    void afterFlushWriteBuffer() override;

  public:
    using ptr = std::shared_ptr<rocksdb_ah_storage_provider>;

    rocksdb_ah_storage_provider( const bfs::path& blockchain_storage_path, const bfs::path& storage_path, appbase::application& app );
    ~rocksdb_ah_storage_provider() override{}

    std::unique_ptr<DB>& getStorage() override;

    void openDb( uint32_t expected_lib, bool cleanDatabase ) override;
    void shutdownDb( bool removeDB = false ) override;
    void wipeDb() override;

    const std::atomic_uint& get_cached_irreversible_block() const override;

    uint64_t get_operationSeqId() const override;
    void set_operationSeqId( uint64_t value ) override;

    uint64_t get_accountHistorySeqId() const override;
    void set_accountHistorySeqId( uint64_t value ) override;

    /// Number of data-chunks for ops being stored inside _writeBuffer. To decide when to flush.
    unsigned int get_collectedOps() const override;
    void set_collectedOps( unsigned int value ) override;

    //stores new value of last irreversible block in DB and _cached_irreversible_block
    void update_lib( uint32_t ) override;

    void flushStorage() override;

    void flushWriteBuffer(DB* storage = nullptr) override;

    ColumnFamilyHandle* getColumnHandle( Columns column ) override;

    CachableWriteBatch& getCachableWriteBuffer() override;
};


}}

