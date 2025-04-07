#pragma once

#include <hive/chain/external_storage/external_storage_provider.hpp>

#include <hive/chain/database.hpp>

#include <rocksdb/options.h>
#include <rocksdb/slice.h>

#include <boost/core/demangle.hpp>
#include <boost/filesystem/path.hpp>

#include <fc/crypto/ripemd160.hpp>
#include <fc/filesystem.hpp>

namespace hive { namespace chain {

namespace bfs = boost::filesystem;

using ::rocksdb::DBOptions;
using ::rocksdb::Options;
using ::rocksdb::ReadOptions;
using ::rocksdb::Slice;
using ::rocksdb::Comparator;
using ::rocksdb::ColumnFamilyDescriptor;
using ::rocksdb::ColumnFamilyOptions;
using ::rocksdb::ColumnFamilyHandle;
using ::rocksdb::WriteBatch;

class rocksdb_storage_provider
{
  public:

    typedef std::vector<ColumnFamilyDescriptor> ColumnDefinitions;

  private:

    bfs::path             _storagePath;
    bfs::path             _blockchainStoragePath;

    appbase::application& theApp;

    /// std::tuple<A, B>
    /// A - returns true if database will need data import.
    /// B - returns false if problems with opening db appeared.
    std::tuple<bool, bool> createDbSchema(const bfs::path& path);

    virtual void loadAdditionalData() = 0;
    virtual ColumnDefinitions prepareColumnDefinitions(bool addDefaultColumn) = 0;

    void cleanupColumnHandles();
    void cleanupColumnHandles(::rocksdb::DB* db);

    void flushStorage();

    void saveStoreVersion();
    void verifyStoreVersion(DB* storageDb);

    void flushWriteBuffer(DB* storage = nullptr);

    template<typename Object>
    const Object& unpackSlice( const Slice& s )
    {
      assert(sizeof(Object) == s.size());
      return *reinterpret_cast<const Object*>(s.data());
    }

  protected:

    std::unique_ptr<DB>               _storage;
    std::vector<ColumnFamilyHandle*>  _columnHandles;
    WriteBatch                        _writeBuffer;

    std::unique_ptr<DB>& getStorage();

    void openDb( bool cleanDatabase );
    void shutdownDb( bool removeDB = false );

  public:

    rocksdb_storage_provider( const bfs::path& blockchain_storage_path, const bfs::path& storage_path, appbase::application& app );
    virtual ~rocksdb_storage_provider();

    void save( const Slice& key, const Slice& value );
    bool read( const Slice& key, PinnableSlice& value );
    void flush();
};

class rocksdb_ah_storage_provider: public rocksdb_storage_provider, public external_ah_storage_provider
{
  private:

    /// IDs to be assigned to object.id field.
    uint64_t                         _operationSeqId = 0;
    uint64_t                         _accountHistorySeqId = 0;

    void loadSeqIdentifiers(DB* storageDb);

    //loads last irreversible block from DB to _cached_irreversible_block
    void load_lib();
    //stores new value of last irreversible block in DB and _cached_irreversible_block
    void update_lib( uint32_t );
    //loads reindex point from DB to _cached_reindex_point
    void load_reindex_point();
    //stores new value of reindex point in DB and _cached_reindex_point
    void update_reindex_point( uint32_t );

    /// <summary>
    /// Block being irreversible atm.
    /// </summary>
    std::atomic_uint                 _cached_irreversible_block;

    /// <summary>
    /// Last block of most recent reindex - all such blocks are in inreversible storage already, since
    /// the data is put there directly during reindex (it also means there is no volatile data for such
    /// blocks), but _cached_irreversible_block might point to earlier block because it reflects
    /// the state of dgpo. Once node starts syncing normally, the _cached_irreversible_block will catch up.
    /// </summary>
    unsigned int                     _cached_reindex_point = 0;

  protected:

    void loadAdditionalData() override;
    ColumnDefinitions prepareColumnDefinitions(bool addDefaultColumn) override;

  public:

    rocksdb_ah_storage_provider( const bfs::path& blockchain_storage_path, const bfs::path& storage_path, appbase::application& app );
    ~rocksdb_ah_storage_provider() override{}

    std::unique_ptr<DB>& getStorage() override;

    void openDb( bool cleanDatabase ) override;
    void shutdownDb( bool removeDB = false ) override;

    const std::atomic_uint& get_cached_irreversible_block() const override;
    unsigned int get_cached_reindex_point() const override;

    uint64_t get_operationSeqId() const override;
    void set_operationSeqId( uint64_t value ) override;
};

class rocksdb_comment_storage_provider: public rocksdb_ah_storage_provider, public external_comment_storage_provider
{
  private:

    void loadAdditionalData() override{}
    ColumnDefinitions prepareColumnDefinitions(bool addDefaultColumn) override;

  public:

    using ptr = std::shared_ptr<rocksdb_comment_storage_provider>;

    rocksdb_comment_storage_provider( const bfs::path& blockchain_storage_path, const bfs::path& storage_path, appbase::application& app );
    ~rocksdb_comment_storage_provider() override{}

    void save( const Slice& key, const Slice& value ) override;
    bool read( const Slice& key, PinnableSlice& value ) override;
    void flush() override;
};

}}

