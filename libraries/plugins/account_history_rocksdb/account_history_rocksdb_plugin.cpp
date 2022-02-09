
#include <hive/chain/hive_fwd.hpp>

#include <hive/plugins/account_history_rocksdb/account_history_rocksdb_plugin.hpp>

#include <hive/chain/database.hpp>
#include <hive/chain/history_object.hpp>
#include <hive/chain/hive_objects.hpp>
#include <hive/chain/index.hpp>
#include <hive/chain/util/impacted.hpp>
#include <hive/chain/util/supplement_operations.hpp>

#include <hive/plugins/chain/chain_plugin.hpp>
#include <hive/plugins/chain/state_snapshot_provider.hpp>

#include <hive/utilities/benchmark_dumper.hpp>
#include <hive/utilities/plugin_utilities.hpp>

#include <appbase/application.hpp>

#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/slice.h>
#include <rocksdb/utilities/backupable_db.h>
#include <rocksdb/utilities/write_batch_with_index.h>

#include <boost/type.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/container/flat_set.hpp>

#include <condition_variable>
#include <mutex>

#include <limits>
#include <string>
#include <typeindex>
#include <typeinfo>

namespace bpo = boost::program_options;

#define HIVE_NAMESPACE_PREFIX "hive::protocol::"
#define OPEN_FILE_LIMIT 750

#define DIAGNOSTIC(s)
//#define DIAGNOSTIC(s) s

enum Columns
{
  CURRENT_LIB = 1,
  LAST_REINDEX_POINT = CURRENT_LIB,
  OPERATION_BY_ID,
  OPERATION_BY_BLOCK,
  AH_INFO_BY_NAME,
  AH_OPERATION_BY_ID,
  BY_TRANSACTION_ID
};

#define WRITE_BUFFER_FLUSH_LIMIT     10
#define ACCOUNT_HISTORY_LENGTH_LIMIT 30
#define ACCOUNT_HISTORY_TIME_LIMIT   30
#define VIRTUAL_OP_FLAG              0x8000000000000000

/** Because localtion_id_pair stores block_number paired with (VIRTUAL_OP_FLAG|operation_id),
  *  max allowed operation-id is max_int (instead of max_uint).
  */
#define MAX_OPERATION_ID             std::numeric_limits<int64_t>::max()

#define STORE_MAJOR_VERSION          1
#define STORE_MINOR_VERSION          0

namespace hive { namespace plugins { namespace account_history_rocksdb {

using hive::protocol::account_name_type;
using hive::protocol::block_id_type;
using hive::protocol::operation;
using hive::protocol::signed_block;
using hive::protocol::signed_block_header;
using hive::protocol::signed_transaction;

using hive::chain::operation_notification;
using hive::chain::transaction_id_type;

using hive::protocol::legacy_asset;
using hive::utilities::benchmark_dumper;

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

/** Represents an AH entry in mapped to account name.
  *  Holds additional informations, which are needed to simplify pruning process.
  *  All operations specific to given account, are next mapped to ID of given object.
  */
class account_history_info
{
public:
  int64_t        id = 0;
  uint32_t       oldestEntryId = 0;
  uint32_t       newestEntryId = 0;
  /// Timestamp of oldest operation, just to quickly decide if start detail prune checking at all.
  time_point_sec oldestEntryTimestamp;

  uint32_t getAssociatedOpCount() const
  {
    return newestEntryId - oldestEntryId + 1;
  }
};

namespace
{
template <class T>
serialize_buffer_t dump(const T& obj)
  {
  serialize_buffer_t serializedObj;
  auto size = fc::raw::pack_size(obj);
  serializedObj.resize(size);
  fc::datastream<char*> ds(serializedObj.data(), size);
  fc::raw::pack(ds, obj);
  return serializedObj;
  }

template <class T>
void load(T& obj, const char* data, size_t size)
  {
  fc::datastream<const char*> ds(data, size);
  fc::raw::unpack(ds, obj);
  }

template <class T>
void load(T& obj, const serialize_buffer_t& source)
  {
  load(obj, source.data(), source.size());
  }

/** Helper class to simplify construction of Slice objects holding primitive type values.
  *
  */
template <typename T>
class PrimitiveTypeSlice final : public Slice
  {
  public:
    explicit PrimitiveTypeSlice(T value) : _value(value)
    {
    data_ = reinterpret_cast<const char*>(&_value);
    size_ = sizeof(T);
    }

    static const T& unpackSlice(const Slice& s)
    {
    assert(sizeof(T) == s.size());
    return *reinterpret_cast<const T*>(s.data());
    }

    static const T& unpackSlice(const std::string& s)
    {
    assert(sizeof(T) == s.size());
    return *reinterpret_cast<const T*>(s.data());
    }

  private:
    T _value;
  };

class TransactionIdSlice : public Slice
  {
  public:
    explicit TransactionIdSlice(const transaction_id_type& trxId) : _trxId(&trxId)
    {
    data_ = _trxId->data();
    size_ = _trxId->data_size();
    }

    static void unpackSlice(const Slice& s, transaction_id_type* storage)
    {
    assert(storage != nullptr);
    assert(storage->data_size() == s.size());
    memcpy(storage->data(), s.data(), s.size());
    }

  private:
    const transaction_id_type* _trxId;
  };

/** Helper base class to cover all common functionality across defined comparators.
  *
  */
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

  /// Pairs account_name storage type and the ID to make possible nonunique index definition over names.
typedef std::pair<account_name_type::Storage, size_t> account_name_storage_id_pair;

template <typename T>
class PrimitiveTypeComparatorImpl final : public AComparator
  {
  public:
    virtual int Compare(const Slice& a, const Slice& b) const override
    {
    if(a.size() != sizeof(T) || b.size() != sizeof(T))
      return a.compare(b);

    const T& id1 = retrieveKey(a);
    const T& id2 = retrieveKey(b);

    if(id1 < id2)
      return -1;

    if(id1 > id2)
      return 1;

    return 0;
    }

    virtual bool Equal(const Slice& a, const Slice& b) const override
    {
    if(a.size() != sizeof(T) || b.size() != sizeof(T))
      return a == b;

    const auto& id1 = retrieveKey(a);
    const auto& id2 = retrieveKey(b);

    return id1 == id2;
    }

  private:
    const T& retrieveKey(const Slice& slice) const
    {
    assert(sizeof(T) == slice.size());
    const char* rawData = slice.data();
    return *reinterpret_cast<const T*>(rawData);
    }
  };

typedef PrimitiveTypeComparatorImpl<uint32_t> by_id_ComparatorImpl;

typedef PrimitiveTypeComparatorImpl<account_name_type::Storage> by_account_name_ComparatorImpl;

typedef PrimitiveTypeSlice< uint32_t > lib_id_slice_t;
typedef PrimitiveTypeSlice< uint32_t > lib_slice_t;

#define LIB_ID lib_id_slice_t( 0 )
#define REINDEX_POINT_ID lib_id_slice_t( 1 )

/** Location index is nonunique. Since RocksDB supports only unique indexes, it must be extended
  *  by some unique part (ie ID).
  *
  */
typedef std::pair< uint32_t, uint64_t > block_op_id_pair;
typedef PrimitiveTypeComparatorImpl< block_op_id_pair > op_by_block_num_ComparatorImpl;

/// Compares account_history_info::id and rocksdb_operation_object::id pair
typedef std::pair< int64_t, uint32_t > ah_op_id_pair;
typedef PrimitiveTypeComparatorImpl< ah_op_id_pair > ah_op_by_id_ComparatorImpl;

typedef PrimitiveTypeSlice< int64_t > id_slice_t;
typedef PrimitiveTypeSlice< block_op_id_pair > op_by_block_num_slice_t;
typedef PrimitiveTypeSlice< uint32_t > by_block_slice_t;
typedef PrimitiveTypeSlice< account_name_type::Storage > ah_info_by_name_slice_t;
typedef PrimitiveTypeSlice< ah_op_id_pair > ah_op_by_id_slice_t;


class TransactionIdComparator final : public AComparator
  {
  public:
    virtual int Compare(const Slice& a, const Slice& b) const override
    {
    /// Nothing more to do. Just compare buffers holding 20Bytes hash
    return a.compare(b);
    }

    virtual bool Equal(const Slice& a, const Slice& b) const override
    {
    return a == b;
    }
  };

typedef std::pair<uint32_t, uint32_t> block_no_tx_in_block_pair;
typedef PrimitiveTypeSlice<block_no_tx_in_block_pair> block_no_tx_in_block_slice_t;

const Comparator* by_id_Comparator()
{
  static by_id_ComparatorImpl c;
  return &c;
}

const Comparator* op_by_block_num_Comparator()
{
  static op_by_block_num_ComparatorImpl c;
  return &c;
}

const Comparator* by_account_name_Comparator()
{
  static by_account_name_ComparatorImpl c;
  return &c;
}

const Comparator* ah_op_by_id_Comparator()
{
  static ah_op_by_id_ComparatorImpl c;
  return &c;
}

const Comparator* by_txId_Comparator()
  {
  static TransactionIdComparator c;
  return &c;
  }

#define checkStatus(s) FC_ASSERT((s).ok(), "Data access failed: ${m}", ("m", (s).ToString()))

class operation_name_provider
{
public:
  typedef void result_type;

  static std::string getName(const operation& op)
  {
    operation_name_provider provider;
    op.visit(provider);
    return provider._name;
  }

  template<typename Op>
  void operator()( const Op& ) const
  {
    _name = fc::get_typename<Op>::name();
  }

private:
  operation_name_provider() = default;

/// Class attributes:
private:
  mutable std::string _name;
};

class CachableWriteBatch : public WriteBatch
{
public:
  CachableWriteBatch(const std::unique_ptr<DB>& storage, const std::vector<ColumnFamilyHandle*>& columnHandles) :
    _storage(storage), _columnHandles(columnHandles) {}

  bool getAHInfo(const account_name_type& name, account_history_info* ahInfo) const
  {
    auto fi = _ahInfoCache.find(name);
    if(fi != _ahInfoCache.end())
    {
      *ahInfo = fi->second;
      return true;
    }

    ah_info_by_name_slice_t key(name.data);
    PinnableSlice buffer;
    auto s = _storage->Get(ReadOptions(), _columnHandles[Columns::AH_INFO_BY_NAME], key, &buffer);
    if(s.ok())
    {
      load(*ahInfo, buffer.data(), buffer.size());
      return true;
    }

    FC_ASSERT(s.IsNotFound());
    return false;
  }

  void putAHInfo(const account_name_type& name, const account_history_info& ahInfo)
  {
    _ahInfoCache[name] = ahInfo;
    auto serializeBuf = dump(ahInfo);
    ah_info_by_name_slice_t nameSlice(name.data);
    auto s = Put(_columnHandles[Columns::AH_INFO_BY_NAME], nameSlice, Slice(serializeBuf.data(), serializeBuf.size()));
    checkStatus(s);
  }

  void Clear()
  {
    _ahInfoCache.clear();
    WriteBatch::Clear();
  }

private:
  const std::unique_ptr<DB>&                        _storage;
  const std::vector<ColumnFamilyHandle*>&           _columnHandles;
  std::map<account_name_type, account_history_info> _ahInfoCache;
};

} /// anonymous

class account_history_rocksdb_plugin::impl final
{
public:
  impl( account_history_rocksdb_plugin& self, const bpo::variables_map& options, const bfs::path& storagePath) :
    _self(self),
    _mainDb(appbase::app().get_plugin<hive::plugins::chain::chain_plugin>().db()),
    _blockchainStoragePath(appbase::app().get_plugin<hive::plugins::chain::chain_plugin>().state_storage_dir()),
    _storagePath(storagePath),
    _writeBuffer(_storage, _columnHandles)
    {
    collectOptions(options);

    _mainDb.add_pre_reindex_handler([&]( const hive::chain::reindex_notification& note ) -> void
      {
        on_pre_reindex( note );
      }, _self, 0);

    _mainDb.add_post_reindex_handler([&]( const hive::chain::reindex_notification& note ) -> void
      {
        on_post_reindex( note );
      }, _self, 0);

    _mainDb.add_snapshot_supplement_handler([&](const hive::chain::prepare_snapshot_supplement_notification& note) -> void
      {
        supplement_snapshot(note);
      }, _self, 0);

    _mainDb.add_snapshot_supplement_handler([&](const hive::chain::load_snapshot_supplement_notification& note) -> void
      {
        load_additional_data_from_snapshot(note);
      }, _self, 0);

    _on_pre_apply_operation_con = _mainDb.add_pre_apply_operation_handler(
      [&]( const operation_notification& note )
      {
        on_pre_apply_operation(note);
      },
      _self
    );

    _on_irreversible_block_conn = _mainDb.add_irreversible_block_handler(
      [&]( uint32_t block_num )
      {
        on_irreversible_block( block_num );
      },
      _self
    );

    _on_pre_apply_block_conn = _mainDb.add_pre_apply_block_handler(
      [&](const block_notification& bn)
      {
        on_pre_apply_block(bn);
      },
      _self
    );

    _on_post_apply_block_conn = _mainDb.add_post_apply_block_handler(
      [&](const block_notification& bn)
      {
        on_post_apply_block(bn);
      },
      _self
    );

    _on_fail_apply_block_conn = _mainDb.add_fail_apply_block_handler(
      [&](const block_notification& bn)
      {
        on_post_apply_block(bn);
      },
      _self
    );

    is_processed_block_mtx_locked.store(false);
    _cached_irreversible_block.store(0);
    _currently_processed_block.store(0);
    _cached_reindex_point = 0;

    HIVE_ADD_PLUGIN_INDEX(_mainDb, volatile_operation_index);
    }

  ~impl()
  {

    chain::util::disconnect_signal(_on_pre_apply_operation_con);
    chain::util::disconnect_signal(_on_irreversible_block_conn);
    chain::util::disconnect_signal(_on_pre_apply_block_conn);
    chain::util::disconnect_signal(_on_post_apply_block_conn);
    chain::util::disconnect_signal(_on_fail_apply_block_conn);
    shutdownDb();
  }

  void openDb()
  {
    //Very rare case -  when a synchronization starts from the scratch and a node has AH plugin with rocksdb enabled and directories don't exist yet
    bfs::create_directories( _blockchainStoragePath );

    createDbSchema(_storagePath);

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
      loadSeqIdentifiers(storageDb);
      _storage.reset(storageDb);

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
    else
    {
      elog("RocksDB cannot open database at location: `${p}'.\nReturned error: ${e}",
        ("p", strPath)("e", status.ToString()));
    }
  }

  void printReport(uint32_t blockNo, const char* detailText) const;
  void on_pre_reindex( const hive::chain::reindex_notification& note );
  void on_post_reindex( const hive::chain::reindex_notification& note );

  /// Allows to start immediate data import (outside replay process).
  void importData(unsigned int blockLimit);

  void find_account_history_data(const account_name_type& name, uint64_t start, uint32_t limit, bool include_reversible,
    std::function<bool(unsigned int, const rocksdb_operation_object&)> processor) const;
  bool find_operation_object(size_t opId, rocksdb_operation_object* op) const;
  /// Allows to look for all operations present in given block and call `processor` for them.
  void find_operations_by_block(size_t blockNum, bool include_reversible,
    std::function<void(const rocksdb_operation_object&)> processor) const;
  /// Allows to enumerate all operations registered in given block range.
  std::pair< uint32_t/*nr last block*/, uint64_t/*operation-id to resume from*/ > enumVirtualOperationsFromBlockRange(
    uint32_t blockRangeBegin, uint32_t blockRangeEnd, bool include_reversible,
    fc::optional<uint64_t> operationBegin, fc::optional<uint32_t> limit,
    std::function<bool(const rocksdb_operation_object&, uint64_t, bool)> processor) const;

  bool find_transaction_info(const protocol::transaction_id_type& trxId, bool include_reversible, uint32_t* blockNo,
    uint32_t* txInBlock) const;

  void shutdownDb()
  {
    if(_storage)
    {
      flushStorage();
      cleanupColumnHandles();
      _storage->Close();
      _storage.reset();
    }
  }

private:
  void supplement_snapshot(const hive::chain::prepare_snapshot_supplement_notification& note);
  void load_additional_data_from_snapshot(const hive::chain::load_snapshot_supplement_notification& note);

  //loads last irreversible block from DB to _cached_irreversible_block
  void load_lib();
  //stores new value of last irreversible block in DB and _cached_irreversible_block
  void update_lib( uint32_t );
  //loads reindex point from DB to _cached_reindex_point
  void load_reindex_point();
  //stores new value of reindex point in DB and _cached_reindex_point
  void update_reindex_point( uint32_t );

  typedef std::vector<ColumnFamilyDescriptor> ColumnDefinitions;
  ColumnDefinitions prepareColumnDefinitions(bool addDefaultColumn);

  /// Returns true if database will need data import.
  bool createDbSchema(const bfs::path& path);

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

  template< typename T >
  void importOperation( rocksdb_operation_object& obj, const T& impacted )
  {
    if(_lastTx != obj.trx_id)
    {
      ++_txNo;
      _lastTx = obj.trx_id;
      storeTransactionInfo(obj.trx_id, obj.block, obj.trx_in_block);
    }

    obj.id = _operationSeqId++;

    serialize_buffer_t serializedObj;
    auto size = fc::raw::pack_size(obj);
    serializedObj.resize(size);
    {
      fc::datastream<char*> ds(serializedObj.data(), size);
      fc::raw::pack(ds, obj);
    }

    id_slice_t idSlice(obj.id);
    auto s = _writeBuffer.Put(_columnHandles[Columns::OPERATION_BY_ID], idSlice, Slice(serializedObj.data(), serializedObj.size()));
    checkStatus(s);

    // uint64_t location = ( (uint64_t) obj.trx_in_block << 32 ) | ( (uint64_t) obj.op_in_trx << 16 ) | ( obj.virtual_op );

    //if obj is a virtual operation, encode this fact into top bit of id to speed up queries that need to distinguish ops vs virtual ops
    uint64_t encoded_id = obj.virtual_op ? VIRTUAL_OP_FLAG | obj.id : obj.id;

    op_by_block_num_slice_t blockLocSlice(block_op_id_pair(obj.block, encoded_id));

    s = _writeBuffer.Put(_columnHandles[Columns::OPERATION_BY_BLOCK], blockLocSlice, idSlice);
    checkStatus(s);

    for(const auto& name : impacted)
      buildAccountHistoryRecord( name, obj );

    if(++_collectedOps >= _collectedOpsWriteLimit)
      flushWriteBuffer();

    ++_totalOps;
  }

  void buildAccountHistoryRecord( const account_name_type& name, const rocksdb_operation_object& obj );
  void storeTransactionInfo(const chain::transaction_id_type& trx_id, uint32_t blockNo, uint32_t trx_in_block);

  void prunePotentiallyTooOldItems(account_history_info* ahInfo, const account_name_type& name,
    const fc::time_point_sec& now);

  void saveStoreVersion()
  {
    PrimitiveTypeSlice<uint32_t> majorVSlice(STORE_MAJOR_VERSION);
    PrimitiveTypeSlice<uint32_t> minorVSlice(STORE_MINOR_VERSION);

    auto s = _writeBuffer.Put(Slice("STORE_MAJOR_VERSION"), majorVSlice);
    checkStatus(s);
    s = _writeBuffer.Put(Slice("STORE_MINOR_VERSION"), minorVSlice);
    checkStatus(s);
  }

  void verifyStoreVersion(DB* storageDb)
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

  void storeSequenceIds()
  {
    Slice opSeqIdName("OPERATION_SEQ_ID");
    Slice ahSeqIdName("AH_SEQ_ID");

    id_slice_t opId(_operationSeqId);
    id_slice_t ahId(_accountHistorySeqId);

    auto s = _writeBuffer.Put(opSeqIdName, opId);
    checkStatus(s);
    s = _writeBuffer.Put(ahSeqIdName, ahId);
    checkStatus(s);
  }

  void loadSeqIdentifiers(DB* storageDb)
  {
    Slice opSeqIdName("OPERATION_SEQ_ID");
    Slice ahSeqIdName("AH_SEQ_ID");

    ReadOptions rOptions;

    std::string buffer;
    auto s = storageDb->Get(rOptions, opSeqIdName, &buffer);
    checkStatus(s);
    _operationSeqId = id_slice_t::unpackSlice(buffer);

    s = storageDb->Get(rOptions, ahSeqIdName, &buffer);
    checkStatus(s);
    _accountHistorySeqId = id_slice_t::unpackSlice(buffer);

    ilog("Loaded OperationObject seqId: ${o}, AccountHistoryObject seqId: ${ah}.",
      ("o", _operationSeqId)("ah", _accountHistorySeqId));
  }

  void flushWriteBuffer(DB* storage = nullptr)
  {
    storeSequenceIds();

    if(storage == nullptr)
      storage = _storage.get();

    ::rocksdb::WriteOptions wOptions;
    auto s = storage->Write(wOptions, _writeBuffer.GetWriteBatch());
    checkStatus(s);
    _writeBuffer.Clear();
    _collectedOps = 0;
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

  void on_pre_apply_operation(const operation_notification& opNote);

  void on_irreversible_block( uint32_t block_num );

  void on_post_apply_block(const block_notification& bn);
  void on_pre_apply_block(const  block_notification& bn);

  void collectOptions(const bpo::variables_map& options);

  /** Returns true if given account is tracked.
    *  Depends on `"account-history-whitelist-ops"`, `account-history-blacklist-ops` option usage.
    *  Only some accounts can be chosen for tracking operation history.
    */
  bool isTrackedAccount(const account_name_type& name) const;

  /** Returns a collection of the accounts being impacted by given `op` and
    *  also tracked, because of passed options.
    *
    *  \see isTrackedAccount.
    */
  std::vector<account_name_type> getImpactedAccounts(const operation& op) const;

  /** Returns true if given operation should be collected.
    *  Depends on `account-history-blacklist-ops`, `account-history-whitelist-ops`.
    */
  bool isTrackedOperation(const operation& op) const;

  void storeOpFilteringParameters(const std::vector<std::string>& opList,
    flat_set<std::string>* storage) const;

  std::vector<rocksdb_operation_object> collectReversibleOps(uint32_t* blockRangeBegin, uint32_t* blockRangeEnd,
    uint32_t* collectedIrreversibleBlock) const;

/// Class attributes:
private:
  typedef flat_map< account_name_type, account_name_type > account_name_range_index;

  account_history_rocksdb_plugin&  _self;
  chain::database&                 _mainDb;
  bfs::path                        _blockchainStoragePath;
  bfs::path                        _storagePath;
  std::unique_ptr<DB>              _storage;
  std::vector<ColumnFamilyHandle*> _columnHandles;
  CachableWriteBatch               _writeBuffer;

  boost::signals2::connection      _on_pre_apply_operation_con;
  boost::signals2::connection      _on_irreversible_block_conn;
  boost::signals2::connection      _on_pre_apply_block_conn;
  boost::signals2::connection      _on_post_apply_block_conn;
  boost::signals2::connection      _on_fail_apply_block_conn;

  /// Helper member to be able to detect another incomming tx and increment tx-counter.
  transaction_id_type              _lastTx;
  size_t                           _txNo = 0;
  /// Total processed ops in this session (counts every operation, even excluded by filtering).
  size_t                           _totalOps = 0;
  /// Total number of ops being skipped by filtering options.
  size_t                           _excludedOps = 0;
  /// Total number of accounts (impacted by ops) excluded from processing because of filtering.
  mutable size_t                   _excludedAccountCount = 0;
  /// IDs to be assigned to object.id field.
  uint64_t                         _operationSeqId = 0;
  uint64_t                         _accountHistorySeqId = 0;

  /// Number of data-chunks for ops being stored inside _writeBuffer. To decide when to flush.
  unsigned int                     _collectedOps = 0;
  /** Limit which value depends on block data source:
    *    - if blocks come from network, there is no need for delaying write, becasue they appear quite rare (limit == 1)
    *    - if reindex process or direct import has been spawned, this massive operation can need reduction of direct
        writes (limit == WRITE_BUFFER_FLUSH_LIMIT).
    */
  unsigned int                     _collectedOpsWriteLimit = 1;

  /// <summary>
  /// Information if mutex is locked/unlocked.
  /// </summary>
  std::atomic_bool   is_processed_block_mtx_locked;

  /// <summary>
  /// Last block of most recent reindex - all such blocks are in inreversible storage already, since
  /// the data is put there directly during reindex (it also means there is no volatile data for such
  /// blocks), but _cached_irreversible_block might point to earlier block because it reflects
  /// the state of dgpo. Once node starts syncing normally, the _cached_irreversible_block will catch up.
  /// </summary>
  unsigned int                     _cached_reindex_point = 0;

  /// <summary>
  /// Block being irreversible atm.
  /// </summary>
  std::atomic_uint                 _cached_irreversible_block;
  /// <summary>
  ///  Block currently processed by node
  /// </summary>
  std::atomic_uint                 _currently_processed_block;
  /// <summary>
  ///  Allows to hold API threads trying to read data coming from currently processed block until it will
  ///  finish its processing and data will be complete.
  /// </summary>
  mutable std::mutex               _currently_processed_block_mtx;

  mutable std::condition_variable  _currently_processed_block_cv;
  /// <summary>
  /// Controls MT access to the volatile_operation_index being cleared during `on_irreversible_block` handler execution.
  /// </summary>
  mutable std::mutex               _currently_persisted_irreversible_mtx;
  std::atomic_uint                 _currently_persisted_irreversible_block{0};
  mutable std::condition_variable  _currently_persisted_irreversible_cv;

  account_name_range_index         _tracked_accounts;
  flat_set<std::string>            _op_list;
  flat_set<std::string>            _blacklisted_op_list;

  bool                             _reindexing = false;

  bool                             _prune = false;

  struct saved_balances
  {
    asset hive_balance = asset(0, HIVE_SYMBOL);
    asset savings_hive_balance = asset(0, HIVE_SYMBOL);

    asset hbd_balance = asset(0, HBD_SYMBOL);
    asset savings_hbd_balance = asset(0, HBD_SYMBOL);

    asset vesting_shares = asset(0, VESTS_SYMBOL);
    asset delegated_vesting_shares = asset(0, VESTS_SYMBOL);
    asset received_vesting_shares = asset(0, VESTS_SYMBOL);

    asset reward_hbd_balance = asset(0, HBD_SYMBOL);
    asset reward_hive_balance = asset(0, HIVE_SYMBOL);
    asset reward_vesting_balance = asset(0, VESTS_SYMBOL);
    asset reward_vesting_hive_balance = asset(0, HIVE_SYMBOL);
  };
  optional<string>                 _balance_csv_filename;
  std::ofstream                    _balance_csv_file;
  std::map<string, saved_balances> _saved_balances;
  unsigned                         _balance_csv_line_count = 0;
};

void account_history_rocksdb_plugin::impl::collectOptions(const boost::program_options::variables_map& options)
{
  fc::mutable_variant_object state_opts;

  typedef std::pair< account_name_type, account_name_type > pairstring;
  HIVE_LOAD_VALUE_SET(options, "account-history-rocksdb-track-account-range", _tracked_accounts, pairstring);

  state_opts[ "account-history-rocksdb-track-account-range" ] = _tracked_accounts;

  if(options.count("account-history-rocksdb-whitelist-ops"))
  {
    const auto& args = options.at("account-history-rocksdb-whitelist-ops").as<std::vector<std::string>>();
    storeOpFilteringParameters(args, &_op_list);

    if( _op_list.size() )
    {
      state_opts["account-history-rocksdb-whitelist-ops"] = _op_list;
    }
  }

  if(_op_list.empty() == false)
    ilog( "Account History: whitelisting ops ${o}", ("o", _op_list) );

  if(options.count("account-history-rocksdb-blacklist-ops"))
  {
    const auto& args = options.at("account-history-rocksdb-blacklist-ops").as<std::vector<std::string>>();
    storeOpFilteringParameters(args, &_blacklisted_op_list);

    if( _blacklisted_op_list.size() )
    {
      state_opts["account-history-rocksdb-blacklist-ops"] = _blacklisted_op_list;
    }
  }

  if(_blacklisted_op_list.empty() == false)
    ilog( "Account History: blacklisting ops ${o}", ("o", _blacklisted_op_list) );

  if (options.count("account-history-rocksdb-dump-balance-history"))
  {
    _balance_csv_filename = options.at("account-history-rocksdb-dump-balance-history").as<std::string>();
    _balance_csv_file.open(*_balance_csv_filename, std::ios_base::out | std::ios_base::trunc);
    _balance_csv_file << "account" << ","
                      << "block_num" << ","
                      << "timestamp" << ","
                      << "hive" << ","
                      << "savings_hive" << ","
                      << "hbd" << ","
                      << "savings_hbd" << ","
                      << "vesting_shares" << ","
                      << "delegated_vesting_shares" << ","
                      << "received_vesting_shares" << ","
                      << "reward_hive" << ","
                      << "reward_hbd" << ","
                      << "reward_vesting_shares" << ","
                      << "reward_vesting_hive" << "\n";
    _balance_csv_file.flush();
  }

  appbase::app().get_plugin< chain::chain_plugin >().report_state_options( _self.name(), state_opts );
}

inline bool account_history_rocksdb_plugin::impl::isTrackedAccount(const account_name_type& name) const
{
  if(_tracked_accounts.empty())
    return true;

  /// Code below is based on original contents of account_history_plugin_impl::on_pre_apply_operation
  auto itr = _tracked_accounts.lower_bound(name);

  /*
    * The map containing the ranges uses the key as the lower bound and the value as the upper bound.
    * Because of this, if a value exists with the range (key, value], then calling lower_bound on
    * the map will return the key of the next pair. Under normal circumstances of those ranges not
    * intersecting, the value we are looking for will not be present in range that is returned via
    * lower_bound.
    *
    * Consider the following example using ranges ["a","c"], ["g","i"]
    * If we are looking for "bob", it should be tracked because it is in the lower bound.
    * However, lower_bound( "bob" ) returns an iterator to ["g","i"]. So we need to decrement the iterator
    * to get the correct range.
    *
    * If we are looking for "g", lower_bound( "g" ) will return ["g","i"], so we need to make sure we don't
    * decrement.
    *
    * If the iterator points to the end, we should check the previous (equivalent to rbegin)
    *
    * And finally if the iterator is at the beginning, we should not decrement it for obvious reasons
    */
  if(itr != _tracked_accounts.begin() &&
    ((itr != _tracked_accounts.end() && itr->first != name ) || itr == _tracked_accounts.end()))
  {
    --itr;
  }

  bool inRange = itr != _tracked_accounts.end() && itr->first <= name && name <= itr->second;

  _excludedAccountCount += inRange;

  DIAGNOSTIC(
    if(inRange)
      ilog("Account: ${a} matched to defined account range: [${b},${e}]", ("a", name)("b", itr->first)("e", itr->second) );
    else
      ilog("Account: ${a} ignored due to defined tracking filters.", ("a", name));
  );

  return inRange;
}

std::vector<account_name_type> account_history_rocksdb_plugin::impl::getImpactedAccounts(const operation& op) const
{
  flat_set<account_name_type> impacted;
  hive::app::operation_get_impacted_accounts(op, impacted);
  std::vector<account_name_type> retVal;

  if(impacted.empty())
    return retVal;

  if(_tracked_accounts.empty())
  {
    retVal.insert(retVal.end(), impacted.begin(), impacted.end());
    return retVal;
  }

  retVal.reserve(impacted.size());

  for(const auto& name : impacted)
  {
    if(isTrackedAccount(name))
      retVal.push_back(name);
  }

  return retVal;
}

inline bool account_history_rocksdb_plugin::impl::isTrackedOperation(const operation& op) const
{
  if(_op_list.empty() && _blacklisted_op_list.empty())
    return true;

  auto opName = operation_name_provider::getName(op);

  if(_blacklisted_op_list.find(opName) != _blacklisted_op_list.end())
    return false;

  bool isTracked = (_op_list.empty() || (_op_list.find(opName) != _op_list.end()));
  return isTracked;
}

void account_history_rocksdb_plugin::impl::storeOpFilteringParameters(const std::vector<std::string>& opList,
  flat_set<std::string>* storage) const
  {
    for(const auto& arg : opList)
    {
      std::vector<std::string> ops;
      boost::split(ops, arg, boost::is_any_of(" \t,"));

      for(const string& op : ops)
      {
        if( op.empty() == false )
          storage->insert( HIVE_NAMESPACE_PREFIX + op );
      }
    }
  }

std::vector<rocksdb_operation_object>
account_history_rocksdb_plugin::impl::collectReversibleOps(uint32_t* blockRangeBegin, uint32_t* blockRangeEnd,
  uint32_t* collectedIrreversibleBlock) const
{
  FC_ASSERT(*blockRangeBegin < *blockRangeEnd, "Wrong block range");

  if(_currently_persisted_irreversible_block >= *blockRangeBegin && _currently_persisted_irreversible_block <= *blockRangeEnd)
  {
    ilog("Awaiting for the end of save current irreversible block ${b} block, requested by call: [${rb}, ${re}]",
      ("b", _currently_persisted_irreversible_block.operator unsigned int())("rb", *blockRangeBegin)("re", *blockRangeEnd));

    /** Api requests data of currently saved irreversible block, so it must wait for the end of storage and cleanup of
    *   volatile ops container.
    */
    std::unique_lock<std::mutex> lk(_currently_persisted_irreversible_mtx);

    _currently_persisted_irreversible_cv.wait(lk,
      [this, blockRangeBegin, blockRangeEnd]() -> bool
      {
        return _currently_persisted_irreversible_block < *blockRangeBegin || _currently_persisted_irreversible_block > * blockRangeEnd;
      }
    );

    ilog("Resumed evaluation a range containing currently just written irreversible block, requested by call: [${rb}, ${re}]",
      ("rb", *blockRangeBegin)("re", *blockRangeEnd));
  }

  *collectedIrreversibleBlock = _cached_irreversible_block;
  if( *collectedIrreversibleBlock < _cached_reindex_point )
  {
    wlog( "Dynamic correction of last irreversible block from ${a} to ${b} due to reindex point value",
      ( "a", *collectedIrreversibleBlock )( "b", _cached_reindex_point ) );
    *collectedIrreversibleBlock = _cached_reindex_point;
  }

  if( *blockRangeEnd < *collectedIrreversibleBlock )
    return std::vector<rocksdb_operation_object>();

  if(_currently_processed_block >= *blockRangeBegin && _currently_processed_block <= *blockRangeEnd)
  {
    ilog("Awaiting for the end of evaluation of ${b} block, requested by call: [${rb}, ${re}]",
      ("b", _currently_processed_block.operator unsigned int())("rb", *blockRangeBegin)("re", *blockRangeEnd));
    /** Api requests data of currently processed (evaluated) block, so it must wait for the end of evaluation
    *   to collect all contained operations.
    */
    std::unique_lock<std::mutex> lk(_currently_processed_block_mtx);

    _currently_processed_block_cv.wait(lk,
      [this, blockRangeBegin, blockRangeEnd]() -> bool
        {
          return _currently_processed_block < *blockRangeBegin || _currently_processed_block > *blockRangeEnd;
        }
    );

    ilog("Resumed evaluation a range containing currently processed block, requested by call: [${rb}, ${re}]",
      ("rb", *blockRangeBegin)("re", *blockRangeEnd));
  }

  std::vector<rocksdb_operation_object> retVal;
  const auto& volatileIdx = _mainDb.get_index< volatile_operation_index, by_block >();

  retVal.reserve(volatileIdx.size());

  auto opIterator = volatileIdx.lower_bound(*blockRangeBegin);
  for(; opIterator != volatileIdx.end() && opIterator->block < *blockRangeEnd; ++opIterator)
  {
    uint64_t opId = opIterator->op_in_trx;
    opId |= static_cast<uint64_t>(opIterator->trx_in_block) << 32;

    rocksdb_operation_object persistentOp(*opIterator);
    persistentOp.id = opId;
    retVal.emplace_back(std::move(persistentOp));
  }

  if(retVal.empty() == false)
  {
    *blockRangeBegin = retVal.front().block;
    *blockRangeEnd = retVal.back().block + 1;
  }

  return retVal;
}

void account_history_rocksdb_plugin::impl::find_account_history_data(const account_name_type& name, uint64_t start,
  uint32_t limit, bool include_reversible, std::function<bool(unsigned int, const rocksdb_operation_object&)> processor) const
{
  if(limit == 0)
    return;

  ReadOptions rOptions;

  ah_info_by_name_slice_t nameSlice(name.data);
  PinnableSlice buffer;
  auto s = _storage->Get(rOptions, _columnHandles[Columns::AH_INFO_BY_NAME], nameSlice, &buffer);

  if(s.IsNotFound())
    return;

  checkStatus(s);

  account_history_info ahInfo;
  load(ahInfo, buffer.data(), buffer.size());

  ah_op_by_id_slice_t lowerBoundSlice(std::make_pair(ahInfo.id, ahInfo.oldestEntryId));
  ah_op_by_id_slice_t upperBoundSlice(std::make_pair(ahInfo.id, ahInfo.newestEntryId+1));

  rOptions.iterate_lower_bound = &lowerBoundSlice;
  rOptions.iterate_upper_bound = &upperBoundSlice;

  ah_op_by_id_slice_t key(std::make_pair(ahInfo.id, start));
  id_slice_t ahIdSlice(ahInfo.id);

  std::unique_ptr<::rocksdb::Iterator> it(_storage->NewIterator(rOptions, _columnHandles[Columns::AH_OPERATION_BY_ID]));

  it->SeekForPrev(key);

  if(it->Valid() == false)
    return;

  auto keySlice = it->key();
  auto keyValue = ah_op_by_id_slice_t::unpackSlice(keySlice);

  unsigned int count = 0;

  for(; it->Valid(); it->Prev())
  {
    auto keySlice = it->key();
    if(keySlice.starts_with(ahIdSlice) == false)
      break;

    keyValue = ah_op_by_id_slice_t::unpackSlice(keySlice);

    auto valueSlice = it->value();
    const auto& opId = id_slice_t::unpackSlice(valueSlice);
    rocksdb_operation_object oObj;
    bool found = find_operation_object(opId, &oObj);
    FC_ASSERT(found, "Missing operation?");

    if(processor(keyValue.second, oObj))
    {
      ++count;
      if(count >= limit)
        break;
    }
  }
}

bool account_history_rocksdb_plugin::impl::find_operation_object(size_t opId, rocksdb_operation_object* op) const
{
  std::string data;
  id_slice_t idSlice(opId);
  ::rocksdb::Status s = _storage->Get(ReadOptions(), _columnHandles[Columns::OPERATION_BY_ID], idSlice, &data);

  if(s.ok())
  {
    load(*op, data.data(), data.size());
    return true;
  }

  FC_ASSERT(s.IsNotFound());

  return false;
}

void account_history_rocksdb_plugin::impl::find_operations_by_block(size_t blockNum, bool include_reversible,
  std::function<void(const rocksdb_operation_object&)> processor) const
{
  if(include_reversible)
  {
    uint32_t collectedIrreversibleBlock = 0;
    uint32_t rangeBegin = blockNum;
    uint32_t rangeEnd = blockNum + 1;
    auto reversibleOps = collectReversibleOps(&rangeBegin, &rangeEnd, &collectedIrreversibleBlock);
    for(const auto& op : reversibleOps)
    {
      processor(op);
    }

    if(blockNum > collectedIrreversibleBlock)
      return; /// If requested block was reversible, there is no more data in the persistent storage to process.
  }

  std::unique_ptr<::rocksdb::Iterator> it(_storage->NewIterator(ReadOptions(), _columnHandles[Columns::OPERATION_BY_BLOCK]));
  by_block_slice_t blockNumSlice(blockNum);
  op_by_block_num_slice_t key(block_op_id_pair(blockNum, 0));

  for(it->Seek(key); it->Valid() && it->key().starts_with(blockNumSlice); it->Next())
  {
    auto valueSlice = it->value();
    const auto& opId = id_slice_t::unpackSlice(valueSlice);

    rocksdb_operation_object op;
    bool found = find_operation_object(opId, &op);
    FC_ASSERT(found);

    processor(op);
  }
}

std::pair< uint32_t, uint64_t > account_history_rocksdb_plugin::impl::enumVirtualOperationsFromBlockRange(
  uint32_t blockRangeBegin, uint32_t blockRangeEnd, bool include_reversible,
  fc::optional<uint64_t> resumeFromOperation, fc::optional<uint32_t> limit,
  std::function<bool(const rocksdb_operation_object&, uint64_t, bool)> processor) const
{
  FC_ASSERT(blockRangeEnd > blockRangeBegin, "Block range must be upward");

  uint64_t lastProcessedOperationId = 0;
  if(resumeFromOperation.valid())
    lastProcessedOperationId = *resumeFromOperation;

  std::vector<rocksdb_operation_object> reversibleOps;
  uint32_t collectedIrreversibleBlock = false;
  auto collectedReversibleRangeBegin = blockRangeBegin;
  auto collectedReversibleRangeEnd = blockRangeEnd;
  bool hasTrailingReversibleBlocks = false;

  uint32_t cntLimit = 0;
  fc::optional< uint64_t > nextElementAfterLimit;
  uint32_t lastFoundBlock = 0;

  if(include_reversible)
  {
    auto collection = collectReversibleOps(&collectedReversibleRangeBegin, &collectedReversibleRangeEnd,
      &collectedIrreversibleBlock);
    reversibleOps = std::move(collection);

    if(blockRangeBegin > collectedIrreversibleBlock)
    {
      /// Simplest case, whole requested range is located inside reversible space

      for(const auto& op : reversibleOps)
      {
        if(limit.valid() && (cntLimit >= *limit) && op.block != lastFoundBlock)
        {
          /** There is no available any stable identifier to be next stored inside persistent storage.
          *   Such identifier would be required for case when paging will be supported here and
          *   block have been converted into irreversible between calls.
          *   So for simplicity, just all ops will be returned up to processed next block.
          */
          nextElementAfterLimit = 0;
          lastFoundBlock = op.block;
          break;
        }

        /// Accept only virtual operations
        if (op.virtual_op)
          if(processor(op, op.id, false))
            ++cntLimit;

        lastFoundBlock = op.block;
      }

      if(nextElementAfterLimit.valid())
        return std::make_pair(lastFoundBlock, *nextElementAfterLimit);
      else
        return std::make_pair(0, 0);
    }

    /// Partial case: rangeBegin is <= collectedIrreversibleBlock but blockRangeEnd > collectedIrreversibleBlock
    if(blockRangeEnd > collectedIrreversibleBlock)
    {
      blockRangeEnd = collectedIrreversibleBlock; /// strip processing to irreversible subrange
      hasTrailingReversibleBlocks = true;
    }
  }

  op_by_block_num_slice_t upperBoundSlice(block_op_id_pair(blockRangeEnd, 0));

  op_by_block_num_slice_t rangeBeginSlice(block_op_id_pair(blockRangeBegin, lastProcessedOperationId));

  ReadOptions rOptions;
  rOptions.iterate_upper_bound = &upperBoundSlice;

  std::unique_ptr<::rocksdb::Iterator> it(_storage->NewIterator(rOptions, _columnHandles[Columns::OPERATION_BY_BLOCK]));

  for(it->Seek(rangeBeginSlice); it->Valid(); it->Next())
  {
    auto keySlice = it->key();
    const auto& key = op_by_block_num_slice_t::unpackSlice(keySlice);

    /// Accept only virtual operations
    if(key.second & VIRTUAL_OP_FLAG)
    {
      auto valueSlice = it->value();
      auto opId = id_slice_t::unpackSlice(valueSlice);

      rocksdb_operation_object op;
      bool found = find_operation_object(opId, &op);
      FC_ASSERT(found);

      ///Number of retrieved operations can't be greater then limit
      if(limit.valid() && (cntLimit >= *limit))
      {
        nextElementAfterLimit = key.second;
        break;
      }

      if(processor(op, key.second, true))
        ++cntLimit;

      lastFoundBlock = key.first;
    }
  }

  if( nextElementAfterLimit.valid() )
  {
    return std::make_pair( lastFoundBlock, *nextElementAfterLimit );
  }
  else
  {
    if(include_reversible && hasTrailingReversibleBlocks)
    {
      for(const auto& op : reversibleOps)
      {
        if(limit.valid() && (cntLimit >= *limit) && op.block != lastFoundBlock)
        {
          /** There is no available any stable identifier to be next stored inside persistent storage.
          *   Such identifier would be required for case when paging will be supported here and
          *   block have been converted into irreversible between calls.
          *   So for simplicity, just all ops will be returned up to processed next block.
          */
          nextElementAfterLimit = 0;
          lastFoundBlock = op.block;
          break;
        }

        /// Accept only virtual operations
        if (op.virtual_op)
          if(processor(op, op.id, false))
            ++cntLimit;

        lastFoundBlock = op.block;
      }

      if(nextElementAfterLimit.valid())
        return std::make_pair(lastFoundBlock, *nextElementAfterLimit);
      else
        return std::make_pair(0, 0);
    }

    lastFoundBlock = blockRangeEnd; /// start lookup from next block basing on processed range end

    op_by_block_num_slice_t lowerBoundSlice(block_op_id_pair(lastFoundBlock, 0));
    rOptions = ReadOptions();
    rOptions.iterate_lower_bound = &lowerBoundSlice;
    it.reset(_storage->NewIterator(rOptions, _columnHandles[Columns::OPERATION_BY_BLOCK]));

    op_by_block_num_slice_t nextRangeBeginSlice(block_op_id_pair(lastFoundBlock, 0));
    for(it->Seek(nextRangeBeginSlice); it->Valid(); it->Next())
    {
      auto keySlice = it->key();
      const auto& key = op_by_block_num_slice_t::unpackSlice(keySlice);

      if(key.second & VIRTUAL_OP_FLAG)
        return std::make_pair( key.first, 0 );
    }
  }

  return std::make_pair( 0, 0 );
}

bool account_history_rocksdb_plugin::impl::find_transaction_info(const protocol::transaction_id_type& trxId,
  bool include_reversible, uint32_t* blockNo, uint32_t* txInBlock) const
  {
  ReadOptions rOptions;
  TransactionIdSlice idSlice(trxId);
  std::string dataBuffer;
  ::rocksdb::Status s = _storage->Get(rOptions, _columnHandles[Columns::BY_TRANSACTION_ID], idSlice, &dataBuffer);

  if(s.ok())
    {
    const auto& data = block_no_tx_in_block_slice_t::unpackSlice(dataBuffer);
    *blockNo = data.first;
    *txInBlock = data.second;

    return true;
    }

  FC_ASSERT(s.IsNotFound());

  return false;
  }

void account_history_rocksdb_plugin::impl::supplement_snapshot(const hive::chain::prepare_snapshot_supplement_notification& note)
{
  fc::path actual_path(note.external_data_storage_base_path);
  actual_path /= "account_history_rocksdb_data";

  if(bfs::exists(actual_path) == false)
    bfs::create_directories(actual_path);

  auto pathString = actual_path.to_native_ansi_path();

  ::rocksdb::Env* backupEnv = ::rocksdb::Env::Default();
  ::rocksdb::BackupableDBOptions backupableDbOptions(pathString);

  std::unique_ptr<::rocksdb::BackupEngine> backupEngine;
  ::rocksdb::BackupEngine* _backupEngine = nullptr;
  auto status = ::rocksdb::BackupEngine::Open(backupEnv, backupableDbOptions, &_backupEngine);

  checkStatus(status);

  backupEngine.reset(_backupEngine);

  ilog("Attempting to create an AccountHistoryRocksDB backup in the location: `${p}'", ("p", pathString));

  std::string meta_data = "Account History RocksDB plugin data. Current head block: ";
  meta_data += std::to_string(_mainDb.head_block_num());

  status = _backupEngine->CreateNewBackupWithMetadata(this->_storage.get(), meta_data, true);
  checkStatus(status);

  std::vector<::rocksdb::BackupInfo> backupInfos;
  _backupEngine->GetBackupInfo(&backupInfos);

  FC_ASSERT(backupInfos.size() == 1);

  note.dump_helper.store_external_data_info(_self, actual_path);
}

void account_history_rocksdb_plugin::impl::load_additional_data_from_snapshot(const hive::chain::load_snapshot_supplement_notification& note)
{
  fc::path extdata_path;
  if(note.load_helper.load_external_data_info(_self, &extdata_path) == false)
  {
    wlog("No external data present for Account History RocksDB plugin...");
    return;
  }

  auto pathString = extdata_path.to_native_ansi_path();

  ilog("Attempting to load external data for Account History RocksDB plugin from location: `${p}'", ("p", pathString));

  ::rocksdb::Env* backupEnv = ::rocksdb::Env::Default();
  ::rocksdb::BackupableDBOptions backupableDbOptions(pathString);

  std::unique_ptr<::rocksdb::BackupEngineReadOnly> backupEngine;
  ::rocksdb::BackupEngineReadOnly* _backupEngine = nullptr;
  auto status = ::rocksdb::BackupEngineReadOnly::Open(backupEnv, backupableDbOptions, &_backupEngine);

  checkStatus(status);

  backupEngine.reset(_backupEngine);

  ilog("Attempting to restore an AccountHistoryRocksDB backup from the backup location: `${p}'", ("p", pathString));

  shutdownDb();

  ilog("Attempting to destroy current AHR storage...");
  ::rocksdb::Options destroyOpts;
  ::rocksdb::DestroyDB(_storagePath.string(), destroyOpts);

  ilog("AccountHistoryRocksDB has been destroyed at location: ${p}.", ("p", _storagePath.string()));

  ilog("Starting restore of AccountHistoryRocksDB backup into storage location: ${p}.", ("p", _storagePath.string()));

  bfs::path walDir(_storagePath);
  walDir /= "WAL";
  status = backupEngine->RestoreDBFromLatestBackup(_storagePath.string(), walDir.string());
  checkStatus(status);

  ilog("Restoring AccountHistoryRocksDB backup from the location: `${p}' finished", ("p", pathString));

  openDb();
}

void account_history_rocksdb_plugin::impl::load_lib()
{
  std::string data;
  auto s = _storage->Get(ReadOptions(), _columnHandles[Columns::CURRENT_LIB], LIB_ID, &data );

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

void account_history_rocksdb_plugin::impl::update_lib( uint32_t lib )
{
  //ilog( "RocksDB LIB set to ${l}.", ( "l", lib ) ); too frequent
  _cached_irreversible_block.store(lib);
  auto s = _writeBuffer.Put( _columnHandles[Columns::CURRENT_LIB], LIB_ID, lib_slice_t( lib ) );
  checkStatus( s );
}

void account_history_rocksdb_plugin::impl::load_reindex_point()
{
  std::string data;
  auto s = _storage->Get( ReadOptions(), _columnHandles[Columns::LAST_REINDEX_POINT], REINDEX_POINT_ID, &data );

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

void account_history_rocksdb_plugin::impl::update_reindex_point( uint32_t rp )
{
  ilog( "RocksDB reindex point set to ${p}.", ( "p", rp ) );
  _cached_reindex_point = rp;
  auto s = _writeBuffer.Put( _columnHandles[Columns::LAST_REINDEX_POINT], REINDEX_POINT_ID, lib_slice_t( rp ) );
  checkStatus( s );
}

account_history_rocksdb_plugin::impl::ColumnDefinitions account_history_rocksdb_plugin::impl::prepareColumnDefinitions(bool addDefaultColumn)
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

bool account_history_rocksdb_plugin::impl::createDbSchema(const bfs::path& path)
{
  DB* db = nullptr;

  auto columnDefs = prepareColumnDefinitions(true);
  auto strPath = path.string();
  Options options;
  /// Optimize RocksDB. This is the easiest way to get RocksDB to perform well
  options.IncreaseParallelism();
  options.OptimizeLevelStyleCompaction();

  auto s = DB::OpenForReadOnly(options, strPath, columnDefs, &_columnHandles, &db);

  if(s.ok())
  {
    cleanupColumnHandles(db);
    delete db;
    return false; /// DB does not need data import.
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

    return true; /// DB needs data import
  }
  else
  {
    elog("RocksDB can not create storage at location: `${p}'.\nReturned error: ${e}",
      ("p", strPath)("e", s.ToString()));

    return false;
  }
}

void account_history_rocksdb_plugin::impl::buildAccountHistoryRecord( const account_name_type& name, const rocksdb_operation_object& obj )
{
  std::string strName = name;

  ReadOptions rOptions;
  //rOptions.tailing = true;

  ah_info_by_name_slice_t nameSlice(name.data);

  account_history_info ahInfo;
  bool found = _writeBuffer.getAHInfo(name, &ahInfo);

  if(found)
  {
    auto count = ahInfo.getAssociatedOpCount();

    if(_prune && count > ACCOUNT_HISTORY_LENGTH_LIMIT &&
      ((obj.timestamp - ahInfo.oldestEntryTimestamp) > fc::days(ACCOUNT_HISTORY_TIME_LIMIT)))
      {
        prunePotentiallyTooOldItems(&ahInfo, name, obj.timestamp);
      }

    auto nextEntryId = ++ahInfo.newestEntryId;
    _writeBuffer.putAHInfo(name, ahInfo);

    ah_op_by_id_slice_t ahInfoOpSlice(std::make_pair(ahInfo.id, nextEntryId));
    id_slice_t valueSlice(obj.id);
    auto s = _writeBuffer.Put(_columnHandles[Columns::AH_OPERATION_BY_ID], ahInfoOpSlice, valueSlice);
    checkStatus(s);
  }
  else
  {
    /// New entry must be created - there is first operation recorded.
    ahInfo.id = _accountHistorySeqId++;
    ahInfo.newestEntryId = ahInfo.oldestEntryId = 0;
    ahInfo.oldestEntryTimestamp = obj.timestamp;

    _writeBuffer.putAHInfo(name, ahInfo);

    ah_op_by_id_slice_t ahInfoOpSlice(std::make_pair(ahInfo.id, 0));
    id_slice_t valueSlice(obj.id);
    auto s = _writeBuffer.Put(_columnHandles[Columns::AH_OPERATION_BY_ID], ahInfoOpSlice, valueSlice);
    checkStatus(s);
  }
}

void account_history_rocksdb_plugin::impl::storeTransactionInfo(const chain::transaction_id_type& trx_id, uint32_t blockNo, uint32_t trx_in_block)
  {
  TransactionIdSlice txSlice(trx_id);
  block_no_tx_in_block_pair block_no_tx_no(blockNo, trx_in_block);
  block_no_tx_in_block_slice_t valueSlice(block_no_tx_no);

  auto s = _writeBuffer.Put(_columnHandles[Columns::BY_TRANSACTION_ID], txSlice, valueSlice);
  checkStatus(s);
  }

void account_history_rocksdb_plugin::impl::prunePotentiallyTooOldItems(account_history_info* ahInfo, const account_name_type& name,
  const fc::time_point_sec& now)
{
  std::string strName = name;

  auto ageLimit =  fc::days(ACCOUNT_HISTORY_TIME_LIMIT);

  ah_op_by_id_slice_t oldestEntrySlice(
    std::make_pair(ahInfo->id, ahInfo->oldestEntryId));
  auto lookupUpperBound = std::make_pair(ahInfo->id + 1,
    ahInfo->newestEntryId - ACCOUNT_HISTORY_LENGTH_LIMIT + 1);

  ah_op_by_id_slice_t newestEntrySlice(lookupUpperBound);

  ReadOptions rOptions;
  //rOptions.tailing = true;
  rOptions.iterate_lower_bound = &oldestEntrySlice;
  rOptions.iterate_upper_bound = &newestEntrySlice;

  auto s = _writeBuffer.SingleDelete(_columnHandles[Columns::AH_OPERATION_BY_ID], oldestEntrySlice);
  checkStatus(s);

  std::unique_ptr<::rocksdb::Iterator> dataItr(_storage->NewIterator(rOptions, _columnHandles[Columns::AH_OPERATION_BY_ID]));

  /** To clean outdated records we have to iterate over all AH records having subsequent number greater than limit
    *  and additionally verify date of operation, to clean up only these exceeding a date limit.
    *  So just operations having a list position > ACCOUNT_HISTORY_LENGTH_LIMIT and older that ACCOUNT_HISTORY_TIME_LIMIT
    *  shall be removed.
    */
  dataItr->Seek(oldestEntrySlice);

  /// Boundaries of keys to be removed
  //uint32_t leftBoundary = ahInfo->oldestEntryId;
  uint32_t rightBoundary = ahInfo->oldestEntryId;

  for(; dataItr->Valid(); dataItr->Next())
  {
    auto key = dataItr->key();
    auto foundEntry = ah_op_by_id_slice_t::unpackSlice(key);

    if(foundEntry.first != ahInfo->id || foundEntry.second >= lookupUpperBound.second)
      break;

    auto value = dataItr->value();

    auto pointedOpId = id_slice_t::unpackSlice(value);
    rocksdb_operation_object op;
    find_operation_object(pointedOpId, &op);

    auto age = now - op.timestamp;

    if(age > ageLimit)
    {
      rightBoundary = foundEntry.second;
      ah_op_by_id_slice_t rightBoundarySlice(
        std::make_pair(ahInfo->id, rightBoundary));
      s = _writeBuffer.SingleDelete(_columnHandles[4], rightBoundarySlice);
      checkStatus(s);
    }
    else
    {
      ahInfo->oldestEntryId = foundEntry.second;
      ahInfo->oldestEntryTimestamp = op.timestamp;
      FC_ASSERT(ahInfo->oldestEntryId <= ahInfo->newestEntryId);

      break;
    }
  }
}

void account_history_rocksdb_plugin::impl::on_pre_reindex(const hive::chain::reindex_notification& note)
{
  shutdownDb();
  std::string strPath = _storagePath.string();

  if( note.force_replay )
  {
    ilog("Received onReindexStart request, attempting to clean database storage.");
    auto s = ::rocksdb::DestroyDB(strPath, ::rocksdb::Options());
    checkStatus(s);
  }

  openDb();

  ilog("Setting write limit to massive level");

  _collectedOpsWriteLimit = WRITE_BUFFER_FLUSH_LIMIT;

  _lastTx = transaction_id_type();
  _txNo = 0;
  _totalOps = 0;
  _excludedOps = 0;
  _reindexing = true;

  ilog("onReindexStart request completed successfully.");
}

void account_history_rocksdb_plugin::impl::on_post_reindex(const hive::chain::reindex_notification& note)
{
  ilog("Reindex completed up to block: ${b}. Setting back write limit to non-massive level.",
    ("b", note.last_block_number));

  flushStorage();
  _collectedOpsWriteLimit = 1;
  _reindexing = false;
  uint32_t last_irreversible_block_num =_mainDb.get_last_irreversible_block_num();
  update_lib( last_irreversible_block_num ); // Set same value as in main database, as result of witness participation
  update_reindex_point( note.last_block_number );

  printReport( note.last_block_number, "RocksDB data reindex finished." );
}

std::string get_asset_amount(const asset& amount)
{
  std::string asset_with_amount_string = legacy_asset::from_asset(amount).to_string();
  size_t space_pos = asset_with_amount_string.find(' ');
  assert(space_pos != std::string::npos);
  return asset_with_amount_string.substr(0, space_pos);
}

void account_history_rocksdb_plugin::impl::printReport(uint32_t blockNo, const char* detailText) const
{
  ilog("${t}Processed blocks: ${n}, containing: ${tx} transactions and ${op} operations.\n"
      "${ep} operations have been filtered out due to configured options.\n"
      "${ea} accounts have been filtered out due to configured options.",
    ("t", detailText)
    ("n", blockNo)
    ("tx", _txNo)
    ("op", _totalOps)
    ("ep", _excludedOps)
    ("ea", _excludedAccountCount)
    );
}

void account_history_rocksdb_plugin::impl::importData(unsigned int blockLimit)
{
  if(_storage == nullptr)
  {
    ilog("RocksDB has no opened storage. Skipping data import...");
    return;
  }

  ilog("Starting data import...");

  block_id_type lastBlock;
  size_t blockNo = 0;

  _lastTx = transaction_id_type();
  _txNo = 0;
  _totalOps = 0;
  _excludedOps = 0;

  benchmark_dumper dumper;
  dumper.initialize([](benchmark_dumper::database_object_sizeof_cntr_t&){}, "rocksdb_data_import.json");

  _mainDb.foreach_operation([blockLimit, &blockNo, &lastBlock, this](
    const signed_block_header& prevBlockHeader, const signed_block& block, const signed_transaction& tx,
    uint32_t txInBlock, const operation& op, uint16_t opInTx) -> bool
  {
    if(lastBlock != block.previous)
    {
      blockNo = block.block_num();
      lastBlock = block.previous;

      if(blockLimit != 0 && blockNo > blockLimit)
      {
        ilog( "RocksDb data import stopped because of block limit reached.");
        return false;
      }

      if(blockNo % 1000 == 0)
      {
        printReport(blockNo, "Executing data import has ");
      }
    }

    auto impacted = getImpactedAccounts( op );
    if( impacted.empty() )
      return true;

    hive::util::supplement_operation( op, _mainDb );

    rocksdb_operation_object obj;
    obj.trx_id = tx.id();
    obj.block = blockNo;
    obj.trx_in_block = txInBlock;
    obj.op_in_trx = opInTx;
    obj.timestamp = _mainDb.head_block_time();
    auto size = fc::raw::pack_size( op );
    obj.serialized_op.resize( size );
    fc::datastream< char* > ds( obj.serialized_op.data(), size );
    fc::raw::pack( ds, op );

    importOperation( obj, impacted );

    return true;
  });

  flushWriteBuffer();

  const auto& measure = dumper.measure(blockNo, [](benchmark_dumper::index_memory_details_cntr_t&, bool){});
  ilog( "RocksDb data import - Performance report at block ${n}. Elapsed time: ${rt} ms (real), ${ct} ms (cpu). Memory usage: ${cm} (current), ${pm} (peak) kilobytes.",
    ("n", blockNo)
    ("rt", measure.real_ms)
    ("ct", measure.cpu_ms)
    ("cm", measure.current_mem)
    ("pm", measure.peak_mem) );

  printReport(blockNo, "RocksDB data import finished. ");
}

void account_history_rocksdb_plugin::impl::on_pre_apply_operation(const operation_notification& n)
{
  if( n.block % 10000 == 0 && n.trx_in_block == 0 && n.op_in_trx == 0 && n.virtual_op == 0 )
  {
    ilog("RocksDb data import processed blocks: ${n}, containing: ${tx} transactions and ${op} operations.\n"
        " ${ep} operations have been filtered out due to configured options.\n"
        " ${ea} accounts have been filtered out due to configured options.",
      ("n", n.block)
      ("tx", _txNo)
      ("op", _totalOps)
      ("ep", _excludedOps)
      ("ea", _excludedAccountCount)
      );
  }

  if( !isTrackedOperation(n.op) )
  {
    ++_excludedOps;
    return;
  }

  auto impacted = getImpactedAccounts(n.op);
  if( impacted.empty() )
    return; // Ignore operations not impacting any account (according to original implementation)

  hive::util::supplement_operation( n.op, _mainDb );

  if( _reindexing )
  {
    rocksdb_operation_object obj;
    obj.trx_id = n.trx_id;
    obj.block = n.block;
    obj.trx_in_block = n.trx_in_block;
    obj.op_in_trx = n.op_in_trx;
    obj.virtual_op = n.virtual_op;
    obj.timestamp = _mainDb.head_block_time();
    auto size = fc::raw::pack_size( n.op );
    obj.serialized_op.resize( size );
    fc::datastream< char* > ds( obj.serialized_op.data(), size );
    fc::raw::pack( ds, n.op );

    importOperation( obj, impacted );
  }
  else
  {
    _mainDb.create< volatile_operation_object >( [&]( volatile_operation_object& o )
    {
      o.trx_id = n.trx_id;
      o.block = n.block;
      o.trx_in_block = n.trx_in_block;
      o.op_in_trx = n.op_in_trx;
      o.virtual_op = n.virtual_op;
      o.timestamp = _mainDb.head_block_time();
      auto size = fc::raw::pack_size( n.op );
      o.serialized_op.resize( size );
      fc::datastream< char* > ds( o.serialized_op.data(), size );
      fc::raw::pack( ds, n.op );
      o.impacted.insert( o.impacted.end(), impacted.begin(), impacted.end() );
    });
  }
}

void account_history_rocksdb_plugin::impl::on_irreversible_block( uint32_t block_num )
{
  if( _reindexing ) return;

  if( block_num <= _cached_reindex_point )
  {
    // during reindex all data is pushed directly to storage, however the LIB reflects
    // state in dgpo; this means there is a small window of inconsistency when data is stored
    // in permanent storage but LIB would indicate it should be in volatile index
    wlog( "Incoming LIB value ${l} within reindex range ${r} - that block is already effectively irreversible",
      ( "l", block_num )( "r", _cached_reindex_point ) );
  }
  else
  {
    FC_ASSERT( block_num > _cached_irreversible_block, "New irreversible block: ${nb} can't be less than already stored one: ${ob}",
      ( "nb", block_num )( "ob", static_cast< uint32_t >(_cached_irreversible_block) ) );
  }

  auto& volatileOpsGenericIndex = _mainDb.get_mutable_index<volatile_operation_index>();

  const auto& volatile_idx = _mainDb.get_index< volatile_operation_index, by_block >();
  auto itr = volatile_idx.begin();

  _currently_persisted_irreversible_block.store(block_num);

  {
    std::lock_guard<std::mutex> lk(_currently_persisted_irreversible_mtx);

    // it is unlikely we get here but flush storage in this case
    if(itr != volatile_idx.end() && itr->block < block_num)
    {
      flushStorage();

      while(itr != volatile_idx.end() && itr->block < block_num)
      {
        auto comp = []( const rocksdb_operation_object& lhs, const rocksdb_operation_object& rhs )
        {
          return std::tie( lhs.block, lhs.trx_in_block, lhs.op_in_trx, lhs.trx_id, lhs.virtual_op ) < std::tie( rhs.block, rhs.trx_in_block, rhs.op_in_trx, rhs.trx_id, rhs.virtual_op );
        };
        std::set< rocksdb_operation_object, decltype(comp) > ops( comp );
        find_operations_by_block(itr->block, false, // don't include reversible, only already imported ops
          [&ops](const rocksdb_operation_object& op)
          {
            ops.emplace(op);
          }
        );

        uint32_t this_itr_block = itr->block;
        while(itr != volatile_idx.end() && itr->block == this_itr_block)
        {
          rocksdb_operation_object obj(*itr);
          // check that operation is already stored as irreversible as it will be not imported
          FC_ASSERT(ops.count(obj), "operation in block ${block} was not imported until irreversible block ${irreversible}", ("block", this_itr_block)("irreversible", block_num));
          hive::protocol::operation op = fc::raw::unpack_from_buffer< hive::protocol::operation >( obj.serialized_op );
          wlog("prevented importing duplicate operation from block ${block} when handling irreversible block ${irreversible}, operation: ${op}",
            ("block", obj.block)("irreversible", block_num)("op", fc::json::to_string(op)));
          itr++;
        }
      }
    }

    /// Range of reversible (volatile) ops to be processed should come from blocks (_cached_irreversible_block, block_num]
    auto moveRangeBeginI = volatile_idx.upper_bound( _cached_irreversible_block );

    FC_ASSERT(moveRangeBeginI == volatile_idx.begin() || moveRangeBeginI == volatile_idx.end(), "All volatile ops processed by previous irreversible blocks should be already flushed");

    auto moveRangeEndI = volatile_idx.upper_bound(block_num);
    FC_ASSERT( block_num > _cached_reindex_point || moveRangeBeginI == moveRangeEndI,
      "There should be no volatile data for block ${b} since it was processed during reindex up to ${r}",
      ( "b", block_num )( "r", _cached_reindex_point ) );

    volatileOpsGenericIndex.move_to_external_storage<by_block>(moveRangeBeginI, moveRangeEndI, [this](const volatile_operation_object& operation) -> void
      {
        rocksdb_operation_object obj(operation);
        importOperation(obj, operation.impacted);
      }
    );

    update_lib(block_num);
    //flushStorage(); it is apparently needed to properly write LIB so it can be read later, however it kills performance - alternative solution used currently just masks problem
  }

  _currently_persisted_irreversible_block.store(0);
  _currently_persisted_irreversible_cv.notify_all();
}

void account_history_rocksdb_plugin::impl::on_pre_apply_block(const block_notification& bn)
{
  if(_reindexing) return;

  _currently_processed_block.store(bn.block_num);
  is_processed_block_mtx_locked.store(true);
  _currently_processed_block_mtx.lock();
}

void account_history_rocksdb_plugin::impl::on_post_apply_block(const block_notification& bn)
{
  if (_balance_csv_filename)
  {
    // check the balances of all tracked accounts, emit a line in the CSV file if any have changed
    // since the last block
    const auto& account_idx = _mainDb.get_index<hive::chain::account_index>().indices().get<hive::chain::by_name>();
    for (auto range : _tracked_accounts)
    {
      const std::string& lower = range.first;
      const std::string& upper = range.second;

      auto account_iter = account_idx.lower_bound(range.first);
      while (account_iter != account_idx.end() &&
             account_iter->name <= upper)
      {
        const account_object& account = *account_iter;

        auto saved_balance_iter = _saved_balances.find(account_iter->name);
        bool balances_changed = saved_balance_iter == _saved_balances.end();
        saved_balances& saved_balance_record = _saved_balances[account_iter->name];

        if (saved_balance_record.hive_balance != account.balance)
        {
          saved_balance_record.hive_balance = account.balance;
          balances_changed = true;
        }
        if (saved_balance_record.savings_hive_balance != account.savings_balance)
        {
          saved_balance_record.savings_hive_balance = account.savings_balance;
          balances_changed = true;
        }
        if (saved_balance_record.hbd_balance != account.hbd_balance)
        {
          saved_balance_record.hbd_balance = account.hbd_balance;
          balances_changed = true;
        }
        if (saved_balance_record.savings_hbd_balance != account.savings_hbd_balance)
        {
          saved_balance_record.savings_hbd_balance = account.savings_hbd_balance;
          balances_changed = true;
        }

        if (saved_balance_record.reward_hbd_balance != account.reward_hbd_balance)
        {
          saved_balance_record.reward_hbd_balance = account.reward_hbd_balance;
          balances_changed = true;
        }
        if (saved_balance_record.reward_hive_balance != account.reward_hive_balance)
        {
          saved_balance_record.reward_hive_balance = account.reward_hive_balance;
          balances_changed = true;
        }
        if (saved_balance_record.reward_vesting_balance != account.reward_vesting_balance)
        {
          saved_balance_record.reward_vesting_balance = account.reward_vesting_balance;
          balances_changed = true;
        }
        if (saved_balance_record.reward_vesting_hive_balance != account.reward_vesting_hive)
        {
          saved_balance_record.reward_vesting_hive_balance = account.reward_vesting_hive;
          balances_changed = true;
        }

        if (saved_balance_record.vesting_shares != account.vesting_shares)
        {
          saved_balance_record.vesting_shares = account.vesting_shares;
          balances_changed = true;
        }
        if (saved_balance_record.delegated_vesting_shares != account.delegated_vesting_shares)
        {
          saved_balance_record.delegated_vesting_shares = account.delegated_vesting_shares;
          balances_changed = true;
        }
        if (saved_balance_record.received_vesting_shares != account.received_vesting_shares)
        {
          saved_balance_record.received_vesting_shares = account.received_vesting_shares;
          balances_changed = true;
        }

        if (balances_changed)
        {
          _balance_csv_file << (string)account.name << ","
                            << bn.block.block_num() << ","
                            << bn.block.timestamp.to_iso_string() << ","
                            << get_asset_amount(saved_balance_record.hive_balance) << ","
                            << get_asset_amount(saved_balance_record.savings_hive_balance) << ","
                            << get_asset_amount(saved_balance_record.hbd_balance) << ","
                            << get_asset_amount(saved_balance_record.savings_hbd_balance) << ","
                            << get_asset_amount(saved_balance_record.vesting_shares) << ","
                            << get_asset_amount(saved_balance_record.delegated_vesting_shares) << ","
                            << get_asset_amount(saved_balance_record.received_vesting_shares) << ","
                            << get_asset_amount(saved_balance_record.reward_hive_balance) << ","
                            << get_asset_amount(saved_balance_record.reward_hbd_balance) << ","
                            << get_asset_amount(saved_balance_record.reward_vesting_balance) << ","
                            << get_asset_amount(saved_balance_record.reward_vesting_hive_balance) << "\n";
          // flush every 10k lines
          ++_balance_csv_line_count;
          if (_balance_csv_line_count > 10000)
          {
            _balance_csv_line_count = 0;
            _balance_csv_file.flush();
          }
        }
        ++account_iter;
      }
    }
  }

  if(_reindexing) return;

  _currently_processed_block.store(0);
  if( is_processed_block_mtx_locked )
  {
    is_processed_block_mtx_locked.store(false);
    _currently_processed_block_mtx.unlock();
  }

  _currently_processed_block_cv.notify_all();
}

account_history_rocksdb_plugin::account_history_rocksdb_plugin()
{
}

account_history_rocksdb_plugin::~account_history_rocksdb_plugin()
{
}

void account_history_rocksdb_plugin::set_program_options(
  boost::program_options::options_description &command_line_options,
  boost::program_options::options_description &cfg)
{
  cfg.add_options()
    ("account-history-rocksdb-path", bpo::value<bfs::path>()->default_value("blockchain/account-history-rocksdb-storage"),
      "The location of the rocksdb database for account history. By default it is $DATA_DIR/blockchain/account-history-rocksdb-storage")
    ("account-history-rocksdb-track-account-range", boost::program_options::value< std::vector<std::string> >()->composing()->multitoken(), "Defines a range of accounts to track as a json pair [\"from\",\"to\"] [from,to] Can be specified multiple times.")
    ("account-history-rocksdb-whitelist-ops", boost::program_options::value< std::vector<std::string> >()->composing(), "Defines a list of operations which will be explicitly logged.")
    ("account-history-rocksdb-blacklist-ops", boost::program_options::value< std::vector<std::string> >()->composing(), "Defines a list of operations which will be explicitly ignored.")

  ;
  command_line_options.add_options()
    ("account-history-rocksdb-immediate-import", bpo::bool_switch()->default_value(false),
      "Allows to force immediate data import at plugin startup. By default storage is supplied during reindex process.")
    ("account-history-rocksdb-stop-import-at-block", bpo::value<uint32_t>()->default_value(0),
      "Allows to specify block number, the data import process should stop at.")
    ("account-history-rocksdb-dump-balance-history", boost::program_options::value< string >(), "Dumps balances for all tracked accounts to a CSV file every time they change")
  ;
}

void account_history_rocksdb_plugin::plugin_initialize(const boost::program_options::variables_map& options)
{
  if(options.count("account-history-rocksdb-stop-import-at-block"))
    _blockLimit = options.at("account-history-rocksdb-stop-import-at-block").as<uint32_t>();

  _doImmediateImport = options.at("account-history-rocksdb-immediate-import").as<bool>();

  bfs::path dbPath;

  if(options.count("account-history-rocksdb-path"))
    dbPath = options.at("account-history-rocksdb-path").as<bfs::path>();

  if(dbPath.is_absolute() == false)
  {
    auto basePath = appbase::app().data_dir();
    auto actualPath = basePath / dbPath;
    dbPath = actualPath;
  }

  _my = std::make_unique<impl>( *this, options, dbPath );

  _my->openDb();
}

void account_history_rocksdb_plugin::plugin_startup()
{
  ilog("Starting up account_history_rocksdb_plugin...");

  if(_doImmediateImport)
    _my->importData(_blockLimit);
}

void account_history_rocksdb_plugin::plugin_shutdown()
{
  ilog("Shutting down account_history_rocksdb_plugin...");
  _my->shutdownDb();
}

void account_history_rocksdb_plugin::find_account_history_data(const account_name_type& name, uint64_t start, uint32_t limit,
  bool include_reversible, std::function<bool(unsigned int, const rocksdb_operation_object&)> processor) const
{
  _my->find_account_history_data(name, start, limit, include_reversible, processor);
}

bool account_history_rocksdb_plugin::find_operation_object(size_t opId, rocksdb_operation_object* op) const
{
  return _my->find_operation_object(opId, op);
}

void account_history_rocksdb_plugin::find_operations_by_block(size_t blockNum, bool include_reversible,
  std::function<void(const rocksdb_operation_object&)> processor) const
{
  _my->find_operations_by_block(blockNum, include_reversible, processor);
}

std::pair< uint32_t, uint64_t > account_history_rocksdb_plugin::enum_operations_from_block_range(uint32_t blockRangeBegin, uint32_t blockRangeEnd,
  bool include_reversible, fc::optional<uint64_t> operationBegin, fc::optional<uint32_t> limit,
  std::function<bool(const rocksdb_operation_object&, uint64_t, bool)> processor) const
{
  return _my->enumVirtualOperationsFromBlockRange(blockRangeBegin, blockRangeEnd, include_reversible, operationBegin, limit, processor);
}

bool account_history_rocksdb_plugin::find_transaction_info(const protocol::transaction_id_type& trxId, bool include_reversible, uint32_t* blockNo,
  uint32_t* txInBlock) const
  {
  return _my->find_transaction_info(trxId, include_reversible, blockNo, txInBlock);
  }

} } }

FC_REFLECT( hive::plugins::account_history_rocksdb::account_history_info,
  (id)(oldestEntryId)(newestEntryId)(oldestEntryTimestamp) )
