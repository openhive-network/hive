
#include <hive/chain/external_storage/rocksdb_storage_provider.hpp>

#include <hive/chain/external_storage/comment_rocksdb_objects.hpp>
#include <hive/chain/external_storage/utilities.hpp>

namespace hive { namespace chain {

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

rocksdb_storage_provider::rocksdb_storage_provider( const bfs::path& storage_path, bool cleanDatabase )
{
  _storagePath = storage_path;
  openDb( cleanDatabase );
}

rocksdb_storage_provider::~rocksdb_storage_provider()
{
  shutdownDb();
}

void rocksdb_storage_provider::openDb( bool cleanDatabase )
{
  if( cleanDatabase )
  {
    ilog("Clean a database at location ${path}", ("path", _storagePath.string()));
    ::rocksdb::DestroyDB( _storagePath.string(), ::rocksdb::Options() );
  }

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

void rocksdb_storage_provider::shutdownDb( bool removeDB )
{
  if(_storage)
  {
    ilog("Shutdown RocksDB: flush storage");
    flushStorage();
    ilog("Shutdown RocksDB: cleanup column handles");
    cleanupColumnHandles();
    ilog("Shutdown RocksDB: close storage");
    _storage->Close();
    ilog("Shutdown RocksDB: clear storage");
    _storage.reset();

    if( removeDB )
    {
      ilog( "Attempting to destroy current AHR storage..." );
      ::rocksdb::DestroyDB( _storagePath.string(), ::rocksdb::Options() );
      ilog( "AccountHistoryRocksDB has been destroyed at location: ${p}.", ( "p", _storagePath.string() ) );
    }
  }
}

rocksdb_storage_provider::ColumnDefinitions rocksdb_storage_provider::prepareColumnDefinitions( bool addDefaultColumn)
{
  ColumnDefinitions columnDefs;

  if(addDefaultColumn)
    columnDefs.emplace_back("default", ColumnFamilyOptions());

  columnDefs.emplace_back("my_hashes", ColumnFamilyOptions());
  auto& byTxIdColumn = columnDefs.back();
  byTxIdColumn.options.comparator = by_Hash_Comparator();

  return columnDefs;
}

std::tuple<bool, bool> rocksdb_storage_provider::createDbSchema(const bfs::path& path)
{
  DB* db = nullptr;

  ilog("Prepare column definitions");
  auto columnDefs = prepareColumnDefinitions(true);
  auto strPath = path.string();
  Options options;
  /// Optimize RocksDB. This is the easiest way to get RocksDB to perform well
  options.IncreaseParallelism();
  options.OptimizeLevelStyleCompaction();
  options.max_open_files = 1000;

  ilog("Open RocksDB for read only");
  auto s = DB::OpenForReadOnly(options, strPath, columnDefs, &_columnHandles, &db);

  if(s.ok())
  {
    ilog("Cleanup column handles in read only database");
    cleanupColumnHandles(db);
    delete db;
    return { false, true }; /// { DB does not need data import, an application is not closed }
  }

  options.create_if_missing = true;

  ilog("Open RocksDB");
  s = DB::Open(options, strPath, &db);
  if(s.ok())
  {
    ilog("Prepare column definitions");
    columnDefs = prepareColumnDefinitions(false);
    ilog("Create column families");
    s = db->CreateColumnFamilies(columnDefs, &_columnHandles);
    if(s.ok())
    {
      ilog("Flush write buffer");
      /// Store initial values of Seq-IDs for held objects.
      flushWriteBuffer(db);
      ilog("Cleanup column handles");
      cleanupColumnHandles(db);
    }
    else
    {
      elog("RocksDB can not create column definitions at location: `${p}'.\nReturned error: ${e}",
        ("p", strPath)("e", s.ToString()));
    }

    ilog("Destroy a temporary database");
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

void rocksdb_storage_provider::cleanupColumnHandles()
{
  if(_storage)
    cleanupColumnHandles(_storage.get());
}

void rocksdb_storage_provider::cleanupColumnHandles(::rocksdb::DB* db)
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

void rocksdb_storage_provider::flushWriteBuffer(DB* storage)
{
  if(storage == nullptr)
    storage = _storage.get();

  ::rocksdb::WriteOptions wOptions;
  auto s = storage->Write(wOptions, _writeBuffer.GetWriteBatch());
  checkStatus(s);
  _writeBuffer.Clear();
}

void rocksdb_storage_provider::flushStorage()
{
  if(_storage == nullptr)
    return;

  // lib (last irreversible block) has not been saved so far
  ilog("Flush storage: flush write buffer");
  flushWriteBuffer();

  ::rocksdb::FlushOptions fOptions;
  ilog("Flush storage: flush column handles");
  for(const auto& cf : _columnHandles)
  {
    ilog("Flush storage: flush column handle");
    auto s = _storage->Flush(fOptions, cf);
    checkStatus(s);
  }
}

void rocksdb_storage_provider::save( const Slice& key, const Slice& value, const uint32_t& column_number )
{
  auto s = _writeBuffer.Put( _columnHandles[column_number], key, value );
  checkStatus(s);
}

bool rocksdb_storage_provider::read( const Slice& key, PinnableSlice& value, const uint32_t& column_number )
{
  ReadOptions rOptions;

  ::rocksdb::Status s = _storage->Get( rOptions, _columnHandles[column_number], key, &value );
  return s.ok();
}

void rocksdb_storage_provider::remove( const Slice& key, const uint32_t& column_number )
{
  ::rocksdb::Status s = _writeBuffer.Delete( _columnHandles[column_number], key );
  checkStatus(s);
}

void rocksdb_storage_provider::flush()
{
  flushWriteBuffer();
}

}}
