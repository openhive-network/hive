
#include <hive/chain/hive_fwd.hpp>

#include <hive/plugins/account_history_rocksdb/account_history_rocksdb_plugin.hpp>

#include <hive/chain/database.hpp>
#include <hive/chain/hive_objects.hpp>
#include <hive/chain/index.hpp>
#include <hive/chain/util/impacted.hpp>
#include <hive/chain/util/supplement_operations.hpp>
#include <hive/chain/util/data_filter.hpp>
#include <hive/chain/util/type_registrar_definition.hpp>

#include <hive/plugins/chain/chain_plugin.hpp>

#include <hive/chain/external_storage/state_snapshot_provider.hpp>
#include <hive/chain/external_storage/rocksdb_snapshot.hpp>
#include <hive/chain/external_storage/types.hpp>
#include <hive/plugins/account_history_rocksdb/rocksdb_ah_storage_provider.hpp>

#include <hive/utilities/benchmark_dumper.hpp>
#include <hive/utilities/signal.hpp>

#include <appbase/application.hpp>

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

#include <condition_variable>
#include <mutex>

#include <memory>
#include <limits>
#include <string>
#include <typeindex>
#include <typeinfo>

namespace bpo = boost::program_options;

#define HIVE_NAMESPACE_PREFIX "hive::protocol::"

#define WRITE_BUFFER_FLUSH_LIMIT     10
#define ACCOUNT_HISTORY_LENGTH_LIMIT 30
#define ACCOUNT_HISTORY_TIME_LIMIT   30

namespace hive { namespace plugins { namespace account_history_rocksdb {

using hive::protocol::account_name_type;
using hive::protocol::block_id_type;
using hive::protocol::operation;

using hive::chain::operation_notification;
using hive::chain::transaction_id_type;

using hive::protocol::legacy_asset;

using ::rocksdb::DB;
using ::rocksdb::PinnableSlice;
using ::rocksdb::ReadOptions;
using ::rocksdb::Slice;
using ::rocksdb::ColumnFamilyDescriptor;
using ::rocksdb::ColumnFamilyHandle;

namespace
{
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

} /// anonymous

class account_history_rocksdb_plugin::impl final
{
public:
  impl( account_history_rocksdb_plugin& self, const bpo::variables_map& options, const bfs::path& storagePath, appbase::application& app ) :
    _self(self),
    _mainDb(app.get_plugin<hive::plugins::chain::chain_plugin>().db()),
    _blockchainStoragePath(app.get_plugin<hive::plugins::chain::chain_plugin>().state_storage_dir()),
    _storagePath(storagePath),
    _block_reader( app.get_plugin< hive::plugins::chain::chain_plugin >().block_reader() ),
    _filter("ah-rb"),
    theApp( app )
  {
    _provider = std::make_shared<rocksdb_ah_storage_provider>( _blockchainStoragePath, _storagePath, theApp );
    _snapshot = std::shared_ptr<rocksdb_snapshot>(
      new rocksdb_snapshot( "Account History RocksDB", "account_history_rocksdb_data", _mainDb, _storagePath, _provider ) );

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
      _snapshot->save_snapshot(note);
    }, _self, 0);

    _mainDb.add_snapshot_supplement_handler([&](const hive::chain::load_snapshot_supplement_notification& note) -> void
    {
      _snapshot->load_snapshot(note);
    }, _self, 0);

    _on_pre_apply_operation_con = _mainDb.add_pre_apply_operation_handler( [&]( const operation_notification& note )
    {
      on_pre_apply_operation(note);
    }, _self );

    _on_irreversible_block_conn = _mainDb.add_irreversible_block_handler( [&]( uint32_t block_num )
    {
      on_irreversible_block( block_num );
    }, _self );

    _on_post_apply_block_conn = _mainDb.add_post_apply_block_handler( [&](const block_notification& bn)
    {
      on_post_apply_block(bn);
    }, _self );

    _on_fail_apply_block_conn = _mainDb.add_fail_apply_block_handler( [&](const block_notification& bn)
    {
      on_post_apply_block(bn);
    }, _self );

    HIVE_ADD_PLUGIN_INDEX(_mainDb, volatile_operation_index);
  }

  void init()
  {
    if( not _initialized )
    {
      _initialized = true;
      _provider->openDb( _mainDb.get_last_irreversible_block_num() );
    }
  }

  ~impl()
  {
    hive::utilities::disconnect_signal(_on_pre_apply_operation_con);
    hive::utilities::disconnect_signal(_on_irreversible_block_conn);
    hive::utilities::disconnect_signal(_on_post_apply_block_conn);
    hive::utilities::disconnect_signal(_on_fail_apply_block_conn);

    _provider->shutdownDb();
    _initialized = false;
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

  void shutdownDb();

  bfs::path get_storage_path() const { return _storagePath; }

private:

  uint64_t build_next_operation_id(const rocksdb_operation_object& obj, const hive::protocol::operation& processed_op)
  {
    auto number_in_block = _provider->get_operationSeqId();
    _provider->set_operationSeqId( _provider->get_operationSeqId() + 1 );

    //msb.....................lsb
    // || block | seq | type ||
    // ||  32b  | 24b |  8b  ||
    constexpr auto  TYPE_ID_LIMIT = 255; // 2^8-1
    constexpr auto NUMBER_IN_BLOCK_LIMIT = 16777215; // 2^24-1
    FC_ASSERT(processed_op.which() <= TYPE_ID_LIMIT, "Operation type is to large to fit in 8 bits");
    FC_ASSERT(number_in_block <= NUMBER_IN_BLOCK_LIMIT, "Operation in block number is to large to fit in 24 bits");
    uint64_t operation_id = obj.block;
    operation_id <<= 32;
    operation_id |= (number_in_block << 8);
    operation_id |= processed_op.which();

    return operation_id;
  }

  template< typename T >
  void importOperation( rocksdb_operation_object& obj, const hive::protocol::operation& processed_op, const T& impacted )
  {
    if(_lastTx != obj.trx_id && obj.trx_id != transaction_id_type()/*An virtual operation shouldn't increase `_txNo` counter.*/ )
    {
      ++_txNo;
      _lastTx = obj.trx_id;
      storeTransactionInfo(obj.trx_id, obj.block, obj.trx_in_block);
    }

    obj.id = build_next_operation_id(obj, processed_op);

    serialize_buffer_t serializedObj;
    auto size = fc::raw::pack_size(obj);
    serializedObj.resize(size);
    {
      fc::datastream<char*> ds(serializedObj.data(), size);
      fc::raw::pack(ds, obj);
    }

    id_slice_t idSlice(obj.id);
    auto s = _provider->getCachableWriteBuffer().Put(_provider->getColumnHandle( Columns::OPERATION_BY_ID ), idSlice, Slice(serializedObj.data(), serializedObj.size()));
    checkStatus(s);

    op_by_block_num_slice_t blockLocSlice(block_op_id_pair(obj.block, operation_id_vop_pair(obj.id, obj.is_virtual)));

    s = _provider->getCachableWriteBuffer().Put(_provider->getColumnHandle( Columns::OPERATION_BY_BLOCK ), blockLocSlice, idSlice);
    checkStatus(s);

    for(const auto& name : impacted)
      buildAccountHistoryRecord( name, obj );

    _provider->set_collectedOps( _provider->get_collectedOps() + 1 );
    if(_provider->get_collectedOps() >= _collectedOpsWriteLimit)
      _provider->flushWriteBuffer();

    ++_totalOps;
  }

  void buildAccountHistoryRecord( const account_name_type& name, const rocksdb_operation_object& obj );
  void storeTransactionInfo(const chain::transaction_id_type& trx_id, uint32_t blockNo, uint32_t trx_in_block);

  void prunePotentiallyTooOldItems(account_history_info* ahInfo, const account_name_type& name,
    const fc::time_point_sec& now);

  void storeSequenceIds()
  {
    Slice ahSeqIdName("AH_SEQ_ID");

    hive::chain::id_slice_t ahId( _provider->get_accountHistorySeqId() );

    auto s = _provider->getCachableWriteBuffer().Put(ahSeqIdName, ahId);
    checkStatus(s);
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
  const hive::chain::block_read_i& _block_reader;

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

  /** Limit which value depends on block data source:
    *    - if blocks come from network, there is no need for delaying write, becasue they appear quite rare (limit == 1)
    *    - if reindex process or direct import has been spawned, this massive operation can need reduction of direct
        writes (limit == WRITE_BUFFER_FLUSH_LIMIT).
    */
  unsigned int                     _collectedOpsWriteLimit = 1;

  account_filter                   _filter;
  flat_set<std::string>            _op_list;
  flat_set<std::string>            _blacklisted_op_list;

  bool                             _reindexing = false;

  bool                             _prune = false;

  /**
    * Lazy init flag. The problem is, AH database cannot be opened during plugin_initialize, because LIB
    * value is not yet available. It cannot be opened during plugin_startup either, because it might already
    * be opened for replay. But it has to be opened when replay is not executed.
    */
  bool                             _initialized = false;

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

  std::optional<std::pair<uint32_t, fc::time_point_sec>> _last_block_and_timestamp;

  appbase::application& theApp;

  rocksdb_ah_storage_provider::ptr  _provider;
  external_storage_snapshot::ptr    _snapshot;
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

  theApp.get_plugin< chain::chain_plugin >().report_state_options( _self.name(), state_opts );
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
    *collectedIrreversibleBlock = _provider->get_cached_irreversible_block();

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

  account_name_slice_t nameSlice(name.data);
  PinnableSlice buffer;
  auto s = _provider->getStorage()->Get(rOptions, _provider->getColumnHandle( Columns::AH_INFO_BY_NAME ), nameSlice, &buffer);

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

  std::unique_ptr<::rocksdb::Iterator> it(_provider->getStorage()->NewIterator(rOptions, _provider->getColumnHandle( Columns::AH_OPERATION_BY_ID )));

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
    uint32_t rangeBegin = _provider->get_cached_irreversible_block();
    if( BOOST_UNLIKELY( rangeBegin == 0) )
      rangeBegin = 1;
    uint32_t rangeEnd = _mainDb.head_block_num() + 1;

    const auto reversibleOps = collectReversibleOps(&rangeBegin, &rangeEnd, &collectedIrreversibleBlock);

    std::vector<rocksdb_operation_object> ops_for_this_account;
    ops_for_this_account.reserve(reversibleOps.size());
    for(const auto& obj : reversibleOps)
    {
      hive::protocol::operation op = fc::raw::unpack_from_buffer< hive::protocol::operation >( obj.serialized_op );
      auto impacted = getImpactedAccounts( op );
      if( std::find( impacted.begin(), impacted.end(), name) != impacted.end() )
        ops_for_this_account.push_back(obj);
    };

    // There's always at least one operation for each account: account_create_operation
    FC_ASSERT(number_of_irreversible_ops + ops_for_this_account.size() > 0);
    // Cannot be negative due to above
    const uint64_t last_index_of_all_account_operations = (number_of_irreversible_ops + ops_for_this_account.size()) - 1l;
    // this if protects from out_of_bound exception (e.x. start = static_cast<uint32_t>(-1))
    const uint64_t start_min = std::min(start, last_index_of_all_account_operations);

    /**
     * Iterate over range [0, last) of ops_for_this_account and pass them to processor
     * with indices [number_of_irreversible_ops, number_of_irreversible_ops+1, ...] in reverse order.
     * last has +1, because we want to include element at index start_min.
     */
    const uint64_t last = start_min - number_of_irreversible_ops + 1;
    FC_ASSERT(last <= ops_for_this_account.size());
    using namespace boost::adaptors;
    for (const auto [idx, oObj] : ops_for_this_account | sliced(0, last) | indexed(number_of_irreversible_ops) | reversed)
    {
      if(processor(idx, oObj))
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
  ::rocksdb::Status s = _provider->getStorage()->Get(ReadOptions(), _provider->getColumnHandle( Columns::OPERATION_BY_ID), idSlice, &data);

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

  std::unique_ptr<::rocksdb::Iterator> it(_provider->getStorage()->NewIterator(ReadOptions(), _provider->getColumnHandle( Columns::OPERATION_BY_BLOCK )));
  uint32_slice_t blockNumSlice(blockNum);
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
      if (op.is_virtual && op.id >= lastProcessedOperationId)
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

  std::unique_ptr<::rocksdb::Iterator> it(_provider->getStorage()->NewIterator(rOptions, _provider->getColumnHandle( Columns::OPERATION_BY_BLOCK )));

  dlog("Looking RocksDB storage for blocks from range: <${b}, ${e})", ("b", blockRangeBegin)("e", lookupUpperBound));

  for(it->Seek(rangeBeginSlice); it->Valid(); it->Next())
  {
    auto keySlice = it->key();
    const auto& key = op_by_block_num_slice_t::unpackSlice(keySlice);

    dlog("Found op. in block: ${b}", ("b", key.first));

    /// Accept only virtual operations
    if(key.second.is_virtual() && key.second.get_id() >= lastProcessedOperationId)
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
    it.reset(_provider->getStorage()->NewIterator(rOptions, _provider->getColumnHandle( Columns::OPERATION_BY_BLOCK )));

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
  ::rocksdb::Status s = _provider->getStorage()->Get(rOptions, _provider->getColumnHandle( Columns::BY_TRANSACTION_ID ), idSlice, &dataBuffer);

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

void account_history_rocksdb_plugin::impl::shutdownDb()
{
  _provider->shutdownDb();
}

void account_history_rocksdb_plugin::impl::buildAccountHistoryRecord( const account_name_type& name, const rocksdb_operation_object& obj )
{
  std::string strName = name;

  ReadOptions rOptions;
  //rOptions.tailing = true;

  account_name_slice_t nameSlice(name.data);

  account_history_info ahInfo;
  bool found = _provider->getCachableWriteBuffer().getAHInfo(name, &ahInfo);

  if(found)
  {
    auto count = ahInfo.getAssociatedOpCount();

    if(_prune && count > ACCOUNT_HISTORY_LENGTH_LIMIT &&
      ((obj.timestamp - ahInfo.oldestEntryTimestamp) > fc::days(ACCOUNT_HISTORY_TIME_LIMIT)))
      {
        prunePotentiallyTooOldItems(&ahInfo, name, obj.timestamp);
      }

    auto nextEntryId = ++ahInfo.newestEntryId;
    _provider->getCachableWriteBuffer().putAHInfo(name, ahInfo);

    ah_op_by_id_slice_t ahInfoOpSlice(std::make_pair(ahInfo.id, nextEntryId));
    id_slice_t valueSlice(obj.id);
    auto s = _provider->getCachableWriteBuffer().Put(_provider->getColumnHandle( Columns::AH_OPERATION_BY_ID ), ahInfoOpSlice, valueSlice);
    checkStatus(s);
  }
  else
  {
    /// New entry must be created - there is first operation recorded.
    ahInfo.id = _provider->get_accountHistorySeqId();
    _provider->set_accountHistorySeqId( _provider->get_accountHistorySeqId() + 1 );
    ahInfo.newestEntryId = ahInfo.oldestEntryId = 0;
    ahInfo.oldestEntryTimestamp = obj.timestamp;

    _provider->getCachableWriteBuffer().putAHInfo(name, ahInfo);

    ah_op_by_id_slice_t ahInfoOpSlice(std::make_pair(ahInfo.id, 0));
    id_slice_t valueSlice(obj.id);
    auto s = _provider->getCachableWriteBuffer().Put(_provider->getColumnHandle( Columns::AH_OPERATION_BY_ID ), ahInfoOpSlice, valueSlice);
    checkStatus(s);
  }
}

void account_history_rocksdb_plugin::impl::storeTransactionInfo(const chain::transaction_id_type& trx_id, uint32_t blockNo, uint32_t trx_in_block)
  {
  TransactionIdSlice txSlice(trx_id);
  block_no_tx_in_block_pair block_no_tx_no(blockNo, trx_in_block);
  block_no_tx_in_block_slice_t valueSlice(block_no_tx_no);

  auto s = _provider->getCachableWriteBuffer().Put(_provider->getColumnHandle( Columns::BY_TRANSACTION_ID ), txSlice, valueSlice);
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

  auto s = _provider->getCachableWriteBuffer().SingleDelete(_provider->getColumnHandle( Columns::AH_OPERATION_BY_ID ), oldestEntrySlice);
  checkStatus(s);

  std::unique_ptr<::rocksdb::Iterator> dataItr(_provider->getStorage()->NewIterator(rOptions, _provider->getColumnHandle( Columns::AH_OPERATION_BY_ID )));

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
      s = _provider->getCachableWriteBuffer().SingleDelete(_provider->getColumnHandle( Columns::OPERATION_BY_BLOCK ), rightBoundarySlice);
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
  _provider->shutdownDb();
  std::string strPath = _storagePath.string();

  if( note.force_replay )
  {
    ilog("Received onReindexStart request, attempting to clean database storage.");
    auto s = ::rocksdb::DestroyDB(strPath, ::rocksdb::Options());
    checkStatus(s);
  }

  _provider->openDb( _mainDb.get_last_irreversible_block_num() );
  _initialized = true; // prevent reopening of database during plugin_startup

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

  _provider->flushStorage();
  _collectedOpsWriteLimit = 1;
  _reindexing = false;
  uint32_t last_irreversible_block_num = _mainDb.get_last_irreversible_block_num();
  _provider->update_lib( last_irreversible_block_num );
  FC_ASSERT( last_irreversible_block_num == note.last_block_number, "Last replayed block should be current LIB" );

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
  if ( _mainDb.is_reapplying_tx() || _mainDb.is_validating_one_tx() )
  {
    return;
  }

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
    obj.timestamp = n.timestamp;
    auto size = fc::raw::pack_size( n.op );
    obj.serialized_op.resize( size );
    fc::datastream< char* > ds( obj.serialized_op.data(), size );
    fc::raw::pack( ds, n.op );

    importOperation( obj, n.op, impacted );
  }
  else
  {
    auto txs = _mainDb.get_tx_status();
    _mainDb.create< volatile_operation_object >( [&]( volatile_operation_object& o )
    {
      o.trx_id = n.trx_id;
      o.block = n.block;
      o.trx_in_block = n.trx_in_block;
      o.op_in_trx = n.op_in_trx;
      o.is_virtual = n.virtual_op;
      o.timestamp = n.timestamp;
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

  /// Here is made assumption, that thread processing block/transaction and sending this notification ALREADY holds write lock.
  /// Usually this lock is held by chain_plugin::start_write_processing() function (the write_processing thread).

  auto& volatileOpsGenericIndex = _mainDb.get_mutable_index<volatile_operation_index>();

  const auto& volatile_idx = _mainDb.get_index< volatile_operation_index, by_block >();

  /// Range of reversible (volatile) ops to be processed should come from blocks (_cached_irreversible_block, block_num]
  auto moveRangeBeginI = volatile_idx.upper_bound( _provider->get_cached_irreversible_block() );

  FC_ASSERT(moveRangeBeginI == volatile_idx.begin() || moveRangeBeginI == volatile_idx.end(), "All volatile ops processed by previous irreversible blocks should be already flushed");

  auto moveRangeEndI = volatile_idx.upper_bound(block_num);

  volatileOpsGenericIndex.move_to_external_storage<by_block>(moveRangeBeginI, moveRangeEndI, [this](const volatile_operation_object& operation) -> bool
    {
      auto txs = static_cast<hive::chain::database::transaction_status>(operation.transaction_status);
      if(txs & hive::chain::database::TX_STATUS_BLOCK)
      {
        //dlog("Flushing operation: id ${id}, block: ${b}, tx_status: ${txs}", ("id", operation.get_id())("b", operation.block)
        //  ("txs", to_string(txs)));
        rocksdb_operation_object obj(operation);
        obj.timestamp = operation.timestamp;
        hive::protocol::operation hive_op = fc::raw::unpack_from_buffer< hive::protocol::operation >(operation.serialized_op);
        importOperation(obj, hive_op, operation.impacted);
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

  _provider->update_lib(block_num);
  //flushStorage(); it is apparently needed to properly write LIB so it can be read later, however it kills performance - alternative solution used currently just masks problem
}

void account_history_rocksdb_plugin::impl::on_post_apply_block(const block_notification& bn)
{
  if (_balance_csv_filename)
  {
    // check the balances of all tracked accounts, emit a line in the CSV file if any have changed
    // since the last block
    const auto& account_idx = _mainDb.get_index<hive::chain::tiny_account_index>().indices().get<hive::chain::by_name>();
    for (auto range : _filter.get_tracked_accounts())
    {
      const std::string& lower = range.first;
      const std::string& upper = range.second;

      auto account_iter = account_idx.lower_bound(range.first);
      while (account_iter != account_idx.end() &&
             account_iter->get_name() <= upper)
      {
        const auto& account = _mainDb.get_account( account_iter->get_name() );

        auto saved_balance_iter = _saved_balances.find(account.get_name());
        bool balances_changed = saved_balance_iter == _saved_balances.end();
        saved_balances& saved_balance_record = _saved_balances[account.get_name()];

        if (saved_balance_record.hive_balance != account.get_balance())
        {
          saved_balance_record.hive_balance = account.get_balance();
          balances_changed = true;
        }
        if (saved_balance_record.savings_hive_balance != account.get_savings())
        {
          saved_balance_record.savings_hive_balance = account.get_savings();
          balances_changed = true;
        }
        if (saved_balance_record.hbd_balance != account.get_hbd_balance())
        {
          saved_balance_record.hbd_balance = account.get_hbd_balance();
          balances_changed = true;
        }
        if (saved_balance_record.savings_hbd_balance != account.get_hbd_savings())
        {
          saved_balance_record.savings_hbd_balance = account.get_hbd_savings();
          balances_changed = true;
        }

        if (saved_balance_record.reward_hbd_balance != account.get_hbd_rewards())
        {
          saved_balance_record.reward_hbd_balance = account.get_hbd_rewards();
          balances_changed = true;
        }
        if (saved_balance_record.reward_hive_balance != account.get_rewards())
        {
          saved_balance_record.reward_hive_balance = account.get_rewards();
          balances_changed = true;
        }
        if (saved_balance_record.reward_vesting_balance != account.get_vest_rewards())
        {
          saved_balance_record.reward_vesting_balance = account.get_vest_rewards();
          balances_changed = true;
        }
        if (saved_balance_record.reward_vesting_hive_balance != account.get_vest_rewards_as_hive())
        {
          saved_balance_record.reward_vesting_hive_balance = account.get_vest_rewards_as_hive();
          balances_changed = true;
        }

        if (saved_balance_record.vesting_shares != account.get_vesting())
        {
          saved_balance_record.vesting_shares = account.get_vesting();
          balances_changed = true;
        }
        if (saved_balance_record.delegated_vesting_shares != account.get_delegated_vesting())
        {
          saved_balance_record.delegated_vesting_shares = account.get_delegated_vesting();
          balances_changed = true;
        }
        if (saved_balance_record.received_vesting_shares != account.get_received_vesting())
        {
          saved_balance_record.received_vesting_shares = account.get_received_vesting();
          balances_changed = true;
        }

        if (balances_changed)
        {
          _balance_csv_file << (string)account.get_name() << ","
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

  _provider->set_operationSeqId(0);
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
    auto basePath = get_app().data_dir();
    auto actualPath = basePath / dbPath;
    dbPath = actualPath;
  }

  _my = std::make_unique<impl>( *this, options, dbPath, get_app() );
}

void account_history_rocksdb_plugin::plugin_startup()
{
  ilog("Starting up account_history_rocksdb_plugin...");
  _my->init();
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

bfs::path account_history_rocksdb_plugin::storage_dir() const
{
  return _my->get_storage_path();
}

} } }

HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::plugins::account_history_rocksdb::volatile_operation_index)
