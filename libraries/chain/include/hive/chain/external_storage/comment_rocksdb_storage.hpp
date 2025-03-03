#pragma once

#include <hive/chain/database.hpp>
#include <hive/chain/external_storage/external_storage_support.hpp>

#include <hive/chain/external_storage//comment_rocksdb_objects.hpp>

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

class comment_rocksdb_storage: public external_storage_provider
{
  public:

    typedef std::vector<ColumnFamilyDescriptor> ColumnDefinitions;

  private:

    bfs::path                         _storagePath;
    std::unique_ptr<DB>               _storage;
    std::vector<ColumnFamilyHandle*>  _columnHandles;
    WriteBatch                        _writeBuffer;

    database& _db;

    void openDb( bool cleanDatabase );

    void shutdownDb( bool removeDB = false );

    ColumnDefinitions prepareColumnDefinitions( bool addDefaultColumn);

    /// std::tuple<A, B>
    /// A - returns true if database will need data import.
    /// B - returns false if problems with opening db appeared.
    std::tuple<bool, bool> createDbSchema(const bfs::path& path);

    void cleanupColumnHandles();

    void cleanupColumnHandles(::rocksdb::DB* db);

    void storeHash( const fc::ripemd160& content );
    std::string getHash( const fc::ripemd160& content );

    void storeString( const std::string& content );
    std::string getString( const std::string& content );

    void flushWriteBuffer(DB* storage = nullptr);

    void flushStorage();

  public:

    comment_rocksdb_storage( const bfs::path& storage_path, bool cleanDatabase, database& db );
    ~comment_rocksdb_storage();

    void store_comment( const comment_id_type& comment_id, uint32_t block_number ) override;
    void cashout_is_done( const comment_id_type& comment_id ) override;

};

}}
