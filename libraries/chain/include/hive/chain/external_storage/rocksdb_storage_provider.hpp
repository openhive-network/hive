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

class rocksdb_storage_provider: public external_storage_provider
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

    template<typename Object>
    const Object& unpackSlice( const Slice& s )
    {
      assert(sizeof(Object) == s.size());
      return *reinterpret_cast<const Object*>(s.data());
    }

  public:

    rocksdb_storage_provider( const bfs::path& storage_path );
    ~rocksdb_storage_provider();

    std::unique_ptr<DB>& get_storage() override;

    void save( const Slice& key, const Slice& value ) override;
    bool read( const Slice& key, PinnableSlice& value ) override;
    void flush() override;
};

}}
