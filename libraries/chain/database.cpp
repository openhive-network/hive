#include <hive/chain/hive_fwd.hpp>

#include <appbase/application.hpp>

#include <hive/protocol/transaction_util.hpp>
#include <hive/protocol/hbd_interest.hpp>

#include <hive/chain/account_object_multiindex.hpp>
#include <hive/chain/block_summary_object_multiindex.hpp>
#include <hive/chain/hardfork_property_object_multiindex.hpp>
#include <hive/chain/compound.hpp>
#include <hive/chain/database.hpp>
#include <hive/chain/database_exceptions.hpp>
#include <hive/chain/db_with.hpp>
#include <hive/chain/dhf_objects.hpp>
#include <hive/chain/evaluator_registry.hpp>
#include <hive/chain/smt_objects.hpp>
#include <hive/chain/hive_evaluator.hpp>
#include <hive/chain/detail/state/liquidity_reward_balance_object_multiindex.hpp>
#include <hive/chain/detail/state/feed_history_object_multiindex.hpp>
#include <hive/chain/detail/state/limit_order_object_multiindex.hpp>
#include <hive/chain/detail/state/withdraw_vesting_route_object_multiindex.hpp>
#include <hive/chain/detail/state/decline_voting_rights_request_object_multiindex.hpp>
#include <hive/chain/detail/state/reward_fund_object_multiindex.hpp>
#include <hive/chain/witness_objects_multiindex.hpp>
#include <hive/chain/transaction_object_multiindex.hpp>
#include <hive/chain/shared_db_merkle.hpp>
#include <hive/chain/witness_schedule.hpp>

#include "database_impl.hpp"
#include "database_vesting_helpers.hpp"

#include <hive/chain/util/asset.hpp>
#include <hive/chain/util/reward.hpp>
#include <hive/chain/util/uint256.hpp>
#include <hive/chain/util/manabar.hpp>
#include <hive/chain/util/rd_setup.hpp>
#include <hive/chain/util/dhf_processor.hpp>
#include <hive/chain/util/delayed_voting.hpp>
#include <hive/chain/util/decoded_types_data_storage.hpp>
#include <hive/chain/util/impacted.hpp>

#include <hive/chain/rc/rc_objects.hpp>

#include <fc/uint128.hpp>

#include <fc/container/deque.hpp>

#include <fc/io/fstream.hpp>

#include <boost/scope_exit.hpp>

#include <iostream>

#include <cstdint>
#include <deque>
#include <fstream>
#include <functional>
#include <stdlib.h>

long next_hf_time()
{
  // current "next hardfork" is HF28
  long hfTime =

#ifdef IS_TEST_NET
    1675818000; // Wednesday, February 8, 2023 1:00:00 AM
#else
    1763557200; // Wed Nov 19 2025 13:00:00 GMT+0000
#endif /// IS_TEST_NET

  const char* value = getenv("HIVE_HF28_TIME");
  if (value)
  {
    hfTime = atol(value);
    ilog("HIVE_HF28_TIME has been specified through environment variable as ${value}, long value: ${hfTime}", (value)(hfTime));
  }

  return hfTime;
}

namespace hive { namespace chain {

struct object_schema_repr
{
  std::pair< uint16_t, uint16_t > space_type;
  std::string type;
};

struct operation_schema_repr
{
  std::string id;
  std::string type;
};

struct db_schema
{
  std::map< std::string, std::string > types;
  std::vector< object_schema_repr > object_types;
  std::string operation_type;
  std::vector< operation_schema_repr > custom_operation_types;
};

} }

FC_REFLECT( hive::chain::object_schema_repr, (space_type)(type) )
FC_REFLECT( hive::chain::operation_schema_repr, (id)(type) )
FC_REFLECT( hive::chain::db_schema, (types)(object_types)(operation_type)(custom_operation_types) )

namespace hive { namespace chain {

database_impl::database_impl( database& self ) : _self(self), _evaluator_registry(self) {}

void database_impl::register_new_type(util::abstract_type_registrar& r)
{
  r.register_type(*_decoded_types_data_storage);
}

void database_impl::delete_decoded_types_data_storage()
{
  FC_ASSERT(_decoded_types_data_storage);
  _decoded_types_data_storage.reset();
}

database::database( appbase::application& app )
  : rc(*this), _my( new database_impl(*this) ), theApp( app )
{}

void database::begin_type_register_process(util::abstract_type_registrar& r)
{
  _my->register_new_type(r);
}

database::~database()
{
  clear_pending();
}

void database::pre_open( const open_args& args )
{
  try
  {
    init_schema();
    helpers::environment_extension_resources environment_extension(
                                                theApp.get_version_string(),
                                                theApp.get_plugins_names(),
                                                []( const std::string& message ){ wlog( message.c_str() ); }
                                              );
    ilog("Opening/creating shared memory from ${path}", ("path", args.shared_mem_dir.generic_string()));
    chainbase::database::open( args.shared_mem_dir, args.chainbase_flags, args.shared_file_size, args.database_cfg, &environment_extension );
    initialize_irreversible_storage();
  }
  FC_CAPTURE_LOG_AND_RETHROW( (args.data_dir)(args.shared_mem_dir)(args.shared_file_size) )
}

void database::open( const open_args& args )
{
  try
  {
    FC_ASSERT(get_is_open(), "database::pre_open must be called first!");
    initialize_state_independent_data(args);
    _block_writer->on_state_independent_data_initialized();
    load_state_initial_data(args);
    verify_match_of_blockchain_configuration();
  }
  FC_CAPTURE_LOG_AND_RETHROW( (args.data_dir)(args.shared_mem_dir)(args.shared_file_size) )
}

void database::initialize_state_independent_data(const open_args& args)
{
  _my->create_new_decoded_types_data_storage();
  _my->_decoded_types_data_storage->register_new_type<irreversible_block_data_type>();

  initialize_indexes();
  get_comments_handler().open();
  verify_match_of_state_objects_definitions_from_shm();
  initialize_evaluators();

  if(!find< dynamic_global_property_object >())
  {
    with_write_lock([&]()
    {
      init_genesis();
    });
  }

  _benchmark_dumper.set_enabled(args.benchmark_is_enabled);
  if( _benchmark_dumper.is_enabled() &&
      ( !_pre_apply_operation_signal.empty() || !_post_apply_operation_signal.empty() ) )
  {
    wlog( "BENCHMARK will run into nested measurements - data on operations that emit vops will be lost!!!" );
  }

  _shared_file_full_threshold = args.shared_file_full_threshold;
  _shared_file_scale_rate = args.shared_file_scale_rate;

  /// Initialize all static (state independent) specific to hardforks
  init_hardforks();
}

void database::load_state_initial_data(const open_args& args)
{
  uint32_t hb = head_block_num();
  uint32_t last_irreversible_block = get_last_irreversible_block_num();

  FC_ASSERT(hb >= last_irreversible_block);

  ilog("Loaded a blockchain database holding a state specific to head block: ${hb} and last irreversible block: ${last_irreversible_block}", (hb)(last_irreversible_block));

  // Rewind all undo state. This should return us to the state at the last irreversible block.
  with_write_lock([&]() {
    ilog("Attempting to rewind all undo state...");

    undo_all();

    ilog("Rewind undo state done.");

    uint32_t new_hb = head_block_num();
    notify_switch_fork( new_hb );


    FC_ASSERT(new_hb >= last_irreversible_block);

    FC_ASSERT(get_last_irreversible_block_num() == last_irreversible_block, "Undo operation should not touch irreversible block value");

    ilog("Blockchain state database is AT IRREVERSIBLE state specific to head block: ${hb} and LIB: ${lb}",
         ("hb", head_block_num())("lb", this->get_last_irreversible_block_num()));

    FC_ASSERT(revision() == head_block_num(), "Chainbase revision does not match head block num.",
             ("rev", revision())("head_block", head_block_num()));
    if (args.do_validate_invariants)
      validate_invariants();
  });

  if (head_block_num())
  {
    std::shared_ptr<full_block_type> head_block = 
      _block_writer->get_block_reader().read_block_by_num(head_block_num());
    // This assertion should be caught and a reindex should occur
    FC_ASSERT(head_block && head_block->get_block_id() == head_block_id(),
    "Chain state {\"block-number\": ${block_number1} \"id\":\"${block_hash1}\"} does not match block log {\"block-number\": ${block_number2} \"id\":\"${block_hash2}\"}. Please reindex blockchain.",
    ("block_number1", head_block_num())("block_hash1", head_block_id())("block_number2", head_block ? head_block->get_block_num() : 0)("block_hash2", head_block ? head_block->get_block_id() : block_id_type()));
  }

  with_read_lock([&]() {
    const auto& hardforks = get_hardfork_property_object();
    FC_ASSERT(hardforks.last_hardfork <= HIVE_NUM_HARDFORKS, "Chain knows of more hardforks than configuration", ("hardforks.last_hardfork", hardforks.last_hardfork)("HIVE_NUM_HARDFORKS", HIVE_NUM_HARDFORKS));
    FC_ASSERT(_hardfork_versions.versions[hardforks.last_hardfork] <= HIVE_BLOCKCHAIN_VERSION, "Blockchain version is older than last applied hardfork");
    FC_ASSERT(HIVE_BLOCKCHAIN_HARDFORK_VERSION >= HIVE_BLOCKCHAIN_VERSION);
    FC_ASSERT(HIVE_BLOCKCHAIN_HARDFORK_VERSION == _hardfork_versions.versions[HIVE_NUM_HARDFORKS], "Blockchain version mismatch", (HIVE_BLOCKCHAIN_HARDFORK_VERSION)(_hardfork_versions.versions[HIVE_NUM_HARDFORKS]));
  });

#ifdef USE_ALTERNATE_CHAIN_ID
  /// Leave the chain-id passed to cmdline option.
#else
  with_read_lock([&]() {
    const auto& hardforks = get_hardfork_property_object();
    if(hardforks.last_hardfork >= HIVE_HARDFORK_1_24)
    {
      ilog("Loaded blockchain which had already processed hardfork 24, setting Hive chain id");
      set_chain_id(HIVE_CHAIN_ID);
    }
  });
#endif /// IS_TEST_NET
}

void database::wipe(const fc::path& shared_mem_dir)
{
  if( get_is_open() )
    close();

  get_comments_handler().wipe();

  notify_wipe();

  chainbase::database::wipe( shared_mem_dir );
}

void database::close()
{
  try
  {
    if(get_is_open() == false)
      wlog("database::close method is MISUSED since it is NOT opened atm...");

    ilog( "Closing database" );

    // Since pop_block() will move tx's in the popped blocks into pending,
    // we have to clear_pending() after we're done popping to get a clean
    // DB state (issue #336).
    clear_pending();

    flush_to_all_storages();
    get_comments_handler().close();

    auto lib = this->get_last_irreversible_block_num();

    ilog("Database flushed at last irreversible block: ${b}", ("b", lib));

    chainbase::database::close();

    ilog( "Database is closed" );
  }
  FC_CAPTURE_AND_RETHROW()
}

/**
  * Only return true *if* the transaction has not expired or been invalidated. If this
  * method is called with a VERY old transaction we will return false, they should
  * query things by blocks if they are that old.
  */
bool database::is_known_transaction( const transaction_id_type& id, bool ignore_pending )const
{ try {
  const auto& trx_idx = get_index<transaction_index, by_trx_id>();
  bool found = trx_idx.find( id ) != trx_idx.end();
  if( found || ignore_pending )
    return found;
  return _pending_tx_index.find( id ) != _pending_tx_index.end();
} FC_CAPTURE_AND_RETHROW() }

chain_id_type database::get_chain_id() const
{
  return hive_chain_id;
}

chain_id_type database::get_old_chain_id() const
{
#ifdef USE_ALTERNATE_CHAIN_ID
  return hive_chain_id; /// In testnet always use the chain-id passed as hived option
#else
  return OLD_CHAIN_ID;
#endif /// IS_TEST_NET
}

chain_id_type database::get_new_chain_id() const
{
#ifdef USE_ALTERNATE_CHAIN_ID
  return hive_chain_id; /// In testnet always use the chain-id passed as hived option
#else
  return HIVE_CHAIN_ID;
#endif /// IS_TEST_NET
}

void database::set_chain_id( const chain_id_type& chain_id )
{
  hive_chain_id = chain_id;

  idump( (hive_chain_id) );
#ifdef USE_ALTERNATE_CHAIN_ID
  set_chain_id_for_transaction_signature_validation(chain_id);
#endif
}

const witness_object& database::get_witness( const account_name_type& name ) const
{ try {
  const auto* _witness = find_witness( name );
  FC_ASSERT( _witness != nullptr, "Witness ${w} doesn't exist", ("w", name) );
  return *_witness;
} FC_CAPTURE_AND_RETHROW( (name) ) }

std::string database::get_treasury_name( uint32_t hardfork ) const
{
  if( hardfork >= HIVE_HARDFORK_1_24_TREASURY_RENAME )
    return NEW_HIVE_TREASURY_ACCOUNT;
  else
    return OBSOLETE_TREASURY_ACCOUNT;
}

bool database::is_treasury( const account_name_type& name )const
{
  return ( name == NEW_HIVE_TREASURY_ACCOUNT ) || ( name == OBSOLETE_TREASURY_ACCOUNT );
}

const account_object& database::get_account( const account_id_type id )const
{ try {
    const auto* _account = find_account( id );
    FC_ASSERT( _account != nullptr && "By id", "Account with id ${acc} doesn't exist", ("acc", id) );
  return *_account;
} FC_CAPTURE_AND_RETHROW( (id) ) }

const account_object* database::find_account( const account_id_type& id )const
{
  return find< account_object, by_id >( id );
}

const account_object& database::get_account( const account_name_type& name )const
{ try {
    const auto* _account = find_account( name );
    FC_ASSERT( _account != nullptr && "By name", "Account ${acc} doesn't exist", ("acc", name) );
    return *_account;
} FC_CAPTURE_AND_RETHROW( (name) ) }

const account_object* database::find_account( const account_name_type& name )const
{
  return find< account_object, by_name >( name );
}

comment database::get_comment( const account_id_type& author, const shared_string& permlink )const
{
  return get_comments_handler().get_comment( author, to_string( permlink ), true /*comment_is_required*/ );
}

comment database::get_comment( const account_name_type& author, const shared_string& permlink )const
{
  const account_object& _account = get_account( author );
  return get_comments_handler().get_comment( _account.get_id(), to_string( permlink ), true /*comment_is_required*/ );
}

comment database::find_comment( const account_id_type& author, const shared_string& permlink )const
{
  return get_comments_handler().get_comment( author, to_string( permlink ), false /*comment_is_required*/ );
}

comment database::find_comment( const account_name_type& author, const shared_string& permlink )const
{
  const account_object* _account = find_account( author );

  if( !_account )
    return comment();

  return get_comments_handler().get_comment( _account->get_id(), to_string( permlink ), false /*comment_is_required*/ );
}

#ifndef ENABLE_STD_ALLOCATOR

comment database::get_comment( const account_id_type& author, const string& permlink )const
{
  return get_comments_handler().get_comment( author, permlink, true /*comment_is_required*/ );
}

comment database::get_comment( const account_name_type& author, const string& permlink )const
{
  const account_object& _account = get_account( author );
  return get_comments_handler().get_comment( _account.get_id(), permlink, true /*comment_is_required*/ );
}

comment database::find_comment( const account_id_type& author, const string& permlink )const
{
  return get_comments_handler().get_comment( author, permlink, false /*comment_is_required*/ );
}

comment database::find_comment( const account_name_type& author, const string& permlink )const
{
  const account_object* _account = find_account( author );

  if( !_account )
    return comment();

  return get_comments_handler().get_comment( _account->get_id(), permlink, false /*comment_is_required*/ );
}

#endif

const limit_order_object& database::get_limit_order( const account_name_type& name, uint32_t orderid )const
{ try {
  if( !has_hardfork( HIVE_HARDFORK_0_6__127 ) )
    orderid = orderid & 0x0000FFFF;

  const auto* _limit_order = find_limit_order( name, orderid );
  FC_ASSERT( _limit_order != nullptr, "Limit order with 'name' ${name} 'order_id' ${orderid} doesn't exist.", (name)(orderid) );
  return *_limit_order;
} FC_CAPTURE_AND_RETHROW( (name)(orderid) ) }

const dynamic_global_property_object&database::get_dynamic_global_properties() const
{ try {
  return get< dynamic_global_property_object >();
} FC_CAPTURE_AND_RETHROW() }

uint32_t database::get_node_skip_flags() const
{
  return _node_property_object.skip_flags;
}

void database::set_node_skip_flags( uint32_t skip_flags )
{
  _node_property_object.skip_flags = skip_flags;
}

const feed_history_object& database::get_feed_history()const
{ try {
  return get< feed_history_object >();
} FC_CAPTURE_AND_RETHROW() }

const hardfork_property_object& database::get_hardfork_property_object()const
{ try {
  return get< hardfork_property_object >();
} FC_CAPTURE_AND_RETHROW() }

const time_point_sec database::calculate_discussion_payout_time( const comment_object& comment )const
{
  const comment_object& _comment = get_comment_for_payout_time( comment );

  const comment_cashout_object* comment_cashout = find_comment_cashout( _comment );
  if( comment_cashout )
    return comment_cashout->get_cashout_time();
  else
    return time_point_sec::maximum();
}

const time_point_sec database::calculate_discussion_payout_time( const comment_object& comment, const comment_cashout_object& comment_cashout )const
{
  const comment_object& _comment = get_comment_for_payout_time( comment );

  if( _comment.get_id() == comment.get_id() )
  {
    return comment_cashout.get_cashout_time();
  }
  else
  {
    const comment_cashout_object* _comment_cashout = find_comment_cashout( _comment );
    if( _comment_cashout )
      return _comment_cashout->get_cashout_time();
    else
      return time_point_sec::maximum();
  }
}

const reward_fund_object& database::get_reward_fund() const
{
  return get< reward_fund_object, by_name >( HIVE_POST_REWARD_FUND_NAME );
}

const comment_cashout_object* database::find_comment_cashout( const comment_object& comment ) const
{
  return find_comment_cashout( comment.get_id() );
}

const comment_cashout_ex_object* database::find_comment_cashout_ex( const comment_object& comment ) const
{
  return find_comment_cashout_ex( comment.get_id() );
}


uint32_t database::witness_participation_rate()const
{
  const dynamic_global_property_object& dpo = get_dynamic_global_properties();
  return uint64_t(HIVE_100_PERCENT) * fc::uint128_popcount(dpo.recent_slots_filled) / 128;
}

bool database::is_fast_confirm_transaction(const std::shared_ptr<full_transaction_type>& full_transaction)
{
  const signed_transaction& trx = full_transaction->get_transaction();
  return trx.operations.size() == 1 && trx.operations.front().which() == operation::tag<witness_block_approve_operation>::value;
}

/**
  * Attempts to push the transaction into the pending queue
  *
  * When called to push a locally generated transaction, set the skip_block_size_check bit on the skip argument. This
  * will allow the transaction to be pushed even if it causes the pending block size to exceed the maximum block size.
  * Although the transaction will probably not propagate further now, as the peers are likely to have their pending
  * queues full as well, it will be kept in the queue to be propagated later when a new block flushes out the pending
  * queues.
  */
void database::process_non_fast_confirm_transaction( const std::shared_ptr<full_transaction_type>& full_transaction, uint32_t skip )
{
  const signed_transaction& trx = full_transaction->get_transaction(); // just for the rethrow
  try
  {
    FC_ASSERT( not is_fast_confirm_transaction(full_transaction),
               "Use chain_plugin::push_transaction for transaction ${trx}", (trx) );

    size_t trx_size = full_transaction->get_transaction_size();
    //ABW: why is that limit related to block size and not HIVE_MAX_TRANSACTION_SIZE?
    //DLN: the block size is dynamically voted by witnesses, so this code ensures that the transaction
    //can fit into the currently voted block size.
    auto trx_size_limit = get_dynamic_global_properties().maximum_block_size - 256;
    FC_ASSERT(trx_size <= trx_size_limit, "Transaction too large - size = ${trx_size}, limit ${trx_size_limit}",
              (trx_size)(trx_size_limit));

    detail::with_skip_flags(*this, skip, [&]()
    {
      BOOST_SCOPE_EXIT( this_ ) { this_->clear_tx_status(); } BOOST_SCOPE_EXIT_END
      set_tx_status( TX_STATUS_UNVERIFIED );
      _push_transaction(full_transaction);
    });
  }
  FC_CAPTURE_AND_RETHROW((trx))
}

struct custom_op_visitor
{
  typedef void result_type;

  typedef decltype( database::_pending_tx_custom_op_count ) counter_map;
  counter_map& _pending_tx_custom_op_count;
  counter_map& _current_tx_custom_op_count;

  custom_op_visitor( counter_map& multi_tx_counter, counter_map& current_tx_counter )
    : _pending_tx_custom_op_count( multi_tx_counter ), _current_tx_custom_op_count( current_tx_counter ) {}

  template< typename T >
  void operator()( const T& )const {}

  void limit_custom_op_count( const operation& op )const
  {
    flat_set< account_name_type > impacted;
    app::operation_get_impacted_accounts( op, impacted );

    for( const account_name_type& account : impacted )
    {
      auto insert_info = _current_tx_custom_op_count.emplace( account, 0 );
      ++( insert_info.first->second );
      if( insert_info.second ) // just added - copy potentially existing counter from pending
      {
        auto it = _pending_tx_custom_op_count.find( account );
        if( it != _pending_tx_custom_op_count.end() )
          insert_info.first->second += it->second;
      }
      FC_ASSERT( insert_info.first->second <= HIVE_CUSTOM_OP_BLOCK_LIMIT,
        "Account ${a} already submitted ${n} custom json operation(s) this block.",
        ( "a", account )( "n", HIVE_CUSTOM_OP_BLOCK_LIMIT ) );
    }
  }

  void operator()( const custom_operation& o )const { limit_custom_op_count( o ); }
  void operator()( const custom_json_operation& o )const { limit_custom_op_count( o ); }
  void operator()( const custom_binary_operation& o )const { limit_custom_op_count( o ); }
};

void database::_push_transaction(const std::shared_ptr<full_transaction_type>& full_transaction)
{
  const auto& trx_id = full_transaction->get_transaction_id();
  FC_ASSERT( !is_known_transaction( trx_id ), "Duplicate transaction check failed", ( trx_id ) );

  // If this is the first transaction pushed after applying a block, start a new undo session.
  // This allows us to quickly rewind to the clean state of the head block, in case a new block arrives.
  if (!_pending_tx_session.has_value())
    _pending_tx_session.emplace( start_undo_session(), start_undo_session() );

  try
  {
    // check if there is not too many custom operations added with current transaction
    decltype( _pending_tx_custom_op_count ) _current_tx_custom_op_count;
    custom_op_visitor visitor( _pending_tx_custom_op_count, _current_tx_custom_op_count );
    for( const auto& op : full_transaction->get_transaction().operations )
      op.visit( visitor );

    _apply_transaction(full_transaction);

    // since transaction was accepted, consolidate custom op counters for that transaction with all pending counters
    for( const auto& counter : _current_tx_custom_op_count )
      _pending_tx_custom_op_count[ counter.first ] = counter.second;

    if( full_transaction->check_privilege() && is_validating_one_tx() )
    {
      // reuse mechanism used for forks to make sure privileged transaction gets rewritten to the start
      // of pendling list and therefore won't be delayed indefinitely by the transaction flood
      _popped_tx.emplace_front( full_transaction );
    }
    else
    {
      _pending_tx.push_back( full_transaction );
      _pending_tx_size += full_transaction->get_transaction_size();
    }

    _pending_tx_index.emplace( trx_id );
    // The transaction applied successfully. Merge its changes into the pending block session.
    _pending_tx_session->second.squash( true );
  }
  catch( ... )
  {
    _pending_tx_session->second.undo( true );
    throw;
  }
}

/**
  * Removes the most recent block from the database and
  * undoes any changes it made.
  */
void database::pop_block()
{
  try
  {
    try
    {
      _pending_tx_session.reset();
    }
    FC_CAPTURE_AND_RETHROW()

    block_id_type head_id = head_block_id();

    /// save the head block so we can recover its transactions
    std::shared_ptr<full_block_type> full_head_block;
    try
    {
      full_head_block = _block_writer->get_block_reader().fetch_block_by_id(head_id);
    }
    FC_CAPTURE_AND_RETHROW()

    HIVE_ASSERT(full_head_block, pop_empty_chain, "there are no blocks to pop");

    try
    {
      _block_writer->pop_block();
    }
    FC_CAPTURE_AND_RETHROW()
    try
    {
      undo();
    }
    FC_CAPTURE_AND_RETHROW()

    try
    {
      _popped_tx.insert(_popped_tx.begin(), full_head_block->get_full_transactions().begin(), full_head_block->get_full_transactions().end());
    }
    FC_CAPTURE_AND_RETHROW()
  }
  FC_CAPTURE_AND_RETHROW()
}

uint32_t database::pop_block_extended( const block_id_type end_block )
{
  try
  {
    while( head_block_id() != end_block )
    {
      const block_id_type id_being_popped = head_block_id();
      ilog(" - Popping block ${id_being_popped}", (id_being_popped));
      pop_block();
      ilog(" - Popped block ${id_being_popped}", (id_being_popped));
    }

    return head_block_num();
  }
  FC_LOG_AND_RETHROW()
}

void database::clear_pending()
{
  try
  {
    assert( _pending_tx.empty() || _pending_tx_session.has_value() );
    _pending_tx.clear();
    _pending_tx_size = 0;
    _pending_tx_session.reset();
    _pending_tx_index.clear();
    _pending_tx_custom_op_count.clear();
  }
  FC_CAPTURE_AND_RETHROW()
}

void database::push_virtual_operation( const operation& op )
{
  FC_ASSERT( is_virtual_operation( op ) && "on push" );
  _current_op_in_trx++;
  operation_notification note = create_operation_notification( op );
  notify_pre_apply_operation( note );
  notify_post_apply_operation( note );
}

void database::pre_push_virtual_operation( const operation& op )
{
  FC_ASSERT( is_virtual_operation( op ) && "on pre-push" );
  _current_op_in_trx++;
  operation_notification note = create_operation_notification( op );
  notify_pre_apply_operation( note );
}

void database::post_push_virtual_operation( const operation& op, const fc::optional<uint64_t>& op_in_trx )
{
  FC_ASSERT( is_virtual_operation( op ) && "on post-push" );
  operation_notification note = create_operation_notification( op );
  if(op_in_trx.valid()) note.op_in_trx = *op_in_trx;
  notify_post_apply_operation( note );
}

void database::notify_pre_apply_operation( const operation_notification& note )
{
  HIVE_TRY_NOTIFY( _pre_apply_operation_signal, note )
}

void database::notify_post_apply_operation( const operation_notification& note )
{
  HIVE_TRY_NOTIFY( _post_apply_operation_signal, note )
}

void database::notify_pre_apply_block( const block_notification& note )
{
  HIVE_TRY_NOTIFY( _pre_apply_block_signal, note )
}

void database::notify_irreversible_block( uint32_t block_num )
{
  HIVE_TRY_NOTIFY( _on_irreversible_block, block_num )
}

void database::notify_switch_fork( uint32_t block_num )
{
  HIVE_TRY_NOTIFY( _switch_fork_signal, block_num )
}

void database::notify_post_apply_block( const block_notification& note )
{
  HIVE_TRY_NOTIFY( _post_apply_block_signal, note )
}

void database::notify_fail_apply_block( const block_notification& note )
{
  HIVE_TRY_NOTIFY( _fail_apply_block_signal, note )
}

void database::notify_pre_apply_transaction( const transaction_notification& note )
{
  HIVE_TRY_NOTIFY( _pre_apply_transaction_signal, note )
}

void database::notify_post_apply_transaction( const transaction_notification& note )
{
  HIVE_TRY_NOTIFY( _post_apply_transaction_signal, note )
}

void database::notify_prepare_snapshot_data_supplement(const prepare_snapshot_supplement_notification& note)
{
  HIVE_TRY_NOTIFY(_prepare_snapshot_supplement_signal, note)
}

void database::notify_load_snapshot_data_supplement(const load_snapshot_supplement_notification& note)
{
  HIVE_TRY_NOTIFY(_load_snapshot_supplement_signal, note)
}

void database::notify_comment_reward(const comment_reward_notification& note)
{
  HIVE_TRY_NOTIFY(_comment_reward_signal, note)
}

void database::notify_end_of_syncing()
{
  flush_to_all_storages();
  get_comments_handler().on_end_of_syncing();

  HIVE_TRY_NOTIFY(_end_of_syncing_signal)
}

void database::notify_pre_apply_custom_operation( const custom_operation_notification& note )
{
  HIVE_TRY_NOTIFY( _pre_apply_custom_operation_signal, note )
}

void database::notify_post_apply_custom_operation( const custom_operation_notification& note )
{
  HIVE_TRY_NOTIFY( _post_apply_custom_operation_signal, note )
}

void database::notify_finish_push_block( const block_notification& note )
{
  HIVE_TRY_NOTIFY( _finish_push_block_signal, note )
}

void database::notify_wipe()
{
  HIVE_TRY_NOTIFY(_wipe_signal)
}

void database::notify_flush()
{
  HIVE_TRY_NOTIFY( _flush_signal )
}

account_name_type database::get_scheduled_witness( uint32_t slot_num )const
{
  const dynamic_global_property_object& dpo = get_dynamic_global_properties();
  const witness_schedule_object& wso = get_witness_schedule_object();
  uint64_t current_aslot = dpo.current_aslot + slot_num;
  return wso.current_shuffled_witnesses[ current_aslot % wso.num_scheduled_witnesses ];
}

fc::time_point_sec database::get_slot_time(uint32_t slot_num)const
{
  if( slot_num == 0 )
    return fc::time_point_sec();

  auto interval = HIVE_BLOCK_INTERVAL;
  const dynamic_global_property_object& dpo = get_dynamic_global_properties();

  if( head_block_num() == 0 )
  {
    // n.b. first block is at genesis_time plus one block interval
    fc::time_point_sec genesis_time = dpo.time;
    return genesis_time + slot_num * interval;
  }

  int64_t head_block_abs_slot = head_block_time().sec_since_epoch() / interval;
  fc::time_point_sec head_slot_time( head_block_abs_slot * interval );

  // "slot 0" is head_slot_time
  // "slot 1" is head_slot_time,
  //   plus maint interval if head block is a maint block
  //   plus block interval if head block is not a maint block
  return head_slot_time + (slot_num * interval);
}

uint32_t database::get_slot_at_time(fc::time_point_sec when)const
{
  fc::time_point_sec first_slot_time = get_slot_time( 1 );
  if( when < first_slot_time )
    return 0;
  return (when - first_slot_time).to_seconds() / HIVE_BLOCK_INTERVAL + 1;
}

/**
  *  Converts HIVE into HBD and adds it to to_account while reducing the HIVE supply
  *  by HIVE and increasing the HBD supply by the specified amount.
  */
std::pair< asset, asset > database::create_hbd( const account_object& to_account, asset hive, bool to_reward_balance )
{
  std::pair< asset, asset > assets( asset( 0, HBD_SYMBOL ), asset( 0, HIVE_SYMBOL ) );

  try
  {
    if( hive.amount == 0 )
      return assets;

    const auto& median_price = get_feed_history().current_median_history;
    const auto& gpo = get_dynamic_global_properties();

    if( !median_price.is_null() )
    {
      auto to_hbd = ( gpo.hbd_print_rate * hive.amount ) / HIVE_100_PERCENT;
      auto to_hive = hive.amount - to_hbd;

      auto hbd = asset( to_hbd, HIVE_SYMBOL ) * median_price;

      if( to_reward_balance )
      {
        adjust_reward_balance( to_account, hbd );
        adjust_reward_balance( to_account, asset( to_hive, HIVE_SYMBOL ) );
      }
      else
      {
        adjust_balance( to_account, hbd );
        adjust_balance( to_account, asset( to_hive, HIVE_SYMBOL ) );
      }

      adjust_supply( asset( -to_hbd, HIVE_SYMBOL ) );
      adjust_supply( hbd );
      assets.first = hbd;
      assets.second = asset( to_hive, HIVE_SYMBOL );
    }
    else
    {
      adjust_balance( to_account, hive );
      assets.second = hive;
    }
  }
  FC_CAPTURE_LOG_AND_RETHROW( (to_account.get_name())(hive) )

  return assets;
}

asset database::adjust_account_vesting_balance(const account_object& to_account, const asset& liquid, bool to_reward_balance, Before&& before_vesting_callback )
{
  try
  {
#ifdef HIVE_ENABLE_SMT
    if( liquid.symbol.space() == asset_symbol_type::smt_nai_space )
    {
      asset new_vesting = calculate_vesting( *this, liquid, to_reward_balance );
      before_vesting_callback( new_vesting );
      // Add new vesting to owner's balance.
      if( to_reward_balance )
        adjust_reward_balance( to_account, liquid, new_vesting );
      else
        adjust_balance( to_account, new_vesting );
      // Update global vesting pool numbers.
      const auto& smt = get< smt_token_object, by_symbol >( liquid.symbol );
      modify( smt, [&]( smt_token_object& smt_object )
      {
        if( to_reward_balance )
        {
          smt_object.pending_rewarded_vesting_shares += new_vesting.amount;
          smt_object.pending_rewarded_vesting_smt += liquid.amount;
        }
        else
        {
          smt_object.total_vesting_fund_smt += liquid.amount;
          smt_object.total_vesting_shares += new_vesting.amount;
        }
      } );

      // NOTE that SMT vesting does not impact witness voting.

      return new_vesting;
    }
#endif
    asset new_vesting = calculate_vesting( *this, liquid, to_reward_balance );
    before_vesting_callback( new_vesting );
    // Add new vesting to owner's balance.

    const auto& cprops = get_dynamic_global_properties();
    auto _now = cprops.time;

    if( to_reward_balance )
    {
      adjust_reward_balance( to_account, liquid, new_vesting );
    }
    else
    {
      if( has_hardfork( HIVE_HARDFORK_0_20 ) )
      {
        modify( to_account, [&]( account_object& a )
        {
          util::update_manabar( cprops, a, new_vesting.amount.value );
        });
        rc.regenerate_rc_mana( to_account, _now );
      }
      adjust_balance( to_account, new_vesting );
      if( has_hardfork( HIVE_HARDFORK_0_20 ) )
        rc.update_account_after_vest_change( to_account, _now );
    }
    // Update global vesting pool numbers.
    modify( cprops, [&]( dynamic_global_property_object& props )
    {
      if( to_reward_balance )
      {
        props.pending_rewarded_vesting_shares += new_vesting;
        props.pending_rewarded_vesting_hive += liquid;
      }
      else
      {
        props.total_vesting_fund_hive += liquid;
        props.total_vesting_shares += new_vesting;
      }
    } );

    return new_vesting;
  }
  FC_CAPTURE_AND_RETHROW( (to_account.get_name())(liquid) )
}

/**
  * @param to_account - the account to receive the new vesting shares
  * @param liquid     - HIVE or liquid SMT to be converted to vesting shares
  */
asset database::create_vesting( const account_object& to_account, const asset& liquid, bool to_reward_balance )
{
  return create_vesting2( *this, to_account, liquid, to_reward_balance, []( asset vests_created ) {} );
}

fc::sha256 database::get_pow_target()const
{
  const auto& dgp = get_dynamic_global_properties();
  fc::sha256 target;
  target._hash[0] = -1;
  target._hash[1] = -1;
  target._hash[2] = -1;
  target._hash[3] = -1;
  target = target >> ((dgp.num_pow_witnesses/4)+4);
  return target;
}

uint32_t database::get_pow_summary_target()const
{
  const dynamic_global_property_object& dgp = get_dynamic_global_properties();
  if( dgp.num_pow_witnesses >= 1004 )
    return 0;

  if( has_hardfork( HIVE_HARDFORK_0_16__551 ) )
    return (0xFE00 - 0x0040 * dgp.num_pow_witnesses ) << 0x10;
  else
    return (0xFC00 - 0x0040 * dgp.num_pow_witnesses) << 0x10;
}

void database::adjust_proxied_witness_votes( const account_object& a,
                        const std::array< share_type, HIVE_MAX_PROXY_RECURSION_DEPTH+1 >& delta,
                        int depth )
{
  if( a.has_proxy() )
  {
    /// nested proxies are not supported, vote will not propagate
    if( depth >= HIVE_MAX_PROXY_RECURSION_DEPTH )
      return;

    const auto& proxy = get_account( a.get_proxy() );

    modify( proxy, [&]( account_object& a )
    {
      for( int i = HIVE_MAX_PROXY_RECURSION_DEPTH - depth - 1; i >= 0; --i )
      {
        a.proxied_vsf_votes[i+depth] += delta[i];
      }
    } );

    adjust_proxied_witness_votes( proxy, delta, depth + 1 );
  }
  else
  {
    share_type total_delta = 0;
    for( int i = HIVE_MAX_PROXY_RECURSION_DEPTH - depth; i >= 0; --i )
      total_delta += delta[i];
    adjust_witness_votes( a, total_delta );
  }
}

void database::adjust_proxied_witness_votes( const account_object& a, share_type delta, int depth )
{
  if( a.has_proxy() )
  {
    /// nested proxies are not supported, vote will not propagate
    if( depth >= HIVE_MAX_PROXY_RECURSION_DEPTH )
      return;

    const auto& proxy = get_account( a.get_proxy() );

    modify( proxy, [&]( account_object& a )
    {
      a.proxied_vsf_votes[depth] += delta;
    } );

    adjust_proxied_witness_votes( proxy, delta, depth + 1 );
  }
  else
  {
    adjust_witness_votes( a, delta );
  }
}

void database::nullify_proxied_witness_votes( const account_object& a )
{
  std::array<share_type, HIVE_MAX_PROXY_RECURSION_DEPTH + 1> delta;
  delta[ 0 ] = -a.get_direct_governance_vote_power();
  for( int i = 0; i < HIVE_MAX_PROXY_RECURSION_DEPTH; ++i )
    delta[ i + 1 ] = -a.proxied_vsf_votes[ i ];
  adjust_proxied_witness_votes( a, delta );
}

void database::adjust_witness_votes( const account_object& a, const share_type& delta )
{
  const auto& vidx = get_index< witness_vote_index >().indices().get< by_account_witness >();
  auto itr = vidx.lower_bound( boost::make_tuple( a.get_name(), account_name_type() ) );
  while( itr != vidx.end() && itr->account == a.get_name() )
  {
    adjust_witness_vote( get< witness_object, by_name >(itr->witness), delta );
    ++itr;
  }
}

void database::adjust_witness_vote( const witness_object& witness, share_type delta )
{
  const witness_schedule_object& wso = has_hardfork(HIVE_HARDFORK_1_27_FIX_TIMESHARE_WITNESS_SCHEDULING) ?
                                       get_witness_schedule_object_for_irreversibility() : get_witness_schedule_object();
  modify( witness, [&]( witness_object& w )
  {
    auto delta_pos = w.votes.value * (wso.current_virtual_time - w.virtual_last_update);
    w.virtual_position += delta_pos;

    w.virtual_last_update = wso.current_virtual_time;
    w.votes += delta;
    FC_ASSERT( w.votes <= get_dynamic_global_properties().total_vesting_shares.amount, "", ("w.votes", w.votes)("props",get_dynamic_global_properties().total_vesting_shares) );

    if( has_hardfork( HIVE_HARDFORK_0_2 ) )
      w.virtual_scheduled_time = w.virtual_last_update + (HIVE_VIRTUAL_SCHEDULE_LAP_LENGTH2 - w.virtual_position)/(w.votes.value+1);
    else
      w.virtual_scheduled_time = w.virtual_last_update + (HIVE_VIRTUAL_SCHEDULE_LAP_LENGTH - w.virtual_position)/(w.votes.value+1);

    /** witnesses with a low number of votes could overflow the time field and end up with a scheduled time in the past */
    if( has_hardfork( HIVE_HARDFORK_0_4 ) )
    {
      if( w.virtual_scheduled_time < wso.current_virtual_time )
        w.virtual_scheduled_time = fc::uint128_max_value();
    }
  } );
}

void database::clear_witness_votes( const account_object& a )
{
  const auto& vidx = get_index< witness_vote_index >().indices().get<by_account_witness>();
  auto itr = vidx.lower_bound( boost::make_tuple( a.get_name(), account_name_type() ) );
  while( itr != vidx.end() && itr->account == a.get_name() )
  {
    const auto& current = *itr;
    ++itr;
    remove(current);
  }

  if( has_hardfork( HIVE_HARDFORK_0_6__104 ) )
    modify( a, [&](account_object& acc )
    {
      acc.witnesses_voted_for = 0;
    });
}

bool database::collect_account_total_balance( const account_object& account, asset* total_hive, asset* total_hbd,
  asset* total_vests, asset* vesting_shares_hive_value )
{
  const auto& gpo = get_dynamic_global_properties();

  *vesting_shares_hive_value = account.get_vesting() * gpo.get_vesting_share_price();
  *total_hive = *vesting_shares_hive_value;
  *total_vests = account.get_vesting();
  *total_hive += account.get_vest_rewards_as_hive();
  *total_vests += account.get_vest_rewards();

  *total_hive += account.get_balance();
  *total_hive += account.get_savings();
  *total_hive += account.get_rewards();

  *total_hbd = account.get_hbd_balance();
  *total_hbd += account.get_hbd_savings();
  *total_hbd += account.get_hbd_rewards();

  return ( total_hive->amount.value != 0 ) || ( total_hbd->amount.value != 0 ) || ( total_vests->amount.value != 0 );
}

void database::clear_null_account_balance()
{
  if( !has_hardfork( HIVE_HARDFORK_0_14__327 ) ) return;

  const auto& null_account = get_account( HIVE_NULL_ACCOUNT );
  asset total_hive, total_hbd, total_vests, vesting_shares_hive_value;

  if( !( collect_account_total_balance( null_account, &total_hive, &total_hbd, &total_vests, &vesting_shares_hive_value ) ) )
    return;

  operation vop_op = clear_null_account_balance_operation();
  clear_null_account_balance_operation& vop = vop_op.get< clear_null_account_balance_operation >();
  if( total_hive.amount.value > 0 )
    vop.total_cleared.push_back( total_hive );
  if( total_vests.amount.value > 0 )
    vop.total_cleared.push_back( total_vests );
  if( total_hbd.amount.value > 0 )
    vop.total_cleared.push_back( total_hbd );
  pre_push_virtual_operation( vop_op );

  /////////////////////////////////////////////////////////////////////////////////////

  if( null_account.get_balance().amount > 0 )
  {
    adjust_balance( null_account, -null_account.get_balance() );
  }

  if( null_account.get_savings().amount > 0 )
  {
    adjust_savings_balance( null_account, -null_account.get_savings() );
  }

  if( null_account.get_hbd_balance().amount > 0 )
  {
    adjust_balance( null_account, -null_account.get_hbd_balance() );
  }

  if( null_account.get_hbd_savings().amount > 0 )
  {
    adjust_savings_balance( null_account, -null_account.get_hbd_savings() );
  }

  if( null_account.get_vesting().amount > 0 )
  {
    const auto& gpo = get_dynamic_global_properties();
    auto _now = gpo.time;

    modify( gpo, [&]( dynamic_global_property_object& g )
    {
      g.total_vesting_shares -= null_account.get_vesting();
      g.total_vesting_fund_hive -= vesting_shares_hive_value;
    });

    if( has_hardfork( HIVE_HARDFORK_0_20 ) )
      rc.regenerate_rc_mana( null_account, _now ); //we could just always set RC value on 'null' to 0
    modify( null_account, [&]( account_object& a )
    {
      a.vesting_shares.amount = 0;
      a.sum_delayed_votes = 0;
      a.delayed_votes.clear();
    });
    if( has_hardfork( HIVE_HARDFORK_0_20 ) )
      rc.update_account_after_vest_change( null_account, _now );
  }

  if( null_account.get_rewards().amount > 0 )
  {
    adjust_reward_balance( null_account, -null_account.get_rewards() );
  }

  if( null_account.get_hbd_rewards().amount > 0 )
  {
    adjust_reward_balance( null_account, -null_account.get_hbd_rewards() );
  }

  if( null_account.get_vest_rewards().amount > 0 )
  {
    const auto& gpo = get_dynamic_global_properties();

    modify( gpo, [&]( dynamic_global_property_object& g )
    {
      g.pending_rewarded_vesting_shares -= null_account.get_vest_rewards();
      g.pending_rewarded_vesting_hive -= null_account.get_vest_rewards_as_hive();
    });

    modify( null_account, [&]( account_object& a )
    {
      a.reward_vesting_hive.amount = 0;
      a.reward_vesting_balance.amount = 0;
    });
  }

  //////////////////////////////////////////////////////////////

  if( total_hive.amount > 0 )
    adjust_supply( -total_hive );

  if( total_hbd.amount > 0 )
    adjust_supply( -total_hbd );

  post_push_virtual_operation( vop_op );
}

void database::consolidate_treasury_balance()
{
  if( !has_hardfork( HIVE_HARDFORK_1_24_TREASURY_RENAME ) )
    return;

  auto treasury_name = get_treasury_name();
  auto old_treasury_name = get_treasury_name( HIVE_HARDFORK_1_24_TREASURY_RENAME - 1 );

  const auto& old_treasury_account = get_account( old_treasury_name );
  asset total_hive, total_hbd, total_vests, vesting_shares_hive_value;

  if( treasury_name == old_treasury_name || //ABW: once we actually include change of treasury in HF24 that part of condition can be removed
    !( collect_account_total_balance( old_treasury_account, &total_hive, &total_hbd, &total_vests, &vesting_shares_hive_value ) ) )
    return;

  const auto& treasury_account = get_treasury();

  operation vop_op = consolidate_treasury_balance_operation();
  consolidate_treasury_balance_operation& vop = vop_op.get< consolidate_treasury_balance_operation >();
  if( total_hive.amount.value > 0 )
    vop.total_moved.push_back( total_hive );
  if( total_vests.amount.value > 0 )
    vop.total_moved.push_back( total_vests );
  if( total_hbd.amount.value > 0 )
    vop.total_moved.push_back( total_hbd );
  pre_push_virtual_operation( vop_op );

  /////////////////////////////////////////////////////////////////////////////////////

  if( old_treasury_account.get_balance().amount > 0 )
  {
    adjust_balance( treasury_account, old_treasury_account.get_balance() );
    adjust_balance( old_treasury_account, -old_treasury_account.get_balance() );
  }

  if( old_treasury_account.get_savings().amount > 0 )
  {
    adjust_savings_balance( treasury_account, old_treasury_account.get_savings() );
    adjust_savings_balance( old_treasury_account, -old_treasury_account.get_savings() );
  }

  if( old_treasury_account.get_hbd_balance().amount > 0 )
  {
    adjust_balance( treasury_account, old_treasury_account.get_hbd_balance() );
    adjust_balance( old_treasury_account, -old_treasury_account.get_hbd_balance() );
  }

  if( old_treasury_account.get_hbd_savings().amount > 0 )
  {
    adjust_savings_balance( treasury_account, old_treasury_account.get_hbd_savings() );
    adjust_savings_balance( old_treasury_account, -old_treasury_account.get_hbd_savings() );
  }

  if( old_treasury_account.get_vesting().amount > 0 )
  {
    //note that if we wanted to move vests in vested form it would complicate delayed_votes part;
    //not that treasury could gain anything from vests anyway, so it is better to liquify them
    adjust_balance( treasury_account, vesting_shares_hive_value );

    const auto& gpo = get_dynamic_global_properties();
    modify( gpo, [&]( dynamic_global_property_object& g )
    {
      g.total_vesting_shares -= old_treasury_account.get_vesting();
      g.total_vesting_fund_hive -= vesting_shares_hive_value;
    } );

    modify( old_treasury_account, [&]( account_object& a )
    {
      a.vesting_shares.amount = 0;
      a.sum_delayed_votes = 0;
      a.delayed_votes.clear();
    } );
  }

  if( old_treasury_account.get_rewards().amount > 0 )
  {
    adjust_reward_balance( treasury_account, old_treasury_account.get_rewards() );
    adjust_reward_balance( old_treasury_account, -old_treasury_account.get_rewards() );
  }

  if( old_treasury_account.get_hbd_rewards().amount > 0 )
  {
    adjust_reward_balance( treasury_account, old_treasury_account.get_hbd_rewards() );
    adjust_reward_balance( old_treasury_account, -old_treasury_account.get_hbd_rewards() );
  }

  if( old_treasury_account.get_vest_rewards().amount > 0 )
  {
    //see above handling of regular vests
    adjust_balance( treasury_account, old_treasury_account.get_vest_rewards_as_hive() );

    const auto& gpo = get_dynamic_global_properties();
    modify( gpo, [ & ]( dynamic_global_property_object& g )
    {
      g.pending_rewarded_vesting_shares -= old_treasury_account.get_vest_rewards();
      g.pending_rewarded_vesting_hive -= old_treasury_account.get_vest_rewards_as_hive();
    } );

    modify( old_treasury_account, [&]( account_object& a )
    {
      a.reward_vesting_hive.amount = 0;
      a.reward_vesting_balance.amount = 0;
    } );
  }

  //////////////////////////////////////////////////////////////

  post_push_virtual_operation( vop_op );
}

void database::lock_account( const account_object& account )
{
  auto* account_auth = find< account_authority_object, by_account >( account.get_name() );
  if( account_auth == nullptr )
  {
    create< account_authority_object >( [&]( account_authority_object& auth )
    {
      auth.account = account.get_name();
      auth.owner.weight_threshold = 1;
      auth.active.weight_threshold = 1;
      auth.posting.weight_threshold = 1;
    } );
  }
  else
  {
    modify( *account_auth, []( account_authority_object& auth )
    {
      auth.owner.weight_threshold = 1;
      auth.owner.clear();

      auth.active.weight_threshold = 1;
      auth.active.clear();

      auth.posting.weight_threshold = 1;
      auth.posting.clear();
    } );
  }

  modify( account, []( account_object& a )
  {
    a.set_recovery_account( a );
    a.memo_key = public_key_type();
  } );

  auto rec_req = find< account_recovery_request_object, by_account >( account.get_name() );
  if( rec_req )
    remove( *rec_req );

  auto change_request = find< change_recovery_account_request_object, by_account >( account.get_name() );
  if( change_request )
    remove( *change_request );
}

void database::restore_accounts( const std::set< std::string >& restored_accounts )
{
  const auto& hardforks = get_hardfork_property_object();

  const auto& treasury_account = get_treasury();
  auto treasury_name = get_treasury_name();

  for( auto& name : restored_accounts )
  {
    auto found = hardforks.h23_balances.find( name );

    if( found == hardforks.h23_balances.end() )
    {
      #ifndef IS_TEST_NET
        ilog( "The account ${acc} hadn't removed balances, balances can't be restored", ( "acc", name ) );
      #endif
      continue;
    }

    const auto* account_ptr = find_account( name );
    if( account_ptr == nullptr )
    {
      ilog( "The account ${acc} doesn't exist at all, balances can't be restored", ( "acc", name ) );
      continue;
    }

    adjust_balance( treasury_account, -found->second.hbd_balance );
    adjust_balance( treasury_account, -found->second.balance );

    adjust_balance( *account_ptr, found->second.hbd_balance );
    adjust_balance( *account_ptr, found->second.balance );

    operation vop = hardfork_hive_restore_operation( name, treasury_name, found->second.hbd_balance, found->second.balance );
    push_virtual_operation( vop );

    ilog( "Balances ${hbd} and ${hive} for the account ${acc} were restored", ( "hbd", found->second.hbd_balance )( "hive", found->second.balance )( "acc", name ) );
  }
}

void database::gather_balance( const std::string& name, const asset& balance, const asset& hbd_balance )
{
  modify( get_hardfork_property_object(), [&]( hardfork_property_object& hfp )
  {
    hfp.h23_balances.emplace( std::make_pair( name, hf23_item{ balance, hbd_balance } ) );
  } );
}

void database::clear_accounts( const std::set< std::string >& cleared_accounts )
{
  auto treasury_name = get_treasury_name();
  for( const auto& account_name : cleared_accounts )
  {
    const auto* account_ptr = find_account( account_name );
    if( account_ptr == nullptr )
      continue;

    clear_account( *account_ptr );
  }
}

void database::clear_account( const account_object& account )
{
  FC_ASSERT( has_hardfork( HIVE_HARDFORK_0_20 ) ); // routine assumes RC is already active

  const auto& account_name = account.get_name();
  FC_ASSERT( account_name != get_treasury_name(), "Can't clear treasury account" );

  const auto& treasury_account = get_treasury();
  const auto& cprops = get_dynamic_global_properties();
  auto now = cprops.time;

  hardfork_hive_operation vop( account_name, treasury_account.get_name() );

  HIVE_asset total_transferred_hive( 0 );
  HBD_asset total_transferred_hbd( 0 );
  VEST_asset total_converted_vests( 0 );
  HIVE_asset total_hive_from_vests( 0 );

  if( account.get_vesting().amount > 0 )
  {
    rc.regenerate_rc_mana( account, now );

    // Remove all active delegations
    VEST_asset freed_delegations( 0 );

    const auto& delegation_idx = get_index< vesting_delegation_index, by_delegation >();
    auto delegation_itr = delegation_idx.lower_bound( account.get_id() );
    while( delegation_itr != delegation_idx.end() && delegation_itr->get_delegator() == account.get_id() )
    {
      const auto& delegation = *delegation_itr;
      const auto& delegatee = get_account( delegation_itr->get_delegatee() );
      ++delegation_itr;

      vop.other_affected_accounts.emplace_back( delegatee.get_name() );

      rc.regenerate_rc_mana( delegatee, now );
      modify( delegatee, [&]( account_object& a )
      {
        util::update_manabar( cprops, a );
        a.received_vesting_shares -= delegation.get_vesting();
        freed_delegations += delegation.get_vesting();

        a.voting_manabar.use_mana( delegation.get_vesting().amount.value );

        a.downvote_manabar.use_mana(
          fc::uint128_to_int64( ( uint128_t( delegation.get_vesting().amount.value ) * cprops.downvote_pool_percent ) /
          HIVE_100_PERCENT ) );
      } );

      remove( delegation );

      rc.update_account_after_vest_change( delegatee, now, true, true );
    }

    // Remove pending expired delegations
    const auto& exp_delegation_idx = get_index< vesting_delegation_expiration_index, by_account_expiration >();
    auto exp_delegation_itr = exp_delegation_idx.lower_bound( account.get_id() );
    while( exp_delegation_itr != exp_delegation_idx.end() && exp_delegation_itr->get_delegator() == account.get_id() )
    {
      auto& delegation = *exp_delegation_itr;
      ++exp_delegation_itr;

      freed_delegations += delegation.get_vesting();
      remove( delegation );
    }

    VEST_asset vests_to_convert = account.get_vesting();
    HIVE_asset converted_hive = vests_to_convert * cprops.get_vesting_share_price();
    total_converted_vests += account.get_vesting();
    total_hive_from_vests += converted_hive;

    adjust_proxied_witness_votes( account, -account.get_vesting().amount );

    modify( account, [&]( account_object& a )
    {
      util::update_manabar( cprops, a );
      a.voting_manabar.current_mana = 0;
      a.downvote_manabar.current_mana = 0;
      a.vesting_shares = VEST_asset( 0 );
      //FC_ASSERT( a.delegated_vesting_shares == freed_delegations, "Inconsistent amount of delegations" );
      a.delegated_vesting_shares = VEST_asset( 0 );
      a.vesting_withdraw_rate.amount = 0;
      a.next_vesting_withdrawal = fc::time_point_sec::maximum();
      a.to_withdraw.amount = 0;
      a.withdrawn.amount = 0;

      if( has_hardfork( HIVE_HARDFORK_1_24 ) )
      {
        a.delayed_votes.clear();
        a.sum_delayed_votes = 0;
      }

      rc.update_account_after_vest_change( account, now, true, true );
    } );

    adjust_balance( treasury_account, converted_hive );
    modify( cprops, [&]( dynamic_global_property_object& o )
    {
      o.total_vesting_fund_hive -= converted_hive;
      o.total_vesting_shares -= vests_to_convert;
    } );
  }

  remove_pending_escrows( account, account_name );

  // Remove open limit orders (return balance to account - compare with clear_expired_orders())
  const auto& order_idx = get_index< chain::limit_order_index, chain::by_account >();
  auto order_itr = order_idx.lower_bound( account_name );
  while( order_itr != order_idx.end() && order_itr->seller == account_name )
  {
    auto& order = *order_itr;
    ++order_itr;

    cancel_order( order, true );
  }

  remove_pending_conversion_requests( account );

  remove_pending_savings_withdraws( account, account_name );

  // Touch HBD balances (to be sure all interests are added to balances)
  if( has_hardfork( HIVE_HARDFORK_1_24 ) )
  {
    adjust_balance( account, HBD_asset( 0 ) );
    adjust_savings_balance( account, HBD_asset( 0 ) );
    adjust_reward_balance( account, HBD_asset( 0 ) );
  }

  // Remove remaining savings balances
  total_transferred_hive += account.get_savings();
  total_transferred_hbd += account.get_hbd_savings();
  adjust_balance( treasury_account, account.get_savings() );
  adjust_savings_balance( account, -account.get_savings() );
  adjust_balance( treasury_account, account.get_hbd_savings() );
  adjust_savings_balance( account, -account.get_hbd_savings() );

  // Remove HBD and HIVE balances
  total_transferred_hive += account.get_balance();
  total_transferred_hbd += account.get_hbd_balance();
  adjust_balance( treasury_account, account.get_balance() );
  adjust_balance( account, -account.get_balance() );
  adjust_balance( treasury_account, account.get_hbd_balance() );
  adjust_balance( account, -account.get_hbd_balance() );

  // Transfer reward balances
  total_transferred_hive += account.get_rewards();
  total_transferred_hbd += account.get_hbd_rewards();
  adjust_balance( treasury_account, account.get_rewards() );
  adjust_reward_balance( account, -account.get_rewards() );
  adjust_balance( treasury_account, account.get_hbd_rewards() );
  adjust_reward_balance( account, -account.get_hbd_rewards() );

  // Convert and transfer vesting rewards
  adjust_balance( treasury_account, account.get_vest_rewards_as_hive() );
  total_converted_vests += account.get_vest_rewards();
  total_hive_from_vests += account.get_vest_rewards_as_hive();

  modify( get_dynamic_global_properties(), [&]( dynamic_global_property_object& gpo )
  {
    gpo.pending_rewarded_vesting_shares -= account.get_vest_rewards();
    gpo.pending_rewarded_vesting_hive -= account.get_vest_rewards_as_hive();
  } );

  modify( account, []( account_object &a )
  {
    a.reward_vesting_balance = VEST_asset( 0 );
    a.reward_vesting_hive = HIVE_asset( 0 );
  } );

  vop.hbd_transferred = total_transferred_hbd;
  vop.hive_transferred = total_transferred_hive;
  vop.vests_converted = total_converted_vests;
  vop.total_hive_from_vests = total_hive_from_vests;

  gather_balance( account_name, vop.hive_transferred, vop.hbd_transferred );

  push_virtual_operation( vop );
}

void database::process_proposals( const block_notification& note )
{
  if( has_hardfork( HIVE_PROPOSALS_HARDFORK ) )
  {
    dhf_processor dhf( *this );
    dhf.run( note );
  }
}

void database::process_delayed_voting( const block_notification& note )
{
  if( has_hardfork( HIVE_HARDFORK_1_24 ) )
  {
    delayed_voting dv( *this );
    dv.run( note.get_block_timestamp() );
  }
}

void database::remove_proposal_votes_for_accounts_without_voting_rights()
{
  std::vector<account_name_type> voters;

  const auto& proposal_votes_idx = get_index< proposal_vote_index, by_voter_proposal >();

  auto itr = proposal_votes_idx.begin();
  while( itr != proposal_votes_idx.end() )
  {
    voters.push_back( itr->voter );
    ++itr;
  }

  //Lack of voters.
  if( voters.empty() )
    return;

  std::vector<account_name_type> accounts;

  for( auto& voter : voters )
  {
    const auto& account = get_account( voter );
    if( !account.can_vote )
      accounts.push_back( account.get_name() );
  }

  //Lack of voters who declined voting rights.
  if( accounts.empty() )
    return;

  /*
    For every account set a request to remove proposal votes.
    Current time is set, because we want to start removing proposal votes as soon as possible.
  */
  const auto& request_idx = get_index< decline_voting_rights_request_index, by_account >();

  for( auto& account : accounts )
  {
    auto found = request_idx.find( account );
    if( found !=request_idx.end() )
    {
      /*
        Before HF28 it was possible to create `decline_voting_rights` operation again, even if an account had `can_vote` set to false.
        In this case `effective_date` must be changed otherwise a new object is created.
      */
      modify( *found, [&]( decline_voting_rights_request_object& req )
      {
        req.effective_date = head_block_time();
      });
    }
    else
    {
      create< decline_voting_rights_request_object >( [&]( decline_voting_rights_request_object& req )
      {
        req.account = account;
        req.effective_date = head_block_time();
      });
    }
  }
}

/**
  * This method updates total_reward_shares2 on DGPO, and children_rshares2 on comments, when a comment's rshares2 changes
  * from old_rshares2 to new_rshares2.  Maintaining invariants that children_rshares2 is the sum of all descendants' rshares2,
  * and dgpo.total_reward_shares2 is the total number of rshares2 outstanding.
  */
void database::adjust_rshares2( fc::uint128_t old_rshares2, fc::uint128_t new_rshares2 )
{
  const auto& dgpo = get_dynamic_global_properties();
  modify( dgpo, [&]( dynamic_global_property_object& p )
  {
    p.total_reward_shares2 -= old_rshares2;
    p.total_reward_shares2 += new_rshares2;
  } );
}

void database::update_owner_authority( const account_object& account, const authority& owner_authority )
{
  if( head_block_num() >= HIVE_OWNER_AUTH_HISTORY_TRACKING_START_BLOCK_NUM )
  {
    create< owner_authority_history_object >( account, get< account_authority_object, by_account >( account.get_name() ).owner, head_block_time() );
  }

  modify( get< account_authority_object, by_account >( account.get_name() ), [&]( account_authority_object& auth )
  {
    auth.owner = owner_authority;
    auth.previous_owner_update = auth.last_owner_update;
    auth.last_owner_update = head_block_time();
  });
}

void database::process_vesting_withdrawals()
{
  const auto& widx = get_index< account_index, by_next_vesting_withdrawal >();
  const auto& didx = get_index< withdraw_vesting_route_index, by_withdraw_route >();
  auto current = widx.begin();

  const auto& cprops = get_dynamic_global_properties();
  auto now = cprops.time;

  int count = 0;
  if( _benchmark_dumper.is_enabled() )
    _benchmark_dumper.begin();
  while( current != widx.end() && current->next_vesting_withdrawal <= now )
  {
    const auto& from_account = *current; ++current; ++count;

    share_type to_withdraw = from_account.get_next_vesting_withdrawal();
    if( !has_hardfork( HIVE_HARDFORK_1_28_FIX_POWER_DOWN ) && to_withdraw < from_account.vesting_withdraw_rate.amount )
      to_withdraw = from_account.to_withdraw.amount % from_account.vesting_withdraw_rate.amount;
    // see history of first (and so far the only) power down of 'gil' account: https://hiveblocks.com/@gil
    // the situation was caused by HF1, where vesting_withdraw_rate changed from 9615 before split to 9615.384615
    // (instead of correct 9615.000000); that is the source of nonequivalence between taking all the rest of power down
    // (0.769270 VESTS) and modulo of "all % weekly rate" (0.000040 VESTS);
    // it is possible that other accounts were also affected in similar way, 'gil' was just the first where the difference
    // occurred
    bool invalid_power_down = to_withdraw > from_account.get_vesting().amount;
    if( invalid_power_down )
    {
      elog( "NOTIFYALERT! somehow account was scheduled to power down more than it has on balance (${s} vs ${h})",
        ( "s", to_withdraw )( "h", from_account.get_vesting().amount ) );
#ifdef USE_ALTERNATE_CHAIN_ID
      FC_ASSERT( not invalid_power_down, "Somehow account was scheduled to power down more than it has on balance (${s} vs ${h})",
        ( "s", to_withdraw )( "h", from_account.get_vesting().amount ) );
#endif
      to_withdraw = from_account.get_vesting().amount;
    }

    optional< delayed_voting > dv;
    delayed_voting::opt_votes_update_data_items _votes_update_data_items;

    if( has_hardfork( HIVE_HARDFORK_1_24 ) )
    {
      dv = delayed_voting( *this );
      _votes_update_data_items = delayed_voting::votes_update_data_items();
    }

    share_type vests_deposited = 0;

    auto process_routing = [ &, this ]( bool auto_vest_mode )
    {
      for( auto itr = didx.upper_bound( boost::make_tuple( from_account.get_name(), account_name_type() ) );
        itr != didx.end() && itr->from_account == from_account.get_name();
        ++itr )
      {
        if( !( auto_vest_mode ^ itr->auto_vest ) )
        {
          share_type to_deposit = fc::uint128_to_uint64( ( fc::uint128_t ( to_withdraw.value ) * itr->percent ) / HIVE_100_PERCENT );
          vests_deposited += to_deposit;

          if( to_deposit > 0 )
          {
            const auto& to_account = get< account_object, by_name >( itr->to_account );

            asset vests = asset( to_deposit, VESTS_SYMBOL );
            asset routed = auto_vest_mode ? vests : ( vests * cprops.get_vesting_share_price() );
            operation vop = fill_vesting_withdraw_operation( from_account.get_name(), to_account.get_name(), vests, routed );

            pre_push_virtual_operation( vop );

            modify( to_account, [&]( account_object& a )
            {
              if( auto_vest_mode )
              {
                if( has_hardfork( HIVE_HARDFORK_0_20 ) )
                  rc.regenerate_rc_mana( a, now );
                a.vesting_shares += routed;
              }
              else
              {
                a.balance += routed;
              }
            });

            if( auto_vest_mode )
            {
              if( has_hardfork( HIVE_HARDFORK_0_20 ) )
                rc.update_account_after_vest_change( to_account, now );

              if( has_hardfork( HIVE_HARDFORK_1_24 ) )
              {
                FC_ASSERT( dv.valid(), "The object processing `delayed votes` must exist" );

                dv->add_votes( _votes_update_data_items,
                          to_account.get_id() == from_account.get_id()/*withdraw_executor*/,
                          routed.amount.value/*val*/,
                          to_account/*account*/
                        );
              }
              else
              {
                adjust_proxied_witness_votes( to_account, to_deposit );
              }
            }
            else
            {
              modify( cprops, [&]( dynamic_global_property_object& o )
              {
                o.total_vesting_fund_hive     -= routed;
                o.total_vesting_shares.amount -= to_deposit;
              });
            }

            post_push_virtual_operation( vop );
          }
        }
      }
    };

    // Do two passes, the first for VESTS, the second for HIVE. Try to maintain as much accuracy for VESTS as possible.
    process_routing( true/*auto_vest_mode*/ );
    process_routing( false/*auto_vest_mode*/ );

    share_type to_convert = to_withdraw - vests_deposited;
    FC_ASSERT( to_convert >= 0, "Deposited more vests than were supposed to be withdrawn" );

    auto converted_hive = asset( to_convert, VESTS_SYMBOL ) * cprops.get_vesting_share_price();
    operation vop = fill_vesting_withdraw_operation( from_account.get_name(), from_account.get_name(), asset( to_convert, VESTS_SYMBOL ), converted_hive );
    //note: it has to be generated even if to_convert is zero because we've accumulated change on from_account
    //and only now we are going to update that account's VESTS (see issue #337)
    pre_push_virtual_operation( vop );

    if( has_hardfork( HIVE_HARDFORK_0_20 ) )
      rc.regenerate_rc_mana( from_account, now );
    if( has_hardfork( HIVE_HARDFORK_1_24 ) )
    {
      FC_ASSERT( dv.valid() && "The object processing `delayed votes` must exist" );

      dv->add_votes( _votes_update_data_items,
                true/*withdraw_executor*/,
                -to_withdraw.value/*val*/,
                from_account/*account*/
              );
    }

    modify( from_account, [&]( account_object& a )
    {
      a.vesting_shares.amount -= to_withdraw;
      a.balance += converted_hive;
      a.withdrawn.amount += to_withdraw;

      if( a.get_total_vesting_withdrawal() <= 0 || a.get_vesting().amount == 0 )
      {
        a.vesting_withdraw_rate.amount = 0;
        a.next_vesting_withdrawal = fc::time_point_sec::maximum();
        a.to_withdraw.amount = 0;
        a.withdrawn.amount = 0;
      }
      else
      {
        a.next_vesting_withdrawal += fc::seconds( HIVE_VESTING_WITHDRAW_INTERVAL_SECONDS );
      }
    });

    if( has_hardfork( HIVE_HARDFORK_0_20 ) )
      rc.update_account_after_vest_change( from_account, now, true, true );

    modify( cprops, [&]( dynamic_global_property_object& o )
    {
      o.total_vesting_fund_hive -= converted_hive;
      o.total_vesting_shares.amount -= to_convert;
    });

    if( has_hardfork( HIVE_HARDFORK_1_24 ) )
    {
      FC_ASSERT( dv.valid() && "Sincd HF24 the object processing `delayed votes` must exist" );

      fc::optional< ushare_type > leftover = dv->update_votes( _votes_update_data_items, now );
      FC_ASSERT( leftover.valid(), "Something went wrong" );
      if( leftover.valid() && ( *leftover ) > 0 )
        adjust_proxied_witness_votes( from_account, -( ( *leftover ).value ) );
    }
    else
    {
      if( to_withdraw > 0 )
        adjust_proxied_witness_votes( from_account, -to_withdraw );
    }

    post_push_virtual_operation( vop );
  }
  if( _benchmark_dumper.is_enabled() && count )
    _benchmark_dumper.end( "processing", "hive::protocol::withdraw_vesting_operation", count );
}

/**
  *  Overall the network has an inflation rate of 102% of virtual hive per year
  *  90% of inflation is directed to vesting shares
  *  10% of inflation is directed to subjective proof of work voting
  *  1% of inflation is directed to liquidity providers
  *  1% of inflation is directed to block producers
  *
  *  This method pays out vesting and reward shares every block, and liquidity shares once per day.
  *  This method does not pay out witnesses.
  */
void database::process_funds()
{
  const auto& props = get_dynamic_global_properties();
  const auto& wso = get_witness_schedule_object();
  const auto& feed = get_feed_history();

  if( has_hardfork( HIVE_HARDFORK_0_16__551) )
  {
    /**
      * At block 7,000,000 have a 9.5% instantaneous inflation rate, decreasing to 0.95% at a rate of 0.01%
      * every 250k blocks. This narrowing will take approximately 20.5 years and will complete on block 220,750,000
      */
    int64_t start_inflation_rate = int64_t( HIVE_INFLATION_RATE_START_PERCENT );
    int64_t inflation_rate_adjustment = int64_t( head_block_num() / HIVE_INFLATION_NARROWING_PERIOD );
    int64_t inflation_rate_floor = int64_t( HIVE_INFLATION_RATE_STOP_PERCENT );

    // below subtraction cannot underflow int64_t because inflation_rate_adjustment is <2^32
    int64_t current_inflation_rate = std::max( start_inflation_rate - inflation_rate_adjustment, inflation_rate_floor );

    safe<int64_t> new_hive;
    if( has_hardfork( HIVE_HARDFORK_1_28_NO_DHF_HBD_IN_INFLATION ) )
    {
      auto median_price = get_feed_history().current_median_history;
      FC_ASSERT( median_price.is_null() == false );

      const auto& treasury_account = get_treasury();
      const auto hbd_supply_without_treasury = (props.get_current_hbd_supply() - treasury_account.hbd_balance).amount < 0 ? asset(0, HBD_SYMBOL) : (props.get_current_hbd_supply() - treasury_account.hbd_balance);
      const auto virtual_supply_without_treasury = hbd_supply_without_treasury * median_price + props.current_supply;

      new_hive = (virtual_supply_without_treasury.amount * current_inflation_rate) / (int64_t(HIVE_100_PERCENT) * int64_t(HIVE_BLOCKS_PER_YEAR));
    }
    else
    {
      new_hive = (props.virtual_supply.amount * current_inflation_rate) / (int64_t(HIVE_100_PERCENT) * int64_t(HIVE_BLOCKS_PER_YEAR));
    }

    auto content_reward = ( new_hive * props.content_reward_percent ) / HIVE_100_PERCENT;
    if( has_hardfork( HIVE_HARDFORK_0_17__774 ) )
      content_reward = pay_reward_funds( content_reward );
    auto vesting_reward = ( new_hive * props.vesting_reward_percent ) / HIVE_100_PERCENT;
    auto dhf_new_funds = ( new_hive * props.proposal_fund_percent ) / HIVE_100_PERCENT;
    auto witness_reward = new_hive - content_reward - vesting_reward - dhf_new_funds;

    const auto& cwit = get_witness( props.current_witness );
    witness_reward *= HIVE_MAX_WITNESSES;

    if( cwit.schedule == witness_object::timeshare )
      witness_reward *= wso.timeshare_weight;
    else if( cwit.schedule == witness_object::miner )
      witness_reward *= wso.miner_weight;
    else if( cwit.schedule == witness_object::elected )
      witness_reward *= wso.elected_weight;
    else
    {
      push_virtual_operation( system_warning_operation( FC_LOG_MESSAGE( warn,
        "Encountered unknown witness type for witness: ${w}", ( "w", cwit.owner ) ).get_message() ) );
    }

    witness_reward /= wso.witness_pay_normalization_factor;

    auto new_hbd = asset( 0, HBD_SYMBOL );

    if( dhf_new_funds.value )
    {
      new_hbd = asset( dhf_new_funds, HIVE_SYMBOL ) * feed.current_median_history;
      adjust_balance( get_treasury_name(), new_hbd );
    }

    new_hive = content_reward + vesting_reward + witness_reward;

    modify( props, [&]( dynamic_global_property_object& p )
    {
      p.total_vesting_fund_hive += asset( vesting_reward, HIVE_SYMBOL );
      if( !has_hardfork( HIVE_HARDFORK_0_17__774 ) )
        p.total_reward_fund_hive += asset( content_reward, HIVE_SYMBOL );
      p.current_supply      += asset( new_hive, HIVE_SYMBOL );
      p.current_hbd_supply  += new_hbd;
      p.virtual_supply      += asset( new_hive + dhf_new_funds, HIVE_SYMBOL );
      p.dhf_interval_ledger += new_hbd;
    });

    operation vop = producer_reward_operation( cwit.owner, asset( 0, VESTS_SYMBOL ) );
    create_vesting2( *this, get_account( cwit.owner ), asset( witness_reward, HIVE_SYMBOL ), false,
      [&]( const asset& vesting_shares )
      {
        vop.get< producer_reward_operation >().vesting_shares = vesting_shares;
        pre_push_virtual_operation( vop );
      } );
    post_push_virtual_operation( vop );
  }
  else
  {
    auto content_reward = get_content_reward();
    auto curate_reward = get_curation_reward();
    auto witness_pay = get_producer_reward();
    auto vesting_reward = content_reward + curate_reward + witness_pay;

    content_reward = content_reward + curate_reward;

    if( props.head_block_number < HIVE_START_VESTING_BLOCK )
      vesting_reward.amount = 0;
    else
      vesting_reward.amount.value *= 9;

    modify( props, [&]( dynamic_global_property_object& p )
    {
        p.total_vesting_fund_hive += vesting_reward;
        p.total_reward_fund_hive  += content_reward;
        p.current_supply += content_reward + witness_pay + vesting_reward;
        p.virtual_supply += content_reward + witness_pay + vesting_reward;
    } );
  }
}

void database::process_subsidized_accounts()
{
  const witness_schedule_object& wso = get_witness_schedule_object();
  const dynamic_global_property_object& gpo = get_dynamic_global_properties();

  // Update global pool.
  modify( gpo, [&]( dynamic_global_property_object& g )
  {
    g.available_account_subsidies = rd_apply( wso.account_subsidy_rd, g.available_account_subsidies );
  } );

  // Update per-witness pool for current witness.
  const witness_object& current_witness = get_witness( gpo.current_witness );
  if( current_witness.schedule == witness_object::elected )
  {
    modify( current_witness, [&]( witness_object& w )
    {
      w.available_witness_account_subsidies = rd_apply( wso.account_subsidy_witness_rd, w.available_witness_account_subsidies );
    } );
  }
}

asset database::get_liquidity_reward()const
{
  // There is no need to update virtual_supply to take into account treasury as it's done in process_funds
  if( has_hardfork( HIVE_HARDFORK_0_12__178 ) )
    return asset( 0, HIVE_SYMBOL );

  const auto& props = get_dynamic_global_properties();
  static_assert( HIVE_LIQUIDITY_REWARD_PERIOD_SEC == 60*60, "this code assumes a 1 hour time interval" ); // NOLINT(misc-redundant-expression)
  asset percent( protocol::calc_percent_reward_per_hour< HIVE_LIQUIDITY_APR_PERCENT >( props.virtual_supply.amount ), HIVE_SYMBOL );
  return std::max( percent, HIVE_MIN_LIQUIDITY_REWARD );
}

asset database::get_content_reward()const
{
  // There is no need to update virtual_supply to take into account treasury as it's done in process_funds
  const auto& props = get_dynamic_global_properties();
  static_assert( HIVE_BLOCK_INTERVAL == 3, "this code assumes a 3-second time interval" );
  asset percent( protocol::calc_percent_reward_per_block< HIVE_CONTENT_APR_PERCENT >( props.virtual_supply.amount ), HIVE_SYMBOL );
  return std::max( percent, HIVE_MIN_CONTENT_REWARD );
}

asset database::get_curation_reward()const
{
  // There is no need to update virtual_supply to take into account treasury as it's done in process_funds
  const auto& props = get_dynamic_global_properties();
  static_assert( HIVE_BLOCK_INTERVAL == 3, "this code assumes a 3-second time interval" );
  asset percent( protocol::calc_percent_reward_per_block< HIVE_CURATE_APR_PERCENT >( props.virtual_supply.amount ), HIVE_SYMBOL);
  return std::max( percent, HIVE_MIN_CURATE_REWARD );
}

asset database::get_producer_reward()
{
  // There is no need to update virtual_supply to take into account treasury as it's done in process_funds
  const auto& props = get_dynamic_global_properties();
  static_assert( HIVE_BLOCK_INTERVAL == 3, "this code assumes a 3-second time interval" );
  asset percent( protocol::calc_percent_reward_per_block< HIVE_PRODUCER_APR_PERCENT >( props.virtual_supply.amount ), HIVE_SYMBOL);
  auto pay = std::max( percent, HIVE_MIN_PRODUCER_REWARD );
  const auto& witness_account = get_account( props.current_witness );

  /// pay witness in vesting shares
  if( props.head_block_number >= HIVE_START_MINER_VOTING_BLOCK || (witness_account.get_vesting().amount.value == 0) )
  {
    // const auto& witness_obj = get_witness( props.current_witness );
    operation vop = producer_reward_operation( witness_account.get_name(), asset( 0, VESTS_SYMBOL ) );
    create_vesting2( *this, witness_account, pay, false,
      [&]( const asset& vesting_shares )
      {
        vop.get< producer_reward_operation >().vesting_shares = vesting_shares;
        pre_push_virtual_operation( vop );
      } );
    post_push_virtual_operation( vop );
  }
  else
  {
    modify( get_account( witness_account.get_name() ), [&]( account_object& a )
    {
      a.balance += pay;
    } );
    push_virtual_operation( producer_reward_operation( witness_account.get_name(), pay ) );
  }

  return pay;
}

asset database::get_pow_reward()const
{
  // There is no need to update virtual_supply to take into account treasury as it's done in process_funds
  const auto& props = get_dynamic_global_properties();

#ifndef IS_TEST_NET
  /// 0 block rewards until at least HIVE_MAX_WITNESSES have produced a POW
  if( props.num_pow_witnesses < HIVE_MAX_WITNESSES && props.head_block_number < HIVE_START_VESTING_BLOCK )
    return asset( 0, HIVE_SYMBOL );
#endif

  static_assert( HIVE_BLOCK_INTERVAL == 3, "this code assumes a 3-second time interval" );
  static_assert( HIVE_MAX_WITNESSES == 21, "this code assumes 21 per round" );
  asset percent( calc_percent_reward_per_round< HIVE_POW_APR_PERCENT >( props.virtual_supply.amount ), HIVE_SYMBOL);
  return std::max( percent, HIVE_MIN_POW_REWARD );
}


void database::pay_liquidity_reward()
{
#ifdef IS_TEST_NET
  if( !liquidity_rewards_enabled )
    return;
#endif

  if( (head_block_num() % HIVE_LIQUIDITY_REWARD_BLOCKS) == 0 )
  {
    auto reward = get_liquidity_reward();

    if( reward.amount == 0 )
      return;

    const auto& ridx = get_index< liquidity_reward_balance_index >().indices().get< by_volume_weight >();
    auto itr = ridx.begin();
    if( itr != ridx.end() && itr->volume_weight() > 0 )
    {
      adjust_supply( reward, true );
      adjust_balance( get(itr->owner), reward );
      modify( *itr, [&]( liquidity_reward_balance_object& obj )
      {
        obj.hive_volume = 0;
        obj.hbd_volume  = 0;
        obj.last_update = head_block_time();
        obj.weight      = 0;
      } );

      push_virtual_operation( liquidity_reward_operation( get(itr->owner).get_name(), reward ) );
    }
  }
}

uint16_t database::get_curation_rewards_percent() const
{
  if( has_hardfork( HIVE_HARDFORK_0_17__774 ) )
    return get_reward_fund().percent_curation_rewards;
  else if( has_hardfork( HIVE_HARDFORK_0_8__116 ) )
    return HIVE_1_PERCENT * 25;
  else
    return HIVE_1_PERCENT * 50;
}

share_type database::pay_reward_funds( const share_type& reward )
{
  const auto& reward_idx = get_index< reward_fund_index, by_id >();
  share_type used_rewards = 0;

  for( auto itr = reward_idx.begin(); itr != reward_idx.end(); ++itr )
  {
    // reward is a per block reward and the percents are 16-bit. This should never overflow
    auto r = ( reward * itr->percent_content_rewards ) / HIVE_100_PERCENT;

    modify( *itr, [&]( reward_fund_object& rfo )
    {
      rfo.reward_balance += asset( r, HIVE_SYMBOL );
    });

    used_rewards += r;

    // Sanity check to ensure we aren't printing more HIVE than has been allocated through inflation
    FC_ASSERT( used_rewards <= reward );
  }

  return used_rewards;
}

asset database::to_hbd( const asset& hive )const
{
  return util::to_hbd( get_feed_history().current_median_history, hive );
}

asset database::to_hive( const asset& hbd )const
{
  return util::to_hive( get_feed_history().current_median_history, hbd );
}

void database::account_recovery_processing()
{
  // Clear expired recovery requests
  const auto& rec_req_idx = get_index< account_recovery_request_index, by_expiration >();
  auto rec_req = rec_req_idx.begin();

  while( rec_req != rec_req_idx.end() && rec_req->get_expiration_time() <= head_block_time() )
  {
    remove( *rec_req );
    rec_req = rec_req_idx.begin();
  }

  // Clear invalid historical authorities
  const auto& hist_idx = get_index< owner_authority_history_index >().indices(); //by id
  auto hist = hist_idx.begin();

  while( hist != hist_idx.end() && time_point_sec( hist->last_valid_time + HIVE_OWNER_AUTH_RECOVERY_PERIOD ) < head_block_time() )
  {
    remove( *hist );
    hist = hist_idx.begin();
  }

  // Apply effective recovery_account changes
  const auto& change_req_idx = get_index< change_recovery_account_request_index, by_effective_date >();
  auto change_req = change_req_idx.begin();

  while( change_req != change_req_idx.end() && change_req->get_execution_time() <= head_block_time() )
  {
    const auto& account = get_account( change_req->get_account_to_recover() );
    account_name_type old_recovery_account_name;
    if( account.has_recovery_account() )
      old_recovery_account_name = get_account( account.get_recovery_account() ).get_name();
    const auto& new_recovery_account = get_account( change_req->get_recovery_account() );
    modify( account, [&]( account_object& a )
    {
      a.set_recovery_account( new_recovery_account );
    });

    push_virtual_operation(changed_recovery_account_operation( account.get_name(), old_recovery_account_name, new_recovery_account.get_name() ));

    remove( *change_req );
    change_req = change_req_idx.begin();
  }
}

void database::process_decline_voting_rights()
{
  const auto& request_idx = get_index< decline_voting_rights_request_index >().indices().get< by_effective_date >();
  auto itr = request_idx.begin();

  int count = 0;
  if( _benchmark_dumper.is_enabled() )
    _benchmark_dumper.begin();

  const auto& proposal_votes = get_index< proposal_vote_index, by_voter_proposal >();
  remove_guard obj_perf( get_remove_threshold() );

  while( itr != request_idx.end() && itr->effective_date <= head_block_time() )
  {
    const auto& account = get< account_object, by_name >( itr->account );

    if( !has_hardfork( HIVE_HARDFORK_1_28 ) || dhf_helper::remove_proposal_votes( account, proposal_votes, *this, obj_perf ) )
    {
      nullify_proxied_witness_votes( account );
      clear_witness_votes( account );

      if( account.has_proxy() )
        push_virtual_operation( proxy_cleared_operation( account.get_name(), get_account( account.get_proxy() ).get_name()) );

      push_virtual_operation( declined_voting_rights_operation( itr->account ) );

      modify( account, [&]( account_object& a )
      {
        a.can_vote = false;
        a.clear_proxy();
      });

      remove( *itr );
      itr = request_idx.begin();
      ++count;
    }
    else
    {
      ilog("Threshold exceeded while deleting proposal votes for account ${account}.",
        ("account", account.get_name())); // to be continued in next block
      break;
    }
  }
  if( _benchmark_dumper.is_enabled() && count )
    _benchmark_dumper.end( "processing", "hive::protocol::decline_voting_rights_operation", count );
}

time_point_sec database::head_block_time()const
{
  return get_dynamic_global_properties().time;
}

uint32_t database::head_block_num()const
{
  return get_dynamic_global_properties().head_block_number;
}

block_id_type database::head_block_id()const
{
  return get_dynamic_global_properties().head_block_id;
}

uint32_t database::get_last_irreversible_block_num() const
{
  //ilog("getting irreversible_block_num irreversible is ${l}", ("l", last_irreversible_object->_irreversible_block_num));
  //ilog("getting irreversible_block_num head is ${l}", ("l", head_block_num()));
  return last_irreversible_object->_irreversible_block_num;
}

void database::set_last_irreversible_block_num(uint32_t block_num)
{
  //dlog("setting irreversible_block_num previous ${l}", ("l", last_irreversible_object->irreversible_block_num));
  FC_ASSERT(block_num >= last_irreversible_object->_irreversible_block_num, "Irreversible block can only move forward. Old: ${o}, new: ${n}",
    ("o", last_irreversible_object->_irreversible_block_num)("n", block_num));

  last_irreversible_object->_irreversible_block_num = block_num;
  //dlog("setting irreversible_block_num new ${l}", ("l", last_irreversible_object->irreversible_block_num));
}

void database::set_last_irreversible_block_data(std::shared_ptr<full_block_type> new_block)
{
  FC_ASSERT( new_block );
  FC_ASSERT(last_irreversible_object, "Premature access to non-initialized irreversible object data.");
  //dlog("setting irreversible_block_data previous ${l}", ("l", last_irreversible_object->irreversible_block_data));

  block_data_type& libd = last_irreversible_object->_irreversible_block_data;
  FC_ASSERT(new_block->get_block_num() >= protocol::block_header::num_from_id(libd._block_id),
            "Irreversible block can only move forward. Old: ${o}, new: ${n}",
              ("o", protocol::block_header::num_from_id(libd._block_id))
              ("n", new_block->get_block_num()));

  libd = *new_block;

  cached_lib = new_block;

  //dlog("setting irreversible_block_num new ${l}", ("l", last_irreversible_object->irreversible_block_data));
}

std::shared_ptr<full_block_type> database::get_last_irreversible_block_data() const
{
  FC_ASSERT(last_irreversible_object && "Premature access to non-initialized irreversible object data.");

  return cached_lib;
}

const irreversible_block_data_type* database::get_last_irreversible_object() const
{
  return last_irreversible_object;
}

// Initialization functions moved to database_init.cpp:
// - initialize_evaluators()
// - register_custom_operation_interpreter()
// - get_custom_json_evaluator()
// - initialize_indexes()
// - initialize_irreversible_storage()
// - verify_match_of_state_objects_definitions_from_shm()
// - verify_match_of_blockchain_configuration()
// - get_current_decoded_types_data_json()
// - get_json_schema()
// - init_schema()
// - init_genesis()
// - set_flush_interval()

//////////////////// private methods ////////////////////

void database::apply_block( const std::shared_ptr<full_block_type>& full_block, uint32_t skip, const block_flow_control* block_ctrl )
{ try {
  //fc::time_point begin_time = fc::time_point::now();

  detail::with_skip_flags( *this, skip, [&]()
  {
    _apply_block( full_block, block_ctrl );
  } );

  /*try
  {
  /// check invariants
  if( is_in_control() || !( skip & skip_validate_invariants ) )
    validate_invariants();
  }
  FC_CAPTURE_AND_RETHROW( (next_block) );*/

  uint32_t block_num = full_block->get_block_num();

  //fc::time_point end_time = fc::time_point::now();
  //fc::microseconds dt = end_time - begin_time;
  if( _flush_blocks != 0 )
  {
    if( _next_flush_block == 0 )
    {
      uint32_t lep = block_num + 1 + _flush_blocks * 9 / 10;
      uint32_t rep = block_num + 1 + _flush_blocks;

      // use time_point::now() as RNG source to pick block randomly between lep and rep
      uint32_t span = rep - lep;
      uint32_t x = lep;
      if( span > 0 )
      {
        uint64_t now = uint64_t( fc::time_point::now().time_since_epoch().count() );
        x += now % span;
      }
      _next_flush_block = x;
      //ilog( "Next flush scheduled at block ${b}", ("b", x) );
    }

    if( _next_flush_block == block_num )
    {
      _next_flush_block = 0;
      //ilog( "Flushing database shared memory at block ${b}", ("b", block_num) );
      flush_to_all_storages();
    }
  }

} FC_CAPTURE_AND_RETHROW((full_block->get_block())) }

void database::apply_block_extended(
  const std::shared_ptr<full_block_type>& full_block,
  uint32_t skip /*= skip_nothing*/,
  const block_flow_control* block_ctrl /*= nullptr*/ )
{
  // Note that we're treating the block as completely new.
  BOOST_SCOPE_EXIT( this_ ) { this_->clear_tx_status(); } BOOST_SCOPE_EXIT_END
  set_tx_status( database::TX_STATUS_P2P_BLOCK );

  if( skip & skip_undo_block )
  {
    apply_block( full_block, skip, block_ctrl );
  }
  else
  {
    auto block_session = start_undo_session();
    apply_block( full_block, skip, block_ctrl );
    block_session.push();
  }
}

void database::check_free_memory( bool force_print, uint32_t current_block_num )
{
  uint64_t free_mem = get_free_memory();
  uint64_t max_mem = get_max_memory();

  if( BOOST_UNLIKELY( _shared_file_full_threshold != 0 && _shared_file_scale_rate != 0 && free_mem < fc::uint128_to_uint64( ( uint128_t( HIVE_100_PERCENT - _shared_file_full_threshold ) * max_mem ) / HIVE_100_PERCENT ) ) )
  {
    uint64_t new_max = fc::uint128_to_uint64( uint128_t( max_mem * _shared_file_scale_rate ) / HIVE_100_PERCENT ) + max_mem;

    wlog( "Memory is almost full, increasing to ${mem}M", ("mem", new_max / (1024*1024)) );

    resize( new_max );

    uint32_t free_mb = uint32_t( get_free_memory() / (1024*1024) );
    wlog( "Free memory is now ${free}M", ("free", free_mb) );
    _last_free_gb_printed = free_mb / 1024;
  }
  else
  {
    uint32_t free_gb = uint32_t( free_mem / (1024*1024*1024) );
    if( BOOST_UNLIKELY( force_print || (free_gb < _last_free_gb_printed) || (free_gb > _last_free_gb_printed+1) ) )
    {
      ilog( "Free memory is now ${n}G. Current block number: ${block}", ("n", free_gb)("block",current_block_num) );
      _last_free_gb_printed = free_gb;
    }

    if( BOOST_UNLIKELY( free_gb == 0 ) )
    {
      uint32_t free_mb = uint32_t( free_mem / (1024*1024) );

  #ifdef IS_TEST_NET
    if( !disable_low_mem_warning )
  #endif
      if( free_mb <= 100 && head_block_num() % 10 == 0 )
        elog( "Free memory is now ${n}M. Increase shared file size immediately!" , ("n", free_mb) );
    }
  }
}

void database::_apply_block( const std::shared_ptr<full_block_type>& full_block, const block_flow_control* block_ctrl )
{
  const signed_block& block = full_block->get_block();
  const uint32_t block_num = full_block->get_block_num();
  block_notification note(full_block);

  try {
  notify_pre_apply_block( note );
  rc.reset_block_info();

  BOOST_SCOPE_EXIT( this_ )
  {
    this_->_currently_processing_block_id.reset();
  } BOOST_SCOPE_EXIT_END
  _currently_processing_block_id = full_block->get_block_id();

  uint32_t skip = get_node_skip_flags();
  _current_block_num    = block_num;
  _current_timestamp    = block.timestamp;
  _current_trx_in_block = 0;

  if( BOOST_UNLIKELY( block_num == 1 ) )
  {
    process_genesis_accounts();

    // For every existing before the head_block_time (genesis time), apply the hardfork
    // This allows the test net to launch with past hardforks and apply the next harfork when running

    uint32_t n;
    for( n=0; n<HIVE_NUM_HARDFORKS; n++ )
    {
      if( _hardfork_versions.times[n+1] > block.timestamp )
        break;
    }

    if( n > 0 )
    {
      ilog( "Processing ${n} genesis hardforks", ("n", n) );
      set_hardfork( n, true );
#ifdef IS_TEST_NET
      if( n < HIVE_NUM_HARDFORKS )
      {
        ilog( "Next hardfork scheduled for ${t} (current block timestamp ${c})",
          ( "t", _hardfork_versions.times[ n + 1 ] )( "c", block.timestamp ) );
      }
#endif

      const hardfork_property_object& hardfork_state = get_hardfork_property_object();
      FC_ASSERT( hardfork_state.current_hardfork_version == _hardfork_versions.versions[n], "Unexpected genesis hardfork state" );

      const auto& witness_idx = get_index<witness_index>().indices().get<by_id>();
      vector<witness_id_type> wit_ids_to_update;
      for( auto it=witness_idx.begin(); it!=witness_idx.end(); ++it )
        wit_ids_to_update.push_back( it->get_id() );

      for( witness_id_type wit_id : wit_ids_to_update )
      {
        modify( get( wit_id ), [&]( witness_object& wit )
        {
          wit.running_version = _hardfork_versions.versions[n];
          wit.hardfork_version_vote = _hardfork_versions.versions[n];
          wit.hardfork_time_vote = _hardfork_versions.times[n];
        } );
      }
    }
  }

  if( !( skip & skip_merkle_check ) )
  {
    if( _benchmark_dumper.is_enabled() )
      _benchmark_dumper.begin();

    auto merkle_root = full_block->get_merkle_root();

    try
    {
      FC_FINALIZABLE_ASSERT( block.transaction_merkle_root == merkle_root, "Merkle check failed",
                (block.transaction_merkle_root)(merkle_root)(block)("id", full_block->get_block_id()));
    }
    catch( fc::assert_exception& e )
    { //don't record & throw error if this is a block with a known bad merkle root
      const auto& merkle_map = get_shared_db_merkle();
      auto itr = merkle_map.find( block_num );
      if( itr == merkle_map.end() || itr->second != merkle_root )
      {
        FC_FINALIZE_ASSERT( e );
        throw e;
      }
    }

    if( _benchmark_dumper.is_enabled() )
      _benchmark_dumper.end( "block", "merkle check" );
  }

  const witness_object& signing_witness = validate_block_header(skip, full_block);

  const auto& gprops = get_dynamic_global_properties();
  uint32_t block_size = full_block->get_uncompressed_block().raw_size;
  if( has_hardfork( HIVE_HARDFORK_0_12 ) )
    FC_ASSERT(block_size <= gprops.maximum_block_size, "Block size is larger than voted on block size", (block_num)(block_size)("max",gprops.maximum_block_size));

  if( block_size < HIVE_MIN_BLOCK_SIZE )
    elog("Block size is too small", (block_num)(block_size)("min", HIVE_MIN_BLOCK_SIZE));

  /// modify current witness so transaction evaluators can know who included the transaction,
  /// this is mostly for POW operations which must pay the current_witness
  modify( gprops, [&]( dynamic_global_property_object& dgp ){
    dgp.current_witness = block.witness;
  });

  /// parse witness version reporting
  process_header_extensions( block );

  if( has_hardfork( HIVE_HARDFORK_0_5__54 ) ) // Cannot remove after hardfork
  {
    const auto& witness = get_witness( block.witness );
    const auto& hardfork_state = get_hardfork_property_object();
    FC_ASSERT(witness.running_version >= hardfork_state.current_hardfork_version,
              "Block produced by witness that is not running current hardfork",
              (witness)(block.witness)(hardfork_state));
  }

  for( const std::shared_ptr<full_transaction_type>& trx : full_block->get_full_transactions() )
  {
    /* We do not need to push the undo state for each transaction
      * because they either all apply and are valid or the
      * entire block fails to apply.  We only need an "undo" state
      * for transactions when validating broadcast transactions or
      * when building a block.
      */
    apply_transaction( trx, skip );
    ++_current_trx_in_block;
  }

  _current_trx_in_block = -1;
  _current_op_in_trx = 0;

  if( block_ctrl != nullptr )
  {
    FC_ASSERT( _currently_processing_block_id.value() == block_ctrl->get_full_block()->get_block_id(),
      "Wrong block control passed to the call" ); // block being processed and block in block_ctrl must be the same
    block_ctrl->on_end_of_transactions();
  }

  update_global_dynamic_data(block);
  update_signing_witness(signing_witness, block);

  uint32_t old_last_irreversible = get_last_irreversible_block_num();
  if( skip & skip_undo_block )
    set_last_irreversible_block_num( head_block_num() );
  else
    update_last_irreversible_block( std::optional<switch_forks_t>() );

  create_block_summary(full_block);
  clear_expired_transactions();
  clear_expired_orders();
  clear_expired_delegations();

  update_witness_schedule(*this);

  update_median_feed();
  update_virtual_supply(); //accommodate potentially new price

  clear_null_account_balance();
  consolidate_treasury_balance();
  process_funds();
  process_conversions();
  process_comment_cashout();
  process_vesting_withdrawals();
  process_savings_withdraws();
  process_subsidized_accounts();
  pay_liquidity_reward();
  update_virtual_supply(); //cover changes in HBD supply from above processes

  account_recovery_processing();
  expire_escrow_ratification();
  process_decline_voting_rights();
  process_proposals( note ); //new HBD converted here does not count towards limit
  process_delayed_voting( note );
  remove_expired_governance_votes();

  process_recurrent_transfers();

  rc.handle_expired_delegations();
  rc.finalize_block();

  process_hardforks();

  // notify observers that the block has been applied
  notify_post_apply_block( note );

  // This moves newly irreversible blocks from the fork db to the block log
  // and commits irreversible state to the database. This should always be the
  // last call of applying a block because it is the only thing that is not
  // reversible.
  migrate_irreversible_state(old_last_irreversible);

  _my->_last_pushed_block_number.store(gprops.head_block_number, std::memory_order_release);
  _my->_last_pushed_block_time.store(gprops.time.sec_since_epoch(), std::memory_order_release);
} FC_CAPTURE_CALL_LOG_AND_RETHROW( std::bind( &database::notify_fail_apply_block, this, note ), (block_num) ) }

struct process_header_visitor
{
  process_header_visitor( const std::string& witness, database& db ) :
    _witness( witness ),
    _db( db ) {}

  typedef void result_type;

  const std::string& _witness;
  database& _db;

  void operator()( const void_t& obj ) const
  {
    //Nothing to do.
  }

  void operator()( const version& reported_version ) const
  {
    const auto& signing_witness = _db.get_witness( _witness );
    //idump( (next_block.witness)(signing_witness.running_version)(reported_version) );

    if( reported_version != signing_witness.running_version )
    {
      _db.modify( signing_witness, [&]( witness_object& wo )
      {
        wo.running_version = reported_version;
      });
    }
  }

  void operator()( const hardfork_version_vote& hfv ) const
  {
    const auto& signing_witness = _db.get_witness( _witness );
    //idump( (next_block.witness)(signing_witness.running_version)(hfv) );

    if( hfv.hf_version != signing_witness.hardfork_version_vote || hfv.hf_time != signing_witness.hardfork_time_vote )
      _db.modify( signing_witness, [&]( witness_object& wo )
      {
        wo.hardfork_version_vote = hfv.hf_version;
        wo.hardfork_time_vote = hfv.hf_time;
      });
  }
};

void database::process_header_extensions( const signed_block& next_block )
{
  process_header_visitor _v( next_block.witness, *this );

  for( const auto& e : next_block.extensions )
    e.visit( _v );
}

void database::process_genesis_accounts()
{
  /*
    This method is evaluated after processing all transactions,
    so parameters of vop notification (trx_in_block = -1)
    matches system pattern (vops generated by chain).

    Calling this method before processing transaction makes
    notifications simillar to theese produced by normal operations,
    which is invalid, because there is no such operation
    to refer (no access to genesis [0] block).
  */

  // create virtual operations for accounts created in genesis
  const int32_t trx_in_block_prev{ _current_trx_in_block };
  _current_trx_in_block = -1;
  const auto& account_idx = get_index< chain::account_index >().indices().get< chain::by_id >();
  std::for_each(
      account_idx.begin()
    , account_idx.end()
    , [&]( const account_object& obj ){
        push_virtual_operation(
          account_created_operation( obj.get_name(), obj.get_name(), asset(0, VESTS_SYMBOL), asset(0, VESTS_SYMBOL) ) );
      }
  );
  _current_trx_in_block = trx_in_block_prev;
}

void database::update_median_feed()
{
try {
  if( (head_block_num() % HIVE_FEED_INTERVAL_BLOCKS) != 0 )
    return;

  auto now = head_block_time();
  const witness_schedule_object& wso = get_witness_schedule_object();
  vector<price> feeds; feeds.reserve( wso.num_scheduled_witnesses );
  for( int i = 0; i < wso.num_scheduled_witnesses; i++ )
  {
    const auto& wit = get_witness( wso.current_shuffled_witnesses[i] );
    if( has_hardfork( HIVE_HARDFORK_0_19__822 ) )
    {
      if( now < wit.get_last_hbd_exchange_update() + HIVE_MAX_FEED_AGE_SECONDS
        && !wit.get_hbd_exchange_rate().is_null() )
      {
        feeds.push_back( wit.get_hbd_exchange_rate() );
      }
    }
    else if( wit.get_last_hbd_exchange_update() < now + HIVE_MAX_FEED_AGE_SECONDS &&
        !wit.get_hbd_exchange_rate().is_null() )
    {
      feeds.push_back( wit.get_hbd_exchange_rate() );
    }
  }

  if( feeds.size() >= HIVE_MIN_FEEDS )
  {
    std::sort( feeds.begin(), feeds.end() );
    auto median_feed = feeds[feeds.size()/2];

    modify( get_feed_history(), [&]( feed_history_object& fho )
    {
      fho.price_history.push_back( median_feed );
      size_t hive_feed_history_window = HIVE_FEED_HISTORY_WINDOW_PRE_HF_16;
      if( has_hardfork( HIVE_HARDFORK_0_16__551) )
        hive_feed_history_window = HIVE_FEED_HISTORY_WINDOW;

      if( fho.price_history.size() > hive_feed_history_window )
        fho.price_history.pop_front();

      if( fho.price_history.size() )
      {
        std::vector< price > copy( fho.price_history.begin(), fho.price_history.end() );
        std::sort( copy.begin(), copy.end() );
        fho.current_median_history = copy[copy.size()/2];
        fho.market_median_history = fho.current_median_history;
        fho.current_min_history = copy.front();
        fho.current_max_history = copy.back();

#ifdef IS_TEST_NET
        if( skip_price_feed_limit_check )
          return;
#endif
        if( has_hardfork( HIVE_HARDFORK_0_14__230 ) )
        {
          // This block limits the effective median price to force HBD to remain at or
          // below HIVE_HBD_HARD_LIMIT of the combined market cap of HIVE and HBD.
          // The reason is to prevent individual with a lot of HBD to use sharp decline
          // in HIVE price to make in-chain-but-out-of-market conversion to HIVE and take
          // over the blockchain
          //
          // For example (for 10% hard limit), if we have 500 HIVE and 100 HBD, the price is
          // limited to 900 HBD / 500 HIVE which works out to be $1.80. At this price, 500 HIVE
          // would be valued at 500 * $1.80 = $900. 100 HBD is by definition always $100,
          // so the combined market cap is $900 + $100 = $1000.
          //
          // Generalized formula:
          // With minimal price we want existing amount of HBD (X) to be HIVE_HBD_HARD_LIMIT (L) of
          // combined market cap (CMC), meaning the existing amount of HIVE (Y) has to be 100%-L of CMC.
          // X + Y*price = CMC, X = L*CMC, Y*price = (100%-L)*CMC
          // (100% - L)*CMC = (100% - L)*X/L = (100%/L - 1)*X therefore minimal price is
          // (100%/L - 1)*X HBD per Y HIVE
          // the above has one big problem - accuracy; f.e. with L = 30% the price will be the same
          // as for L = 33%; for better accuracy we can express the price as (100%-L)*X HBD per L*Y HIVE

          const auto& dgpo = get_dynamic_global_properties();
          auto hbd_supply = dgpo.get_current_hbd_supply();
          if( has_hardfork( HIVE_HARDFORK_1_25_HBD_HARD_CAP ) )
            hbd_supply -= get_treasury().get_hbd_balance();
          if( hbd_supply.amount > 0 )
          {
            uint16_t limit = HIVE_HBD_HARD_LIMIT_PRE_HF26;
            if( has_hardfork( HIVE_HARDFORK_1_26_HBD_HARD_CAP ) )
              limit = HIVE_HBD_HARD_LIMIT;
            static_assert( ( HIVE_HBD_HARD_LIMIT % HIVE_1_PERCENT ) == 0, "Hard cap has to be expressed in full percentage points" );
            limit /= HIVE_1_PERCENT; //ABW: this is just to have two more levels of magnitude bigger margin;
              //even without it we can still fit within 64bit value, even though numbers used here are pretty big
            price min_price( asset( ( HIVE_100_PERCENT/HIVE_1_PERCENT - limit ) * hbd_supply.amount, HBD_SYMBOL ),
                             asset( limit * dgpo.get_current_supply().amount, HIVE_SYMBOL ) );

            /*
            ilog( "GREP${daily}: ${block}, ${minfeed}, ${medfeed}, ${maxfeed}, ${minprice}, ${medprice}, ${maxprice}, ${capprice}, ${debt}, ${hbdinfl}, ${hivesup}, ${virtsup}, ${hbdsup}",
              ( "daily", ( ( head_block_num() % ( 24 * HIVE_FEED_INTERVAL_BLOCKS ) ) == 0 ) ? "24" : "" )( "block", head_block_num() )
              ( "minfeed", double( feeds.front().base.amount.value ) / double( feeds.front().quote.amount.value ) )
              ( "medfeed", double( median_feed.base.amount.value ) / double( median_feed.quote.amount.value ) )
              ( "maxfeed", double( feeds.back().base.amount.value ) / double( feeds.back().quote.amount.value ) )
              ( "minprice", double( fho.current_min_history.base.amount.value ) / double( fho.current_min_history.quote.amount.value ) )
              ( "medprice", double( fho.current_median_history.base.amount.value ) / double( fho.current_median_history.quote.amount.value ) )
              ( "maxprice", double( fho.current_max_history.base.amount.value ) / double( fho.current_max_history.quote.amount.value ) )
              ( "capprice", double( min_price.base.amount.value ) / double( min_price.quote.amount.value ) )
              ( "debt", double( calculate_HBD_percent() ) / 100.0 )
              ( "hbdinfl", double( dgpo.get_hbd_interest_rate() ) / 100.0 )
              ( "hivesup", dgpo.get_current_supply().amount.value )
              ( "virtsup", dgpo.virtual_supply.amount.value )
              ( "hbdsup", hbd_supply.amount.value )
            );
            */

            if( min_price > fho.current_median_history )
            {
              push_virtual_operation( system_warning_operation( FC_LOG_MESSAGE( warn,
                "HIVE price corrected upward due to ${limit}% HBD cutoff rule, from ${actual} to ${corrected}",
                ( "limit", limit )( "actual", fho.current_median_history )( "corrected", min_price )).get_message()));

              fho.current_median_history = min_price;
            }
            //should we also correct current_min_history? note that we MUST NOT change market_median_history
            if( min_price > fho.current_max_history )
              fho.current_max_history = min_price;
          }
        }
      }
    });
  }
} FC_CAPTURE_AND_RETHROW() }

void database::apply_transaction(const std::shared_ptr<full_transaction_type>& trx, uint32_t skip)
{
  detail::with_skip_flags( *this, skip, [&]() { _apply_transaction(trx); });
}

void database::validate_transaction(const std::shared_ptr<full_transaction_type>& full_transaction, uint32_t skip)
{
  const signed_transaction& trx = full_transaction->get_transaction();

  //Skip all manner of expiration and TaPoS checking if we're on block 1; It's impossible that the transaction is
  //expired, and TaPoS makes no sense as no blocks exist.
  if (BOOST_LIKELY(head_block_num() > 0))
  {
    fc::time_point_sec now = head_block_time();

    if( full_transaction->get_runtime_expiration() == fc::time_point_sec::min() )
    {
      if( has_hardfork( HIVE_HARDFORK_1_28_EXPIRATION_TIME ) )
      {
        HIVE_ASSERT(trx.expiration <= now + HIVE_MAX_TIME_UNTIL_SIGNATURE_EXPIRATION, transaction_expiration_exception,
                    "", (trx.expiration)(now)("max_til_exp", HIVE_MAX_TIME_UNTIL_SIGNATURE_EXPIRATION));

        full_transaction->set_runtime_expiration( std::min( trx.expiration, now + HIVE_MAX_TIME_UNTIL_EXPIRATION ) );
      }
      else
      {
        HIVE_ASSERT(trx.expiration <= now + HIVE_MAX_TIME_UNTIL_EXPIRATION, transaction_expiration_exception,
                    "", (trx.expiration)(now)("max_til_exp", HIVE_MAX_TIME_UNTIL_EXPIRATION));

        full_transaction->set_runtime_expiration( trx.expiration );
      }
    }

    // the hardfork check is needed, f.e. https://explore.openhive.network/transaction/3c1eae5754cc4f70cf0efb12f9e4f9671a8df2a0
    if (has_hardfork(HIVE_HARDFORK_0_9)) // Simple solution to pending trx bug when now == trx.expiration
      HIVE_ASSERT(now < full_transaction->get_runtime_expiration(), transaction_expiration_exception, "", (now)(full_transaction->get_runtime_expiration()));
    else
      HIVE_ASSERT(now <= full_transaction->get_runtime_expiration(), transaction_expiration_exception, "", (now)(full_transaction->get_runtime_expiration()));

    if (!(skip & skip_tapos_check))
    {
      if (_benchmark_dumper.is_enabled())
        _benchmark_dumper.begin();

      block_summary_object::id_type bsid(trx.ref_block_num);
      const auto& tapos_block_summary = get<block_summary_object>(bsid);
      //Verify TaPoS block summary has correct ID prefix, and that this block's time is not past the expiration
      HIVE_ASSERT(trx.ref_block_prefix == tapos_block_summary.block_id._hash[1], transaction_tapos_exception,
                  "", (trx.ref_block_prefix)("tapos_block_summary", tapos_block_summary.block_id._hash[1]));

      if (_benchmark_dumper.is_enabled())
        _benchmark_dumper.end("transaction", "tapos check");
    }
  }

  if (!(skip & skip_validate)) /* issue #505 explains why this skip_flag is disabled */
  {
    if (_benchmark_dumper.is_enabled())
    {
      std::string name;
      full_transaction->validate([&](const operation& op, bool post) {
        if (!post)
        {
          name = _my->_evaluator_registry.get_evaluator(op).get_name(op);
          _benchmark_dumper.begin();
        }
        else
        {
          _benchmark_dumper.end("validate", name);
        }
      });
    }
    else
    {
      full_transaction->validate();
    }

    if (!has_hardfork(HIVE_HARDFORK_1_26_ENABLE_NEW_SERIALIZATION))
      HIVE_ASSERT(full_transaction->is_legacy_pack(), hive::protocol::transaction_auth_exception, "legacy serialization must be used until hardfork 26");
  }

  if (!(skip & (skip_transaction_signatures | skip_authority_check)))
  {
    auto get_active  =    [&]( const string& name ) { return authority( get< account_authority_object, by_account >( name ).active ); };
    auto get_owner   =    [&]( const string& name ) { return authority( get< account_authority_object, by_account >( name ).owner );  };
    auto get_posting =    [&]( const string& name ) { return authority( get< account_authority_object, by_account >( name ).posting );  };
    auto get_witness_key = [&]( const string& name ) { try { return get_witness( name ).signing_key; } FC_CAPTURE_AND_RETHROW((name)) };

    try
    {
      if( _benchmark_dumper.is_enabled() )
        _benchmark_dumper.begin();

      const flat_set<public_key_type>& signature_keys = full_transaction->get_signature_keys();
      const required_authorities_type& required_authorities = full_transaction->get_required_authorities();

      hive::protocol::verify_authority(has_hardfork( HIVE_HARDFORK_1_28_ALLOW_STRICT_AND_MIXED_AUTHORITIES ),
                                       has_hardfork( HIVE_HARDFORK_1_28_ALLOW_REDUNDANT_SIGNATURES ),
                                       required_authorities,
                                       signature_keys,
                                       get_active,
                                       get_owner,
                                       get_posting,
                                       get_witness_key,
                                       HIVE_MAX_SIG_CHECK_DEPTH,
                                       has_hardfork(HIVE_HARDFORK_0_20) ? HIVE_MAX_AUTHORITY_MEMBERSHIP : 0,
                                       has_hardfork(HIVE_HARDFORK_0_20) ? HIVE_MAX_SIG_CHECK_ACCOUNTS : 0,
                                       false,
                                       flat_set<account_name_type>(),
                                       flat_set<account_name_type>(),
                                       flat_set<account_name_type>());

      if (_benchmark_dumper.is_enabled())
        _benchmark_dumper.end("transaction", "verify_authority", trx.signatures.size());
    }
    catch( protocol::tx_missing_active_auth& e )
    {
      if( get_shared_db_merkle().find( head_block_num() + 1 ) == get_shared_db_merkle().end() )
      {
        FC_FINALIZE_ASSERT( e );
        throw;
      }
    }
    catch( fc::assert_exception& e )
    {
      FC_FINALIZE_ASSERT( e );
      throw;
    }
  }
}

void database::_apply_transaction(const std::shared_ptr<full_transaction_type>& full_transaction)
{ try {
  bool invalid_status_condition = (_current_tx_status == TX_STATUS_NONE);
  if( invalid_status_condition )
  {
    wlog( "Missing tx processing indicator" );
#ifdef USE_ALTERNATE_CHAIN_ID
    FC_ASSERT( not invalid_status_condition, "Missing tx processing indicator" );
#endif
    // make sure to call set_tx_status() with proper status when your call can lead here
  }

  transaction_notification note( full_transaction );
  BOOST_SCOPE_EXIT(this_) { this_->_current_trx_id = transaction_id_type(); } BOOST_SCOPE_EXIT_END
  _current_trx_id = full_transaction->get_transaction_id();
  const transaction_id_type& trx_id = full_transaction->get_transaction_id();

  uint32_t skip = get_node_skip_flags();

  if( !( skip & skip_transaction_dupe_check ) )
  {
    FC_ASSERT( !is_known_transaction( trx_id, true ), "Duplicate transaction check failed", ( trx_id ) );
  }

  const signed_transaction& trx = full_transaction->get_transaction();

  validate_transaction(full_transaction, skip);

  //Insert transaction into unique transactions database.
  if( !(skip & skip_transaction_dupe_check) )
  {
    if( _benchmark_dumper.is_enabled() )
      _benchmark_dumper.begin();

    create<transaction_object>([&](transaction_object& transaction) {
      transaction.trx_id = trx_id;
      /// WARNING: here we have to use original expiration time (instead of runtime_expiration) since transaction is valid up to HIVE_MAX_TIME_UNTIL_SIGNATURE_EXPIRATION
      transaction.expiration = trx.expiration;
    });

    if( _benchmark_dumper.is_enabled() )
      _benchmark_dumper.end( "transaction", "dupe check" );
  }

  notify_pre_apply_transaction( note );
  rc.reset_tx_info( trx );

  //Finally process the operations
  _current_op_in_trx = 0;
  for (const auto& op : trx.operations)
  { try {
    apply_operation(op);
    ++_current_op_in_trx;
  } FC_CAPTURE_AND_RETHROW( (op) ) }

  rc.finalize_transaction( *full_transaction.get() );
  notify_post_apply_transaction( note );

} FC_CAPTURE_AND_RETHROW( (full_transaction->get_transaction()) ) }


struct applied_operation_info_controller
{
  applied_operation_info_controller(const struct operation_notification** storage, const operation_notification& note) :
    _storage(storage)
    {
    *_storage = &note;
    }

  ~applied_operation_info_controller()
  {
    *_storage = nullptr;
  }

private:
  const struct operation_notification** _storage = nullptr;
};

void database::apply_operation(const operation& op)
{
  operation_notification note = create_operation_notification( op );

  applied_operation_info_controller ctrlr(&_current_applied_operation_info, note);

  notify_pre_apply_operation( note );

  std::string name;
  if( _benchmark_dumper.is_enabled() )
  {
    name = _my->_evaluator_registry.get_evaluator( op ).get_name( op );
    _benchmark_dumper.begin();
  }

  if( has_hardfork( HIVE_HARDFORK_0_20 ) )
    rc.handle_operation_discount< operation >( op );

  _my->_evaluator_registry.get_evaluator( op ).apply( op );

  if( _benchmark_dumper.is_enabled() )
    _benchmark_dumper.end( name );

  notify_post_apply_operation( note );
}

template <typename TFunction> struct fcall {};

template <typename TResult, typename... TArgs>
struct fcall<TResult(TArgs...)>
{
  using TNotification = std::function<TResult(TArgs...)>;

  fcall() = default;
  fcall(const TNotification& func, util::advanced_benchmark_dumper& dumper,
    const abstract_plugin& plugin, const std::string& context, const std::string& item_name)
    : _func(func), _benchmark_dumper(dumper), _context(context), _name(item_name) {}

  void operator () (TArgs&&... args)
  {
    if (_benchmark_dumper.is_enabled())
      _benchmark_dumper.begin();

    _func(std::forward<TArgs>(args)...);

    if (_benchmark_dumper.is_enabled())
      _benchmark_dumper.end( _context, _name );
  }

private:
  TNotification                    _func;
  util::advanced_benchmark_dumper& _benchmark_dumper;
  std::string                      _context;
  std::string                      _name;
};

template <typename TResult, typename... TArgs>
struct fcall<std::function<TResult(TArgs...)>>
  : public fcall<TResult(TArgs...)>
{
  typedef fcall<TResult(TArgs...)> TBase;
  using TBase::TBase;
};

template <bool IS_PRE_OPERATION, typename TSignal, typename TNotification>
boost::signals2::connection database::connect_impl( TSignal& signal, const TNotification& func,
  const abstract_plugin& plugin, int32_t group, const std::string& item_name )
{
  fcall<TNotification> fcall_wrapper( func, _benchmark_dumper, plugin,
    util::advanced_benchmark_dumper::generate_context_desc<IS_PRE_OPERATION>( plugin.get_name() ), item_name );

  return signal.connect(group, fcall_wrapper);
}

template< bool IS_PRE_OPERATION >
boost::signals2::connection database::any_apply_operation_handler_impl( const apply_operation_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  std::string context = util::advanced_benchmark_dumper::generate_context_desc< IS_PRE_OPERATION >( plugin.get_name() );
  auto complex_func = [this, func, &plugin, context]( const operation_notification& o )
  {
    std::string name;

    if (_benchmark_dumper.is_enabled())
    {
      name = o.op.get_stored_type_name();
      _benchmark_dumper.begin();
    }

    func( o );

    if (_benchmark_dumper.is_enabled())
      _benchmark_dumper.end( context, name );
  };

  if( IS_PRE_OPERATION )
    return _pre_apply_operation_signal.connect(group, complex_func);
  else
    return _post_apply_operation_signal.connect(group, complex_func);
}

boost::signals2::connection database::add_pre_apply_operation_handler( const apply_operation_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  return any_apply_operation_handler_impl< true/*IS_PRE_OPERATION*/ >( func, plugin, group );
}

boost::signals2::connection database::add_post_apply_operation_handler( const apply_operation_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  return any_apply_operation_handler_impl< false/*IS_PRE_OPERATION*/ >( func, plugin, group );
}

boost::signals2::connection database::add_pre_apply_transaction_handler( const apply_transaction_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  return connect_impl<true>(_pre_apply_transaction_signal, func, plugin, group, "transaction");
}

boost::signals2::connection database::add_post_apply_transaction_handler( const apply_transaction_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  return connect_impl<false>(_post_apply_transaction_signal, func, plugin, group, "transaction");
}

boost::signals2::connection database::add_pre_apply_custom_operation_handler ( const apply_custom_operation_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  return connect_impl< true/*IS_PRE_OPERATION*/ >(_pre_apply_custom_operation_signal, func, plugin, group, "custom");
}

boost::signals2::connection database::add_post_apply_custom_operation_handler( const apply_custom_operation_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  return connect_impl< false/*IS_PRE_OPERATION*/ >(_post_apply_custom_operation_signal, func, plugin, group, "custom");
}

boost::signals2::connection database::add_pre_apply_block_handler( const apply_block_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  return connect_impl<true>(_pre_apply_block_signal, func, plugin, group, "block");
}

boost::signals2::connection database::add_post_apply_block_handler( const apply_block_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  return connect_impl<false>(_post_apply_block_signal, func, plugin, group, "block");
}

boost::signals2::connection database::add_fail_apply_block_handler( const apply_block_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  return connect_impl<false>(_fail_apply_block_signal, func, plugin, group, "failed block");
}

boost::signals2::connection database::add_irreversible_block_handler( const irreversible_block_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  return connect_impl<false>(_on_irreversible_block, func, plugin, group, "irreversible");
}

boost::signals2::connection database::add_switch_fork_handler( const switch_fork_handler_t& func,
                                                                      const abstract_plugin& plugin, int32_t group )
{
  return connect_impl<false>(_switch_fork_signal, func, plugin, group, "switch_fork");
}

boost::signals2::connection database::add_pre_reindex_handler(const reindex_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  return connect_impl<true>(_pre_reindex_signal, func, plugin, group, "reindex");
}

boost::signals2::connection database::add_post_reindex_handler(const reindex_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  return connect_impl<false>(_post_reindex_signal, func, plugin, group, "reindex");
}

boost::signals2::connection database::add_finish_push_block_handler( const push_block_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  return connect_impl<false>(_finish_push_block_signal, func, plugin, group, "block");
}

boost::signals2::connection database::add_prepare_snapshot_handler(const prepare_snapshot_handler_t& func, const abstract_plugin& plugin, int32_t group)
{
  return connect_impl<true>(_prepare_snapshot_signal, func, plugin, group, "prepare_snapshot");
}

boost::signals2::connection database::add_snapshot_supplement_handler(const prepare_snapshot_data_supplement_handler_t& func, const abstract_plugin& plugin, int32_t group)
{
  return connect_impl<true>(_prepare_snapshot_supplement_signal, func, plugin, group, "prepare_snapshot_data_supplement");
}

boost::signals2::connection database::add_snapshot_supplement_handler(const load_snapshot_data_supplement_handler_t& func, const abstract_plugin& plugin, int32_t group)
{
  return connect_impl<true>(_load_snapshot_supplement_signal, func, plugin, group, "load_snapshot_data_supplement");
}

boost::signals2::connection database::add_comment_reward_handler(const comment_reward_notification_handler_t& func, const abstract_plugin& plugin, int32_t group)
{
  return connect_impl<true>(_comment_reward_signal, func, plugin, group, "comment_reward");
}

boost::signals2::connection database::add_end_of_syncing_handler(const end_of_syncing_notification_handler_t& func, const abstract_plugin& plugin, int32_t group)
{
  return connect_impl<false>(_end_of_syncing_signal, func, plugin, group, "->syncing_end");
}

boost::signals2::connection database::add_wipe_handler(const wipe_notification_handler_t& func, const abstract_plugin& plugin, int32_t group)
{
  return connect_impl<false>(_wipe_signal, func, plugin, group, "wipe storages");
}

boost::signals2::connection database::add_flush_handler( const flush_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  return connect_impl<false>(_flush_signal, func, plugin, group, "flush");
}

void database::flush_to_all_storages()
{
  get_comments_handler().flush();

  chainbase::database::flush();

  notify_flush();
}

const witness_object& database::validate_block_header( uint32_t skip, const std::shared_ptr<full_block_type>& full_block )const
{ try {
  const signed_block_header& next_block_header = full_block->get_block_header();
  FC_ASSERT( head_block_id() == next_block_header.previous, "", ("head_block_id", head_block_id())("next.prev", next_block_header.previous) );
  FC_ASSERT( head_block_time() < next_block_header.timestamp, "", ("head_block_time", head_block_time())("next", next_block_header.timestamp)("blocknum", full_block->get_block_num()) );
  const witness_object& witness = get_witness( next_block_header.witness );

  if (!(skip & skip_witness_signature))
    FC_ASSERT(witness.signing_key == full_block->get_signing_key());

  if( !(skip&skip_witness_schedule_check) )
  {
    uint32_t slot_num = get_slot_at_time( next_block_header.timestamp );
    FC_ASSERT( slot_num > 0 );

    string scheduled_witness = get_scheduled_witness( slot_num );

    FC_ASSERT(witness.owner == scheduled_witness, "Witness produced block at wrong time",
              ("block witness", next_block_header.witness)(scheduled_witness)(slot_num));
  }

  return witness;
} FC_CAPTURE_AND_RETHROW() }

void database::create_block_summary(const std::shared_ptr<full_block_type>& full_block)
{ try {
  block_summary_object::id_type bsid( full_block->get_block_num() & 0xffff );
  modify( get< block_summary_object >( bsid ), [&](block_summary_object& p) {
      p.block_id = full_block->get_block_id();
  });
} FC_CAPTURE_AND_RETHROW() }

void database::update_global_dynamic_data( const signed_block& b )
{ try {
  const dynamic_global_property_object& _dgp = get_dynamic_global_properties();

  uint32_t missed_blocks = get_slot_at_time( b.timestamp );
;
  assert( missed_blocks != 0 );
  missed_blocks--;

#ifdef USE_ALTERNATE_CHAIN_ID
  // For non-mainnet configurations generation of virtual operations related to missed blocks
  // is optional and configurable, see comments to configuration::generate_missed_block_operations
  if( configuration_data.get_generate_missed_block_operations() )
#endif
  if( head_block_num() != 0 )
  {
    auto start_time = fc::time_point::now();
    for( uint32_t i = 0; i < missed_blocks; ++i )
    {
      const auto& witness_missed = get_witness( get_scheduled_witness( i + 1 ) );
      if(  witness_missed.owner != b.witness )
      {
        modify( witness_missed, [&]( witness_object& w )
        {
          w.total_missed++;
          if( has_hardfork( HIVE_HARDFORK_0_14__278 ) && !has_hardfork( HIVE_HARDFORK_0_20__SP190 ) )
          {
            if( head_block_num() - w.last_confirmed_block_num  > HIVE_WITNESS_SHUTDOWN_THRESHOLD )
            {
              w.signing_key = public_key_type();
              push_virtual_operation( shutdown_witness_operation( w.owner ) );
            }
          }
          push_virtual_operation( producer_missed_operation( w.owner ) );
        } );
      }
    }
    
    if (missed_blocks != 0)
    {
      fc::microseconds loop_time = fc::time_point::now() - start_time;
      if( loop_time.count() > 1000000ll ) // Report delay longer than 1s.
        ilog("Missed blocks: ${missed_blocks}, time spent in loop: ${ms} ms (${us} us)", 
          (missed_blocks)("ms", loop_time.count()/1000)("us", loop_time) );
    }
  }

  _current_timestamp.reset();
  // dynamic global properties updating
  modify( _dgp, [&]( dynamic_global_property_object& dgp )
  {
    // This is constant time assuming 100% participation. It is O(B) otherwise (B = Num blocks between update)
    for( uint32_t i = 0; i < missed_blocks + 1; i++ )
    {
      dgp.participation_count -= fc::uint128_high_bits(dgp.recent_slots_filled) & 0x8000000000000000ULL ? 1 : 0;
      dgp.recent_slots_filled = ( dgp.recent_slots_filled << 1 ) + ( i == 0 ? 1 : 0 );
      dgp.participation_count += ( i == 0 ? 1 : 0 );
    }

    dgp.head_block_number = b.block_num();
    // Following FC_ASSERT should never fail, as _currently_processing_block_id is always set by caller
    FC_ASSERT( _currently_processing_block_id.valid() );
    dgp.head_block_id = *_currently_processing_block_id;
    dgp.time = b.timestamp;
    dgp.current_aslot += missed_blocks+1;
  } );

  if( !(get_node_skip_flags() & skip_undo_history_check) )
  {
    HIVE_ASSERT( _dgp.head_block_number - get_last_irreversible_block_num() < HIVE_MAX_UNDO_HISTORY, undo_database_exception,
                 "The database does not have enough undo history to support a blockchain with so many missed blocks. "
                 "Please add a checkpoint if you would like to continue applying blocks beyond this point.",
                 ("irreversible_block_num", get_last_irreversible_block_num())("head", _dgp.head_block_number)
                 ("max_undo", HIVE_MAX_UNDO_HISTORY) );
  }
} FC_CAPTURE_AND_RETHROW() }

uint16_t database::calculate_HBD_percent()
{
  auto median_price = get_feed_history().current_median_history;
  if( median_price.is_null() )
    return 0;

  const auto& dgpo = get_dynamic_global_properties();
  auto hbd_supply = dgpo.get_current_hbd_supply();
  auto virtual_supply = dgpo.virtual_supply;
  if( has_hardfork( HIVE_HARDFORK_1_24 ) )
  {
    // Removing the hbd in the treasury from the debt ratio calculations
    hbd_supply -= get_treasury().get_hbd_balance();
    if( hbd_supply.amount < 0 )
      hbd_supply = asset( 0, HBD_SYMBOL );
    virtual_supply = hbd_supply * median_price + dgpo.get_current_supply();
  }

  auto hbd_as_hive = fc::uint128_t( ( hbd_supply * median_price ).amount.value );
  hbd_as_hive *= HIVE_100_PERCENT;
  if( has_hardfork( HIVE_HARDFORK_0_21 ) )
    hbd_as_hive += virtual_supply.amount.value / 2; // added rounding
  return uint16_t( fc::uint128_to_uint64( hbd_as_hive / virtual_supply.amount.value ) );
}

void database::update_virtual_supply()
{ try {
  modify( get_dynamic_global_properties(), [&]( dynamic_global_property_object& dgp )
  {
    auto median_price = get_feed_history().current_median_history;
    dgp.virtual_supply = dgp.current_supply
      + ( median_price.is_null() ? asset( 0, HIVE_SYMBOL ) : dgp.get_current_hbd_supply() * median_price );

    if( !median_price.is_null() && has_hardfork( HIVE_HARDFORK_0_14__230 ) )
    {
      uint16_t percent_hbd = calculate_HBD_percent();

      if( percent_hbd >= dgp.hbd_stop_percent )
        dgp.hbd_print_rate = 0;
      else if( percent_hbd <= dgp.hbd_start_percent )
        dgp.hbd_print_rate = HIVE_100_PERCENT;
      else
        dgp.hbd_print_rate = ( ( dgp.hbd_stop_percent - percent_hbd ) * HIVE_100_PERCENT ) / ( dgp.hbd_stop_percent - dgp.hbd_start_percent );
    }
  });
} FC_CAPTURE_AND_RETHROW() }

void database::update_signing_witness(const witness_object& signing_witness, const signed_block& new_block)
{ try {
  const dynamic_global_property_object& dpo = get_dynamic_global_properties();
  uint64_t new_block_aslot = dpo.current_aslot + get_slot_at_time( new_block.timestamp );

  modify( signing_witness, [&]( witness_object& _wit )
  {
    _wit.last_aslot = new_block_aslot;
    _wit.last_confirmed_block_num = new_block.block_num();
  } );
} FC_CAPTURE_AND_RETHROW() }

const witness_schedule_object& database::get_witness_schedule_object_for_irreversibility() const
{
  if (has_hardfork(HIVE_HARDFORK_1_26_FUTURE_WITNESS_SCHEDULE))
    return get_future_witness_schedule_object();
  else
    return get_witness_schedule_object();
}

void database::process_fast_confirm_transaction(const std::shared_ptr<full_transaction_type>& full_transaction,
  switch_forks_t sf)
{ try {
  FC_ASSERT(has_hardfork(HIVE_HARDFORK_1_26_FAST_CONFIRMATION), "Fast confirmation transactions not valid until HF26");

  signed_transaction trx = full_transaction->get_transaction();

  FC_ASSERT( is_fast_confirm_transaction(full_transaction),
             "Use chain_plugin::push_transaction for transaction ${trx}", (trx) );

  // fast-confirm transactions are processed outside of the normal transaction processing flow,
  // so we need to explicitly call validation here.
  // Skip the tapos check for these transactions -- a witness could be on a different fork from
  // ours (so their tapos would reference a block not in our chain), but we still want to know.
  // If we have that block in our fork database and a supermajority of witnesses approve it, we
  // want to try to switch to it.
  validate_transaction(full_transaction, skip_tapos_check);

  const witness_block_approve_operation& block_approve_op = trx.operations.front().get<witness_block_approve_operation>();
  // dlog("Processing fast-confirm transaction from witness ${witness}", ("witness", block_approve_op.witness));

  const witness_schedule_object& wso_for_irreversibility = get_witness_schedule_object_for_irreversibility();
  bool witness_is_scheduled = std::any_of(wso_for_irreversibility.current_shuffled_witnesses.begin(),
                                          wso_for_irreversibility.current_shuffled_witnesses.begin() + wso_for_irreversibility.num_scheduled_witnesses,
                                          [&](const account_name_type& witness_account_name) {
                                            return block_approve_op.witness == get_witness(witness_account_name).owner;
                                          });
  if (!witness_is_scheduled)
  {
    ilog("Received a fast-confirm from witness ${witness} who is not on the schedule, ignoring", ("witness", block_approve_op.witness));
    return;
  }

  const uint32_t new_approved_block_number = block_header::num_from_id( block_approve_op.block_id );
  if (auto iter = _my->_last_fast_approved_block_by_witness.find(block_approve_op.witness);
      iter != _my->_last_fast_approved_block_by_witness.end())
  {
    const uint32_t previous_approved_block_number = block_header::num_from_id(iter->second);
    if (new_approved_block_number <= previous_approved_block_number)
    {
      ilog("Received a fast-confirm from witness ${witness} for block #${new_approved_block_number}, "
           "but we already have a fast-confirm for a block #${previous_approved_block_number} which is at least as new, ignoring",
           ("witness", block_approve_op.witness)
           (previous_approved_block_number)(new_approved_block_number));
      return;
    }
  }
  _my->_last_fast_approved_block_by_witness[block_approve_op.witness] = block_approve_op.block_id;
  dlog("Accepted fast-confirm from witness ${witness} for block #${new_approved_block_number} (${id})", ("witness", block_approve_op.witness)
       (new_approved_block_number)("id", block_approve_op.block_id));
  // ddump((_my->_last_fast_approved_block_by_witness));

  uint32_t old_last_irreversible_block = update_last_irreversible_block( std::optional<switch_forks_t>( sf ) );

  migrate_irreversible_state(old_last_irreversible_block);
} FC_CAPTURE_AND_RETHROW() }

uint32_t database::update_last_irreversible_block( std::optional<switch_forks_t> sf )
{ try {
  uint32_t old_last_irreversible = get_last_irreversible_block_num();
  /**
    * Prior to voting taking over, we must be more conservative...
    */
  if (head_block_num() < HIVE_START_MINER_VOTING_BLOCK)
  {
    if (head_block_num() > HIVE_MAX_WITNESSES)
    {
      uint32_t new_last_irreversible_block_num = head_block_num() - HIVE_MAX_WITNESSES;

      if (new_last_irreversible_block_num > get_last_irreversible_block_num())
        set_last_irreversible_block_num(new_last_irreversible_block_num);
    }
    return old_last_irreversible;
  }

  uint32_t head_block_number = head_block_num();
  if (old_last_irreversible == head_block_number)
  {
    //dlog("Head-block is already irreversible, nothing to do");
    return old_last_irreversible;
  }
  // ddump((head_block_number)(old_last_irreversible));

  // we'll need to know who the current scheduled witnesses are, because their opinions are
  // the only ones that matter
  std::vector<const witness_object*> scheduled_witness_objects;
  const witness_schedule_object& wso_for_irreversibility = get_witness_schedule_object_for_irreversibility();
  std::transform(wso_for_irreversibility.current_shuffled_witnesses.begin(),
                 wso_for_irreversibility.current_shuffled_witnesses.begin() + wso_for_irreversibility.num_scheduled_witnesses,
                 std::back_inserter(scheduled_witness_objects),
                 [&](const account_name_type& witness_account_name) {
                   return &get_witness(witness_account_name);
                 });

  const size_t offset = (HIVE_100_PERCENT - HIVE_IRREVERSIBLE_THRESHOLD) * scheduled_witness_objects.size() / HIVE_100_PERCENT;

  if (get_node_skip_flags() & skip_block_log)
  {
    // if we're doing a replay where we're not pushing blocks to the fork_db, use the old algorithm to compute last_irreversible
    // because the new algorithm requires the fork_db to have the blocks
    std::nth_element(scheduled_witness_objects.begin(), scheduled_witness_objects.begin() + offset, scheduled_witness_objects.end(),
                     [](const witness_object* a, const witness_object* b) { return a->last_confirmed_block_num < b->last_confirmed_block_num; });
    uint32_t new_last_irreversible_block_num = scheduled_witness_objects[offset]->last_confirmed_block_num;
    if (new_last_irreversible_block_num > old_last_irreversible)
      set_last_irreversible_block_num(new_last_irreversible_block_num);
    return old_last_irreversible;
  }

  // the forkdb is active, so we can use the new algorithm.  Start by getting a list of witnesses by name
  std::set<account_name_type> scheduled_witnesses;
  std::transform(scheduled_witness_objects.begin(), scheduled_witness_objects.end(),
                 std::inserter(scheduled_witnesses, scheduled_witnesses.end()),
                 [&](const witness_object* witness_obj) {
                   return witness_obj->owner;
                 });
  const unsigned witnesses_required_for_irreversiblity = scheduled_witnesses.size() - offset;
  auto new_lib_info =
    _block_writer->find_new_last_irreversible_block(  scheduled_witness_objects,
                                                      _my->_last_fast_approved_block_by_witness,
                                                      witnesses_required_for_irreversiblity,
                                                      old_last_irreversible );
  if( not new_lib_info )
  {
    return old_last_irreversible;
  }

  if (new_lib_info->found_on_another_fork)
  {
    // we found a new last irreversible block on another fork
    if (new_lib_info->new_head_block->get_block_num() < head_block_num())
    {
      // we need to switch to the fork containing our new last irreversible block candidate, but
      // that fork is shorter than our current head block, so our head block number would decrease
      // as a result.  There may be other places in the code that assume the head block number
      // never decreases.  Since we're not sure, just postpone the fork switch until we get
      // enough blocks on the candidate fork to at least equal our current head block
      return old_last_irreversible;
    }

    if( sf )
    {
      std::optional<uint32_t> return_value = (*sf)( new_lib_info->new_head_block, old_last_irreversible );
      if( return_value )
        return *return_value;
    }
    else
    {
      // we shouldn't ever trigger a fork switch when this function is called near the end of
      // _apply_block().  If it ever happens, we'll just delay the fork switch until the next
      // time we get a new block or a fast-confirm operation.
      return old_last_irreversible;
    }
  }

  // don't set last_irreversible_block to a block not yet processed (could happen during a fork switch)
  set_last_irreversible_block_num(std::min(new_lib_info->new_last_irreversible_block_num, head_block_num()));

  // clean up any fast-confirms that are no longer relevant to reversible blocks
  for (auto iter = _my->_last_fast_approved_block_by_witness.begin(); iter != _my->_last_fast_approved_block_by_witness.end();)
    if (block_header::num_from_id(iter->second) <= get_last_irreversible_block_num())
      iter = _my->_last_fast_approved_block_by_witness.erase(iter);
    else
      ++iter;

  return old_last_irreversible;
} FC_CAPTURE_AND_RETHROW() }

void database::migrate_irreversible_state(uint32_t old_last_irreversible)
{
  auto new_last_irreversible = get_last_irreversible_block_num();
  // This method should happen atomically. We cannot prevent unclean shutdown in the middle
  // of the call, but all side effects happen at the end to minimize the chance that state
  // invariants will be violated.
  try
  {
    _block_writer->store_block( new_last_irreversible, head_block_num() );

    // This deletes undo state (when present)
    commit( new_last_irreversible );

    if( old_last_irreversible < new_last_irreversible )
    {
      //ilog("Updating last irreversible block to: ${b}. Old last irreversible was: ${ob}.",
      //  ("b", new_last_irreversible)("ob", old_last_irreversible));

      get_comments_handler().on_irreversible_block( new_last_irreversible );

      //ABW: notifying one by one seems like a potential waste - notification receiver can't optimize its work
      for (uint32_t i = old_last_irreversible + 1; i <= new_last_irreversible; ++i)
        notify_irreversible_block(i);
    }

  }
  FC_CAPTURE_CALL_LOG_AND_RETHROW( [this]()
  {
    elog( "An error occured during migrating an irreversible state. The node will be closed." );
    theApp.generate_interrupt_request();
  }, ( new_last_irreversible )( old_last_irreversible ) )
}


bool database::apply_order( const limit_order_object& new_order_object )
{
  auto order_id = new_order_object.get_id();

  const auto& limit_price_idx = get_index<limit_order_index>().indices().get<by_price>();

  auto max_price = ~new_order_object.sell_price;
  auto limit_itr = limit_price_idx.lower_bound(max_price.max());
  auto limit_end = limit_price_idx.upper_bound(max_price);

  bool finished = false;
  while( !finished && limit_itr != limit_end )
  {
    auto old_limit_itr = limit_itr;
    ++limit_itr;
    // match returns 2 when only the old order was fully filled. In this case, we keep matching; otherwise, we stop.
    finished = ( match(new_order_object, *old_limit_itr, old_limit_itr->sell_price) & 0x1 );
  }

  return find< limit_order_object >( order_id ) == nullptr;
}

int database::match( const limit_order_object& new_order, const limit_order_object& old_order, const price& match_price )
{
  bool has_hf_20__1815 = has_hardfork( HIVE_HARDFORK_0_20__1815 );

FC_TODO( " Remove if(), do assert unconditionally after HF20 occurs" )
  if( has_hf_20__1815 )
  {
    HIVE_ASSERT( new_order.sell_price.quote.symbol == old_order.sell_price.base.symbol,
      order_match_exception, "error matching orders: ${new_order} ${old_order} ${match_price}",
      ("new_order", new_order)("old_order", old_order)("match_price", match_price) );
    HIVE_ASSERT( new_order.sell_price.base.symbol  == old_order.sell_price.quote.symbol,
      order_match_exception, "error matching orders: ${new_order} ${old_order} ${match_price}",
      ("new_order", new_order)("old_order", old_order)("match_price", match_price) );
    HIVE_ASSERT( new_order.for_sale > 0 && old_order.for_sale > 0,
      order_match_exception, "error matching orders: ${new_order} ${old_order} ${match_price}",
      ("new_order", new_order)("old_order", old_order)("match_price", match_price) );
    HIVE_ASSERT( match_price.quote.symbol == new_order.sell_price.base.symbol,
      order_match_exception, "error matching orders: ${new_order} ${old_order} ${match_price}",
      ("new_order", new_order)("old_order", old_order)("match_price", match_price) );
    HIVE_ASSERT( match_price.base.symbol == old_order.sell_price.base.symbol,
      order_match_exception, "error matching orders: ${new_order} ${old_order} ${match_price}",
      ("new_order", new_order)("old_order", old_order)("match_price", match_price) );
  }

  auto new_order_for_sale = new_order.amount_for_sale();
  auto old_order_for_sale = old_order.amount_for_sale();

  asset new_order_pays, new_order_receives, old_order_pays, old_order_receives;

  if( new_order_for_sale <= old_order_for_sale * match_price )
  {
    old_order_receives = new_order_for_sale;
    new_order_receives  = new_order_for_sale * match_price;
  }
  else
  {
    //This line once read: assert( old_order_for_sale < new_order_for_sale * match_price );
    //This assert is not always true -- see trade_amount_equals_zero in operation_tests.cpp
    //Although new_order_for_sale is greater than old_order_for_sale * match_price, old_order_for_sale == new_order_for_sale * match_price
    //Removing the assert seems to be safe -- apparently no asset is created or destroyed.
    new_order_receives = old_order_for_sale;
    old_order_receives = old_order_for_sale * match_price;
  }

  old_order_pays = new_order_receives;
  new_order_pays = old_order_receives;

FC_TODO( " Remove if(), do assert unconditionally after HF20 occurs" )
  if( has_hf_20__1815 )
  {
    HIVE_ASSERT( new_order_pays == new_order.amount_for_sale() ||
              old_order_pays == old_order.amount_for_sale(),
      order_match_exception, "error matching orders: ${new_order} ${old_order} ${match_price}",
      ("new_order", new_order)("old_order", old_order)("match_price", match_price) );
  }

  auto age = head_block_time() - old_order.created;
  if( !has_hardfork( HIVE_HARDFORK_0_12__178 ) &&
      ( (age >= HIVE_MIN_LIQUIDITY_REWARD_PERIOD_SEC && !has_hardfork( HIVE_HARDFORK_0_10__149)) ||
      (age >= HIVE_MIN_LIQUIDITY_REWARD_PERIOD_SEC_HF10 && has_hardfork( HIVE_HARDFORK_0_10__149) ) ) )
  {
    if( old_order_receives.symbol == HIVE_SYMBOL )
    {
      adjust_liquidity_reward( get_account( old_order.seller ), old_order_receives, false );
      adjust_liquidity_reward( get_account( new_order.seller ), -old_order_receives, false );
    }
    else
    {
      adjust_liquidity_reward( get_account( old_order.seller ), new_order_receives, true );
      adjust_liquidity_reward( get_account( new_order.seller ), -new_order_receives, true );
    }
  }

  push_virtual_operation( fill_order_operation( new_order.seller, new_order.orderid, new_order_pays, old_order.seller, old_order.orderid, old_order_pays ) );

  int result = 0;
  result |= fill_order( new_order, new_order_pays, new_order_receives );
  result |= fill_order( old_order, old_order_pays, old_order_receives ) << 1;

FC_TODO( " Remove if(), do assert unconditionally after HF20 occurs" )
  if( has_hf_20__1815 )
  {
    HIVE_ASSERT( result != 0,
      order_match_exception, "error matching orders: ${new_order} ${old_order} ${match_price}",
      ("new_order", new_order)("old_order", old_order)("match_price", match_price) );
  }
  return result;
}


void database::adjust_liquidity_reward( const account_object& owner, const asset& volume, bool is_hbd )
{
  const auto& ridx = get_index< liquidity_reward_balance_index >().indices().get< by_owner >();
  auto itr = ridx.find( owner.get_id() );
  if( itr != ridx.end() )
  {
    modify<liquidity_reward_balance_object>( *itr, [&]( liquidity_reward_balance_object& r )
    {
      if( head_block_time() - r.last_update >= HIVE_LIQUIDITY_TIMEOUT_SEC )
      {
        r.hbd_volume = 0;
        r.hive_volume = 0;
        r.weight = 0;
      }

      if( is_hbd )
        r.hbd_volume += volume.amount.value;
      else
        r.hive_volume += volume.amount.value;

      r.update_weight( has_hardfork( HIVE_HARDFORK_0_10__141 ) );
      r.last_update = head_block_time();
    } );
  }
  else
  {
    create<liquidity_reward_balance_object>( [&](liquidity_reward_balance_object& r )
    {
      r.owner = owner.get_id();
      if( is_hbd )
        r.hbd_volume = volume.amount.value;
      else
        r.hive_volume = volume.amount.value;

      r.update_weight( has_hardfork( HIVE_HARDFORK_0_9__141 ) );
      r.last_update = head_block_time();
    } );
  }
}


bool database::fill_order( const limit_order_object& order, const asset& pays, const asset& receives )
{
  try
  {
    HIVE_ASSERT( order.amount_for_sale().symbol == pays.symbol,
      order_fill_exception, "error filling orders: ${order} ${pays} ${receives}",
      ("order", order)("pays", pays)("receives", receives) );
    HIVE_ASSERT( pays.symbol != receives.symbol,
      order_fill_exception, "error filling orders: ${order} ${pays} ${receives}",
      ("order", order)("pays", pays)("receives", receives) );

    adjust_balance( order.seller, receives );

    if( pays == order.amount_for_sale() )
    {
      remove( order );
      return true;
    }
    else
    {
FC_TODO( " Remove if(), do assert unconditionally after HF20 occurs" )
      if( has_hardfork( HIVE_HARDFORK_0_20__1815 ) )
      {
        HIVE_ASSERT( pays < order.amount_for_sale(),
          order_fill_exception, "error filling orders: ${order} ${pays} ${receives}",
          ("order", order)("pays", pays)("receives", receives) );
      }

      modify( order, [&]( limit_order_object& b )
      {
        b.for_sale -= pays.amount;
      } );
      /**
        *  There are times when the AMOUNT_FOR_SALE * SALE_PRICE == 0 which means that we
        *  have hit the limit where the seller is asking for nothing in return.  When this
        *  happens we must refund any balance back to the seller, it is too small to be
        *  sold at the sale price.
        */
      if( order.amount_to_receive().amount == 0 )
      {
        cancel_order(order);
        return true;
      }
      return false;
    }
  }
  FC_CAPTURE_AND_RETHROW( (order)(pays)(receives) )
}

void database::cancel_order( const limit_order_object& order, bool suppress_vop )
{
  auto amount_back = order.amount_for_sale();

  adjust_balance( order.seller, amount_back );
  if( !suppress_vop )
    push_virtual_operation(limit_order_cancelled_operation(order.seller, order.orderid, amount_back));

  remove(order);
}


void database::clear_expired_transactions()
{
  //Look for expired transactions in the deduplication list, and remove them.
  //Transactions must have expired by at least two forking windows in order to be removed.
  auto& transaction_idx = get_index< transaction_index >();
  const auto& dedupe_index = transaction_idx.indices().get< by_expiration >();
  while( ( !dedupe_index.empty() ) && ( head_block_time() > dedupe_index.begin()->expiration ) )
    remove( *dedupe_index.begin() );
}

void database::clear_expired_orders()
{
  auto now = head_block_time();
  const auto& orders_by_exp = get_index<limit_order_index>().indices().get<by_expiration>();
  auto itr = orders_by_exp.begin();
  while( itr != orders_by_exp.end() && itr->expiration < now )
  {
    cancel_order( *itr );
    itr = orders_by_exp.begin();
  }
}

void database::clear_expired_delegations()
{
  auto now = head_block_time();
  const auto& delegations_by_exp = get_index< vesting_delegation_expiration_index, by_expiration >();
  auto itr = delegations_by_exp.begin();
  const auto& gpo = get_dynamic_global_properties();

  while( itr != delegations_by_exp.end() && itr->get_expiration_time() < now )
  {
    auto& delegator = get_account( itr->get_delegator() );
    operation vop = return_vesting_delegation_operation( delegator.get_name(), itr->get_vesting() );
    try{
    pre_push_virtual_operation( vop );

    modify( delegator, [&]( account_object& a )
    {
      if( has_hardfork( HIVE_HARDFORK_0_20 ) )
      {
        util::update_manabar( gpo, a, itr->get_vesting().amount.value );
        rc.regenerate_rc_mana( a, now );
      }

      a.delegated_vesting_shares -= itr->get_vesting();
    });
    if( has_hardfork( HIVE_HARDFORK_0_20 ) )
      rc.update_account_after_vest_change( delegator, now );

    post_push_virtual_operation( vop );

    remove( *itr );
    itr = delegations_by_exp.begin();
  } FC_CAPTURE_AND_RETHROW( (vop) ) }
}
#ifdef HIVE_ENABLE_SMT
template< typename smt_balance_object_type, typename modifier_type >
void database::adjust_smt_balance( const account_object& owner, const asset& delta, modifier_type&& modifier )
{
  asset_symbol_type liquid_symbol = delta.symbol.is_vesting() ? delta.symbol.get_paired_symbol() : delta.symbol;
  const smt_balance_object_type* bo = find< smt_balance_object_type, by_owner_liquid_symbol >( boost::make_tuple( owner.get_id(), liquid_symbol ) );
  // Note that SMT related code, being post-20-hf needs no hf-guard to do balance checks.
  if( bo == nullptr )
  {
    // No balance object related to the SMT means '0' balance. Check delta to avoid creation of negative balance.
    FC_ASSERT( delta.amount.value >= 0, "Insufficient SMT ${smt} funds", ("smt", delta.symbol) );
    // No need to create object with '0' balance (see comment above).
    if( delta.amount.value == 0 )
      return;

    auto& new_balance_object = create< smt_balance_object_type >( owner, liquid_symbol );
    bo = &new_balance_object;
  }

  modify( *bo, std::forward<modifier_type>( modifier ) );
  if( bo->is_empty() )
  {
    // Zero balance is the same as non object balance at all.
    // Remove balance object if both liquid and vesting balances are zero.
    remove( *bo );
  }
}
#endif

void database::modify_balance( const account_object& a, const asset& delta, bool check_balance )
{
  const bool trace_balance_change = false; //a.get_name() == "X";
  std::string op_context;

  if(trace_balance_change)
  {
    if(_current_applied_operation_info != nullptr)
      op_context = fc::json::to_string(_current_applied_operation_info->op);
    else
      op_context = "No operation context";
  }

  modify( a, [&]( account_object& acnt )
  {
    if( delta.symbol.asset_num == HIVE_ASSET_NUM_HIVE )
    {
      auto b = acnt.balance;
      acnt.balance += delta;
      if(trace_balance_change)
        ilog("${a} HIVE balance changed to ${nb} (previous: ${b} ) at block: ${block}. Operation context: ${c}", ("a", a.get_name())("b", b.amount)("nb", acnt.balance.amount)("block", _current_block_num)("c", op_context));

      if( check_balance )
      {
        FC_ASSERT( acnt.get_balance().amount.value >= 0, "Insufficient HIVE funds" );
      }
    }
    else if( delta.symbol.asset_num == HIVE_ASSET_NUM_HBD )
    {
      /// Starting from HF 25 HBD interest will be paid only from saving balance.
      if( has_hardfork(HIVE_HARDFORK_1_25) == false && a.hbd_seconds_last_update != head_block_time() )
      {
        const auto _head_block_time = head_block_time();
        const bool update_hdb_balance = (_head_block_time - a.hbd_last_interest_payment).to_seconds() > HIVE_HBD_INTEREST_COMPOUND_INTERVAL_SEC;
        const auto interest = hive::protocol::hbd_interest::evaluate_hbd_interest(&acnt.hbd_seconds, _head_block_time, a.get_hbd_balance(), a.hbd_seconds_last_update,
          get_dynamic_global_properties().get_hbd_interest_rate(), update_hdb_balance);
        acnt.hbd_seconds_last_update = _head_block_time;

        if( acnt.hbd_seconds > 0 && update_hdb_balance )
        {
          asset interest_paid(fc::uint128_to_uint64(interest), HBD_SYMBOL);
          acnt.hbd_balance += interest_paid;
          acnt.hbd_seconds = 0;
          acnt.hbd_last_interest_payment = _head_block_time;

          if(interest > 0)
            push_virtual_operation( interest_operation( a.get_name(), interest_paid, true ) );

          modify( get_dynamic_global_properties(), [&]( dynamic_global_property_object& props)
          {
            props.current_hbd_supply += interest_paid;
            props.virtual_supply += interest_paid * get_feed_history().current_median_history;
          } );
        }
      }

      auto b = acnt.hbd_balance;
      acnt.hbd_balance += delta;

      if(trace_balance_change)
        ilog("${a} HBD balance changed to ${nb} (previous: ${b} ) at block: ${block}. Operation context: ${c}", ("a", a.get_name())("b", b.amount)("nb", acnt.hbd_balance.amount)("block", _current_block_num)("c", op_context));

      if( check_balance )
      {
        FC_ASSERT( acnt.get_hbd_balance().amount.value >= 0, "Insufficient HBD funds" );
      }
    }
    else
    {
      FC_ASSERT( delta.symbol.asset_num == HIVE_ASSET_NUM_VESTS, "invalid symbol" );

      acnt.vesting_shares += delta;
      if( check_balance )
      {
        FC_ASSERT( acnt.get_vesting().amount.value >= 0, "Insufficient VESTS funds" );
      }
    }
  } );
}

void database::modify_reward_balance( const account_object& a, const asset& value_delta, const asset& share_delta, bool check_balance )
{
  modify( a, [&]( account_object& acnt )
  {
    if( value_delta.symbol.asset_num == HIVE_ASSET_NUM_HIVE )
    {
      if( share_delta.amount.value == 0 )
      {
        acnt.reward_hive_balance += value_delta;
        if( check_balance )
        {
          FC_ASSERT( acnt.get_rewards().amount.value >= 0, "Insufficient reward HIVE funds" );
        }
      }
      else
      {
        acnt.reward_vesting_hive += value_delta;
        acnt.reward_vesting_balance += share_delta;
        if( check_balance )
        {
          FC_ASSERT( acnt.get_vest_rewards().amount.value >= 0, "Insufficient reward VESTS funds" );
        }
      }
    }
    else
    {
      FC_ASSERT( value_delta.symbol.asset_num == HIVE_ASSET_NUM_HBD, "invalid symbol" );
      FC_ASSERT( share_delta.amount.value == 0 );
      acnt.reward_hbd_balance += value_delta;
      if( check_balance )
      {
        FC_ASSERT( acnt.get_hbd_rewards().amount.value >= 0, "Insufficient reward HBD funds" );
      }
    }
  });
}

void database::adjust_balance( const account_object& a, const asset& delta )
{
  if ( delta.amount < 0 )
  {
    asset available = get_balance( a, delta.symbol );
    FC_ASSERT( available >= -delta,
      "Account ${acc} does not have sufficient funds for balance adjustment. Required: ${r}, Available: ${a}",
        ("acc", a.get_name())("r", delta)("a", available) );
  }

  bool check_balance = has_hardfork( HIVE_HARDFORK_0_20__1811 );

#ifdef HIVE_ENABLE_SMT
  if( delta.symbol.space() == asset_symbol_type::smt_nai_space )
  {
    // No account object modification for SMT balance, hence separate handling here.
    // Note that SMT related code, being post-20-hf needs no hf-guard to do balance checks.
    adjust_smt_balance< account_regular_balance_object >( a, delta, [&]( account_regular_balance_object& bo )
    {
      if( delta.symbol.is_vesting() )
      {
        bo.vesting += delta;
        // Check result to avoid negative balance storing.
        FC_ASSERT( bo.vesting.amount >= 0, "Insufficient SMT ${smt} funds", ( "smt", delta.symbol ) ); //ABW: redundant with check at start
      }
      else
      {
        bo.liquid += delta;
        // Check result to avoid negative balance storing.
        FC_ASSERT( bo.liquid.amount >= 0, "Insufficient SMT ${smt} funds", ( "smt", delta.symbol ) ); //ABW: redundant with check at start
      }
    } );
  }
  else
#endif
  {
    modify_balance( a, delta, check_balance );
  }
}

void database::adjust_savings_balance( const account_object& a, const asset& delta )
{
  bool check_balance = has_hardfork( HIVE_HARDFORK_0_20__1811 );

  modify( a, [&]( account_object& acnt )
  {
    if( delta.symbol.asset_num == HIVE_ASSET_NUM_HIVE )
    {
      acnt.savings_balance += delta;
      if( check_balance )
      {
        FC_ASSERT( acnt.get_savings().amount.value >= 0, "Insufficient savings HIVE funds" );
      }
    }
    else
    {
      FC_ASSERT( delta.symbol.asset_num == HIVE_ASSET_NUM_HBD && "invalid symbol" );
      if( a.savings_hbd_seconds_last_update != head_block_time() )
      {
        const auto _head_block_time = head_block_time();
        const bool update_savings_hdb_balance = (_head_block_time - a.savings_hbd_last_interest_payment).to_seconds() > HIVE_HBD_INTEREST_COMPOUND_INTERVAL_SEC;
        const auto interest = hive::protocol::hbd_interest::evaluate_hbd_interest(&acnt.savings_hbd_seconds, _head_block_time, a.get_hbd_savings(), a.savings_hbd_seconds_last_update,
          get_dynamic_global_properties().get_hbd_interest_rate(), update_savings_hdb_balance);
        acnt.savings_hbd_seconds_last_update = _head_block_time;

        if( acnt.savings_hbd_seconds > 0 && update_savings_hdb_balance )
        {
          asset interest_paid(fc::uint128_to_uint64(interest), HBD_SYMBOL);
          acnt.savings_hbd_balance += interest_paid;
          acnt.savings_hbd_seconds = 0;
          acnt.savings_hbd_last_interest_payment = _head_block_time;

          if(interest > 0)
            push_virtual_operation( interest_operation( a.get_name(), interest_paid, false ) );

          modify( get_dynamic_global_properties(), [&]( dynamic_global_property_object& props)
          {
            props.current_hbd_supply += interest_paid;
            props.virtual_supply += interest_paid * get_feed_history().current_median_history;
          } );
        }
      }
      acnt.savings_hbd_balance += delta;
      if( check_balance )
      {
        FC_ASSERT( acnt.get_hbd_savings().amount.value >= 0, "Insufficient savings HBD funds" );
      }
    }
  } );
}

void database::adjust_reward_balance( const account_object& a, const asset& value_delta,
                          const asset& share_delta /*= asset(0,VESTS_SYMBOL)*/ )
{
  bool check_balance = has_hardfork( HIVE_HARDFORK_0_20__1811 );
  FC_ASSERT( value_delta.symbol.is_vesting() == false && share_delta.symbol.is_vesting() );

#ifdef HIVE_ENABLE_SMT
  // No account object modification for SMT balance, hence separate handling here.
  // Note that SMT related code, being post-20-hf needs no hf-guard to do balance checks.
  if( value_delta.symbol.space() == asset_symbol_type::smt_nai_space )
  {
    adjust_smt_balance< account_rewards_balance_object >( a, value_delta, [&]( account_rewards_balance_object& bo )
    {
      if( share_delta.amount.value != 0 )
      {
        bo.pending_vesting_value += value_delta;
        bo.pending_vesting_shares += share_delta;
        // Check result to avoid negative balance storing.
        FC_ASSERT( bo.pending_vesting_value.amount >= 0, "Insufficient SMT ${smt} funds", ( "smt", value_delta.symbol ) );
      }
      else
      {
        bo.pending_liquid += value_delta;
        // Check result to avoid negative balance storing.
        FC_ASSERT( bo.pending_liquid.amount >= 0, "Insufficient SMT ${smt} funds", ( "smt", value_delta.symbol ) );
      }
    } );
  }
  else
#endif
  {
    modify_reward_balance( a, value_delta, share_delta, check_balance );
  }
}

void database::adjust_supply( const asset& delta, bool adjust_vesting )
{
#ifdef HIVE_ENABLE_SMT
  if( delta.symbol.space() == asset_symbol_type::smt_nai_space )
  {
    const auto& smt = get< smt_token_object, by_symbol >( delta.symbol );
    auto smt_new_supply = smt.current_supply + delta.amount;
    FC_ASSERT( smt_new_supply >= 0 );
    modify( smt, [smt_new_supply]( smt_token_object& smt )
    {
      smt.current_supply = smt_new_supply;
    });
    return;
  }
#endif

  bool check_supply = has_hardfork( HIVE_HARDFORK_0_20__1811 );

  const auto& props = get_dynamic_global_properties();
  if( props.head_block_number < HIVE_BLOCKS_PER_DAY*7 )
    adjust_vesting = false;

  modify( props, [&]( dynamic_global_property_object& props )
  {
    if( delta.symbol.asset_num == HIVE_ASSET_NUM_HIVE )
    {
      asset new_vesting( (adjust_vesting && delta.amount > 0) ? delta.amount * 9 : 0, HIVE_SYMBOL );
      props.current_supply += delta + new_vesting;
      props.virtual_supply += delta + new_vesting;
      props.total_vesting_fund_hive += new_vesting;
      if( check_supply )
      {
        FC_ASSERT( props.current_supply.amount.value >= 0 );
      }
    }
    else
    {
      FC_ASSERT( delta.symbol.asset_num == HIVE_ASSET_NUM_HBD, "invalid symbol" );
      props.current_hbd_supply += delta;
      props.virtual_supply = props.get_current_hbd_supply() * get_feed_history().current_median_history + props.current_supply;
      if( check_supply )
      {
        FC_ASSERT( props.get_current_hbd_supply().amount.value >= 0 );
      }
    }
  } );
}


asset database::get_balance( const account_object& a, asset_symbol_type symbol )const
{
  if( symbol.asset_num == HIVE_ASSET_NUM_HIVE )
  {
    return a.get_balance();
  }
  else
  {
#ifndef HIVE_ENABLE_SMT
    FC_ASSERT( symbol.asset_num == HIVE_ASSET_NUM_HBD, "Invalid symbol: ${s}", ("s", symbol) );
    return a.get_hbd_balance();
#else
    if( symbol.asset_num == HIVE_ASSET_NUM_HBD )
      return a.get_hbd_balance();
    else
    {
      FC_ASSERT( symbol.space() == asset_symbol_type::smt_nai_space, "Invalid symbol: ${s}", ("s", symbol) );
      auto key = boost::make_tuple( a.get_id(), symbol.is_vesting() ? symbol.get_paired_symbol() : symbol );
      const account_regular_balance_object* arbo = find< account_regular_balance_object, by_owner_liquid_symbol >( key );
      if( arbo == nullptr )
      {
        return asset(0, symbol);
      }
      else
      {
        return symbol.is_vesting() ? arbo->vesting : arbo->liquid;
      }
    }
#endif
  }
}

asset database::get_savings_balance( const account_object& a, asset_symbol_type symbol )const
{
  if( symbol.asset_num == HIVE_ASSET_NUM_HIVE )
  {
    return a.get_savings();
  }
  else
  {
    FC_ASSERT( symbol.asset_num == HIVE_ASSET_NUM_HBD && "invalid symbol" );
    return a.get_hbd_savings();
  }
}

// Hardfork functions moved to database_hardfork.cpp:
// - init_hardforks()
// - process_hardforks()
// - has_hardfork()
// - get_hardfork()
// - set_hardfork()
// - apply_hardfork()
void database::retally_liquidity_weight() {
  const auto& ridx = get_index< liquidity_reward_balance_index >().indices().get< by_owner >();
  for( const auto& i : ridx ) {
    modify( i, []( liquidity_reward_balance_object& o ){
      o.update_weight(true/*HAS HARDFORK10 if this method is called*/);
    });
  }
}

/**
  * Verifies all supply invariantes check out
  */
void database::validate_invariants()const
{
  try
  {
    const auto& account_idx = get_index< account_index, by_name >();
    asset total_supply = asset( 0, HIVE_SYMBOL );
    asset total_hbd = asset( 0, HBD_SYMBOL );
    asset total_vesting = asset( 0, VESTS_SYMBOL );
    asset pending_vesting_hive = asset( 0, HIVE_SYMBOL );
    share_type total_vsf_votes = share_type( 0 );
    ushare_type total_delayed_votes = ushare_type( 0 );

    uint64_t witness_no = 0;
    uint64_t account_no = 0;
    uint64_t convert_no = 0;
    uint64_t collateralized_convert_no = 0;
    uint64_t order_no = 0;
    uint64_t escrow_no = 0;
    uint64_t withdrawal_no = 0;
    uint64_t reward_fund_no = 0;
    uint64_t contribution_no = 0;

    auto& gpo = get_dynamic_global_properties();

    /// verify no witness has too many votes
    const auto& witness_idx = get_index< witness_index >().indices();
    for( auto itr = witness_idx.begin(); itr != witness_idx.end(); ++itr )
    {
      FC_ASSERT( itr->votes <= gpo.total_vesting_shares.amount, "", ("itr",*itr) );
      ++witness_no;
    }

    for( auto itr = account_idx.begin(); itr != account_idx.end(); ++itr )
    {
      total_supply += itr->get_balance();
      total_supply += itr->get_savings();
      total_supply += itr->get_rewards();
      total_hbd += itr->get_hbd_balance();
      total_hbd += itr->get_hbd_savings();
      total_hbd += itr->get_hbd_rewards();
      total_vesting += itr->get_vesting();
      total_vesting += itr->get_vest_rewards();
      pending_vesting_hive += itr->get_vest_rewards_as_hive();
      total_vsf_votes += ( !itr->has_proxy() ?
                      itr->get_governance_vote_power() :
                      ( HIVE_MAX_PROXY_RECURSION_DEPTH > 0 ?
                          itr->proxied_vsf_votes[HIVE_MAX_PROXY_RECURSION_DEPTH - 1] :
                          itr->get_direct_governance_vote_power() ) );
      total_delayed_votes += itr->sum_delayed_votes;
      ushare_type sum_delayed_votes{ 0ul };
      for( auto& dv : itr->delayed_votes )
        sum_delayed_votes += dv.val;
      FC_ASSERT( sum_delayed_votes == itr->sum_delayed_votes, "", ("sum_delayed_votes",sum_delayed_votes)("itr->sum_delayed_votes",itr->sum_delayed_votes) );
      FC_ASSERT( sum_delayed_votes.value <= itr->get_vesting().amount, "", ("sum_delayed_votes",sum_delayed_votes)("itr->vesting_shares.amount",itr->get_vesting().amount)("account",itr->get_name()) );
      ++account_no;
    }

    {
      asset convert_hbd( 0, HBD_SYMBOL );
      asset collateral_hive( 0, HIVE_SYMBOL );
      get_convert_request_totals( convert_hbd, collateral_hive, convert_no, collateralized_convert_no );
      total_hbd += convert_hbd;
      total_supply += collateral_hive;
    }

    const auto& limit_order_idx = get_index< limit_order_index >().indices();

    for( auto itr = limit_order_idx.begin(); itr != limit_order_idx.end(); ++itr )
    {
      if( itr->sell_price.base.symbol == HIVE_SYMBOL )
      {
        total_supply += asset( itr->for_sale, HIVE_SYMBOL );
      }
      else if ( itr->sell_price.base.symbol == HBD_SYMBOL )
      {
        total_hbd += asset( itr->for_sale, HBD_SYMBOL );
      }
      ++order_no;
    }

    {
      asset escrow_hive( 0, HIVE_SYMBOL );
      asset escrow_hbd( 0, HBD_SYMBOL );
      get_escrow_totals( escrow_hive, escrow_hbd, escrow_no );
      total_supply += escrow_hive;
      total_hbd += escrow_hbd;
    }

    {
      asset withdraw_hive( 0, HIVE_SYMBOL );
      asset withdraw_hbd( 0, HBD_SYMBOL );
      get_savings_withdraw_totals( withdraw_hive, withdraw_hbd, withdrawal_no );
      total_supply += withdraw_hive;
      total_hbd += withdraw_hbd;
    }

    const auto& reward_idx = get_index< reward_fund_index, by_id >();

    for( auto itr = reward_idx.begin(); itr != reward_idx.end(); ++itr )
    {
      total_supply += itr->reward_balance;
      ++reward_fund_no;
    }

#ifdef HIVE_ENABLE_SMT
    const auto& smt_contribution_idx = get_index< smt_contribution_index, by_id >();

    for ( auto itr = smt_contribution_idx.begin(); itr != smt_contribution_idx.end(); ++itr )
    {
      total_supply += itr->contribution;
      ++contribution_no;
    }
#endif

    total_supply += gpo.get_total_vesting_fund_hive() + gpo.get_total_reward_fund_hive() + gpo.get_pending_rewarded_vesting_hive();

    FC_ASSERT( gpo.current_supply == total_supply, "", ("gpo.current_supply",gpo.current_supply)("total_supply",total_supply) );
    FC_ASSERT( gpo.get_current_hbd_supply() == total_hbd, "", ("gpo.current_hbd_supply",gpo.get_current_hbd_supply())("total_hbd",total_hbd) );
    FC_ASSERT( gpo.total_vesting_shares + gpo.pending_rewarded_vesting_shares == total_vesting, "", ("gpo.total_vesting_shares",gpo.total_vesting_shares)("total_vesting",total_vesting) );
    FC_ASSERT( gpo.total_vesting_shares.amount == total_vsf_votes + total_delayed_votes.value, "", ("total_vesting_shares",gpo.total_vesting_shares)("total_vsf_votes + total_delayed_votes",total_vsf_votes + total_delayed_votes.value) );
    FC_ASSERT( gpo.get_pending_rewarded_vesting_hive() == pending_vesting_hive, "", ("pending_rewarded_vesting_hive",gpo.get_pending_rewarded_vesting_hive())("pending_vesting_hive", pending_vesting_hive));

    FC_ASSERT( gpo.virtual_supply >= gpo.current_supply );
    if ( !get_feed_history().current_median_history.is_null() )
    {
      FC_ASSERT( gpo.get_current_hbd_supply()* get_feed_history().current_median_history + gpo.current_supply
        == gpo.virtual_supply, "", ("gpo.current_hbd_supply",gpo.get_current_hbd_supply())("get_feed_history().current_median_history",get_feed_history().current_median_history)("gpo.current_supply",gpo.current_supply)("gpo.virtual_supply",gpo.virtual_supply) );
    }

    ilog( "validate_invariants @${b}:", ( "b", head_block_num() ) );
    ilog( "successful scan of ${p} witnesses, ${a} accounts, ${c} convert requests, ${cc} collateralized convert requests, ${o} limit orders, ${e} escrow transfers, ${w} saving withdrawals, ${r} reward funds and ${s} SMT contributions.",
      ( "p", witness_no )( "a", account_no )( "c", convert_no )( "cc", collateralized_convert_no )( "o", order_no )( "e", escrow_no )( "w", withdrawal_no )( "r", reward_fund_no )( "s", contribution_no ) );
    ilog( "HIVE supply: ${h}", ( "h", gpo.current_supply.amount.value ) );
    ilog( "HBD supply: ${s} ( including ${i} initial )", ( "s", gpo.get_current_hbd_supply().amount.value )( "i", gpo.init_hbd_supply.amount.value ) );
    ilog( "virtual supply (HIVE): ${w}", ( "w", gpo.virtual_supply.amount.value ) );
    ilog( "VESTS: ${v} ( + ${p} pending ) worth (HIVE): ${x} ( + ${y} )", ( "v", gpo.total_vesting_shares.amount.value )( "p", gpo.pending_rewarded_vesting_shares.amount.value )( "x", gpo.get_total_vesting_fund_hive().amount.value )( "y", gpo.get_pending_rewarded_vesting_hive().amount.value ) );
  }
  FC_CAPTURE_LOG_AND_RETHROW( (head_block_num()) );
}

#ifdef HIVE_ENABLE_SMT

namespace {
  template <typename index_type, typename lambda>
  void add_from_balance_index(const index_type& balance_idx, lambda callback )
  {
    auto it = balance_idx.begin();
    auto end = balance_idx.end();
    for( ; it != end; ++it )
    {
      const auto& balance = *it;
      callback( balance );
    }
  }
}

/**
  * SMT version of validate_invariants.
  */
void database::validate_smt_invariants()const
{
  try
  {
    // Get total balances.
    typedef struct {
      asset liquid;
      asset vesting;
      asset pending_liquid;
      asset pending_vesting_shares;
      asset pending_vesting_value;
    } TCombinedBalance;
    typedef std::map< asset_symbol_type, TCombinedBalance > TCombinedSupplyMap;
    TCombinedSupplyMap theMap;

    // - Process regular balances, collecting SMT counterparts of 'balance' & 'vesting_shares'.
    const auto& balance_idx = get_index< account_regular_balance_index, by_id >();
    add_from_balance_index( balance_idx, [ &theMap ] ( const account_regular_balance_object& regular )
    {
      asset zero_liquid = asset( 0, regular.liquid.symbol );
      asset zero_vesting = asset( 0, regular.vesting.symbol );
      auto insertInfo = theMap.emplace( regular.liquid.symbol,
        TCombinedBalance( { regular.liquid, regular.vesting, zero_liquid, zero_vesting, zero_liquid } ) );
      if( insertInfo.second == false )
        {
        TCombinedBalance& existing_balance = insertInfo.first->second;
        existing_balance.liquid += regular.liquid;
        existing_balance.vesting += regular.vesting;
        }
    });

    // - Process reward balances, collecting SMT counterparts of 'reward_hive_balance', 'reward_vesting_balance' & 'reward_vesting_hive'.
    const auto& rewards_balance_idx = get_index< account_rewards_balance_index, by_id >();
    add_from_balance_index( rewards_balance_idx, [ &theMap ] ( const account_rewards_balance_object& rewards )
    {
      asset zero_liquid = asset( 0, rewards.pending_vesting_value.symbol );
      asset zero_vesting = asset( 0, rewards.pending_vesting_shares.symbol );
      auto insertInfo = theMap.emplace( rewards.pending_liquid.symbol, TCombinedBalance( { zero_liquid, zero_vesting,
        rewards.pending_liquid, rewards.pending_vesting_shares, rewards.pending_vesting_value } ) );
      if( insertInfo.second == false )
        {
        TCombinedBalance& existing_balance = insertInfo.first->second;
        existing_balance.pending_liquid += rewards.pending_liquid;
        existing_balance.pending_vesting_shares += rewards.pending_vesting_shares;
        existing_balance.pending_vesting_value += rewards.pending_vesting_value;
        }
    });

    // - Market orders
    const auto& limit_order_idx = get_index< limit_order_index >().indices();
    for( auto itr = limit_order_idx.begin(); itr != limit_order_idx.end(); ++itr )
    {
      if( itr->sell_price.base.symbol.space() == asset_symbol_type::smt_nai_space )
      {
        asset a( itr->for_sale, itr->sell_price.base.symbol );
        FC_ASSERT( a.symbol.is_vesting() == false );
        asset zero_liquid = asset( 0, a.symbol );
        asset zero_vesting = asset( 0, a.symbol.get_paired_symbol() );
        auto insertInfo = theMap.emplace( a.symbol, TCombinedBalance( { a, zero_vesting, zero_liquid, zero_vesting, zero_liquid } ) );
        if( insertInfo.second == false )
          insertInfo.first->second.liquid += a;
      }
    }

    // - Reward funds
FC_TODO( "Add reward_fund_object iteration here once they support SMTs." )

    // - Escrow & savings - no support of SMT is expected.

    // Do the verification of total balances.
    auto itr = get_index< smt_token_index, by_id >().begin();
    auto end = get_index< smt_token_index, by_id >().end();
    for( ; itr != end; ++itr )
    {
      const smt_token_object& smt = *itr;
      asset_symbol_type vesting_symbol = smt.liquid_symbol.get_paired_symbol();
      auto totalIt = theMap.find( smt.liquid_symbol );
      // Check liquid SMT supply.
      asset total_liquid_supply = totalIt == theMap.end() ? asset(0, smt.liquid_symbol) :
        ( totalIt->second.liquid + totalIt->second.pending_liquid );
      total_liquid_supply += asset( smt.total_vesting_fund_smt, smt.liquid_symbol )
                    /*+ gpo.get_total_reward_fund_hive() */
                    + asset( smt.pending_rewarded_vesting_smt, smt.liquid_symbol );
FC_TODO( "Supplement ^ once SMT rewards are implemented" )
      FC_ASSERT( asset(smt.current_supply, smt.liquid_symbol) == total_liquid_supply,
              "", ("smt current_supply",smt.current_supply)("total_liquid_supply",total_liquid_supply) );
      // Check vesting SMT supply.
      asset total_vesting_supply = totalIt == theMap.end() ? asset(0, vesting_symbol) :
        ( totalIt->second.vesting + totalIt->second.pending_vesting_shares );
      asset smt_vesting_supply = asset(smt.total_vesting_shares + smt.pending_rewarded_vesting_shares, vesting_symbol);
      FC_ASSERT( smt_vesting_supply == total_vesting_supply,
              "", ("smt vesting supply",smt_vesting_supply)("total_vesting_supply",total_vesting_supply) );
      // Check pending_vesting_value
      asset pending_vesting_value = totalIt == theMap.end() ? asset(0, smt.liquid_symbol) : totalIt->second.pending_vesting_value;
      FC_ASSERT( asset(smt.pending_rewarded_vesting_smt, smt.liquid_symbol) == pending_vesting_value, "",
        ("smt pending_rewarded_vesting_smt", smt.pending_rewarded_vesting_smt)("pending_vesting_value", pending_vesting_value));
    }
  }
  FC_CAPTURE_LOG_AND_RETHROW( (head_block_num()) );
}
#endif

void database::retally_witness_votes()
{
  const auto& witness_idx = get_index< witness_index >().indices();

  // Clear all witness votes
  for( auto itr = witness_idx.begin(); itr != witness_idx.end(); ++itr )
  {
    modify( *itr, [&]( witness_object& w )
    {
      w.votes = 0;
      w.virtual_position = 0;
    } );
  }

  const auto& account_idx = get_index< account_index >().indices();

  // Apply all existing votes by account
  for( auto itr = account_idx.begin(); itr != account_idx.end(); ++itr )
  {
    if( itr->has_proxy() ) continue;

    const auto& a = *itr;

    const auto& vidx = get_index<witness_vote_index>().indices().get<by_account_witness>();
    auto wit_itr = vidx.lower_bound( boost::make_tuple( a.get_name(), account_name_type() ) );
    while( wit_itr != vidx.end() && wit_itr->account == a.get_name() )
    {
      adjust_witness_vote( get< witness_object, by_name >(wit_itr->witness), a.get_governance_vote_power() );
      ++wit_itr;
    }
  }
}

void database::retally_witness_vote_counts( bool force )
{
  const auto& account_idx = get_index< account_index >().indices();

  // Check all existing votes by account
  for( auto itr = account_idx.begin(); itr != account_idx.end(); ++itr )
  {
    const auto& a = *itr;
    uint16_t witnesses_voted_for = 0;
    if( force || a.has_proxy() )
    {
      const auto& vidx = get_index< witness_vote_index >().indices().get< by_account_witness >();
      auto wit_itr = vidx.lower_bound( boost::make_tuple( a.get_name(), account_name_type() ) );
      while( wit_itr != vidx.end() && wit_itr->account == a.get_name() )
      {
        ++witnesses_voted_for;
        ++wit_itr;
      }
    }
    if( a.witnesses_voted_for != witnesses_voted_for )
    {
      modify( a, [&]( account_object& account )
      {
        account.witnesses_voted_for = witnesses_voted_for;
      } );
    }
  }
}

void database::remove_expired_governance_votes()
{
  if (!has_hardfork(HIVE_HARDFORK_1_25))
    return;

  const auto& accounts = get_index<account_index, by_governance_vote_expiration_ts>();
  auto acc_it = accounts.begin();
  time_point_sec first_expiring = acc_it->get_governance_vote_expiration_ts();
  time_point_sec now = head_block_time();

  if( first_expiring > now )
    return;

  const auto& proposal_votes = get_index< proposal_vote_index, by_voter_proposal >();
  remove_guard obj_perf( get_remove_threshold() );

  while( acc_it != accounts.end() && acc_it->get_governance_vote_expiration_ts() <= now )
  {
    const auto& account = *acc_it;
    ++acc_it;

    if( dhf_helper::remove_proposal_votes( account, proposal_votes, *this, obj_perf ) )
    {
      nullify_proxied_witness_votes( account );
      clear_witness_votes( account );

      if( account.has_proxy() )
        push_virtual_operation( proxy_cleared_operation( account.get_name(), get_account( account.get_proxy() ).get_name()) );

      modify( account, [&]( account_object& a )
      {
        a.clear_proxy();
        a.set_governance_vote_expired();
      } );
      push_virtual_operation( expired_account_notification_operation( account.get_name() ) );
    }
    else
    {
      ilog("Threshold exceeded while processing account ${account} with expired governance vote.",
        ("account", account.get_name())); // to be continued in next block
      break;
    }
  }
}

void database::set_block_writer( block_write_i* writer )
{
  _block_writer = writer;
}

database::node_status_t database::get_node_status()
{
  node_status_t result;
  result.last_processed_block_num = _my->_last_pushed_block_number.load(std::memory_order_consume);
  result.last_processed_block_time = fc::time_point_sec(_my->_last_pushed_block_time.load(std::memory_order_consume));
  return result;
}

} } //hive::chain

