#pragma once

#include <hive/chain/external_storage/external_storage_provider.hpp>
#include <hive/chain/external_storage/rocksdb_storage_provider.hpp>

#include <rocksdb/options.h>
#include <rocksdb/slice.h>

#include <boost/core/demangle.hpp>
#include <boost/filesystem/path.hpp>

#include <fc/crypto/ripemd160.hpp>
#include <fc/filesystem.hpp>

namespace hive { namespace chain {

class rocksdb_base_storage_provider: public rocksdb_storage_provider, public external_comment_storage_provider
{
  private:

    WriteBatch _writeBuffer;

    WriteBatch& getWriteBuffer() override;

  protected:

    void loadSeqIdentifiers(DB* storageDb) override{};

  public:

    rocksdb_base_storage_provider( const bfs::path& blockchain_storage_path, const bfs::path& storage_path, appbase::application& app );
    ~rocksdb_base_storage_provider() override{}

    std::unique_ptr<DB>& getStorage() override;

    void openDb( bool cleanDatabase ) override;
    void shutdownDb( bool removeDB = false ) override;
    void wipeDb() override;

    void flush() override;

    void update_lib( uint32_t ) override;
    void update_reindex_point( uint32_t ) override;
};

}}

