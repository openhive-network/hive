
#include <hive/chain/hive_fwd.hpp>

#include <hive/plugins/account_history_rocksdb/account_history_rocksdb_plugin.hpp>

#include <hive/chain/database.hpp>
#include <hive/chain/hive_objects.hpp>
#include <hive/chain/index.hpp>
#include <hive/chain/util/impacted.hpp>
#include <hive/chain/util/supplement_operations.hpp>
#include <hive/chain/util/data_filter.hpp>

#include <hive/plugins/chain/chain_plugin.hpp>
#include <hive/plugins/chain/state_snapshot_provider.hpp>

#include <hive/utilities/benchmark_dumper.hpp>

#include <appbase/application.hpp>

#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/slice.h>
#include <rocksdb/utilities/backup_engine.h>
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

/** Because localtion_id_pair stores block_number paired with operation_id_vop_pair, which stores operation id on 63 bits,
  *  max allowed operation-id is max_int64 (instead of max_uint64).
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

struct operation_id_vop_pair
{
  operation_id_vop_pair(uint64_t id = 0, bool is_virtual_op = false) : _op_id(id), _is_virtual_op(is_virtual_op)
  {
    FC_ASSERT(id < static_cast<uint64_t>(MAX_OPERATION_ID));
  }

  bool operator < (const operation_id_vop_pair& rhs) const
  {
    return _op_id < rhs._op_id; /// Intentionally skipped _is_virtual_op field, which holds only information about pointed operation
  }

  bool operator > (const operation_id_vop_pair& rhs) const
  {
    return _op_id > rhs._op_id; /// Intentionally skipped _is_virtual_op field, which holds only information about pointed operation
  }

  bool operator == (const operation_id_vop_pair& rhs) const
  {
    return _op_id == rhs._op_id; /// Intentionally skipped _is_virtual_op field, which holds only information about pointed operation
  }

  uint64_t get_id() const { return _op_id; }
  bool is_virtual() const { return _is_virtual_op; }

private:
  uint64_t _op_id : 63;
  uint64_t _is_virtual_op : 1;
};

/** Location index is nonunique. Since RocksDB supports only unique indexes, it must be extended
  *  by some unique part (ie ID).
  *
  */
typedef std::pair< uint32_t, operation_id_vop_pair > block_op_id_pair;
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
    _writeBuffer(_storage, _columnHandles),
    _filter("ah-rb")
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

    _cached_irreversible_block.store(0);
    _cached_reindex_point = 0;

    HIVE_ADD_PLUGIN_INDEX(_mainDb, volatile_operation_index);
    }

  ~impl()
  {

    chain::util::disconnect_signal(_on_pre_apply_operation_con);
    chain::util::disconnect_signal(_on_irreversible_block_conn);
    chain::util::disconnect_signal(_on_post_apply_block_conn);
    chain::util::disconnect_signal(_on_fail_apply_block_conn);
    shutdownDb();
  }

  void openDb( bool cleanDatabase )
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

  void find_account_history_data(const account_name_type& name, uint64_t start, uint32_t limit, bool include_reversible,
    std::function<bool(unsigned int, const rocksdb_operation_object&)> processor) const;
  uint32_t find_reversible_account_history_data(const account_name_type& name, uint64_t start, uint32_t limit, uint32_t number_of_irreversible_ops,
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

  /// std::tuple<A, B>
  /// A - returns true if database will need data import.
  /// B - returns false if problems with opening db appeared.
  std::tuple<bool, bool> createDbSchema(const bfs::path& path);

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
    if(_lastTx != obj.trx_id && obj.trx_id != transaction_id_type()/*An virtual operation shouldn't increase `_txNo` counter.*/ )
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

    op_by_block_num_slice_t blockLocSlice(block_op_id_pair(obj.block, operation_id_vop_pair(obj.id, obj.is_virtual)));

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
  account_history_rocksdb_plugin&  _self;
  chain::database&                 _mainDb;
  bfs::path                        _blockchainStoragePath;
  bfs::path                        _storagePath;
  std::unique_ptr<DB>              _storage;
  std::vector<ColumnFamilyHandle*> _columnHandles;
  CachableWriteBatch               _writeBuffer;

  boost::signals2::connection      _on_pre_apply_operation_con;
  boost::signals2::connection      _on_irreversible_block_conn;
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

  account_filter                   _filter;
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

  _filter.fill( options, "account-history-rocksdb-track-account-range" );
  state_opts[ "account-history-rocksdb-track-account-range" ] = _filter.get_tracked_accounts();

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
  bool inRange = _filter.is_tracked_account( name );

  _excludedAccountCount += inRange;

  return inRange;
}

std::vector<account_name_type> account_history_rocksdb_plugin::impl::getImpactedAccounts(const operation& op) const
{
  flat_set<account_name_type> impacted;
  hive::app::operation_get_impacted_accounts(op, impacted);
  std::vector<account_name_type> retVal;

  if(impacted.empty())
    return retVal;

  if(_filter.empty())
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

  return _mainDb.with_read_lock([this, blockRangeBegin, blockRangeEnd, collectedIrreversibleBlock]() -> std::vector<rocksdb_operation_object>
  {
    *collectedIrreversibleBlock = _cached_irreversible_block;
    if( *collectedIrreversibleBlock < _cached_reindex_point )
    {
      wlog( "Dynamic correction of last irreversible block from ${a} to ${b} due to reindex point value",
        ( "a", *collectedIrreversibleBlock )( "b", _cached_reindex_point ) );
      *collectedIrreversibleBlock = _cached_reindex_point;
    }

    if( *blockRangeEnd < *collectedIrreversibleBlock )
      return std::vector<rocksdb_operation_object>();

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
  });
}

void account_history_rocksdb_plugin::impl::find_account_history_data(const account_name_type& name, uint64_t start,
  uint32_t limit, bool include_reversible, std::function<bool(unsigned int, const rocksdb_operation_object&)> processor) const
{
  if(limit == 0)
    return;

  unsigned int count = 0;
  unsigned int number_of_irreversible_ops = 0;

  ReadOptions rOptions;

  ah_info_by_name_slice_t nameSlice(name.data);
  PinnableSlice buffer;
  auto s = _storage->Get(rOptions, _columnHandles[Columns::AH_INFO_BY_NAME], nameSlice, &buffer);

  if(s.IsNotFound())
  {
    if(include_reversible)
      find_reversible_account_history_data(name, start, limit, number_of_irreversible_ops, processor);
    return;
  }

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
  {
    if(include_reversible)
      find_reversible_account_history_data(name, start, limit, number_of_irreversible_ops, processor);
    return;
  }

  auto keySlice = it->key();
  auto keyValue = ah_op_by_id_slice_t::unpackSlice(keySlice);

  number_of_irreversible_ops = keyValue.second + 1;
  if(include_reversible)
    count += find_reversible_account_history_data(name, start, limit, number_of_irreversible_ops, processor);

  for(; it->Valid() && count<limit; it->Prev())
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

uint32_t account_history_rocksdb_plugin::impl::find_reversible_account_history_data(const account_name_type& name, uint64_t start,
  uint32_t limit, uint32_t number_of_irreversible_ops, std::function<bool(unsigned int, const rocksdb_operation_object&)> processor) const
{
  uint32_t count = 0;
  // switching from `<` to `<=` cover case when first operation for account is in reversible block
  // set and user queried for start=0 and limit=1 and because this api returns: <start, start - limit)
  // this is legit call
  if(number_of_irreversible_ops <= start)
  {
    uint32_t collectedIrreversibleBlock = 0;
    uint32_t rangeBegin = _cached_irreversible_block;
    if( BOOST_UNLIKELY( rangeBegin == 0) )
      rangeBegin = 1;
    uint32_t rangeEnd = _mainDb.head_block_num() + 1;

    auto reversibleOps = collectReversibleOps(&rangeBegin, &rangeEnd, &collectedIrreversibleBlock);

    std::vector<rocksdb_operation_object> ops_for_this_account;
    ops_for_this_account.emplace_back(); // push empty, never reachable rocksdb op object, to gently shift index by one
    const int start_offset = ops_for_this_account.size();
    ops_for_this_account.reserve(reversibleOps.size() + ops_for_this_account.size());
    for(const auto& obj : reversibleOps)
    {
      hive::protocol::operation op = fc::raw::unpack_from_buffer< hive::protocol::operation >( obj.serialized_op );
      auto impacted = getImpactedAccounts( op );
      if( std::find( impacted.begin(), impacted.end(), name) != impacted.end() )
        ops_for_this_account.push_back(obj);
    };

    // -2 is because of `size() - 1` gets last index of reversible ops and
    // next -1 is because of extra element in vector
    int64_t signed_start = static_cast<int64_t>(start);
    const int64_t last_index_of_all_account_operations = std::max<int64_t>(0l, number_of_irreversible_ops + (ops_for_this_account.size() - 1l) - start_offset);

    // this if protects from out_of_bound exception (e.x. start = static_cast<uint32_t>(-1))
    if(start > last_index_of_all_account_operations)
      signed_start = last_index_of_all_account_operations;

    // offset by one because of one extra item
    signed_start += start_offset;

    for(int i = signed_start-number_of_irreversible_ops; i>=start_offset; i--)
    {
      rocksdb_operation_object oObj = ops_for_this_account[i];
      if(processor(number_of_irreversible_ops + i, oObj))
      {
        ++count;
        if(count >= limit)
          return count;
      }
    }
  }
  return count;
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
  constexpr static uint32_t block_range_limit = 2'000;

  FC_ASSERT(blockRangeEnd > blockRangeBegin, "Block range must be upward");
  FC_ASSERT(blockRangeEnd - blockRangeBegin <= block_range_limit, "Block range distance must be less than or equal to ${lim}", ("lim", block_range_limit));

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

  uint32_t lookupUpperBound = blockRangeEnd;

  /**
   * Iterates over `reversibleOps` and returns pair of two ints, where:
   * * first - informs about last _block number_ that was touched
   *
   * * second - informs about operation id of first virtual operation over
   *      limit (0 if iterated over whole reversible set
   *      and no more virtual operations left)
   */
  const auto process_reversible_blocks = [&]{
    uint32_t last_block_number{0u};
    for(const auto& op : reversibleOps)
    {
      if (op.is_virtual)
      {
        if(limit.valid() && (cntLimit >= *limit))
        {
          return std::make_pair(op.block, (op.block != last_block_number ? 0ul : op.id));
        }else if(processor(op, op.id, false))
          ++cntLimit;
        last_block_number = op.block;
      }
    }
    return std::make_pair<uint32_t, uint64_t>(0, 0);
  };

  if(include_reversible)
  {
    auto collection = collectReversibleOps(&collectedReversibleRangeBegin, &collectedReversibleRangeEnd, &collectedIrreversibleBlock);
    reversibleOps = std::move(collection);

    dlog("EnumVirtualOps for blockRangeBegin: ${b}, blockRangeEnd: ${e}, irreversible block: ${i}",("b", blockRangeBegin)("e", blockRangeEnd)("i", collectedIrreversibleBlock));

    /// Simplest case, whole requested range is located inside reversible space
    if(blockRangeBegin > collectedIrreversibleBlock)
      return process_reversible_blocks();

    /// Partial case: rangeBegin is <= collectedIrreversibleBlock but blockRangeEnd > collectedIrreversibleBlock
    if(lookupUpperBound > collectedIrreversibleBlock)
    {
      hasTrailingReversibleBlocks = true;
      lookupUpperBound = collectedIrreversibleBlock+1; /// strip processing to full irreversible subrange
    }
    else if(lookupUpperBound == collectedIrreversibleBlock)
    {
      hasTrailingReversibleBlocks = false;
      lookupUpperBound = collectedIrreversibleBlock; /// strip processing to irreversible subrange (excluding LIB)
    }
  }

  op_by_block_num_slice_t upperBoundSlice(block_op_id_pair(lookupUpperBound, 0));

  op_by_block_num_slice_t rangeBeginSlice(block_op_id_pair(blockRangeBegin, lastProcessedOperationId));

  ReadOptions rOptions;
  rOptions.iterate_upper_bound = &upperBoundSlice;

  std::unique_ptr<::rocksdb::Iterator> it(_storage->NewIterator(rOptions, _columnHandles[Columns::OPERATION_BY_BLOCK]));

  dlog("Looking RocksDB storage for blocks from range: <${b}, ${e})", ("b", blockRangeBegin)("e", lookupUpperBound));

  for(it->Seek(rangeBeginSlice); it->Valid(); it->Next())
  {
    auto keySlice = it->key();
    const auto& key = op_by_block_num_slice_t::unpackSlice(keySlice);

    dlog("Found op. in block: ${b}", ("b", key.first));

    /// Accept only virtual operations
    if(key.second.is_virtual())
    {
      auto valueSlice = it->value();
      auto opId = id_slice_t::unpackSlice(valueSlice);

      rocksdb_operation_object op;
      bool found = find_operation_object(opId, &op);
      FC_ASSERT(found);

      ///Number of retrieved operations can't be greater then limit
      if(limit.valid() && (cntLimit >= *limit))
      {
        nextElementAfterLimit = key.second.get_id();
        lastFoundBlock = key.first;
        break;
      }

      if(processor(op, key.second.get_id(), true))
        ++cntLimit;

      lastFoundBlock = key.first;
    }
  }

  if( nextElementAfterLimit.valid() )
    return std::make_pair( lastFoundBlock, *nextElementAfterLimit );
  else
  {
    if(include_reversible && hasTrailingReversibleBlocks)
      return process_reversible_blocks();

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

      if(key.second.is_virtual())
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

  if(include_reversible)
  {
    return _mainDb.with_read_lock([this, &trxId, blockNo, txInBlock]() -> bool
    {
      const auto& volatileIdx = _mainDb.get_index< volatile_operation_index, by_block >();

      for(auto opIterator = volatileIdx.begin(); opIterator != volatileIdx.end(); ++opIterator)
      {
        if(opIterator->trx_id == trxId)
        {
          *blockNo = opIterator->block;
          *txInBlock = opIterator->trx_in_block;
          return true;
        }
      }

      return false;
    }
    );
  }

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
  ::rocksdb::BackupEngineOptions backupableDbOptions(pathString);

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
  ::rocksdb::BackupEngineOptions backupableDbOptions(pathString);

  std::unique_ptr<::rocksdb::BackupEngineReadOnly> backupEngine;
  ::rocksdb::BackupEngineReadOnly* _backupEngine = nullptr;
  auto status = ::rocksdb::BackupEngineReadOnly::Open(backupEnv, backupableDbOptions, &_backupEngine);

  checkStatus(status);

  backupEngine.reset(_backupEngine);

  ilog("Attempting to restore an AccountHistoryRocksDB backup from the backup location: `${p}'", ("p", pathString));

  shutdownDb( true );

  ilog("Starting restore of AccountHistoryRocksDB backup into storage location: ${p}.", ("p", _storagePath.string()));

  bfs::path walDir(_storagePath);
  walDir /= "WAL";
  status = backupEngine->RestoreDBFromLatestBackup(_storagePath.string(), walDir.string());
  checkStatus(status);

  ilog("Restoring AccountHistoryRocksDB backup from the location: `${p}' finished", ("p", pathString));

  openDb( false );
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
  //dlog( "RocksDB LIB set to ${l}.", ( "l", lib ) ); //too frequent
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

std::tuple<bool, bool> account_history_rocksdb_plugin::impl::createDbSchema(const bfs::path& path)
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

    appbase::app().generate_interrupt_request();

    return { false, false };/// { DB does not need data import, an application is closed }
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

  openDb( false );

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

std::string to_string(hive::chain::database::transaction_status txs)
{
  switch(txs)
  {
    case hive::chain::database::TX_STATUS_NONE:
      return "TX_STATUS_NONE";
    case hive::chain::database::TX_STATUS_UNVERIFIED:
      return "TX_STATUS_UNVERIFIED";
    case hive::chain::database::TX_STATUS_PENDING:
      return "TX_STATUS_PENDING";
    case hive::chain::database::TX_STATUS_BLOCK:
      return "TX_STATUS_BLOCK";
    case hive::chain::database::TX_STATUS_P2P_BLOCK:
      return "TX_STATUS_P2P_BLOCK";
    case hive::chain::database::TX_STATUS_GEN_BLOCK:
      return "TX_STATUS_GEN_BLOCK";
  }
  FC_ASSERT(false);

  return "broken tx status";
}

void account_history_rocksdb_plugin::impl::on_pre_apply_operation(const operation_notification& n)
{
  if( n.block % 10000 == 0 && n.trx_in_block == 0 && n.op_in_trx == 0 && !n.virtual_op)
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
    obj.is_virtual = n.virtual_op;
    obj.timestamp = _mainDb.head_block_time();
    auto size = fc::raw::pack_size( n.op );
    obj.serialized_op.resize( size );
    fc::datastream< char* > ds( obj.serialized_op.data(), size );
    fc::raw::pack( ds, n.op );

    importOperation( obj, impacted );
  }
  else
  {
    auto txs = _mainDb.get_tx_status();
    //const auto& newOp =
    _mainDb.create< volatile_operation_object >( [&]( volatile_operation_object& o )
    {
      o.trx_id = n.trx_id;
      o.block = n.block;
      o.trx_in_block = n.trx_in_block;
      o.op_in_trx = n.op_in_trx;
      o.is_virtual = n.virtual_op;
      o.timestamp = _mainDb.head_block_time();
      auto size = fc::raw::pack_size( n.op );
      o.serialized_op.resize( size );
      fc::datastream< char* > ds( o.serialized_op.data(), size );
      fc::raw::pack( ds, n.op );
      o.impacted.insert( o.impacted.end(), impacted.begin(), impacted.end() );
      if( txs == database::TX_STATUS_NONE && o.is_virtual ) //vop from automatic state processing
        o.transaction_status = database::TX_STATUS_BLOCK;
      else
        o.transaction_status = txs;
    });

    //dlog("Adding operation: id ${id}, block: ${b}, tx_status: ${txs}, body: ${body}", ("id", newOp.get_id())("b", n.block)
    //  ("txs", to_string(txs))("body", fc::json::to_string(n.op)));
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

  /// Here is made assumption, that thread processing block/transaction and sending this notification ALREADY holds write lock.
  /// Usually this lock is held by chain_plugin::start_write_processing() function (the write_processing thread).

  auto& volatileOpsGenericIndex = _mainDb.get_mutable_index<volatile_operation_index>();

  const auto& volatile_idx = _mainDb.get_index< volatile_operation_index, by_block >();

  /// Range of reversible (volatile) ops to be processed should come from blocks (_cached_irreversible_block, block_num]
  auto moveRangeBeginI = volatile_idx.upper_bound( _cached_irreversible_block );

  FC_ASSERT(moveRangeBeginI == volatile_idx.begin() || moveRangeBeginI == volatile_idx.end(), "All volatile ops processed by previous irreversible blocks should be already flushed");

  auto moveRangeEndI = volatile_idx.upper_bound(block_num);
  FC_ASSERT( block_num > _cached_reindex_point || moveRangeBeginI == moveRangeEndI,
    "There should be no volatile data for block ${b} since it was processed during reindex up to ${r}",
    ( "b", block_num )( "r", _cached_reindex_point ) );

  volatileOpsGenericIndex.move_to_external_storage<by_block>(moveRangeBeginI, moveRangeEndI, [this](const volatile_operation_object& operation) -> bool
    {
      auto txs = static_cast<hive::chain::database::transaction_status>(operation.transaction_status);
      if(txs & hive::chain::database::TX_STATUS_BLOCK)
      {
        //dlog("Flushing operation: id ${id}, block: ${b}, tx_status: ${txs}", ("id", operation.get_id())("b", operation.block)
        //  ("txs", to_string(txs)));
        rocksdb_operation_object obj(operation);
        importOperation(obj, operation.impacted);
        return true; /// Allow move_to_external_storage internals to erase this object
      }
      else
      {
        dlog("Skipped operation: id ${id}, block: ${b}, tx_status: ${txs}, body: ${body}", ("id", operation.get_id())("b", operation.block)
          ("txs", to_string(txs))("body", fc::json::to_string(fc::raw::unpack_from_buffer< hive::protocol::operation >(operation.serialized_op))));

        return false; /// Disallow object erasing inside move_to_external_storage internals
      }
    }
  );

  update_lib(block_num);
  //flushStorage(); it is apparently needed to properly write LIB so it can be read later, however it kills performance - alternative solution used currently just masks problem
}

void account_history_rocksdb_plugin::impl::on_post_apply_block(const block_notification& bn)
{
  if (_balance_csv_filename)
  {
    // check the balances of all tracked accounts, emit a line in the CSV file if any have changed
    // since the last block
    const auto& account_idx = _mainDb.get_index<hive::chain::account_index>().indices().get<hive::chain::by_name>();
    for (auto range : _filter.get_tracked_accounts())
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
                            << bn.block_num << ","
                            << bn.get_block_timestamp().to_iso_string() << ","
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
    ("account-history-rocksdb-stop-import-at-block", bpo::value<uint32_t>()->default_value(0),
      "Allows to specify block number, the data import process should stop at.")
    ("account-history-rocksdb-dump-balance-history", boost::program_options::value< string >(), "Dumps balances for all tracked accounts to a CSV file every time they change")
  ;
}

void account_history_rocksdb_plugin::plugin_initialize(const boost::program_options::variables_map& options)
{
  if(options.count("account-history-rocksdb-stop-import-at-block"))
    _blockLimit = options.at("account-history-rocksdb-stop-import-at-block").as<uint32_t>();

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

  _my->openDb(_destroyOnStartup);
}

void account_history_rocksdb_plugin::plugin_startup()
{
  ilog("Starting up account_history_rocksdb_plugin...");
}

void account_history_rocksdb_plugin::plugin_shutdown()
{
  ilog("Shutting down account_history_rocksdb_plugin...");
  _my->shutdownDb(_destroyOnShutdown);
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
