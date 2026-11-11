#pragma once

#include <hive/chain/external_storage/external_comment_storage_provider.hpp>
#include <hive/chain/external_storage/rocksdb_storage_provider.hpp>

#include <hive/chain/database.hpp>

#include <rocksdb/cache.h>
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

    /// Block cache shared by all column families of the archive.
    std::shared_ptr<::rocksdb::Cache> _blockCache;

    ColumnFamilyOptions makeColumnOptions() const;

    ColumnDefinitions prepareColumnDefinitions(bool addDefaultColumn) override;

    WriteBatch& getWriteBuffer() override;

  public:

    using ptr = std::shared_ptr<rocksdb_comment_storage_provider>;

    static constexpr size_t DEFAULT_BLOCK_CACHE_SIZE = 1024 * 1024 * 1024;

    /// Block caches of at least this size get full filters and a binary search index, smaller
    /// ones partitioned index and filters.
    static constexpr size_t PARTITIONED_METADATA_CACHE_LIMIT = 1024 * 1024 * 1024;

    rocksdb_comment_storage_provider( const bfs::path& blockchain_storage_path, const bfs::path& storage_path, appbase::application& app,
      size_t block_cache_size = DEFAULT_BLOCK_CACHE_SIZE );
    ~rocksdb_comment_storage_provider() override{}

    void save( const Slice& key, const Slice& value ) override;
    bool read( const Slice& key, PinnableSlice& value ) override;
    void multi_read( const std::vector<Slice>& keys, std::vector<PinnableSlice>& values, std::vector<::rocksdb::Status>& statuses ) override;
};

}}

