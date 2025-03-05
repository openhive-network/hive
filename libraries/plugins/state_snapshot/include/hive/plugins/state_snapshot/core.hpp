
#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/slice.h>
#include <rocksdb/utilities/backup_engine.h>
#include <rocksdb/utilities/write_batch_with_index.h>

#include <boost/type.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/container/flat_set.hpp>
#include <boost/range/adaptor/indexed.hpp>
#include <boost/range/adaptor/sliced.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <boost/filesystem/path.hpp>

#include <condition_variable>
#include <mutex>

#include <limits>
#include <string>
#include <typeindex>
#include <typeinfo>

#include <fc/crypto/ripemd160.hpp>
#include <fc/filesystem.hpp>

namespace xxx {

namespace bfs = boost::filesystem;

using ::rocksdb::DB;
using ::rocksdb::DBOptions;
using ::rocksdb::Options;
using ::rocksdb::PinnableSlice;
using ::rocksdb::ReadOptions;
using ::rocksdb::Slice;
using ::rocksdb::Comparator;
using ::rocksdb::ColumnFamilyDescriptor;
using ::rocksdb::ColumnFamilyOptions;
using ::rocksdb::ColumnFamilyHandle;
using ::rocksdb::WriteBatch;

namespace
{
class AComparator : public Comparator
  {
  public:
    virtual const char* Name() const override final
    {
    static const std::string name = boost::core::demangle(typeid(this).name());
    return name.c_str();
    }

    virtual void FindShortestSeparator(std::string* start, const Slice& limit) const override final
    {
      /// Nothing to do.
    }

    virtual void FindShortSuccessor(std::string* key) const override final
    {
      /// Nothing to do.
    }

  protected:
    AComparator() = default;
  };

class HashComparator final : public AComparator
  {
  public:
    virtual int Compare(const Slice& a, const Slice& b) const override
    {
    return a.compare(b);
    }

    virtual bool Equal(const Slice& a, const Slice& b) const override
    {
    return a == b;
    }
  };

const Comparator* by_Hash_Comparator()
  {
  static HashComparator c;
  return &c;
  }

#define checkStatus(s) FC_ASSERT((s).ok(), "Data access failed: ${m}", ("m", (s).ToString()))

} /// anonymous

struct core
{
  core()
  {
    _storagePath = "/home/mario/src/dump";
    openDb(true);
  }

  ~core()
  {
    shutdownDb();
  }

  void openDb( bool cleanDatabase )
  {
    if( cleanDatabase )
      ::rocksdb::DestroyDB( _storagePath.string(), ::rocksdb::Options() );

    auto _result = createDbSchema(_storagePath);
    if(  !std::get<1>( _result ) )
      return;

    auto columnDefs = prepareColumnDefinitions(true);

    DB* storageDb = nullptr;
    auto strPath = _storagePath.string();
    Options options;
    /// Optimize RocksDB. This is the easiest way to get RocksDB to perform well
    options.IncreaseParallelism();
    options.OptimizeLevelStyleCompaction();
    options.max_open_files = 1000;

    DBOptions dbOptions(options);

    auto status = DB::Open(dbOptions, strPath, columnDefs, &_columnHandles, &storageDb);

    if(status.ok())
    {

      _storage.reset(storageDb);

    }
    else
    {
      elog("RocksDB cannot open database at location: `${p}'.\nReturned error: ${e}",
        ("p", strPath)("e", status.ToString()));
    }
  }

  void shutdownDb( bool removeDB = false )
  {
    if(_storage)
    {
      flushStorage();
      cleanupColumnHandles();
      _storage->Close();
      _storage.reset();

      if( removeDB )
      {
        ilog( "Attempting to destroy current AHR storage..." );
        ::rocksdb::DestroyDB( _storagePath.string(), ::rocksdb::Options() );
        ilog( "AccountHistoryRocksDB has been destroyed at location: ${p}.", ( "p", _storagePath.string() ) );
      }
    }
  }

  typedef std::vector<ColumnFamilyDescriptor> ColumnDefinitions;

  ColumnDefinitions prepareColumnDefinitions( bool addDefaultColumn)
  {
    ColumnDefinitions columnDefs;

    if(addDefaultColumn)
      columnDefs.emplace_back("default", ColumnFamilyOptions());

    columnDefs.emplace_back("my_hashes", ColumnFamilyOptions());
    auto& byTxIdColumn = columnDefs.back();
    byTxIdColumn.options.comparator = by_Hash_Comparator();

    return columnDefs;
  }

  /// std::tuple<A, B>
  /// A - returns true if database will need data import.
  /// B - returns false if problems with opening db appeared.
  std::tuple<bool, bool> createDbSchema(const bfs::path& path)
  {
    DB* db = nullptr;

    auto columnDefs = prepareColumnDefinitions(true);
    auto strPath = path.string();
    Options options;
    /// Optimize RocksDB. This is the easiest way to get RocksDB to perform well
    options.IncreaseParallelism();
    options.OptimizeLevelStyleCompaction();
    options.max_open_files = 1000;

    auto s = DB::OpenForReadOnly(options, strPath, columnDefs, &_columnHandles, &db);

    if(s.ok())
    {
      cleanupColumnHandles(db);
      delete db;
      return { false, true }; /// { DB does not need data import, an application is not closed }
    }

    options.create_if_missing = true;

    s = DB::Open(options, strPath, &db);
    if(s.ok())
    {
      columnDefs = prepareColumnDefinitions(false);
      s = db->CreateColumnFamilies(columnDefs, &_columnHandles);
      if(s.ok())
      {

        /// Store initial values of Seq-IDs for held objects.
        flushWriteBuffer(db);
        cleanupColumnHandles(db);
      }
      else
      {
        elog("RocksDB can not create column definitions at location: `${p}'.\nReturned error: ${e}",
          ("p", strPath)("e", s.ToString()));
      }

      delete db;

      return { true, true }; /// { DB needs data import, an application is not closed }
    }
    else
    {
      elog("RocksDB can not create storage at location: `${p}'.\nReturned error: ${e}",
        ("p", strPath)("e", s.ToString()));

      return { false, false };/// { DB does not need data import, an application is closed }
    }
  }

  void cleanupColumnHandles()
  {
    if(_storage)
      cleanupColumnHandles(_storage.get());
  }

  void cleanupColumnHandles(::rocksdb::DB* db)
  {
    for(auto* h : _columnHandles)
    {
      auto s = db->DestroyColumnFamilyHandle(h);
      if(s.ok() == false)
      {
        elog("Cannot destroy column family handle. Error: `${e}'", ("e", s.ToString()));
      }
    }
    _columnHandles.clear();
  }

  void storeHash(const fc::ripemd160& hash)
  {
    Slice keySlice( hash.data(), hash.data_size() );
    Slice valueSlice;

    auto s = _writeBuffer.Put(_columnHandles[0], keySlice, valueSlice);
    checkStatus(s);
  }

  std::string getHash( const std::string& account, const std::string& permlink )
  {
    auto _hash = fc::ripemd160::hash( permlink + "@" + account );

    ReadOptions rOptions;

    Slice keySlice( _hash.data(), _hash.data_size() );

    std::string dataBuffer;
    ::rocksdb::Status s = _storage->Get(rOptions, _columnHandles[0], keySlice, &dataBuffer);

    if( s.ok() )
      return _hash.str();
    else
      return "xxx";

    FC_ASSERT(s.IsNotFound());
  }

  void flushWriteBuffer(DB* storage = nullptr)
  {
    if(storage == nullptr)
      storage = _storage.get();

    ::rocksdb::WriteOptions wOptions;
    auto s = storage->Write(wOptions, _writeBuffer.GetWriteBatch());
    checkStatus(s);
    _writeBuffer.Clear();
  }

  void flushStorage()
  {
    if(_storage == nullptr)
      return;

    // lib (last irreversible block) has not been saved so far
    flushWriteBuffer();

    ::rocksdb::FlushOptions fOptions;
    for(const auto& cf : _columnHandles)
    {
      auto s = _storage->Flush(fOptions, cf);
      checkStatus(s);
    }
  }

/// Class attributes:
private:
  bfs::path                        _storagePath;
  std::unique_ptr<DB>              _storage;
  std::vector<ColumnFamilyHandle*> _columnHandles;
  WriteBatch               _writeBuffer;

};


}
