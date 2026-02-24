#pragma once

#include <appbase/application.hpp>
#include <hive/chain/external_storage/types.hpp>

#include <hive/chain/external_storage/external_basic_provider.hpp>

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
using ::rocksdb::WideColumns;
using ::rocksdb::PinnableWideColumns;

class rocksdb_storage_provider: public external_basic_provider
{
  public:

    typedef std::vector<ColumnFamilyDescriptor> ColumnDefinitions;

  private:

    bool _initialized = false;

    bfs::path             _storagePath;
    bfs::path             _blockchainStoragePath;

    appbase::application& theApp;

    /// std::tuple<A, B>
    /// A - returns true if database will need data import.
    /// B - returns false if problems with opening db appeared.
    std::tuple<bool, bool> createDbSchema( const bfs::path& path );

    //loads last irreversible block from DB to _cached_irreversible_block
    void load_lib();

  private:

    /// <summary>
    /// Block being irreversible atm.
    /// </summary>
    std::atomic_uint                  _cached_irreversible_block;
    std::unique_ptr<DB>               _storage;
    std::vector<ColumnFamilyHandle*>  _columnHandles;

    virtual ColumnDefinitions prepareColumnDefinitions( bool addDefaultColumn ) = 0;

    void cleanupColumnHandles();

    void saveStoreVersion();
    void verifyStoreVersion();

    void update_lib_internal( uint32_t );

  protected:

    const std::string name;

    virtual void loadSeqIdentifiers(){}

    virtual WriteBatch& getWriteBuffer() = 0;

    const std::vector<ColumnFamilyHandle*>& getColumnHandles() const { return _columnHandles; }

  public:

    rocksdb_storage_provider( const bfs::path& blockchain_storage_path, const bfs::path& storage_path, appbase::application& app, const std::string& name );
    virtual ~rocksdb_storage_provider(){}

    std::unique_ptr<DB>& getStorage() override { return _storage; }

    void openDb( uint32_t expected_lib ) override;
    void shutdownDb() override;
    void flushDb() override;
    void flushWriteBuffer() override;
    void wipeDb() override;

    //stores new value of last irreversible block in DB and _cached_irreversible_block
    void update_lib( uint32_t ) override;
    uint32_t get_lib() const override { return _cached_irreversible_block; }

    /// Updates only the in-memory cached LIB without writing to RocksDB write batch.
    /// The caller must call persist_cached_lib() before the next flushDb() to persist.
    void update_cached_lib( uint32_t lib ) { _cached_irreversible_block.store( lib ); }

    /// Writes the currently cached LIB value to the RocksDB write batch.
    /// Only valid for storage providers that have the CURRENT_LIB column.
    void persist_cached_lib();

    void save( ColumnTypes column_type, const Slice& key, const Slice& value );
    bool read( ColumnTypes column_type, const Slice& key, PinnableSlice& value );
    void remove( ColumnTypes column_type, const Slice& key );

    void put_entity( ColumnTypes column_type, const Slice& key, const WideColumns& wide_columns );
    bool get_entity( ColumnTypes column_type, const Slice& key, PinnableWideColumns& wide_columns );

    ColumnFamilyHandle* getColumnHandle( ColumnTypes column );

    void finalizeStorage();
};

}}

