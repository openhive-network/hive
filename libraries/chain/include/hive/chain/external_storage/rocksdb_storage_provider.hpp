#pragma once

#include <hive/chain/external_storage/external_storage_provider.hpp>

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

    rocksdb_storage_provider( const bfs::path& storage_path, bool cleanDatabase );
    ~rocksdb_storage_provider();

    void save( const Slice& key, const Slice& value, const uint32_t& column_number ) override;
    bool read( const Slice& key, PinnableSlice& value, const uint32_t& column_number ) override;
    void remove( const Slice& key, const uint32_t& column_number ) override;
    void flush() override;

    template<typename Key, typename Value>
    uint32_t read( const std::optional<Slice>& start, uint32_t limit, std::vector<std::pair<Key, Value>>& records, const uint32_t& column_number )
    {
      const uint32_t _max_limit = 1'000'000;

      if( limit > _max_limit )
        limit = _max_limit;

      ReadOptions rOptions;
      std::unique_ptr<::rocksdb::Iterator> _it( _storage->NewIterator( rOptions, _columnHandles[column_number]) );

      uint32_t _cnt = 0;

      if( start )
        _it->Seek( *start );
      else
        _it->SeekToFirst();

      for( ; _it->Valid() && _cnt < limit; _it->Next(), ++_cnt )
      {
        records.push_back( std::make_pair( unpackSlice<Key>( _it->key() ), unpackSlice<Value>( _it->value() ) ) );
      }

      return _cnt;
    }
};

}}
