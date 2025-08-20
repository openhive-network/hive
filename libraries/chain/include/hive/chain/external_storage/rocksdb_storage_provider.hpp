#pragma once

#include <appbase/application.hpp>
#include <hive/chain/external_storage/types.hpp>

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

    //loads last irreversible block from DB to _cached_irreversible_block
    void load_lib();

    //loads reindex point from DB to _cached_reindex_point
    void load_reindex_point();

  protected:

    //stores new value of last irreversible block in DB and _cached_irreversible_block
    void update_lib( uint32_t );

    //stores new value of reindex point in DB and _cached_reindex_point
    void update_reindex_point( uint32_t );

  private:

    void loadAdditionalData();

    virtual ColumnDefinitions prepareColumnDefinitions(bool addDefaultColumn) = 0;

    void cleanupColumnHandles();
    void cleanupColumnHandles(::rocksdb::DB* db);

    void saveStoreVersion();
    void verifyStoreVersion(DB* storageDb);

  protected:

    /// <summary>
    /// Last block of most recent reindex - all such blocks are in inreversible storage already, since
    /// the data is put there directly during reindex (it also means there is no volatile data for such
    /// blocks), but _cached_irreversible_block might point to earlier block because it reflects
    /// the state of dgpo. Once node starts syncing normally, the _cached_irreversible_block will catch up.
    /// </summary>
    unsigned int                     _cached_reindex_point = 0;

    /// <summary>
    /// Block being irreversible atm.
    /// </summary>
    std::atomic_uint                 _cached_irreversible_block;

    virtual void loadSeqIdentifiers(DB* storageDb) = 0;

    std::unique_ptr<DB>               _storage;
    std::vector<ColumnFamilyHandle*>  _columnHandles;

    std::unique_ptr<DB>& getStorage() { return _storage; }

    void openDb( bool cleanDatabase );
    void shutdownDb( bool removeDB = false );
    void wipeDb();

    void flushStorage();

    virtual void beforeFlushWriteBuffer(){}
    void flushWriteBuffer(DB* storage = nullptr);
    virtual void afterFlushWriteBuffer(){}

    virtual WriteBatch& getWriteBuffer() = 0;

  public:

    rocksdb_storage_provider( const bfs::path& blockchain_storage_path, const bfs::path& storage_path, appbase::application& app );
    virtual ~rocksdb_storage_provider(){}

    void init( bool destroy_on_startup );

  protected:

    void save( ColumnTypes column_type, const Slice& key, const Slice& value );
    bool read( ColumnTypes column_type, const Slice& key, PinnableSlice& value );
    void remove( ColumnTypes column_type, const Slice& key );

  public:

    void flush();

    ColumnFamilyHandle* getColumnHandle( ColumnTypes column );
};

}}

