#pragma once

#include <hive/chain/database.hpp>

#include <rocksdb/options.h>
#include <rocksdb/slice.h>
#include <rocksdb/db.h>

#include <boost/core/demangle.hpp>
#include <boost/filesystem/path.hpp>

#include <fc/crypto/ripemd160.hpp>
#include <fc/filesystem.hpp>

namespace hive { namespace chain {

namespace bfs = boost::filesystem;

using ::rocksdb::DB;
using ::rocksdb::DBOptions;
using ::rocksdb::Options;
using ::rocksdb::ReadOptions;
using ::rocksdb::Slice;
using ::rocksdb::PinnableSlice;
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

    void saveStoreVersion();
    void verifyStoreVersion(DB* storageDb);

  protected:

    std::unique_ptr<DB>               _storage;
    std::vector<ColumnFamilyHandle*>  _columnHandles;

    std::unique_ptr<DB>& getStorage();

    void openDb( bool cleanDatabase );
    void shutdownDb( bool removeDB = false );

    void flushStorage();

    virtual void beforeFlushWriteBuffer(){}
    void flushWriteBuffer(DB* storage = nullptr);
    virtual void afterFlushWriteBuffer(){}

    virtual WriteBatch& getWriteBuffer() = 0;

  public:

    rocksdb_storage_provider( const bfs::path& blockchain_storage_path, const bfs::path& storage_path, appbase::application& app );
    virtual ~rocksdb_storage_provider(){}

    void init( bool destroy_on_startup );

    void save( const Slice& key, const Slice& value );
    bool read( const Slice& key, PinnableSlice& value );
    void flush();
};

}}

