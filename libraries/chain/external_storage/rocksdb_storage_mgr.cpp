
#include <hive/chain/external_storage/rocksdb_storage_mgr.hpp>

#include <hive/chain/external_storage/comment_rocksdb_objects.hpp>
#include <hive/chain/external_storage/utilities.hpp>

namespace hive { namespace chain {

using ::rocksdb::PinnableSlice;

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

rocksdb_storage_mgr::rocksdb_storage_mgr( const bfs::path& storage_path, bool cleanDatabase, database& db ): _db( db )
{
  _storagePath = storage_path;
  openDb( cleanDatabase );
}

rocksdb_storage_mgr::~rocksdb_storage_mgr()
{
  shutdownDb();
}

void rocksdb_storage_mgr::openDb( bool cleanDatabase )
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

void rocksdb_storage_mgr::shutdownDb( bool removeDB )
{
  if(_storage)
  {
    ilog("xxxxxxxxxxxxxxxxxxx shutdownDb 00");
    flushStorage();
    ilog("xxxxxxxxxxxxxxxxxxx shutdownDb 01");
    cleanupColumnHandles();
    ilog("xxxxxxxxxxxxxxxxxxx shutdownDb 02");
    _storage->Close();
    ilog("xxxxxxxxxxxxxxxxxxx shutdownDb 03");
    _storage.reset();
    ilog("xxxxxxxxxxxxxxxxxxx shutdownDb 04");

    if( removeDB )
    {
      ilog( "Attempting to destroy current AHR storage..." );
      ::rocksdb::DestroyDB( _storagePath.string(), ::rocksdb::Options() );
      ilog( "AccountHistoryRocksDB has been destroyed at location: ${p}.", ( "p", _storagePath.string() ) );
    }
  }
}

rocksdb_storage_mgr::ColumnDefinitions rocksdb_storage_mgr::prepareColumnDefinitions( bool addDefaultColumn)
{
  ColumnDefinitions columnDefs;

  if(addDefaultColumn)
    columnDefs.emplace_back("default", ColumnFamilyOptions());

  columnDefs.emplace_back("my_hashes", ColumnFamilyOptions());
  auto& byTxIdColumn = columnDefs.back();
  byTxIdColumn.options.comparator = by_Hash_Comparator();

  return columnDefs;
}

std::tuple<bool, bool> rocksdb_storage_mgr::createDbSchema(const bfs::path& path)
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

void rocksdb_storage_mgr::cleanupColumnHandles()
{
  if(_storage)
    cleanupColumnHandles(_storage.get());
}

void rocksdb_storage_mgr::cleanupColumnHandles(::rocksdb::DB* db)
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

std::string rocksdb_storage_mgr::getHash( const fc::ripemd160& content )
{
  ReadOptions rOptions;

  Slice keySlice( content.data(), content.data_size() );

  PinnableSlice _buffer;
  ::rocksdb::Status s = _storage->Get(rOptions, _columnHandles[0], keySlice, &_buffer);

  if( s.ok() )
    return { content.str() };
  else
    return "";
}

void rocksdb_storage_mgr::flushWriteBuffer(DB* storage)
{
  ilog("xxxxxxxxxxxxxxxxxxx shutdownDb B");
  if(storage == nullptr)
    storage = _storage.get();

  ::rocksdb::WriteOptions wOptions;
  auto s = storage->Write(wOptions, _writeBuffer.GetWriteBatch());
  ilog("xxxxxxxxxxxxxxxxxxx shutdownDb C");
  checkStatus(s);
  _writeBuffer.Clear();
  ilog("xxxxxxxxxxxxxxxxxxx shutdownDb D");
}

void rocksdb_storage_mgr::flushStorage()
{
  if(_storage == nullptr)
    return;

  // lib (last irreversible block) has not been saved so far
  ilog("xxxxxxxxxxxxxxxxxxx shutdownDb A");
  flushWriteBuffer();

  ::rocksdb::FlushOptions fOptions;
  ilog("xxxxxxxxxxxxxxxxxxx shutdownDb E");
  for(const auto& cf : _columnHandles)
  {
    ilog("xxxxxxxxxxxxxxxxxxx shutdownDb F");
    auto s = _storage->Flush(fOptions, cf);
    ilog("xxxxxxxxxxxxxxxxxxx shutdownDb G");
    checkStatus(s);
  }
  ilog("xxxxxxxxxxxxxxxxxxx shutdownDb H");
}

database& rocksdb_storage_mgr::get_database()
{
  return _db;
}

void rocksdb_storage_mgr::save( const Slice& key, const Slice& value, const uint32_t& column_number )
{
  auto s = _writeBuffer.Put( _columnHandles[column_number], key, value );
  checkStatus(s);
}

void rocksdb_storage_mgr::read( const Slice& key, std::string& value, const uint32_t& column_number )
{
  ReadOptions rOptions;

  ::rocksdb::Status s = _storage->Get( rOptions, _columnHandles[column_number], key, &value );
  checkStatus(s);
}

}}
