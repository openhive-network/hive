#pragma once

#include <hive/chain/external_storage/external_storage_mgr.hpp>

#include <hive/chain/database.hpp>

#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/slice.h>

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
using ::rocksdb::Comparator;
using ::rocksdb::ColumnFamilyDescriptor;
using ::rocksdb::ColumnFamilyOptions;
using ::rocksdb::ColumnFamilyHandle;
using ::rocksdb::WriteBatch;

class rocksdb_storage_mgr: public external_storage_mgr
{
  public:

    typedef std::vector<ColumnFamilyDescriptor> ColumnDefinitions;

  private:

    bfs::path                         _storagePath;
    std::unique_ptr<DB>               _storage;
    std::vector<ColumnFamilyHandle*>  _columnHandles;
    WriteBatch                        _writeBuffer;

    void openDb( bool cleanDatabase );

    void shutdownDb( bool removeDB = false );

    ColumnDefinitions prepareColumnDefinitions( bool addDefaultColumn);

    /// std::tuple<A, B>
    /// A - returns true if database will need data import.
    /// B - returns false if problems with opening db appeared.
    std::tuple<bool, bool> createDbSchema(const bfs::path& path);

    void cleanupColumnHandles();

    void cleanupColumnHandles(::rocksdb::DB* db);

    void flushWriteBuffer(DB* storage = nullptr);

    void flushStorage();

  public:

    rocksdb_storage_mgr( const bfs::path& storage_path, bool cleanDatabase );
    ~rocksdb_storage_mgr();

    void save( const Slice& key, const Slice& value, const uint32_t& column_number ) override;
    void read( const Slice& key, std::string& value, const uint32_t& column_number ) override;
    uint32_t read( const std::optional<Slice>& start, uint32_t limit, std::vector<std::string>& values, const uint32_t& column_number );
};

}}
