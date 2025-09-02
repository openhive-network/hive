#pragma once

#include <hive/chain/external_storage/external_storage_provider.hpp>
#include <hive/chain/external_storage/rocksdb_storage_provider.hpp>

#include <hive/chain/database.hpp>

#include <rocksdb/options.h>
#include <rocksdb/slice.h>

#include <boost/core/demangle.hpp>
#include <boost/filesystem/path.hpp>

#include <fc/crypto/ripemd160.hpp>
#include <fc/filesystem.hpp>

namespace hive { namespace chain {

class rocksdb_comment_storage_provider: public rocksdb_storage_provider, public external_comment_storage_provider
{
  private:

    WriteBatch _writeBuffer;

    ColumnDefinitions prepareColumnDefinitions(bool addDefaultColumn) override;

    WriteBatch& getWriteBuffer() override;

  protected:

    void loadSeqIdentifiers(DB* storageDb) override{};

  public:

    using ptr = std::shared_ptr<rocksdb_comment_storage_provider>;

    rocksdb_comment_storage_provider( const bfs::path& blockchain_storage_path, const bfs::path& storage_path, appbase::application& app );
    ~rocksdb_comment_storage_provider() override{}

    std::unique_ptr<DB>& getStorage() override;

    void openDb( uint32_t expected_lib ) override;
    void shutdownDb() override;
    void wipeDb() override;

    void save( const Slice& key, const Slice& value ) override;
    bool read( const Slice& key, PinnableSlice& value ) override;
    void flush() override;

    void update_lib( uint32_t ) override;
};

}}

