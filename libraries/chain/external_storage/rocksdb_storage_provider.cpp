
#include <hive/chain/external_storage/rocksdb_storage_provider.hpp>

#include <hive/chain/external_storage/comment_rocksdb_objects.hpp>
#include <hive/chain/external_storage/utilities.hpp>
#include <hive/chain/external_storage/types.hpp>

#include <appbase/application.hpp>

namespace hive { namespace chain {

rocksdb_storage_provider::rocksdb_storage_provider( const bfs::path& blockchain_storage_path, const bfs::path& storage_path, appbase::application& app )
                        : _storagePath( storage_path ), _blockchainStoragePath( blockchain_storage_path ), theApp( app )
{
  openDb( false/*cleanDatabase*/ );
}

rocksdb_storage_provider::~rocksdb_storage_provider()
{
  shutdownDb();
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
  auto s = storage->Write(wOptions, _writeBuffer.GetWriteBatch());
  checkStatus(s);
  _writeBuffer.Clear();

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

  auto s = _writeBuffer.Put(Slice("STORE_MAJOR_VERSION"), majorVSlice);
  checkStatus(s);
  s = _writeBuffer.Put(Slice("STORE_MINOR_VERSION"), minorVSlice);
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
  auto s = _writeBuffer.Put( _columnHandles[CommentsColumns::COMMENT], key, value );
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

rocksdb_ah_storage_provider::rocksdb_ah_storage_provider( const bfs::path& blockchain_storage_path, const bfs::path& storage_path, appbase::application& app )
                : rocksdb_storage_provider( blockchain_storage_path, storage_path, app )
{
  _cached_irreversible_block.store(0);
  _cached_reindex_point = 0;
}

void rocksdb_ah_storage_provider::storeSequenceIds()
{
  Slice ahSeqIdName("AH_SEQ_ID");

  hive::chain::id_slice_t ahId( get_accountHistorySeqId() );

  auto s = _writeBuffer.Put(ahSeqIdName, ahId);
  checkStatus(s);
}

void rocksdb_ah_storage_provider::loadSeqIdentifiers(DB* storageDb)
{
  Slice ahSeqIdName("AH_SEQ_ID");

  ReadOptions rOptions;

  std::string buffer;
  /// OP-seq-id is local to block num
  _operationSeqId = 0; /// id_slice_t::unpackSlice(buffer);

  auto s = storageDb->Get(rOptions, ahSeqIdName, &buffer);
  checkStatus(s);
  _accountHistorySeqId = id_slice_t::unpackSlice(buffer);

  ilog("Loaded AccountHistoryObject seqId: ${ah}.", ("ah", _accountHistorySeqId));
}

void rocksdb_ah_storage_provider::load_lib()
{
  std::string data;
  auto s = getStorage()->Get(ReadOptions(), _columnHandles[Columns::CURRENT_LIB], LIB_ID, &data );

  if(s.code() == ::rocksdb::Status::kNotFound)
  {
    ilog( "RocksDB LIB not present in DB." );
    update_lib( 0 ); ilog( "RocksDB LIB set to 0." );
    return;
  }

  FC_ASSERT( s.ok(), "Could not find last irreversible block. Error msg: `${e}'", ("e", s.ToString()) );

  uint32_t lib = lib_slice_t::unpackSlice(data);

  FC_ASSERT( lib >= _cached_irreversible_block,
    "Inconsistency in last irreversible block - cached ${c}, stored ${s}",
    ( "c", static_cast< uint32_t >( _cached_irreversible_block ) )( "s", lib ) );
  _cached_irreversible_block.store( lib );
  ilog( "RocksDB LIB loaded with value ${l}.", ( "l", lib ) );
}

void rocksdb_ah_storage_provider::update_lib( uint32_t lib )
{
  //dlog( "RocksDB LIB set to ${l}.", ( "l", lib ) ); //too frequent
  _cached_irreversible_block.store(lib);
  auto s = _writeBuffer.Put( _columnHandles[Columns::CURRENT_LIB], LIB_ID, lib_slice_t( lib ) );
  checkStatus( s );
}

void rocksdb_ah_storage_provider::load_reindex_point()
{
  std::string data;
  auto s = getStorage()->Get( ReadOptions(), _columnHandles[Columns::LAST_REINDEX_POINT], REINDEX_POINT_ID, &data );

  if( s.code() == ::rocksdb::Status::kNotFound )
  {
    ilog( "RocksDB reindex point not present in DB." );
    update_reindex_point( 0 );
    return;
  }

  FC_ASSERT( s.ok(), "Could not find last reindex point. Error msg: `${e}'", ( "e", s.ToString() ) );

  uint32_t rp = lib_slice_t::unpackSlice(data);

  FC_ASSERT( rp >= _cached_reindex_point,
    "Inconsistency in reindex point - cached ${c}, stored ${s}",
    ( "c", _cached_reindex_point )( "s", rp ) );
  _cached_reindex_point = rp;
  ilog( "RocksDB reindex point loaded with value ${p}.", ( "p", rp ) );
}

void rocksdb_ah_storage_provider::update_reindex_point( uint32_t rp )
{
  ilog( "RocksDB reindex point set to ${p}.", ( "p", rp ) );
  _cached_reindex_point = rp;
  auto s = _writeBuffer.Put( _columnHandles[Columns::LAST_REINDEX_POINT], REINDEX_POINT_ID, lib_slice_t( rp ) );
  checkStatus( s );
}

void rocksdb_ah_storage_provider::flushStorage()
{
  rocksdb_storage_provider::flushStorage();
}

void rocksdb_ah_storage_provider::flushWriteBuffer(DB* storage)
{
  rocksdb_storage_provider::flushWriteBuffer( storage );
}

std::vector<ColumnFamilyHandle*>& rocksdb_ah_storage_provider::getColumnHandles()
{
  return _columnHandles;
}

void rocksdb_ah_storage_provider::loadAdditionalData()
{
  loadSeqIdentifiers(getStorage().get());
  // I do not like using exceptions for control paths, but column definitions are set multiple times
  // opening the db, so that is not a good place to write the initial lib.
  try
  {
    load_lib();
    try
    {
      load_reindex_point();
    }
    catch( fc::assert_exception& )
    {
      update_reindex_point( 0 );
    }
  }
  catch( fc::assert_exception& )
  {
    update_lib( 0 );
    update_reindex_point( 0 );
  }
}

rocksdb_storage_provider::ColumnDefinitions rocksdb_ah_storage_provider::prepareColumnDefinitions(bool addDefaultColumn)
{
  ColumnDefinitions columnDefs;
  if(addDefaultColumn)
    columnDefs.emplace_back(::rocksdb::kDefaultColumnFamilyName, ColumnFamilyOptions());

  //see definition of Columns enum
  columnDefs.emplace_back("current_lib", ColumnFamilyOptions());
  //columnDefs.emplace_back("last_reindex_point", ColumnFamilyOptions() ); reused above as another record

  columnDefs.emplace_back("operation_by_id", ColumnFamilyOptions());
  auto& byIdColumn = columnDefs.back();
  byIdColumn.options.comparator = by_id_Comparator();

  columnDefs.emplace_back("operation_by_block", ColumnFamilyOptions());
  auto& byLocationColumn = columnDefs.back();
  byLocationColumn.options.comparator = op_by_block_num_Comparator();

  columnDefs.emplace_back("account_history_info_by_name", ColumnFamilyOptions());
  auto& byAccountNameColumn = columnDefs.back();
  byAccountNameColumn.options.comparator = by_account_name_Comparator();

  columnDefs.emplace_back("ah_operation_by_id", ColumnFamilyOptions());
  auto& byAHInfoColumn = columnDefs.back();
  byAHInfoColumn.options.comparator = ah_op_by_id_Comparator();

  columnDefs.emplace_back("by_tx_id", ColumnFamilyOptions());
  auto& byTxIdColumn = columnDefs.back();
  byTxIdColumn.options.comparator = by_txId_Comparator();

  return columnDefs;
}

void rocksdb_ah_storage_provider::beforeFlushWriteBuffer()
{
  storeSequenceIds();
}

void rocksdb_ah_storage_provider::afterFlushWriteBuffer()
{
  _collectedOps = 0;
}

std::unique_ptr<DB>& rocksdb_ah_storage_provider::getStorage()
{
  return rocksdb_storage_provider::getStorage();
}

void rocksdb_ah_storage_provider::openDb( bool cleanDatabase )
{
  rocksdb_storage_provider::openDb( cleanDatabase );
}

void rocksdb_ah_storage_provider::shutdownDb( bool removeDB )
{
  rocksdb_storage_provider::shutdownDb( removeDB );
}

const std::atomic_uint& rocksdb_ah_storage_provider::get_cached_irreversible_block() const
{
  return _cached_irreversible_block;
}

unsigned int rocksdb_ah_storage_provider::get_cached_reindex_point() const
{
  return _cached_reindex_point;
}

uint64_t rocksdb_ah_storage_provider::get_operationSeqId() const
{
  return _operationSeqId;
}

void rocksdb_ah_storage_provider::set_operationSeqId( uint64_t value )
{
  _operationSeqId = value;
}

uint64_t rocksdb_ah_storage_provider::get_accountHistorySeqId() const
{
  return _accountHistorySeqId;
}

void rocksdb_ah_storage_provider::set_accountHistorySeqId( uint64_t value )
{
  _accountHistorySeqId = value;
}

unsigned int rocksdb_ah_storage_provider::get_collectedOps() const
{
  return _collectedOps;
}

void rocksdb_ah_storage_provider::set_collectedOps( unsigned int value )
{
  _collectedOps = value;
}

rocksdb_comment_storage_provider::rocksdb_comment_storage_provider( const bfs::path& blockchain_storage_path, const bfs::path& storage_path, appbase::application& app )
                                  : rocksdb_ah_storage_provider( blockchain_storage_path, storage_path, app )
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
