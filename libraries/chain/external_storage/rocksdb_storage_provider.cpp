
#include <hive/chain/external_storage/rocksdb_storage_provider.hpp>

#include <hive/chain/external_storage/comment_rocksdb_objects.hpp>
#include <hive/chain/external_storage/utilities.hpp>
#include <hive/chain/external_storage/types.hpp>

#include <appbase/application.hpp>

namespace hive { namespace chain {

rocksdb_storage_provider::rocksdb_storage_provider( const bfs::path& blockchain_storage_path, const bfs::path& storage_path, appbase::application& app, const std::string& name )
  : _storagePath( storage_path ), _blockchainStoragePath( blockchain_storage_path ), theApp( app ), name( name )
{
  _cached_irreversible_block.store(0);

  ilog("Register comparators in `${name}` RocksDB database.", ("name", name) );
  registerHiveComparators();

  if( !bfs::exists( _storagePath ) )
    bfs::create_directories( _storagePath );
}

void rocksdb_storage_provider::openDb( uint32_t expected_lib )
{
  ilog("Open `${name}` RocksDB database at location: `${p}'.", ("name", name)("p", _storagePath.string()));
  _cached_irreversible_block.store( expected_lib );

  //Very rare case -  when a synchronization starts from the scratch and a node has AH plugin with rocksdb enabled and directories don't exist yet
  bfs::create_directories( _blockchainStoragePath );

  auto _result = createDbSchema(_storagePath);
  if( !std::get<1>( _result ) )
  {
    ilog("Exit because of errors during database schema creation.");
    return;
  }

  ilog("Prepare column definitions." );
  auto columnDefs = prepareColumnDefinitions(true);

  DB* storageDb = nullptr;
  auto strPath = _storagePath.string();
  Options options;
  /// Optimize RocksDB. This is the easiest way to get RocksDB to perform well
  options.IncreaseParallelism();
  options.OptimizeLevelStyleCompaction();
  options.max_open_files = OPEN_FILE_LIMIT;

  auto status = DB::Open( DBOptions( options ), strPath, columnDefs, &_columnHandles, &storageDb );
  ilog( "Database is ${status}.", ("status", status.ok() ? "opened" : "not opened") );

  if( status.ok() )
  {
    ilog("Verify store version.");
    verifyStoreVersion( storageDb );

    getStorage().reset( storageDb );

    ilog("Load additional data.");
    loadAdditionalData();
  }
  else
  {
    elog("RocksDB cannot open `${name}` database at location: `${p}'.\nReturned error: ${e}",
      ("name", name)("p", strPath)("e", status.ToString()));
    FC_ASSERT( false && "Opening database failed." );
  }

  ilog("`${name}` RocksDB database opened successfully storage at location: `${p}'.", ("name", name)("p", strPath));

  _initialized = true;
}

void rocksdb_storage_provider::shutdownDb()
{
  if(getStorage())
  {
    ilog("Shutdown `${name}` RocksDB database.", ("name", name));

    ilog("Flush database.");
    flushDb();

    ilog("Cleanup column handles.");
    cleanupColumnHandles();

    ilog("Close database.");
    getStorage()->Close();
    getStorage().reset();

    ilog("`${name}` RocksDB database has been shutdowned.", ("name", name));
  }
  _cached_irreversible_block.store( 0 );
}

void rocksdb_storage_provider::wipeDb()
{
  ilog( "Destroy `${name}` RocksDB database", ("name", name) );
  ::rocksdb::DestroyDB( _storagePath.string(), ::rocksdb::Options() );
  ilog( "`${name}` RocksDB database has been destroyed at location: ${p}.", ("name", name)( "p", _storagePath.string() ) );
}

std::tuple<bool, bool> rocksdb_storage_provider::createDbSchema( const bfs::path& path )
{
  struct db_wrapper
  {
    DB* db = nullptr;

    ~db_wrapper()
    {
      if( db )
      {
        ilog( "Close database." );

        db->Close();
        delete db;
      }
    }
  };

  auto strPath = path.string();

  Options options;
  /// Optimize RocksDB. This is the easiest way to get RocksDB to perform well
  options.IncreaseParallelism();
  options.OptimizeLevelStyleCompaction();
  options.max_open_files = OPEN_FILE_LIMIT;

  std::tuple<bool, bool> _result{ false, false };

  ilog( "Create schema in `${name}` RocksDB database.", ("name", name) );

  auto open_read_only_db = [&strPath, &options, this]( std::tuple<bool, bool>& result )
  {
    auto _columnDefs = prepareColumnDefinitions(true);

    {
      ilog( "Open database in read only mode." );

      db_wrapper _db;
      auto _status = DB::OpenForReadOnly( options, strPath, _columnDefs, &_columnHandles, &_db.db );

      ilog( "Database is ${status}.", ("status", _status.ok() ? "opened" : "not opened") );
      if( _status.ok() )
      {
        ilog( "Cleanup column handles." );
        cleanupColumnHandles( _db.db );

        result = std::make_tuple( false, true );/// { DB does not need data import, an application is not closed }
      }
      return _status.ok();
    }
  };

  auto open_db = [&strPath, &options, this]( std::tuple<bool, bool>& result )
  {
    ilog( "Open database." );

    options.create_if_missing = true;

    db_wrapper _db;
    auto _status = DB::Open( options, strPath, &_db.db );
    ilog( "Database is ${status}.", ("status", _status.ok() ? "opened" : "not opened") );

    if( _status.ok() )
    {
      ilog( "Prepare column definitions." );
      auto _columnDefs = prepareColumnDefinitions(false);

      ilog( "Create column families." );
      _status = _db.db->CreateColumnFamilies( _columnDefs, &_columnHandles );

      if( _status.ok() )
      {
        ilog( "Save store version." );
        saveStoreVersion();

        ilog( "Flush database." );
        flushDb( _db.db );

        ilog( "Cleanup column handles." );
        cleanupColumnHandles( _db.db );
      }
      else
      {
        elog("RocksDB cannot create column definitions at location: `${p}'.\nReturned error: ${e}",
          ("p", strPath)("e", _status.ToString()));
        FC_ASSERT( false && "Creation of columns failed" );
      }

      result = std::make_tuple( true, true );/// { DB needs data import, an application is not closed }

      return true;
    }
    else
    {
      elog("RocksDB can not create storage at location: `${p}'.\nReturned error: ${e}",
        ("p", strPath)("e", _status.ToString()));
      FC_ASSERT( false && "Creation of database failed" );
    }
  };

  if( open_read_only_db( _result ) )
    return _result;

  open_db( _result );

  ilog( "Schema in `${name}` RocksDB database has been created successfully.", ("name", name) );

  return _result;/// { DB does not need data import, an application is closed }
}

void rocksdb_storage_provider::cleanupColumnHandles()
{
  if( getStorage() )
    cleanupColumnHandles( getStorage().get() );
}

void rocksdb_storage_provider::cleanupColumnHandles( DB* storageDB )
{
  for( auto* h : _columnHandles )
  {
    auto s = storageDB->DestroyColumnFamilyHandle(h);
    if(s.ok() == false)
    {
      elog("Cannot destroy column family handle in `${name}` RocksDB database. Error: `${e}`", ("name", name)("e", s.ToString()));
      FC_ASSERT( false && "Cleaning up database definition failed" );
    }
  }
  _columnHandles.clear();
}

void rocksdb_storage_provider::flushWriteBuffer( DB* storageDB )
{
  if( storageDB == nullptr )
    storageDB = getStorage().get();

  ::rocksdb::WriteOptions wOptions;
  auto s = storageDB->Write(wOptions, getWriteBuffer().GetWriteBatch());
  checkStatus(s);

  getWriteBuffer().Clear();
}

void rocksdb_storage_provider::flushDb( DB* storageDb )
{
  FC_ASSERT( storageDb != nullptr && "Database pointer is null" );

  flushWriteBuffer( storageDb );

  ::rocksdb::FlushOptions fOptions;
  for( const auto& cf : _columnHandles )
  {
    auto s = storageDb->Flush( fOptions, cf );
    checkStatus(s);
  }
}

void rocksdb_storage_provider::flushDb()
{
  if( getStorage() == nullptr )
    return;

  flushDb( getStorage().get() );
}

void rocksdb_storage_provider::saveStoreVersion()
{
  auto s = getWriteBuffer().Put(Slice("STORE_MAJOR_VERSION"), PrimitiveTypeSlice<uint32_t>( STORE_MAJOR_VERSION ) );
  checkStatus(s);

  s = getWriteBuffer().Put(Slice("STORE_MINOR_VERSION"), PrimitiveTypeSlice<uint32_t>( STORE_MINOR_VERSION ) );
  checkStatus(s);
}

void rocksdb_storage_provider::verifyStoreVersion( DB* storageDb )
{
  auto _verifier =[ &storageDb ]( const std::string & key, uint32_t expected_value )
  {
    ReadOptions rOptions;

    std::string _buffer;
    auto s = storageDb->Get(rOptions, key, &_buffer);
    checkStatus(s);
    const auto _value = PrimitiveTypeSlice<uint32_t>::unpackSlice(_buffer);

    FC_ASSERT(_value == expected_value, "Store version mismatch for key `${k}`: expected ${e}, found ${f}",
      ("k", key)("e", expected_value)("f", _value));
  };

  _verifier( "STORE_MAJOR_VERSION", STORE_MAJOR_VERSION );
  _verifier( "STORE_MINOR_VERSION", STORE_MINOR_VERSION );
}

void rocksdb_storage_provider::save( ColumnTypes column_type, const Slice& key, const Slice& value )
{
  auto s = getWriteBuffer().Put( getColumnHandle(column_type), key, value );
  checkStatus(s);
}

bool rocksdb_storage_provider::read( ColumnTypes column_type, const Slice& key, PinnableSlice& value )
{
  ReadOptions rOptions;

  ::rocksdb::Status s = getStorage()->Get( rOptions, getColumnHandle(column_type), key, &value );
  return s.ok();
}

void rocksdb_storage_provider::remove( ColumnTypes column_type, const Slice& key )
{
  auto s = getWriteBuffer().Delete( getColumnHandle(column_type), key );
  checkStatus(s);
}

void rocksdb_storage_provider::put_entity( ColumnTypes column_type, const Slice& key, const WideColumns& wide_columns )
{
  auto s = getWriteBuffer().PutEntity( getColumnHandle(column_type), key, wide_columns );
  checkStatus(s);
}

bool rocksdb_storage_provider::get_entity( ColumnTypes column_type, const Slice& key, PinnableWideColumns& wide_columns )
{
  auto s = getStorage()->GetEntity( ReadOptions(), getColumnHandle(column_type), key, &wide_columns );
  return s.ok();
}

ColumnFamilyHandle* rocksdb_storage_provider::getColumnHandle( ColumnTypes column_type )
{
  FC_ASSERT( column_type < _columnHandles.size() );
  return _columnHandles[column_type];
}

void rocksdb_storage_provider::load_lib()
{
  std::string _value;
  auto s = getStorage()->Get(ReadOptions(), _columnHandles[Columns::CURRENT_LIB], LIB_ID, &_value );

  if( s.code() == ::rocksdb::Status::kNotFound )
  {
    ilog( "RocksDB LIB not present in `${name}` RocksDB database.", ("name", name) );
    FC_ASSERT( 0 == _cached_irreversible_block, "Inconsistency in last irreversible block - expected ${c}",
      ( "c", static_cast<uint32_t>( _cached_irreversible_block ) ) );
    update_lib_internal( 0 ); ilog( "RocksDB LIB set to 0." );
    return;
  }

  FC_ASSERT( s.ok() && "Not found irreversible", "Could not find last irreversible block. Error msg: `${e}'", ("e", s.ToString()) );

  uint32_t lib = lib_slice_t::unpackSlice( _value );

  FC_ASSERT( lib == _cached_irreversible_block, "Inconsistency in last irreversible block - expected ${c}, stored ${s}",
    ( "c", static_cast< uint32_t >( _cached_irreversible_block ) )( "s", lib ) );

  ilog( "`${name}` RocksDB database LIB loaded with value ${l}.", ("name", name)( "l", lib ) );
}

void rocksdb_storage_provider::update_lib_internal( uint32_t lib )
{
  _cached_irreversible_block.store( lib );
  auto s = getWriteBuffer().Put( _columnHandles[Columns::CURRENT_LIB], LIB_ID, lib_slice_t( lib ) );
  checkStatus( s );
}

void rocksdb_storage_provider::update_lib( uint32_t lib )
{
  /*
    We need to have certainity that DB has been initialized before we allow updating LIB.
  */
  FC_ASSERT( _initialized, "Trying to update LIB in `${name}` RocksDB database before initialization.", ("name", name) );
  update_lib_internal( lib );
}

void rocksdb_storage_provider::loadAdditionalData()
{
  loadSeqIdentifiers( getStorage().get() );
  load_lib();
}

}}
