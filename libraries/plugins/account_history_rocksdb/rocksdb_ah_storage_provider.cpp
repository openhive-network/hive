
#include <hive/plugins/account_history_rocksdb/rocksdb_ah_storage_provider.hpp>

namespace hive { namespace chain {

rocksdb_ah_storage_provider::rocksdb_ah_storage_provider( const bfs::path& blockchain_storage_path, const bfs::path& storage_path, appbase::application& app )
                : rocksdb_storage_provider( blockchain_storage_path, storage_path, app ), _writeBuffer( getStorage(), _columnHandles )
{
  _cached_irreversible_block.store(0);
  _cached_reindex_point = 0;
}

void rocksdb_ah_storage_provider::storeSequenceIds()
{
  Slice ahSeqIdName("AH_SEQ_ID");

  hive::chain::id_slice_t ahId( get_accountHistorySeqId() );

  auto s = getWriteBuffer().Put(ahSeqIdName, ahId);
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
  auto s = getWriteBuffer().Put( _columnHandles[Columns::CURRENT_LIB], LIB_ID, lib_slice_t( lib ) );
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

WriteBatch& rocksdb_ah_storage_provider::getWriteBuffer()
{
  return _writeBuffer;
}

void rocksdb_ah_storage_provider::update_reindex_point( uint32_t rp )
{
  ilog( "RocksDB reindex point set to ${p}.", ( "p", rp ) );
  _cached_reindex_point = rp;
  auto s = getWriteBuffer().Put( _columnHandles[Columns::LAST_REINDEX_POINT], REINDEX_POINT_ID, lib_slice_t( rp ) );
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

ColumnFamilyHandle* rocksdb_ah_storage_provider::getColumnHandle( Columns column )
{
  FC_ASSERT( column < _columnHandles.size() );
  return _columnHandles[ column ];
}

CachableWriteBatch& rocksdb_ah_storage_provider::getCachableWriteBuffer()
{
  return _writeBuffer;
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

}}
