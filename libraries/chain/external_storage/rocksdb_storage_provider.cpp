
#include <hive/chain/external_storage/rocksdb_storage_provider.hpp>

#include <hive/chain/external_storage/comment_rocksdb_objects.hpp>
#include <hive/chain/external_storage/utilities.hpp>
#include <hive/chain/external_storage/types.hpp>

#include <appbase/application.hpp>

namespace hive { namespace chain {

rocksdb_storage_provider::rocksdb_storage_provider( const bfs::path& blockchain_storage_path, const bfs::path& storage_path, appbase::application& app )
                        : _storagePath( storage_path ), _blockchainStoragePath( blockchain_storage_path ), theApp( app )
{
}

void rocksdb_storage_provider::init( bool destroy_on_startup )
{
  if( !bfs::exists(_storagePath ) )
    bfs::create_directories( _storagePath );

  openDb( destroy_on_startup );
}

std::unique_ptr<DB>& rocksdb_storage_provider::getStorage()
{
  return _storage;
}

void rocksdb_storage_provider::openDb( bool cleanDatabase )
{
  //Very rare case -  when a synchronization starts from the scratch and a node has AH plugin with rocksdb enabled and directories don't exist yet
  bfs::create_directories( _blockchainStoragePath );

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
  options.max_open_files = OPEN_FILE_LIMIT;

  DBOptions dbOptions(options);

  auto status = DB::Open(dbOptions, strPath, columnDefs, &_columnHandles, &storageDb);

  if(status.ok())
  {
    ilog("RocksDB opened successfully storage at location: `${p}'.", ("p", strPath));
    verifyStoreVersion(storageDb);
    getStorage().reset(storageDb);

    loadAdditionalData();
  }
  else
  {
    elog("RocksDB cannot open database at location: `${p}'.\nReturned error: ${e}",
      ("p", strPath)("e", status.ToString()));
  }
}

void rocksdb_storage_provider::shutdownDb( bool removeDB )
{
  if(getStorage())
  {
    flushStorage();
    cleanupColumnHandles();
    getStorage()->Close();
    getStorage().reset();

    if( removeDB )
    {
      ilog( "Attempting to destroy current AHR storage..." );
      ::rocksdb::DestroyDB( _storagePath.string(), ::rocksdb::Options() );
      ilog( "AccountHistoryRocksDB has been destroyed at location: ${p}.", ( "p", _storagePath.string() ) );
    }
  }
}

std::tuple<bool, bool> rocksdb_storage_provider::createDbSchema(const bfs::path& path)
{
  DB* db = nullptr;

  auto columnDefs = prepareColumnDefinitions(true);
  auto strPath = path.string();
  Options options;
  /// Optimize RocksDB. This is the easiest way to get RocksDB to perform well
  options.IncreaseParallelism();
  options.OptimizeLevelStyleCompaction();
  options.max_open_files = OPEN_FILE_LIMIT;

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
      ilog("RocksDB column definitions created successfully.");
      saveStoreVersion();
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

    theApp.generate_interrupt_request();

    return { false, false };/// { DB does not need data import, an application is closed }
  }
}

void rocksdb_storage_provider::cleanupColumnHandles()
{
  if(getStorage())
    cleanupColumnHandles(getStorage().get());
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
  beforeFlushWriteBuffer();

  if(storage == nullptr)
    storage = getStorage().get();

  ::rocksdb::WriteOptions wOptions;
  auto s = storage->Write(wOptions, getWriteBuffer().GetWriteBatch());
  checkStatus(s);
  getWriteBuffer().Clear();

  afterFlushWriteBuffer();
}

void rocksdb_storage_provider::flushStorage()
{
  if(getStorage() == nullptr)
    return;

  // lib (last irreversible block) has not been saved so far
  flushWriteBuffer();

  ::rocksdb::FlushOptions fOptions;
  for(const auto& cf : _columnHandles)
  {
    auto s = getStorage()->Flush(fOptions, cf);
    checkStatus(s);
  }
}

void rocksdb_storage_provider::saveStoreVersion()
{
  PrimitiveTypeSlice<uint32_t> majorVSlice(STORE_MAJOR_VERSION);
  PrimitiveTypeSlice<uint32_t> minorVSlice(STORE_MINOR_VERSION);

  auto s = getWriteBuffer().Put(Slice("STORE_MAJOR_VERSION"), majorVSlice);
  checkStatus(s);
  s = getWriteBuffer().Put(Slice("STORE_MINOR_VERSION"), minorVSlice);
  checkStatus(s);
}

void rocksdb_storage_provider::verifyStoreVersion(DB* storageDb)
{
  ReadOptions rOptions;

  std::string buffer;
  auto s = storageDb->Get(rOptions, "STORE_MAJOR_VERSION", &buffer);
  checkStatus(s);
  const auto major = PrimitiveTypeSlice<uint32_t>::unpackSlice(buffer);

  FC_ASSERT(major == STORE_MAJOR_VERSION, "Store major version mismatch");

  s = storageDb->Get(rOptions, "STORE_MINOR_VERSION", &buffer);
  checkStatus(s);
  const auto minor = PrimitiveTypeSlice<uint32_t>::unpackSlice(buffer);

  FC_ASSERT(minor == STORE_MINOR_VERSION, "Store minor version mismatch");
}

void rocksdb_storage_provider::save( const Slice& key, const Slice& value )
{
  auto s = getWriteBuffer().Put( _columnHandles[CommentsColumns::COMMENT], key, value );
  checkStatus(s);
}

bool rocksdb_storage_provider::read( const Slice& key, PinnableSlice& value )
{
  ReadOptions rOptions;

  ::rocksdb::Status s = getStorage()->Get( rOptions, _columnHandles[CommentsColumns::COMMENT], key, &value );
  return s.ok();
}

void rocksdb_storage_provider::flush()
{
  flushWriteBuffer();
}

rocksdb_comment_storage_provider::rocksdb_comment_storage_provider( const bfs::path& blockchain_storage_path, const bfs::path& storage_path, appbase::application& app )
                                  : rocksdb_storage_provider( blockchain_storage_path, storage_path, app )
{

}

rocksdb_storage_provider::ColumnDefinitions rocksdb_comment_storage_provider::prepareColumnDefinitions( bool addDefaultColumn)
{
  ColumnDefinitions columnDefs;

  if(addDefaultColumn)
    columnDefs.emplace_back("default", ColumnFamilyOptions());

  columnDefs.emplace_back("account_permlink_hash", ColumnFamilyOptions());
  auto& byTxIdColumn = columnDefs.back();
  byTxIdColumn.options.comparator = by_Hash_Comparator();

  return columnDefs;
}

WriteBatch& rocksdb_comment_storage_provider::getWriteBuffer()
{
  return _writeBuffer;
}

std::unique_ptr<DB>& rocksdb_comment_storage_provider::getStorage()
{
  return rocksdb_storage_provider::getStorage();
}

void rocksdb_comment_storage_provider::openDb( bool cleanDatabase )
{
  rocksdb_storage_provider::openDb( cleanDatabase );
}

void rocksdb_comment_storage_provider::shutdownDb( bool removeDB )
{
  rocksdb_storage_provider::shutdownDb( removeDB );
}

void rocksdb_comment_storage_provider::save( const Slice& key, const Slice& value )
{
  rocksdb_storage_provider::save( key, value );
}

bool rocksdb_comment_storage_provider::read( const Slice& key, PinnableSlice& value )
{
  return rocksdb_storage_provider::read( key, value );
}

void rocksdb_comment_storage_provider::flush()
{
  rocksdb_storage_provider::flush();
}

}}
