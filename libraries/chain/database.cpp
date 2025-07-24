#include <hive/chain/hive_fwd.hpp>

#include <appbase/application.hpp>

#include <hive/protocol/hive_operations.hpp>
#include <hive/protocol/get_config.hpp>
#include <hive/protocol/transaction_util.hpp>

#include <hive/chain/block_summary_object.hpp>
#include <hive/chain/compound.hpp>
#include <hive/chain/custom_operation_interpreter.hpp>
#include <hive/chain/database.hpp>
#include <hive/chain/database_exceptions.hpp>
#include <hive/chain/db_with.hpp>
#include <hive/chain/evaluator_registry.hpp>
#include <hive/chain/global_property_object.hpp>
#include <hive/chain/smt_objects.hpp>
#include <hive/chain/hive_evaluator.hpp>
#include <hive/chain/hive_objects.hpp>
#include <hive/chain/transaction_object.hpp>
#include <hive/chain/shared_db_merkle.hpp>
#include <hive/chain/witness_schedule.hpp>
#include <hive/chain/blockchain_worker_thread_pool.hpp>

#include <hive/chain/util/asset.hpp>
#include <hive/chain/util/reward.hpp>
#include <hive/chain/util/uint256.hpp>
#include <hive/chain/util/reward.hpp>
#include <hive/chain/util/manabar.hpp>
#include <hive/chain/util/rd_setup.hpp>
#include <hive/chain/util/nai_generator.hpp>
#include <hive/chain/util/dhf_processor.hpp>
#include <hive/chain/util/delayed_voting.hpp>
#include <hive/chain/util/decoded_types_data_storage.hpp>
#include <hive/chain/util/state_checker_tools.hpp>
#include <hive/chain/util/impacted.hpp>

#include <hive/chain/rc/rc_objects.hpp>
#include <hive/chain/rc/resource_count.hpp>

#include <hive/jsonball/jsonball.hpp>

#include <fc/smart_ref_impl.hpp>
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
    1739019600; // Saturday, February 8, 2025 13:00:00 UTC
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

struct reward_fund_context
{
  uint128_t   recent_claims = 0;
  asset       reward_balance = asset( 0, HIVE_SYMBOL );
  share_type  hive_awarded = 0;
};

class database_impl
{
  public:
    database_impl( database& self );
    void register_new_type(util::abstract_type_registrar&);
    void delete_decoded_types_data_storage();
    void create_new_decoded_types_data_storage() { _decoded_types_data_storage = std::make_unique<util::decoded_types_data_storage>(); }

    database&                                         _self;
    evaluator_registry< operation >                   _evaluator_registry;
    std::map<account_name_type, block_id_type>        _last_fast_approved_block_by_witness;
    std::unique_ptr<util::decoded_types_data_storage> _decoded_types_data_storage;
    
    // these used for the node_status API, which reads these values from another thread
    // they're only used to determine if the node is in sync, and nothing particulary bad
    // will happen if we get mismatched values
    std::atomic<uint32_t>                             _last_pushed_block_number = {0};
    std::atomic<uint32_t>                             _last_pushed_block_time = {0}; // the value from a time_point_sec
};

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

    bool _allow_wipe_storage = args.force_replay | args.load_snapshot;
    if( _allow_wipe_storage )
    {
      get_comments_handler().wipe();
      get_accounts_handler().wipe();
    }
    chainbase::database::open( args.shared_mem_dir, args.chainbase_flags, args.shared_file_size, args.database_cfg, &environment_extension, _allow_wipe_storage );
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
  get_accounts_handler().open();
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
  get_accounts_handler().wipe();
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

    get_comments_handler().close();
    get_accounts_handler().close();
    chainbase::database::flush();

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

const witness_object* database::find_witness( const account_name_type& name ) const
{
  return find< witness_object, by_name >( name );
}

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
  FC_ASSERT( _account != nullptr, "Account with id ${acc} doesn't exist", ("acc", id) );
  return *_account;
} FC_CAPTURE_AND_RETHROW( (id) ) }

const account_object* database::find_account( const account_id_type& id )const
{
  return find< account_object, by_id >( id );
}

const account_object& database::get_account( const account_name_type& name )const
{ try {
  const auto* _account = find_account( name );
  FC_ASSERT( _account != nullptr, "Account ${acc} doesn't exist", ("acc", name) );
  return *_account;
} FC_CAPTURE_AND_RETHROW( (name) ) }

const account_object* database::find_account( const account_name_type& name )const
{
  return find< account_object, by_name >( name );
}

const comment_object* database::find_comment( comment_id_type comment_id )const
{
  return find< comment_object, by_id >( comment_id );
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

const escrow_object& database::get_escrow( const account_name_type& name, uint32_t escrow_id )const
{ try {
  const auto* _escrow = find_escrow( name, escrow_id );
  FC_ASSERT( _escrow != nullptr, "Escrow balance with 'name' ${name} 'escrow_id' ${escrow_id} doesn't exist.", (name)(escrow_id) );
  return *_escrow;
} FC_CAPTURE_AND_RETHROW( (name)(escrow_id) ) }

const escrow_object* database::find_escrow( const account_name_type& name, uint32_t escrow_id )const
{
  return find< escrow_object, by_from_id >( boost::make_tuple( name, escrow_id ) );
}

const limit_order_object& database::get_limit_order( const account_name_type& name, uint32_t orderid )const
{ try {
  if( !has_hardfork( HIVE_HARDFORK_0_6__127 ) )
    orderid = orderid & 0x0000FFFF;

  const auto* _limit_order = find_limit_order( name, orderid );
  FC_ASSERT( _limit_order != nullptr, "Limit order with 'name' ${name} 'order_id' ${orderid} doesn't exist.", (name)(orderid) );
  return *_limit_order;
} FC_CAPTURE_AND_RETHROW( (name)(orderid) ) }

const limit_order_object* database::find_limit_order( const account_name_type& name, uint32_t orderid )const
{
  if( !has_hardfork( HIVE_HARDFORK_0_6__127 ) )
    orderid = orderid & 0x0000FFFF;

  return find< limit_order_object, by_account >( boost::make_tuple( name, orderid ) );
}

const savings_withdraw_object& database::get_savings_withdraw( const account_name_type& owner, uint32_t request_id )const
{ try {
  const auto* _savings_withdraw = find_savings_withdraw( owner, request_id );
  FC_ASSERT( _savings_withdraw != nullptr, "Savings withdraw for `owner` ${owner} and 'request_id' ${request_id} doesn't exist.", (owner)(request_id) );
  return *_savings_withdraw;
} FC_CAPTURE_AND_RETHROW( (owner)(request_id) ) }

const savings_withdraw_object* database::find_savings_withdraw( const account_name_type& owner, uint32_t request_id )const
{
  return find< savings_withdraw_object, by_from_rid >( boost::make_tuple( owner, request_id ) );
}

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

const witness_schedule_object& database::get_witness_schedule_object()const
{ try {
  return get< witness_schedule_object >();
} FC_CAPTURE_AND_RETHROW() }

const witness_schedule_object& database::get_future_witness_schedule_object() const
{
  try
  {
    return get<witness_schedule_object>(witness_schedule_object::id_type(1));
  }
  catch (const std::out_of_range&)
  {
    FC_THROW_EXCEPTION(fc::key_not_found_exception, "Future witness schedule does not exist");
  }
}

const hardfork_property_object& database::get_hardfork_property_object()const
{ try {
  return get< hardfork_property_object >();
} FC_CAPTURE_AND_RETHROW() }

void database::update_account_metadata( const std::string& account_name, std::function<void(account_metadata_object&)> modifier )
{
#ifdef COLLECT_ACCOUNT_METADATA
  account_metadata _account_metadata = get_accounts_handler().get_account_metadata( account_name );

  if( _account_metadata.is_shm() )
  {
    modify( *_account_metadata, [&]( account_metadata_object& meta )
    {
      modifier( meta );
    });
  }
  else
  {
    modifier( *_account_metadata );
  }

  get_accounts_handler().create_volatile_account_metadata( *_account_metadata );
#endif
}

void database::update_account_authority( account_authority& obj, std::function<void(account_authority_object&)> modifier )
{
  if( obj.is_shm() )
  {
    modify( *obj, [&]( account_authority_object& meta )
    {
      modifier( meta );
    });
  }
  else
  {
    modifier( *obj );
  }

  get_accounts_handler().create_volatile_account_authority( *obj );
}

void database::update_account_authority( const std::string& account_name, std::function<void(account_authority_object&)> modifier )
{
  account_authority _account_authority = get_accounts_handler().get_account_authority( account_name );
  update_account_authority( _account_authority, modifier );
}

const comment_object& database::get_comment_for_payout_time( const comment_object& comment )const
{
  if( has_hardfork( HIVE_HARDFORK_0_17__769 ) || comment.is_root() )
    return comment;
  else
    return get< comment_object >( find_comment_cashout_ex( comment )->get_root_id() );
}

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

const comment_cashout_object* database::find_comment_cashout( comment_id_type comment_id ) const
{
  const auto& idx = get_index< comment_cashout_index, by_id >();
  comment_cashout_object::id_type ccid( comment_id );
  auto found = idx.find( ccid );

  if( found == idx.end() )
    return nullptr;
  else
    return &( *found );
}

const comment_cashout_ex_object* database::find_comment_cashout_ex( const comment_object& comment ) const
{
  return find_comment_cashout_ex( comment.get_id() );
}

const comment_cashout_ex_object* database::find_comment_cashout_ex( comment_id_type comment_id ) const
{
  if( has_hardfork( HIVE_HARDFORK_0_19 ) )
    return nullptr;

  const auto& idx = get_index< comment_cashout_ex_index, by_id >();
  comment_cashout_ex_object::id_type ccid( comment_id );
  auto found = idx.find( ccid );

  FC_ASSERT( found != idx.end() );
  return &( *found );
}

const comment_object& database::get_comment( const comment_cashout_object& comment_cashout ) const
{
  const auto& idx = get_index< comment_index >().indices().get< by_id >();
  auto found = idx.find( comment_cashout.get_comment_id() );

  FC_ASSERT( found != idx.end() );

  return *found;
}

void database::remove_old_cashouts()
{
  // Remove all cashout extras
  auto& comment_cashout_ex_idx = get_mutable_index< comment_cashout_ex_index >();
  comment_cashout_ex_idx.clear();

  // Remove regular cashouts for paid comments
  const auto& idx = get_index< comment_cashout_index, by_cashout_time >();
  auto itr = idx.find( fc::time_point_sec::maximum() );

  auto block_num = head_block_num();
  while( itr != idx.end() )
  {
    const auto& current = *itr;
    const auto& comment = get_comment( current );
    ++itr;
    get_comments_handler().on_cashout( block_num, comment, current );
    remove( current );
  }
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
  FC_ASSERT( is_virtual_operation( op ) );
  _current_op_in_trx++;
  operation_notification note = create_operation_notification( op );
  notify_pre_apply_operation( note );
  notify_post_apply_operation( note );
}

void database::pre_push_virtual_operation( const operation& op )
{
  FC_ASSERT( is_virtual_operation( op ) );
  _current_op_in_trx++;
  operation_notification note = create_operation_notification( op );
  notify_pre_apply_operation( note );
}

void database::post_push_virtual_operation( const operation& op, const fc::optional<uint64_t>& op_in_trx )
{
  FC_ASSERT( is_virtual_operation( op ) );
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

asset calculate_vesting( database& db, const asset& liquid, bool to_reward_balance )
{
  auto calculate_new_vesting = [ liquid ] ( price vesting_share_price ) -> asset
  {
    /**
      *  The ratio of total_vesting_shares / total_vesting_fund_hive should not
      *  change as the result of the user adding funds
      *
      *  V / C  = (V+Vn) / (C+Cn)
      *
      *  Simplifies to Vn = (V * Cn ) / C
      *
      *  If Cn equals o.amount, then we must solve for Vn to know how many new vesting shares
      *  the user should receive.
      *
      *  128 bit math is requred due to multiplying of 64 bit numbers. This is done in asset and price.
      */
    asset new_vesting = liquid * ( vesting_share_price );
    return new_vesting;
  };

#ifdef HIVE_ENABLE_SMT
  if( liquid.symbol.space() == asset_symbol_type::smt_nai_space )
  {
    FC_ASSERT( liquid.symbol.is_vesting() == false );
    // Get share price.
    const auto& smt = db.get< smt_token_object, by_symbol >( liquid.symbol );
    FC_ASSERT( smt.allow_voting == to_reward_balance, "No voting - no rewards" );
    price vesting_share_price = to_reward_balance ? smt.get_reward_vesting_share_price() : smt.get_vesting_share_price();
    // Calculate new vesting from provided liquid using share price.
    return calculate_new_vesting( vesting_share_price );
  }
#endif

  FC_ASSERT( liquid.symbol == HIVE_SYMBOL );
  // ^ A novelty, needed but risky in case someone managed to slip HBD/TESTS here in blockchain history.
  // Get share price.
  const auto& cprops = db.get_dynamic_global_properties();
  price vesting_share_price = to_reward_balance ? cprops.get_reward_vesting_share_price() : cprops.get_vesting_share_price();
  // Calculate new vesting from provided liquid using share price.
  return calculate_new_vesting( vesting_share_price );
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

// Create vesting, then a caller-supplied callback after determining how many shares to create, but before
// we modify the database.
// This allows us to implement virtual op pre-notifications in the Before function.
template< typename Before >
asset create_vesting2( database& db, const account_object& to_account, const asset& liquid, bool to_reward_balance, Before&& before_vesting_callback )
{
  try
  {
    asset new_vesting = db.adjust_account_vesting_balance( to_account, liquid, to_reward_balance, std::forward<Before>( before_vesting_callback ) );

    // Update witness voting numbers.
    if( !to_reward_balance )
      db.adjust_proxied_witness_votes( to_account, new_vesting.amount );

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
      a.set_vesting( VEST_asset( 0 ) );
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
      a.set_vest_rewards_as_hive( HIVE_asset( 0 ) );
      a.set_vest_rewards( VEST_asset( 0 ) );
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
      a.set_vesting( VEST_asset( 0 ) );
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
      a.set_vest_rewards_as_hive( HIVE_asset( 0 ) );
      a.set_vest_rewards( VEST_asset( 0 ) );
    } );
  }

  //////////////////////////////////////////////////////////////

  post_push_virtual_operation( vop_op );
}

void database::lock_account( const account_object& account )
{
  auto account_auth = get_accounts_handler().get_account_authority( account.get_name());
  if( !account_auth )
  {
    create< account_authority_object >( [&]( account_authority_object& auth )
    {
      auth.account = account.get_name();
      auth.owner.weight_threshold = 1;
      auth.active.weight_threshold = 1;
      auth.posting.weight_threshold = 1;
    } );
    get_accounts_handler().create_volatile_account_authority( get< account_authority_object, by_account >( account.get_name() ) );
  }
  else
  {
    auto _modifier = [&]( account_authority_object& auth )
    {
      auth.owner.weight_threshold = 1;
      auth.owner.clear();

      auth.active.weight_threshold = 1;
      auth.active.clear();

      auth.posting.weight_threshold = 1;
      auth.posting.clear();
    };
    update_account_authority( account.get_name(), _modifier );
  }

  modify( account, []( account_object& a )
  {
    a.set_recovery_account( a.get_id() );
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
        a.set_received_vesting( a.get_received_vesting() - delegation.get_vesting() );
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
      a.set_vesting( VEST_asset( 0 ) );
      //FC_ASSERT( a.get_delegated_vesting() == freed_delegations, "Inconsistent amount of delegations" );
      a.set_delegated_vesting( VEST_asset( 0 ) );
      a.set_vesting_withdraw_rate( VEST_asset( 0 ) );
      a.next_vesting_withdrawal = fc::time_point_sec::maximum();
      a.set_to_withdraw( VEST_asset( 0 ) );
      a.set_withdrawn( VEST_asset( 0 ) );

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

  // Remove pending escrows (return balance to account - compare with expire_escrow_ratification())
  const auto& escrow_idx = get_index< chain::escrow_index, chain::by_from_id >();
  auto escrow_itr = escrow_idx.lower_bound( account_name );
  while( escrow_itr != escrow_idx.end() && escrow_itr->from == account_name )
  {
    auto& escrow = *escrow_itr;
    ++escrow_itr;

    adjust_balance( account, escrow.get_hive_balance() );
    adjust_balance( account, escrow.get_hbd_balance() );
    adjust_balance( account, escrow.get_fee() );

    modify( account, []( account_object& a )
    {
      a.pending_escrow_transfers--;
    } );

    remove( escrow );
  }

  // Remove open limit orders (return balance to account - compare with clear_expired_orders())
  const auto& order_idx = get_index< chain::limit_order_index, chain::by_account >();
  auto order_itr = order_idx.lower_bound( account_name );
  while( order_itr != order_idx.end() && order_itr->seller == account_name )
  {
    auto& order = *order_itr;
    ++order_itr;

    cancel_order( order, true );
  }

  // Remove pending convert requests (return balance to account)
  const auto& request_idx = get_index< chain::convert_request_index, chain::by_owner >();
  auto request_itr = request_idx.lower_bound( account.get_id() );
  while( request_itr != request_idx.end() && request_itr->get_owner() == account.get_id() )
  {
    auto& request = *request_itr;
    ++request_itr;

    adjust_balance( account, request.get_convert_amount() );
    remove( request );
  }

  // Make sure there are no pending collateralized convert requests
  // (if we decided to handle them anyway, in case we wanted to reuse this routine outsede HF23 code,
  // we should most likely destroy collateral balance instead of putting it into treasury)
  const auto& collateralized_request_idx = get_index< chain::collateralized_convert_request_index, chain::by_owner >();
  auto collateralized_request_itr = collateralized_request_idx.lower_bound( account.get_id() );
  FC_ASSERT( collateralized_request_itr == collateralized_request_idx.end() || collateralized_request_itr->get_owner() != account.get_id(),
    "Collateralized convert requests not handled by clear_account" );

  // Remove ongoing saving withdrawals (return/pass balance to account)
  const auto& withdraw_from_idx = get_index< savings_withdraw_index, by_from_rid >();
  auto withdraw_from_itr = withdraw_from_idx.lower_bound( account_name );
  while( withdraw_from_itr != withdraw_from_idx.end() && withdraw_from_itr->from == account_name )
  {
    auto& withdrawal = *withdraw_from_itr;
    ++withdraw_from_itr;

    adjust_balance( account, withdrawal.amount );
    modify( account, []( account_object& a )
    {
      a.savings_withdraw_requests--;
    } );

    remove( withdrawal );
  }

  const auto& withdraw_to_idx = get_index< savings_withdraw_index, by_to_complete >();
  auto withdraw_to_itr = withdraw_to_idx.lower_bound( account_name );
  while( withdraw_to_itr != withdraw_to_idx.end() && withdraw_to_itr->to == account_name )
  {
    auto& withdrawal = *withdraw_to_itr;
    ++withdraw_to_itr;

    adjust_balance( account, withdrawal.amount );
    modify( get_account( withdrawal.from ), []( account_object& a )
    {
      a.savings_withdraw_requests--;
    } );

    remove( withdrawal );
  }

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
    a.set_vest_rewards( VEST_asset( 0 ) );
    a.set_vest_rewards_as_hive( HIVE_asset( 0 ) );
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

/**
  *  Iterates over all recurrent transfers with a due date date before
  *  the head block time and then executes the transfers
  */
void database::process_recurrent_transfers()
{
  if( !has_hardfork( HIVE_HARDFORK_1_25 ) )
    return;

  auto now = head_block_time();
  const auto& recurrent_transfers_by_date = get_index< recurrent_transfer_index, by_trigger_date >();
  auto itr = recurrent_transfers_by_date.begin();

  // uint16_t is okay because we stop at 1000, if the limit changes, make sure to check if it fits in the integer.
  uint16_t processed_transfers = 0;
  if( _benchmark_dumper.is_enabled() )
    _benchmark_dumper.begin();
  while( itr != recurrent_transfers_by_date.end() && itr->get_trigger_date() <= now )
  {
    // Since this is an intensive process, we don't want to process too many recurrent transfers in a single block
    if (processed_transfers >= HIVE_MAX_RECURRENT_TRANSFERS_PER_BLOCK)
    {
      ilog("Reached max processed recurrent transfers this block");
      break;
    }

    auto &current_recurrent_transfer = *itr;
    ++itr;

    const auto &from_account = get_account(current_recurrent_transfer.from_id);
    const auto &to_account = get_account(current_recurrent_transfer.to_id);
    asset available = get_balance(from_account, current_recurrent_transfer.amount.symbol);
    FC_ASSERT(current_recurrent_transfer.remaining_executions > 0);
    const auto remaining_executions = current_recurrent_transfer.remaining_executions -1;
    bool remove_recurrent_transfer = false;

    recurrent_transfer_extensions_type _extensions;

    //if `current_recurrent_transfer.pair_id` equals to 0, then it's not necessary to create an item in extensions. It's a default value.
    if( current_recurrent_transfer.pair_id )
      _extensions.emplace( recurrent_transfer_pair_id{ current_recurrent_transfer.pair_id } );

    // If we have enough money, we proceed with the transfer
    if (available >= current_recurrent_transfer.amount)
    {
      adjust_balance(from_account, -current_recurrent_transfer.amount);
      adjust_balance(to_account, current_recurrent_transfer.amount);

      // No need to update the object if we know that we will remove it
      if (remaining_executions == 0)
      {
        remove_recurrent_transfer = true;
      }
      else
      {
        modify(current_recurrent_transfer, [&](recurrent_transfer_object &rt)
        {
          rt.consecutive_failures = 0; // reset the consecutive failures counter
          rt.update_next_trigger_date();
          rt.remaining_executions = remaining_executions;
        });
      }

      push_virtual_operation(fill_recurrent_transfer_operation(from_account.get_name(), to_account.get_name(), current_recurrent_transfer.amount, to_string(current_recurrent_transfer.memo), remaining_executions, _extensions));
    }
    else
    {
      uint8_t consecutive_failures = current_recurrent_transfer.consecutive_failures + 1;

      if (consecutive_failures < HIVE_MAX_CONSECUTIVE_RECURRENT_TRANSFER_FAILURES)
      {
        // No need to update the object if we know that we will remove it
        if (remaining_executions == 0)
        {
          remove_recurrent_transfer = true;
        }
        else
        {
          modify(current_recurrent_transfer, [&](recurrent_transfer_object &rt)
          {
            ++rt.consecutive_failures;
            rt.update_next_trigger_date();
            rt.remaining_executions = remaining_executions;
          });
        }
        // false means the recurrent transfer was not deleted
        push_virtual_operation(failed_recurrent_transfer_operation(from_account.get_name(), to_account.get_name(), current_recurrent_transfer.amount, consecutive_failures, to_string(current_recurrent_transfer.memo), remaining_executions, remove_recurrent_transfer, _extensions));
      }
      else
      {
        // if we had too many consecutive failures, remove the recurrent payment object
        remove_recurrent_transfer = true;
        // true means the recurrent transfer was deleted
        push_virtual_operation(failed_recurrent_transfer_operation(from_account.get_name(), to_account.get_name(), current_recurrent_transfer.amount, consecutive_failures, to_string(current_recurrent_transfer.memo), remaining_executions, true, _extensions));
      }
    }

    if (remove_recurrent_transfer)
    {
      remove( current_recurrent_transfer );
      modify(from_account, [&](account_object& a )
      {
        FC_ASSERT( a.open_recurrent_transfers > 0 );
        a.open_recurrent_transfers--;
      });
    }

    processed_transfers++;
  }
  if( _benchmark_dumper.is_enabled() && processed_transfers )
    _benchmark_dumper.end( "processing", "hive::protocol::recurrent_transfer_operation", processed_transfers );
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
    create< owner_authority_history_object >( account, get_accounts_handler().get_account_authority( account.get_name() )->owner, head_block_time() );
  }

  auto _modifier = [&]( account_authority_object& auth )
  {
    auth.owner = owner_authority;
    auth.previous_owner_update = auth.last_owner_update;
    auth.last_owner_update = head_block_time();
  };
  update_account_authority( account.get_name(), _modifier );
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
    if( !has_hardfork( HIVE_HARDFORK_1_28_FIX_POWER_DOWN ) && to_withdraw < from_account.get_vesting_withdraw_rate().amount )
      to_withdraw = from_account.get_to_withdraw().amount % from_account.get_vesting_withdraw_rate().amount;
    // see history of first (and so far the only) power down of 'gil' account: https://hiveblocks.com/@gil
    // the situation was caused by HF1, where vesting_withdraw_rate changed from 9615 before split to 9615.384615
    // (instead of correct 9615.000000); that is the source of nonequivalence between taking all the rest of power down
    // (0.769270 VESTS) and modulo of "all % weekly rate" (0.000040 VESTS);
    // it is possible that other accounts were also affected in similar way, 'gil' was just the first where the difference
    // occurred
    if( to_withdraw > from_account.get_vesting().amount )
    {
      elog( "NOTIFYALERT! somehow account was scheduled to power down more than it has on balance (${s} vs ${h})",
        ( "s", to_withdraw )( "h", from_account.get_vesting().amount ) );
#ifdef USE_ALTERNATE_CHAIN_ID
      FC_ASSERT( false, "Somehow account was scheduled to power down more than it has on balance (${s} vs ${h})",
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
                a.set_vesting( a.get_vesting() + routed );
              }
              else
              {
                a.set_balance( a.get_balance() + routed );
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
      FC_ASSERT( dv.valid(), "The object processing `delayed votes` must exist" );

      dv->add_votes( _votes_update_data_items,
                true/*withdraw_executor*/,
                -to_withdraw.value/*val*/,
                from_account/*account*/
              );
    }

    modify( from_account, [&]( account_object& a )
    {
      a.set_vesting( a.get_vesting() - asset( to_withdraw, VESTS_SYMBOL ) );
      a.set_balance( a.get_balance() + converted_hive );
      a.set_withdrawn( a.get_withdrawn() + VEST_asset( to_withdraw ) );

      if( a.get_total_vesting_withdrawal() <= 0 || a.get_vesting().amount == 0 )
      {
        a.set_vesting_withdraw_rate( VEST_asset( 0 ) );
        a.next_vesting_withdrawal = fc::time_point_sec::maximum();
        a.set_to_withdraw( VEST_asset( 0 ) );
        a.set_withdrawn( VEST_asset( 0 ) );
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
      FC_ASSERT( dv.valid(), "The object processing `delayed votes` must exist" );

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
  *  This method will iterate through all comment_vote_objects and give them
  *  (max_rewards * weight) / c.total_vote_weight.
  *
  *  @returns unclaimed rewards.
  */
share_type database::pay_curators( const comment_object& comment, const comment_cashout_object& comment_cashout, share_type& max_rewards )
{
  struct cmp
  {
    bool operator()( const comment_vote_object* obj, const comment_vote_object* obj2 ) const
    {
      if( obj->get_weight() == obj2->get_weight() )
        return obj->get_voter() < obj2->get_voter();
      else
        return obj->get_weight() > obj2->get_weight();
    }
  };

  try
  {
    share_type unclaimed_rewards = max_rewards;

    if( !comment_cashout.allows_curation_rewards() )
    {
      unclaimed_rewards = 0;
      max_rewards = 0;
    }
    else if( comment_cashout.get_total_vote_weight() > 0 )
    {
      uint128_t total_weight( comment_cashout.get_total_vote_weight() );

      const auto& cvidx = get_index<comment_vote_index, by_comment_voter>();
      auto itr = cvidx.lower_bound( comment.get_id() );

      std::set< const comment_vote_object*, cmp > proxy_set;
      while( itr != cvidx.end() && itr->get_comment() == comment.get_id() )
      {
        proxy_set.insert( &( *itr ) );
        ++itr;
      }

      const auto& comment_author_name = get_account( comment_cashout.get_author_id() ).get_name();
      for( auto& item : proxy_set )
      { try {
        uint128_t weight( item->get_weight() );
        auto claim = fc::uint128_to_uint64( ( max_rewards.value * weight ) / total_weight );
        if( claim > 0 ) // min_amt is non-zero satoshis
        {
          unclaimed_rewards -= claim;
          const auto& voter = get( item->get_voter() );
          operation vop = curation_reward_operation( voter.get_name(), asset(0, VESTS_SYMBOL), comment_author_name, to_string( comment_cashout.get_permlink() ), has_hardfork( HIVE_HARDFORK_0_17__659 ) );
          create_vesting2( *this, voter, asset( claim, HIVE_SYMBOL ), has_hardfork( HIVE_HARDFORK_0_17__659 ),
            [&]( const asset& reward )
            {
              vop.get< curation_reward_operation >().reward = reward;
              pre_push_virtual_operation( vop );
            } );

            modify( voter, [&]( account_object& a )
            {
              a.set_curation_rewards( a.get_curation_rewards() + HIVE_asset( claim ) );
            });
          post_push_virtual_operation( vop );
        }
      } FC_CAPTURE_AND_RETHROW( (*item) ) }
    }
    max_rewards -= unclaimed_rewards;

    return unclaimed_rewards;
  } FC_CAPTURE_AND_RETHROW( (max_rewards) )
}

share_type database::cashout_comment_helper( util::comment_reward_context& ctx, const comment_object& comment,
  const comment_cashout_object& comment_cashout, const comment_cashout_ex_object* comment_cashout_ex, bool forward_curation_remainder )
{
  try
  {
    share_type claimed_reward = 0;

    if( comment_cashout.get_net_rshares() > 0 )
    {
      ctx.rshares = comment_cashout.get_net_rshares();
      ctx.max_hbd = comment_cashout.get_max_accepted_payout();
      if( comment_cashout_ex )
        ctx.reward_weight = comment_cashout_ex->get_reward_weight();
      else
        ctx.reward_weight = HIVE_100_PERCENT;

      if( has_hardfork( HIVE_HARDFORK_0_17__774 ) )
      {
        const auto& rf = get_reward_fund();
        ctx.reward_curve = rf.author_reward_curve;
        ctx.content_constant = rf.content_constant;
      }

      const share_type reward = util::get_rshare_reward( ctx );
      uint128_t reward_tokens = uint128_t( reward.value );
      share_type curation_tokens;
      share_type author_tokens;

      if( reward_tokens > 0 )
      {
        curation_tokens = fc::uint128_to_uint64( ( reward_tokens * get_curation_rewards_percent() ) / HIVE_100_PERCENT );
        author_tokens = fc::uint128_to_uint64(reward_tokens) - curation_tokens;

        share_type curation_remainder = pay_curators( comment, comment_cashout, curation_tokens );

        if( forward_curation_remainder )
          author_tokens += curation_remainder;

        share_type total_beneficiary = 0;
        claimed_reward = author_tokens + curation_tokens;
        const auto& author = get_account( comment_cashout.get_author_id() );
        const auto& comment_author = author.get_name();

        for( auto& b : comment_cashout.get_beneficiaries() )
        {
          const auto& beneficiary = get_account( b.account_id );

          auto benefactor_tokens = ( author_tokens * b.weight ) / HIVE_100_PERCENT;
          auto benefactor_vesting_hive = benefactor_tokens;
          auto vop = comment_benefactor_reward_operation( beneficiary.get_name(), comment_author, to_string( comment_cashout.get_permlink() ),
            asset( 0, HBD_SYMBOL ), asset( 0, HIVE_SYMBOL ), asset( 0, VESTS_SYMBOL ), has_hardfork( HIVE_HARDFORK_0_17__659 ) );

          if( has_hardfork( HIVE_HARDFORK_0_21__3343 ) && is_treasury( beneficiary.get_name() ) )
          {
            benefactor_vesting_hive = 0;
            vop.hbd_payout = asset( benefactor_tokens, HIVE_SYMBOL ) * get_feed_history().current_median_history;
            vop.payout_must_be_claimed = false;
            adjust_balance( get_treasury(), vop.hbd_payout );
            adjust_supply( asset( -benefactor_tokens, HIVE_SYMBOL ) );
            adjust_supply( vop.hbd_payout );
          }
          else if( has_hardfork( HIVE_HARDFORK_0_20__2022 ) )
          {
            auto benefactor_hbd_hive = ( benefactor_tokens * comment_cashout.get_percent_hbd() ) / ( 2 * HIVE_100_PERCENT ) ;
            benefactor_vesting_hive  = benefactor_tokens - benefactor_hbd_hive;
            auto hbd_payout          = create_hbd( beneficiary, asset( benefactor_hbd_hive, HIVE_SYMBOL ), true );

            vop.hbd_payout   = hbd_payout.first; // HBD portion
            vop.hive_payout = hbd_payout.second; // HIVE portion
          }

          create_vesting2( *this, beneficiary, asset( benefactor_vesting_hive, HIVE_SYMBOL ), has_hardfork( HIVE_HARDFORK_0_17__659 ),
          [&]( const asset& reward )
          {
            vop.vesting_payout = reward;
            pre_push_virtual_operation( vop );
          });

          post_push_virtual_operation( vop );
          total_beneficiary += benefactor_tokens;
        }

        author_tokens -= total_beneficiary;

        auto hbd_hive     = ( author_tokens * comment_cashout.get_percent_hbd() ) / ( 2 * HIVE_100_PERCENT ) ;
        auto vesting_hive = author_tokens - hbd_hive;

        auto hbd_payout = create_hbd( author, asset( hbd_hive, HIVE_SYMBOL ), has_hardfork( HIVE_HARDFORK_0_17__659 ) );

        /*
          Total payout for curators is calculated due to the performance in third party softwares(f.e. `hivemind`).
          During payments calculations it's enough to catch `author_reward_operation`, without adding all values from `curation_reward_operation`.
        */
        auto curators_vesting_payout = calculate_vesting( *this, asset( curation_tokens, HIVE_SYMBOL ), has_hardfork( HIVE_HARDFORK_0_17__659 ) );

        operation vop = author_reward_operation( comment_author, to_string( comment_cashout.get_permlink() ), hbd_payout.first, hbd_payout.second, asset( 0, VESTS_SYMBOL ),
                                                curators_vesting_payout, has_hardfork( HIVE_HARDFORK_0_17__659 ) );

        create_vesting2( *this, author, asset( vesting_hive, HIVE_SYMBOL ), has_hardfork( HIVE_HARDFORK_0_17__659 ),
          [&]( const asset& vesting_payout )
          {
            vop.get< author_reward_operation >().vesting_payout = vesting_payout;
            pre_push_virtual_operation( vop );
          } );
        post_push_virtual_operation( vop );

        asset payout = hbd_payout.first + to_hbd( hbd_payout.second + asset( vesting_hive, HIVE_SYMBOL ) );
        asset curator_payout = to_hbd( asset( curation_tokens, HIVE_SYMBOL ) );
        asset beneficiary_payout = to_hbd( asset( total_beneficiary, HIVE_SYMBOL ) );
        if( !has_hardfork( HIVE_HARDFORK_0_19 ) )
        {
          modify( *comment_cashout_ex, [&]( comment_cashout_ex_object& c_ex )
          {
            c_ex.add_payout_values( payout, curator_payout, beneficiary_payout );
            payout = c_ex.get_total_payout();
            curator_payout = c_ex.get_curator_payout();
            beneficiary_payout = c_ex.get_beneficiary_payout();
          } );
        }
        vop = comment_reward_operation( comment_author, to_string( comment_cashout.get_permlink() ),
          to_hbd( asset( claimed_reward, HIVE_SYMBOL ) ), author_tokens, payout, curator_payout, beneficiary_payout );
        pre_push_virtual_operation( vop );
        post_push_virtual_operation( vop );

        modify( author, [&]( account_object& a )
        {
          a.set_posting_rewards( a.get_posting_rewards() + HIVE_asset( author_tokens ) );
        });
      }

      notify_comment_reward( { reward, author_tokens, curation_tokens } );

      if( !has_hardfork( HIVE_HARDFORK_0_17__774 ) )
        adjust_rshares2( util::evaluate_reward_curve( comment_cashout.get_net_rshares() ), 0 );
    }

    if( !has_hardfork( HIVE_HARDFORK_0_19 ) ) // paid comment is removed after HF19, so no point in modification
    {
      modify( comment_cashout, [&]( comment_cashout_object& c )
      {
        /**
        * A payout is only made for positive rshares, negative rshares hang around
        * for the next time this post might get an upvote.
        */
        int64_t negative_rshares = c.get_net_rshares() < 0 ? c.get_net_rshares() : 0;
        c.on_payout();
        c.accumulate_vote_rshares( negative_rshares, 0 );

        if( has_hardfork( HIVE_HARDFORK_0_17__769 ) )
        {
          c.set_cashout_time();
        }
        else if( comment.is_root() )
        {
          if( has_hardfork( HIVE_HARDFORK_0_12__177 ) && !comment_cashout_ex->was_paid() )
            c.set_cashout_time( head_block_time() + HIVE_SECOND_CASHOUT_WINDOW );
          else
            c.set_cashout_time();
        }
      } );
      modify( *comment_cashout_ex, [&]( comment_cashout_ex_object& c_ex )
      {
        c_ex.on_payout( head_block_time() );
      } );
    }

    if( has_hardfork( HIVE_HARDFORK_0_17__769 ) || calculate_discussion_payout_time( comment, comment_cashout ) == fc::time_point_sec::maximum() )
    {
      push_virtual_operation( comment_payout_update_operation( get_account( comment_cashout.get_author_id() ).get_name(), to_string( comment_cashout.get_permlink() ) ) );
    }

    const auto& vote_idx = get_index< comment_vote_index, by_comment_voter >();
    auto vote_itr = vote_idx.lower_bound( comment.get_id() );
    while( vote_itr != vote_idx.end() && vote_itr->get_comment() == comment.get_id() )
    {
      const auto& cur_vote = *vote_itr;
      ++vote_itr;
      remove( cur_vote );
    }

    return claimed_reward;
  } FC_CAPTURE_AND_RETHROW( (comment)(comment_cashout)(ctx) )
}

void database::process_comment_cashout()
{
  /// don't allow any content to get paid out until the website is ready to launch
  /// and people have had a week to start posting.  The first cashout will be the biggest because it
  /// will represent 2+ months of rewards.
  if( !has_hardfork( HIVE_FIRST_CASHOUT_TIME ) )
    return;

  const auto& gpo = get_dynamic_global_properties();
  auto _now = head_block_time();
  util::comment_reward_context ctx;
  ctx.current_hive_price = get_feed_history().current_median_history;

  vector< reward_fund_context > funds;
  vector< share_type > hive_awarded;
  const auto& reward_idx = get_index< reward_fund_index, by_id >();

  // Decay recent rshares of each fund
  for( auto itr = reward_idx.begin(); itr != reward_idx.end(); ++itr )
  {
    // Add all reward funds to the local cache and decay their recent rshares
    modify( *itr, [&]( reward_fund_object& rfo )
    {
      fc::microseconds decay_time;

      if( has_hardfork( HIVE_HARDFORK_0_19__1051 ) )
        decay_time = HIVE_RECENT_RSHARES_DECAY_TIME_HF19;
      else
        decay_time = HIVE_RECENT_RSHARES_DECAY_TIME_HF17;

      if( ( _now - rfo.last_update ) <= decay_time || !has_hardfork( HIVE_HARDFORK_1_26_CLAIM_UNDERFLOW ) ) //TODO: remove hardfork part after HF26
        rfo.recent_claims -= ( rfo.recent_claims * ( _now - rfo.last_update ).to_seconds() ) / decay_time.to_seconds();
      else
        rfo.recent_claims = 0; // this should never happen - requires chain to be inactive for more than decay_time
      rfo.last_update = _now;
    });

    reward_fund_context rf_ctx;
    rf_ctx.recent_claims = itr->recent_claims;
    rf_ctx.reward_balance = itr->reward_balance;

    // The index is by ID, so the ID should be the current size of the vector (0, 1, 2, etc...)
    assert( funds.size() == itr->get_id().get_value() );

    funds.push_back( rf_ctx );
  }

  const auto& cidx        = get_index< comment_cashout_index, by_cashout_time >();
  const auto& com_by_root = get_index< comment_cashout_ex_index, by_root >();

  auto _current = cidx.begin();
  // add all rshares about to be cashed out to the reward funds. This ensures equal satoshi per rshare payment
  if( has_hardfork( HIVE_HARDFORK_0_17__771 ) )
  {
    while( _current != cidx.end() && _current->get_cashout_time() <= _now )
    {
      if( _current->get_net_rshares() > 0 )
      {
        const auto& rf = get_reward_fund();
        funds[ rf.get_id() ].recent_claims += util::evaluate_reward_curve( _current->get_net_rshares(), rf.author_reward_curve, rf.content_constant );
      }

      ++_current;
    }

    _current = cidx.begin();
  }

  bool forward_curation_remainder = !has_hardfork( HIVE_HARDFORK_0_20__1877 );
  /*
    * Payout all comments
    *
    * Each payout follows a similar pattern, but for a different reason.
    * Cashout comment helper does not know about the reward fund it is paying from.
    * The helper only does token allocation based on curation rewards and the HBD
    * global %, etc.
    *
    * Each context is used by get_rshare_reward to determine what part of each budget
    * the comment is entitled to. Prior to hardfork 17, all payouts are done against
    * the global state updated each payout. After the hardfork, each payout is done
    * against a reward fund state that is snapshotted before all payouts in the block.
    */
  int count = 0;
  auto block_num = head_block_num();
  if( _benchmark_dumper.is_enabled() )
    _benchmark_dumper.begin();
  while( _current != cidx.end() && _current->get_cashout_time() <= _now )
  {
    if( has_hardfork( HIVE_HARDFORK_0_17__771 ) )
    {
      auto fund_id = get_reward_fund().get_id();
      ctx.total_reward_shares2 = funds[ fund_id ].recent_claims;
      ctx.total_reward_fund_hive = funds[ fund_id ].reward_balance;

      const comment_object& _comment = get_comment( *_current );
      funds[ fund_id ].hive_awarded += cashout_comment_helper( ctx, _comment, *_current,
        find_comment_cashout_ex( _comment ), forward_curation_remainder );
      ++count;

      if( has_hardfork( HIVE_HARDFORK_0_19 ) )
      {
        get_comments_handler().on_cashout( block_num, _comment, *_current );
        remove( *_current );
      }
    }
    else
    {
      comment_id_type root_id = find_comment_cashout_ex( _current->get_comment_id() )->get_root_id();
      auto itr = com_by_root.lower_bound( root_id );
      while( itr != com_by_root.end() && itr->get_root_id() == root_id )
      {
        const auto& comment_cashout_ex = *itr; ++itr;
        ctx.total_reward_shares2 = gpo.total_reward_shares2;
        ctx.total_reward_fund_hive = gpo.get_total_reward_fund_hive();

        const comment_object* comment = find_comment( comment_cashout_ex.get_comment_id() );
        FC_ASSERT( comment );
        const comment_cashout_object* comment_cashout = find_comment_cashout( comment_cashout_ex.get_comment_id() );
        FC_ASSERT( comment_cashout );
        auto reward = cashout_comment_helper( ctx, *comment, *comment_cashout, &comment_cashout_ex );
        ++count;

        if( reward > 0 )
        {
          modify( get_dynamic_global_properties(), [&]( dynamic_global_property_object& p )
          {
            p.total_reward_fund_hive.amount -= reward;
          });
        }
      }
    }

    _current = cidx.begin();
  }
  if( _benchmark_dumper.is_enabled() && count )
    _benchmark_dumper.end( "processing", "hive::protocol::comment_operation", count );

  // Write the cached fund state back to the database
  if( funds.size() )
  {
    for( size_t i = 0; i < funds.size(); i++ )
    {
      modify( get< reward_fund_object, by_id >( reward_fund_object::id_type( i ) ), [&]( reward_fund_object& rfo )
      {
        rfo.recent_claims = funds[ i ].recent_claims;
        rfo.reward_balance -= asset( funds[ i ].hive_awarded, HIVE_SYMBOL );
      });
    }
  }
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
      const auto hbd_supply_without_treasury = (props.get_current_hbd_supply() - treasury_account.get_hbd_balance()).amount < 0 ? asset(0, HBD_SYMBOL) : (props.get_current_hbd_supply() - treasury_account.get_hbd_balance());
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

void database::process_savings_withdraws()
{
  const auto& idx = get_index< savings_withdraw_index >().indices().get< by_complete_from_rid >();
  auto itr = idx.begin();

  int count = 0;
  if( _benchmark_dumper.is_enabled() )
    _benchmark_dumper.begin();
  while( itr != idx.end() )
  {
    if( itr->complete > head_block_time() )
      break;
    adjust_balance( get_account( itr->to ), itr->amount );

    modify( get_account( itr->from ), [&]( account_object& a )
    {
      a.savings_withdraw_requests--;
    });

    push_virtual_operation( fill_transfer_from_savings_operation( itr->from, itr->to, itr->amount, itr->request_id, to_string( itr->memo) ) );

    remove( *itr );
    itr = idx.begin();
    ++count;
  }
  if( _benchmark_dumper.is_enabled() && count )
    _benchmark_dumper.end( "processing", "hive::protocol::transfer_from_savings_operation", count );
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
      a.set_balance( a.get_balance() + pay );
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

/**
  *  Iterates over all [collateralized] conversion requests with a conversion date before
  *  the head block time and then converts them to/from HIVE/HBD at the
  *  current median price feed history price times the premium/fee
  *  Collateralized requests might also return excess collateral.
  */
void database::process_conversions()
{
  auto now = head_block_time();
  const auto& fhistory = get_feed_history();
  if( fhistory.current_median_history.is_null() )
    return;

  asset net_hbd( 0, HBD_SYMBOL );
  asset net_hive( 0, HIVE_SYMBOL );

  //regular requests
  int count = 0;
  if( _benchmark_dumper.is_enabled() )
    _benchmark_dumper.begin();
  {
    const auto& request_by_date = get_index< convert_request_index, by_conversion_date >();
    auto itr = request_by_date.begin();

    while( itr != request_by_date.end() && itr->get_conversion_date() <= now )
    {
      auto amount_to_issue = itr->get_convert_amount() * fhistory.current_median_history;
      const auto& owner = get_account( itr->get_owner() );

      adjust_balance( owner, amount_to_issue );

      net_hbd  -= itr->get_convert_amount();
      net_hive += amount_to_issue;

      push_virtual_operation( fill_convert_request_operation( owner.get_name(), itr->get_request_id(),
        itr->get_convert_amount(), amount_to_issue ) );

      remove( *itr );
      itr = request_by_date.begin();

      ++count;
    }
  }
  if( _benchmark_dumper.is_enabled() && count )
    _benchmark_dumper.end( "processing", "hive::protocol::convert_operation", count );

  //collateralized requests
  count = 0;
  if( _benchmark_dumper.is_enabled() )
    _benchmark_dumper.begin();
  {
    const auto& request_by_date = get_index< collateralized_convert_request_index, by_conversion_date >();
    auto itr = request_by_date.begin();

    while( itr != request_by_date.end() && itr->get_conversion_date() <= now )
    {
      const auto& owner = get_account( itr->get_owner() );

      //calculate how much HIVE we'd need for already created HBD at current corrected price
      //note that we are using market median price instead of minimal as it was during immediate part of this
      //conversion or current median price as is used in other conversions; the former is by design - we are supposed
      //to use median price here, minimal during immediate conversion was to prevent gaming; the latter is for rare
      //case, when this conversion happens when median price does not reflect market conditions when hard limit was
      //hit - see update_median_feed()
      auto required_hive = multiply_with_fee( itr->get_converted_amount(), fhistory.market_median_history,
        HIVE_COLLATERALIZED_CONVERSION_FEE, HIVE_SYMBOL );
      auto excess_collateral = itr->get_collateral_amount() - required_hive;
      if( excess_collateral.amount < 0 )
      {
        push_virtual_operation( system_warning_operation( FC_LOG_MESSAGE( warn,
          "Insufficient collateral on conversion ${id} by ${o} - shortfall of ${ec}",
          ( "id", itr->get_request_id() )( "o", owner.get_name() )( "ec", -excess_collateral ) ).get_message() ) );

        required_hive = itr->get_collateral_amount();
        excess_collateral.amount = 0;
      }
      else
      {
        adjust_balance( owner, excess_collateral );
      }

      net_hive -= required_hive;
      //note that HBD was created immediately, so we don't need to correct its supply here
      push_virtual_operation( fill_collateralized_convert_request_operation( owner.get_name(), itr->get_request_id(),
        required_hive, itr->get_converted_amount(), excess_collateral ) );

      remove( *itr );
      itr = request_by_date.begin();

      ++count;
    }
    if( _benchmark_dumper.is_enabled() && count )
      _benchmark_dumper.end( "processing", "hive::protocol::collateralized_convert_operation", count );
  }

  //correct global supply (if needed)
  if( net_hive.amount.value | net_hbd.amount.value )
  {
    modify( get_dynamic_global_properties(), [&]( dynamic_global_property_object& p )
    {
      p.current_supply += net_hive;
      p.current_hbd_supply += net_hbd;
      p.virtual_supply += net_hive;
      p.virtual_supply += net_hbd * fhistory.current_median_history;
    } );
  }
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
      a.set_recovery_account( new_recovery_account.get_id() );
    });

    push_virtual_operation(changed_recovery_account_operation( account.get_name(), old_recovery_account_name, new_recovery_account.get_name() ));

    remove( *change_req );
    change_req = change_req_idx.begin();
  }
}

void database::expire_escrow_ratification()
{
  const auto& escrow_idx = get_index< escrow_index >().indices().get< by_ratification_deadline >();
  auto escrow_itr = escrow_idx.lower_bound( false );

  while( escrow_itr != escrow_idx.end() && !escrow_itr->is_approved() && escrow_itr->ratification_deadline <= head_block_time() )
  {
    const auto& old_escrow = *escrow_itr;
    ++escrow_itr;

    adjust_balance( old_escrow.from, old_escrow.get_hive_balance() );
    adjust_balance( old_escrow.from, old_escrow.get_hbd_balance() );
    adjust_balance( old_escrow.from, old_escrow.get_fee() );

    push_virtual_operation( escrow_rejected_operation( old_escrow.from, old_escrow.to, old_escrow.agent, old_escrow.escrow_id,
      old_escrow.get_hbd_balance(), old_escrow.get_hive_balance(), old_escrow.get_fee() ) );

    modify( get_account( old_escrow.from ), []( account_object& a )
    {
      a.pending_escrow_transfers--;
    } );

    remove( old_escrow );
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
  FC_ASSERT(last_irreversible_object, "Premature access to non-initialized irreversible object data.");

  return cached_lib;
}

const irreversible_block_data_type* database::get_last_irreversible_object() const
{
  return last_irreversible_object;
}

void database::initialize_evaluators()
{
  _my->_evaluator_registry.register_evaluator< vote_evaluator                           >();
  _my->_evaluator_registry.register_evaluator< comment_evaluator                        >();
  _my->_evaluator_registry.register_evaluator< comment_options_evaluator                >();
  _my->_evaluator_registry.register_evaluator< delete_comment_evaluator                 >();
  _my->_evaluator_registry.register_evaluator< transfer_evaluator                       >();
  _my->_evaluator_registry.register_evaluator< transfer_to_vesting_evaluator            >();
  _my->_evaluator_registry.register_evaluator< withdraw_vesting_evaluator               >();
  _my->_evaluator_registry.register_evaluator< set_withdraw_vesting_route_evaluator     >();
  _my->_evaluator_registry.register_evaluator< account_create_evaluator                 >();
  _my->_evaluator_registry.register_evaluator< account_update_evaluator                 >();
  _my->_evaluator_registry.register_evaluator< account_update2_evaluator                >();
  _my->_evaluator_registry.register_evaluator< witness_update_evaluator                 >();
  _my->_evaluator_registry.register_evaluator< account_witness_vote_evaluator           >();
  _my->_evaluator_registry.register_evaluator< account_witness_proxy_evaluator          >();
  _my->_evaluator_registry.register_evaluator< custom_evaluator                         >();
  _my->_evaluator_registry.register_evaluator< custom_binary_evaluator                  >();
  _my->_evaluator_registry.register_evaluator< custom_json_evaluator                    >();
  _my->_evaluator_registry.register_evaluator< pow_evaluator                            >();
  _my->_evaluator_registry.register_evaluator< pow2_evaluator                           >();
  _my->_evaluator_registry.register_evaluator< feed_publish_evaluator                   >();
  _my->_evaluator_registry.register_evaluator< convert_evaluator                        >();
  _my->_evaluator_registry.register_evaluator< collateralized_convert_evaluator         >();
  _my->_evaluator_registry.register_evaluator< limit_order_create_evaluator             >();
  _my->_evaluator_registry.register_evaluator< limit_order_create2_evaluator            >();
  _my->_evaluator_registry.register_evaluator< limit_order_cancel_evaluator             >();
  _my->_evaluator_registry.register_evaluator< claim_account_evaluator                  >();
  _my->_evaluator_registry.register_evaluator< create_claimed_account_evaluator         >();
  _my->_evaluator_registry.register_evaluator< request_account_recovery_evaluator       >();
  _my->_evaluator_registry.register_evaluator< recover_account_evaluator                >();
  _my->_evaluator_registry.register_evaluator< change_recovery_account_evaluator        >();
  _my->_evaluator_registry.register_evaluator< escrow_transfer_evaluator                >();
  _my->_evaluator_registry.register_evaluator< escrow_approve_evaluator                 >();
  _my->_evaluator_registry.register_evaluator< escrow_dispute_evaluator                 >();
  _my->_evaluator_registry.register_evaluator< escrow_release_evaluator                 >();
  _my->_evaluator_registry.register_evaluator< transfer_to_savings_evaluator            >();
  _my->_evaluator_registry.register_evaluator< transfer_from_savings_evaluator          >();
  _my->_evaluator_registry.register_evaluator< cancel_transfer_from_savings_evaluator   >();
  _my->_evaluator_registry.register_evaluator< decline_voting_rights_evaluator          >();
  _my->_evaluator_registry.register_evaluator< reset_account_evaluator                  >();
  _my->_evaluator_registry.register_evaluator< set_reset_account_evaluator              >();
  _my->_evaluator_registry.register_evaluator< claim_reward_balance_evaluator           >();
#ifdef HIVE_ENABLE_SMT
  _my->_evaluator_registry.register_evaluator< claim_reward_balance2_evaluator          >();
#endif
  _my->_evaluator_registry.register_evaluator< account_create_with_delegation_evaluator >();
  _my->_evaluator_registry.register_evaluator< delegate_vesting_shares_evaluator        >();
  _my->_evaluator_registry.register_evaluator< witness_set_properties_evaluator         >();

#ifdef HIVE_ENABLE_SMT
  _my->_evaluator_registry.register_evaluator< smt_setup_evaluator                      >();
  _my->_evaluator_registry.register_evaluator< smt_setup_emissions_evaluator            >();
  _my->_evaluator_registry.register_evaluator< smt_set_setup_parameters_evaluator       >();
  _my->_evaluator_registry.register_evaluator< smt_set_runtime_parameters_evaluator     >();
  _my->_evaluator_registry.register_evaluator< smt_create_evaluator                     >();
  _my->_evaluator_registry.register_evaluator< smt_contribute_evaluator                 >();
#endif

  _my->_evaluator_registry.register_evaluator< create_proposal_evaluator                >();
  _my->_evaluator_registry.register_evaluator< update_proposal_evaluator                >();
  _my->_evaluator_registry.register_evaluator< update_proposal_votes_evaluator          >();
  _my->_evaluator_registry.register_evaluator< remove_proposal_evaluator                >();
  _my->_evaluator_registry.register_evaluator< recurrent_transfer_evaluator             >();
  _my->_evaluator_registry.register_evaluator< witness_block_approve_evaluator          >();

  rc.initialize_evaluators();
}


void database::register_custom_operation_interpreter( std::shared_ptr< custom_operation_interpreter > interpreter )
{
  FC_ASSERT( interpreter );
  bool inserted = _custom_operation_interpreters.emplace( interpreter->get_custom_id(), interpreter ).second;
  // This assert triggering means we're mis-configured (multiple registrations of custom JSON evaluator for same ID)
  FC_ASSERT( inserted );
}

std::shared_ptr< custom_operation_interpreter > database::get_custom_json_evaluator( const custom_id_type& id )
{
  auto it = _custom_operation_interpreters.find( id );
  if( it != _custom_operation_interpreters.end() )
    return it->second;
  return std::shared_ptr< custom_operation_interpreter >();
}

void initialize_core_indexes( database& db );

void database::initialize_indexes()
{
  initialize_core_indexes( *this );
  _plugin_index_signal();
}

void database::initialize_irreversible_storage()
{
  auto s = get_segment_manager();
  last_irreversible_object = s->find_or_construct<irreversible_block_data_type>( "irreversible" )(
    allocator< irreversible_block_data_type >( s )
  );

  cached_lib = last_irreversible_object->_irreversible_block_data.create_full_block();
}

void database::verify_match_of_state_objects_definitions_from_shm()
{
  FC_ASSERT(_my->_decoded_types_data_storage);
  const std::string shm_decoded_state_objects_data = get_decoded_state_objects_data_from_shm();

  if (shm_decoded_state_objects_data.empty())
    set_decoded_state_objects_data(_my->_decoded_types_data_storage->generate_decoded_types_data_json_string());
  else
    util::verify_match_of_state_definitions(*(_my->_decoded_types_data_storage), shm_decoded_state_objects_data);

  _my->delete_decoded_types_data_storage();
}

void database::verify_match_of_blockchain_configuration()
{
  fc::mutable_variant_object current_blockchain_config = protocol::get_config(get_treasury_name(), get_chain_id());
  fc::variant full_current_blockchain_config_as_variant;
  fc::to_variant(current_blockchain_config, full_current_blockchain_config_as_variant);
  const std::string full_current_blockchain_config_as_json_string = fc::json::to_string(full_current_blockchain_config_as_variant);

  const std::string full_stored_blockchain_config_json = get_blockchain_config_from_shm();

  if (full_stored_blockchain_config_json.empty())
    set_blockchain_config(full_current_blockchain_config_as_json_string);
  else if (full_stored_blockchain_config_json != full_current_blockchain_config_as_json_string)
    util::verify_match_of_blockchain_configuration(current_blockchain_config, full_current_blockchain_config_as_variant, full_stored_blockchain_config_json);
}

std::string database::get_current_decoded_types_data_json()
{
  FC_ASSERT(_my->_decoded_types_data_storage);
  const std::string decoded_types_data_json = _my->_decoded_types_data_storage->generate_decoded_types_data_json_string();
  _my->delete_decoded_types_data_storage();
  return decoded_types_data_json;
}

const std::string& database::get_json_schema()const
{
  return _json_schema;
}

void database::init_schema()
{
  /*done_adding_indexes();

  db_schema ds;

  std::vector< std::shared_ptr< abstract_schema > > schema_list;

  std::vector< object_schema > object_schemas;
  get_object_schemas( object_schemas );

  for( const object_schema& oschema : object_schemas )
  {
    ds.object_types.emplace_back();
    ds.object_types.back().space_type.first = oschema.space_id;
    ds.object_types.back().space_type.second = oschema.type_id;
    oschema.schema->get_name( ds.object_types.back().type );
    schema_list.push_back( oschema.schema );
  }

  std::shared_ptr< abstract_schema > operation_schema = get_schema_for_type< operation >();
  operation_schema->get_name( ds.operation_type );
  schema_list.push_back( operation_schema );

  for( const std::pair< std::string, std::shared_ptr< custom_operation_interpreter > >& p : _custom_operation_interpreters )
  {
    ds.custom_operation_types.emplace_back();
    ds.custom_operation_types.back().id = p.first;
    schema_list.push_back( p.second->get_operation_schema() );
    schema_list.back()->get_name( ds.custom_operation_types.back().type );
  }

  graphene::db::add_dependent_schemas( schema_list );
  std::sort( schema_list.begin(), schema_list.end(),
    []( const std::shared_ptr< abstract_schema >& a,
        const std::shared_ptr< abstract_schema >& b )
    {
      return a->id < b->id;
    } );
  auto new_end = std::unique( schema_list.begin(), schema_list.end(),
    []( const std::shared_ptr< abstract_schema >& a,
        const std::shared_ptr< abstract_schema >& b )
    {
      return a->id == b->id;
    } );
  schema_list.erase( new_end, schema_list.end() );

  for( std::shared_ptr< abstract_schema >& s : schema_list )
  {
    std::string tname;
    s->get_name( tname );
    FC_ASSERT( ds.types.find( tname ) == ds.types.end(), "types with different ID's found for name ${tname}", ("tname", tname) );
    std::string ss;
    s->get_str_schema( ss );
    ds.types.emplace( tname, ss );
  }

  _json_schema = fc::json::to_string( ds );
  return;*/
}

void database::init_genesis()
{
  try
  {
    struct auth_inhibitor
    {
      auth_inhibitor(database& db) : db(db), old_flags(db.get_node_skip_flags())
      { db.set_node_skip_flags( old_flags | skip_authority_check ); }
      ~auth_inhibitor()
      { db.set_node_skip_flags( old_flags ); }
    private:
      database& db;
      uint32_t old_flags;
    } inhibitor(*this);

    // Create blockchain accounts
    public_key_type      init_public_key(HIVE_INIT_PUBLIC_KEY);

    create< account_object >( HIVE_MINER_ACCOUNT, HIVE_GENESIS_TIME );
    create< account_authority_object >( [&]( account_authority_object& auth )
    {
      auth.account = HIVE_MINER_ACCOUNT;
      auth.owner.weight_threshold = 1;
      auth.active.weight_threshold = 1;
      auth.posting.weight_threshold = 1;
    });
    get_accounts_handler().create_volatile_account_authority( get< account_authority_object, by_account >( HIVE_MINER_ACCOUNT ), true/*init_genesis*/ );

    create< account_object >( HIVE_NULL_ACCOUNT, HIVE_GENESIS_TIME );
    create< account_authority_object >( [&]( account_authority_object& auth )
    {
      auth.account = HIVE_NULL_ACCOUNT;
      auth.owner.weight_threshold = 1;
      auth.active.weight_threshold = 1;
      auth.posting.weight_threshold = 1;
    });
    get_accounts_handler().create_volatile_account_authority( get< account_authority_object, by_account >( HIVE_NULL_ACCOUNT ), true/*init_genesis*/ );

#if defined(IS_TEST_NET) || defined(HIVE_CONVERTER_ICEBERG_PLUGIN_ENABLED)
    create< account_object >( OBSOLETE_TREASURY_ACCOUNT, HIVE_GENESIS_TIME );
    create< account_authority_object >([&](account_authority_object& auth)
    {
      auth.account = OBSOLETE_TREASURY_ACCOUNT;
      auth.owner.weight_threshold = 1;
      auth.active.weight_threshold = 1;
      auth.posting.weight_threshold = 1;
    } );
    get_accounts_handler().create_volatile_account_authority( get< account_authority_object, by_account >( OBSOLETE_TREASURY_ACCOUNT ), true/*init_genesis*/ );
    create< account_object >( NEW_HIVE_TREASURY_ACCOUNT, HIVE_GENESIS_TIME );
    create< account_authority_object >([&](account_authority_object& auth)
    {
      auth.account = NEW_HIVE_TREASURY_ACCOUNT;
      auth.owner.weight_threshold = 1;
      auth.active.weight_threshold = 1;
      auth.posting.weight_threshold = 1;
    } );
    get_accounts_handler().create_volatile_account_authority( get< account_authority_object, by_account >( NEW_HIVE_TREASURY_ACCOUNT ), true/*init_genesis*/ );
#endif

    create< account_object >( HIVE_TEMP_ACCOUNT, HIVE_GENESIS_TIME );
    create< account_authority_object >( [&]( account_authority_object& auth )
    {
      auth.account = HIVE_TEMP_ACCOUNT;
      auth.owner.weight_threshold = 0;
      auth.active.weight_threshold = 0;
      auth.posting.weight_threshold = 0;
    });
    get_accounts_handler().create_volatile_account_authority( get< account_authority_object, by_account >( HIVE_TEMP_ACCOUNT ), true/*init_genesis*/ );

    const auto init_witness = [&]( const account_name_type& account_name )
    {
      create< account_object >( account_name, HIVE_GENESIS_TIME, init_public_key );

      create< account_authority_object >( [&]( account_authority_object& auth )
      {
        auth.account = account_name;
        auth.owner.add_authority( init_public_key, 1 );
        auth.owner.weight_threshold = 1;
        auth.active  = auth.owner;
        auth.posting = auth.active;
      });
      get_accounts_handler().create_volatile_account_authority( get< account_authority_object, by_account >( account_name ), true/*init_genesis*/ );

      create< witness_object >( [&]( witness_object& w )
      {
        w.owner        = account_name;
        w.signing_key  = init_public_key;
        w.schedule = witness_object::miner;
      } );
    };

    for( int i = 0; i < HIVE_NUM_INIT_MINERS; ++i )
      init_witness( HIVE_INIT_MINER_NAME + ( i ? fc::to_string( i ) : std::string() ) );

#ifdef USE_ALTERNATE_CHAIN_ID
    for( const auto& witness : configuration_data.get_init_witnesses() )
      init_witness( witness );
#endif

    // "steem" account was used as system account even before it was officially created; that bug
    // didn't have effect on the blockchain by chance, but caused problems during optimizations, so
    // now the account is officially created as system account (with all the same features it had -
    // there is not much difference between mined account and genesis one, just creation time);
    // the following transaction mined that account officially:
    // https://hiveblocks.com/tx/46ddcba847f2297d13e32be07d72d15c530a7271
    {
      const char* STEEM_ACCOUNT_NAME = "steem";
      auto STEEM_PUBLIC_KEY = public_key_type( HIVE_STEEM_PUBLIC_KEY_STR );
      create< account_object >( STEEM_ACCOUNT_NAME, STEEM_PUBLIC_KEY, HIVE_GENESIS_TIME, HIVE_GENESIS_TIME, true, nullptr, true, asset( 0, VESTS_SYMBOL ) );
      create< account_authority_object >( [&]( account_authority_object& auth )
      {
        auth.account = STEEM_ACCOUNT_NAME;
#ifdef USE_ALTERNATE_CHAIN_ID
        auth.owner = authority( 1, STEEM_PUBLIC_KEY, 1, init_public_key, 1 );
#else
        auth.owner = authority( 1, STEEM_PUBLIC_KEY, 1 );
#endif
        auth.active = auth.owner;
        auth.posting = auth.owner;
      } );
      get_accounts_handler().create_volatile_account_authority( get< account_authority_object, by_account >( STEEM_ACCOUNT_NAME ), true/*init_genesis*/ );
    }

    const auto& dgpo = create< dynamic_global_property_object >( HIVE_INIT_MINER_NAME );
    create< hardfork_property_object >( HIVE_GENESIS_TIME );

#if defined(IS_TEST_NET) || defined(HIVE_CONVERTER_ICEBERG_PLUGIN_ENABLED)
    create< feed_history_object >( [&]( feed_history_object& o )
    {
      o.current_median_history = price( asset( 1, HBD_SYMBOL ), asset( 1, HIVE_SYMBOL ) );
      o.market_median_history = o.current_median_history;
      o.current_min_history = o.current_median_history;
      o.current_max_history = o.current_median_history;
    } );
    // issue initial token supply to balance of first miner
    if( HIVE_INIT_SUPPLY != 0 || HIVE_HBD_INIT_SUPPLY != 0 )
    {
      HIVE_asset to_vest( HIVE_INITIAL_VESTING );
      VEST_asset initial_vests( to_vest * HIVE_INITIAL_VESTING_PRICE );

      modify( get_account( HIVE_INIT_MINER_NAME ), [&]( account_object& a )
      {
        a.set_balance( HIVE_asset( HIVE_INIT_SUPPLY ) - to_vest );
        a.set_hbd_balance( HBD_asset( HIVE_HBD_INIT_SUPPLY ) );
        a.set_vesting( initial_vests );
        FC_ASSERT( a.get_balance().amount >= 0 && a.get_hbd_balance().amount >= 0 && a.get_vesting().amount >= 0, "Invalid testnet configuration" );
      } );
      modify( dgpo, [&]( dynamic_global_property_object& gpo )
      {
        gpo.current_supply += HIVE_asset( HIVE_INIT_SUPPLY );
        gpo.current_hbd_supply += HBD_asset( HIVE_HBD_INIT_SUPPLY );
        gpo.init_hbd_supply = HBD_asset( HIVE_HBD_INIT_SUPPLY );
        gpo.total_vesting_fund_hive += to_vest;
        gpo.total_vesting_shares += initial_vests;
      } );
      update_virtual_supply();
    }
#else
    // Nothing to do
    create< feed_history_object >( [&]( feed_history_object& o ) {});
    FC_ASSERT( HIVE_INIT_SUPPLY == 0 && HIVE_HBD_INIT_SUPPLY == 0, "Wrong configuration" );
      // for mainnet these values must be 0, mirrornet should be compatible
#endif

    for( int i = 0; i < 0x10000; i++ )
      create< block_summary_object >( [&]( block_summary_object& ) {});

    // Create witness scheduler
    create< witness_schedule_object >( [&]( witness_schedule_object& wso )
    {
      FC_TODO( "Copied from witness_schedule.cpp, do we want to abstract this to a separate function?" );
      wso.current_shuffled_witnesses[0] = HIVE_INIT_MINER_NAME;
      util::rd_system_params account_subsidy_system_params;
      account_subsidy_system_params.resource_unit = HIVE_ACCOUNT_SUBSIDY_PRECISION;
      account_subsidy_system_params.decay_per_time_unit_denom_shift = HIVE_RD_DECAY_DENOM_SHIFT;
      util::rd_user_params account_subsidy_user_params;
      account_subsidy_user_params.budget_per_time_unit = wso.median_props.account_subsidy_budget;
      account_subsidy_user_params.decay_per_time_unit = wso.median_props.account_subsidy_decay;

      util::rd_user_params account_subsidy_per_witness_user_params;
      int64_t w_budget = wso.median_props.account_subsidy_budget;
      w_budget = (w_budget * HIVE_WITNESS_SUBSIDY_BUDGET_PERCENT) / HIVE_100_PERCENT;
      w_budget = std::min( w_budget, int64_t(std::numeric_limits<int32_t>::max()) );
      uint64_t w_decay = wso.median_props.account_subsidy_decay;
      w_decay = (w_decay * HIVE_WITNESS_SUBSIDY_DECAY_PERCENT) / HIVE_100_PERCENT;
      w_decay = std::min( w_decay, uint64_t(std::numeric_limits<uint32_t>::max()) );

      account_subsidy_per_witness_user_params.budget_per_time_unit = int32_t(w_budget);
      account_subsidy_per_witness_user_params.decay_per_time_unit = uint32_t(w_decay);

      util::rd_setup_dynamics_params( account_subsidy_user_params, account_subsidy_system_params, wso.account_subsidy_rd );
      util::rd_setup_dynamics_params( account_subsidy_per_witness_user_params, account_subsidy_system_params, wso.account_subsidy_witness_rd );
    } );

#ifdef HIVE_ENABLE_SMT
    create< nai_pool_object >( [&]( nai_pool_object& npo ) {} );
#endif
  }
  FC_CAPTURE_AND_RETHROW()
}

void database::set_flush_interval( uint32_t flush_blocks )
{
  _flush_blocks = flush_blocks;
  _next_flush_block = 0;
}

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
      chainbase::database::flush();
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
    auto get_active  =    [&]( const string& name ) { return get_accounts_handler().get_account_authority( name )->active; };
    auto get_owner   =    [&]( const string& name ) { return get_accounts_handler().get_account_authority( name )->owner;  };
    auto get_posting =    [&]( const string& name ) { return get_accounts_handler().get_account_authority( name )->posting;  };
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
  if( _current_tx_status == TX_STATUS_NONE )
  {
    wlog( "Missing tx processing indicator" );
#ifdef USE_ALTERNATE_CHAIN_ID
    FC_ASSERT( false, "Missing tx processing indicator" );
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
      get_accounts_handler().on_irreversible_block( new_last_irreversible );

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

      a.set_delegated_vesting( a.get_delegated_vesting() - itr->get_vesting() );
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
    switch( delta.symbol.asset_num )
    {
      case HIVE_ASSET_NUM_HIVE:
      {
        auto b = acnt.get_balance();
        acnt.set_balance( acnt.get_balance() + delta );
        if(trace_balance_change)
          ilog("${a} HIVE balance changed to ${nb} (previous: ${b} ) at block: ${block}. Operation context: ${c}", ("a", a.get_name())("b", b.amount)("nb", acnt.get_balance().amount)("block", _current_block_num)("c", op_context));

        if( check_balance )
        {
          FC_ASSERT( acnt.get_balance().amount.value >= 0, "Insufficient HIVE funds" );
        }
        break;
      }
      case HIVE_ASSET_NUM_HBD:
      {
        /// Starting from HF 25 HBD interest will be paid only from saving balance.
        if( has_hardfork(HIVE_HARDFORK_1_25) == false && a.hbd_seconds_last_update != head_block_time() )
        {
          acnt.hbd_seconds += fc::uint128_t(a.get_hbd_balance().amount.value) * (head_block_time() - a.hbd_seconds_last_update).to_seconds();
          acnt.hbd_seconds_last_update = head_block_time();
          if( acnt.hbd_seconds > 0 &&
              (acnt.hbd_seconds_last_update - acnt.hbd_last_interest_payment).to_seconds() > HIVE_HBD_INTEREST_COMPOUND_INTERVAL_SEC )
          {
            auto interest = acnt.hbd_seconds / HIVE_SECONDS_PER_YEAR;
            interest *= get_dynamic_global_properties().get_hbd_interest_rate();
            interest /= HIVE_100_PERCENT;
            asset interest_paid(fc::uint128_to_uint64(interest), HBD_SYMBOL);
            acnt.set_hbd_balance( acnt.get_hbd_balance() + HBD_asset( interest_paid ) );
            acnt.hbd_seconds = 0;
            acnt.hbd_last_interest_payment = head_block_time();

            if(interest > 0)
              push_virtual_operation( interest_operation( a.get_name(), interest_paid, true ) );

            modify( get_dynamic_global_properties(), [&]( dynamic_global_property_object& props)
            {
              props.current_hbd_supply += interest_paid;
              props.virtual_supply += interest_paid * get_feed_history().current_median_history;
            } );
          }
        }

        auto b = acnt.get_hbd_balance();
        acnt.set_hbd_balance( acnt.get_hbd_balance() + HBD_asset( delta ) );

        if(trace_balance_change)
          ilog("${a} HBD balance changed to ${nb} (previous: ${b} ) at block: ${block}. Operation context: ${c}", ("a", a.get_name())("b", b.amount)("nb", acnt.get_hbd_balance().amount)("block", _current_block_num)("c", op_context));

        if( check_balance )
        {
          FC_ASSERT( acnt.get_hbd_balance().amount.value >= 0, "Insufficient HBD funds" );
        }
        break;
      }
      case HIVE_ASSET_NUM_VESTS:
        acnt.set_vesting( acnt.get_vesting() + delta );
        if( check_balance )
        {
          FC_ASSERT( acnt.get_vesting().amount.value >= 0, "Insufficient VESTS funds" );
        }
        break;
      default:
        FC_ASSERT( false, "invalid symbol" );
    }
  } );
}

void database::modify_reward_balance( const account_object& a, const asset& value_delta, const asset& share_delta, bool check_balance )
{
  modify( a, [&]( account_object& acnt )
  {
    switch( value_delta.symbol.asset_num )
    {
      case HIVE_ASSET_NUM_HIVE:
        if( share_delta.amount.value == 0 )
        {
          acnt.set_rewards( acnt.get_rewards() + value_delta );
          if( check_balance )
          {
            FC_ASSERT( acnt.get_rewards().amount.value >= 0, "Insufficient reward HIVE funds" );
          }
        }
        else
        {
          acnt.set_vest_rewards_as_hive( acnt.get_vest_rewards_as_hive() + HIVE_asset( value_delta ) );
          acnt.set_vest_rewards( acnt.get_vest_rewards() + VEST_asset( share_delta ) );
          if( check_balance )
          {
            FC_ASSERT( acnt.get_vest_rewards().amount.value >= 0, "Insufficient reward VESTS funds" );
          }
        }
        break;
      case HIVE_ASSET_NUM_HBD:
        FC_ASSERT( share_delta.amount.value == 0 );
        acnt.set_hbd_rewards( acnt.get_hbd_rewards() + HBD_asset( value_delta ) );
        if( check_balance )
        {
          FC_ASSERT( acnt.get_hbd_rewards().amount.value >= 0, "Insufficient reward HBD funds" );
        }
        break;
      default:
        FC_ASSERT( false, "invalid symbol" );
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
    switch( delta.symbol.asset_num )
    {
      case HIVE_ASSET_NUM_HIVE:
        acnt.set_savings( acnt.get_savings() + delta );
        if( check_balance )
        {
          FC_ASSERT( acnt.get_savings().amount.value >= 0, "Insufficient savings HIVE funds" );
        }
        break;
      case HIVE_ASSET_NUM_HBD:
        if( a.savings_hbd_seconds_last_update != head_block_time() )
        {
          acnt.savings_hbd_seconds += fc::uint128_t(a.get_hbd_savings().amount.value) * (head_block_time() - a.savings_hbd_seconds_last_update).to_seconds();
          acnt.savings_hbd_seconds_last_update = head_block_time();

          if( acnt.savings_hbd_seconds > 0 &&
              (acnt.savings_hbd_seconds_last_update - acnt.savings_hbd_last_interest_payment).to_seconds() > HIVE_HBD_INTEREST_COMPOUND_INTERVAL_SEC )
          {
            auto interest = acnt.savings_hbd_seconds / HIVE_SECONDS_PER_YEAR;
            interest *= get_dynamic_global_properties().get_hbd_interest_rate();
            interest /= HIVE_100_PERCENT;
            asset interest_paid(fc::uint128_to_uint64(interest), HBD_SYMBOL);
            acnt.set_hbd_savings( acnt.get_hbd_savings() + interest_paid );
            acnt.savings_hbd_seconds = 0;
            acnt.savings_hbd_last_interest_payment = head_block_time();

            if(interest > 0)
              push_virtual_operation( interest_operation( a.get_name(), interest_paid, false ) );

            modify( get_dynamic_global_properties(), [&]( dynamic_global_property_object& props)
            {
              props.current_hbd_supply += interest_paid;
              props.virtual_supply += interest_paid * get_feed_history().current_median_history;
            } );
          }
        }
        acnt.set_hbd_savings( acnt.get_hbd_savings() + delta );
        if( check_balance )
        {
          FC_ASSERT( acnt.get_hbd_savings().amount.value >= 0, "Insufficient savings HBD funds" );
        }
        break;
      default:
        FC_ASSERT( !"invalid symbol" );
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
    switch( delta.symbol.asset_num )
    {
      case HIVE_ASSET_NUM_HIVE:
      {
        asset new_vesting( (adjust_vesting && delta.amount > 0) ? delta.amount * 9 : 0, HIVE_SYMBOL );
        props.current_supply += delta + new_vesting;
        props.virtual_supply += delta + new_vesting;
        props.total_vesting_fund_hive += new_vesting;
        if( check_supply )
        {
          FC_ASSERT( props.current_supply.amount.value >= 0 );
        }
        break;
      }
      case HIVE_ASSET_NUM_HBD:
        props.current_hbd_supply += delta;
        props.virtual_supply = props.get_current_hbd_supply() * get_feed_history().current_median_history + props.current_supply;
        if( check_supply )
        {
          FC_ASSERT( props.get_current_hbd_supply().amount.value >= 0 );
        }
        break;
      default:
        FC_ASSERT( false, "invalid symbol" );
    }
  } );
}


asset database::get_balance( const account_object& a, asset_symbol_type symbol )const
{
  switch( symbol.asset_num )
  {
    case HIVE_ASSET_NUM_HIVE:
      return a.get_balance();
    case HIVE_ASSET_NUM_HBD:
      return a.get_hbd_balance();
    default:
    {
#ifdef HIVE_ENABLE_SMT
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
#else
      FC_ASSERT( false, "Invalid symbol: ${s}", ("s", symbol) );
#endif
    }
  }
}

asset database::get_savings( const account_object& a, asset_symbol_type symbol )const
{
  switch( symbol.asset_num )
  {
    case HIVE_ASSET_NUM_HIVE:
      return a.get_savings();
    case HIVE_ASSET_NUM_HBD:
      return a.get_hbd_savings();
    default: // Note no savings balance for SMT per comments in issue 1682.
      FC_ASSERT( !"invalid symbol" );
  }
}

void database::init_hardforks()
{
  _hardfork_versions.times[ 0 ] = fc::time_point_sec( HIVE_GENESIS_TIME );
  _hardfork_versions.versions[ 0 ] = hardfork_version( 0, 0 );
  FC_ASSERT( HIVE_HARDFORK_0_1 == 1, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_1 ] = fc::time_point_sec( HIVE_HARDFORK_0_1_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_1 ] = HIVE_HARDFORK_0_1_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_2 == 2, "Invlaid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_2 ] = fc::time_point_sec( HIVE_HARDFORK_0_2_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_2 ] = HIVE_HARDFORK_0_2_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_3 == 3, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_3 ] = fc::time_point_sec( HIVE_HARDFORK_0_3_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_3 ] = HIVE_HARDFORK_0_3_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_4 == 4, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_4 ] = fc::time_point_sec( HIVE_HARDFORK_0_4_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_4 ] = HIVE_HARDFORK_0_4_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_5 == 5, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_5 ] = fc::time_point_sec( HIVE_HARDFORK_0_5_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_5 ] = HIVE_HARDFORK_0_5_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_6 == 6, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_6 ] = fc::time_point_sec( HIVE_HARDFORK_0_6_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_6 ] = HIVE_HARDFORK_0_6_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_7 == 7, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_7 ] = fc::time_point_sec( HIVE_HARDFORK_0_7_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_7 ] = HIVE_HARDFORK_0_7_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_8 == 8, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_8 ] = fc::time_point_sec( HIVE_HARDFORK_0_8_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_8 ] = HIVE_HARDFORK_0_8_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_9 == 9, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_9 ] = fc::time_point_sec( HIVE_HARDFORK_0_9_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_9 ] = HIVE_HARDFORK_0_9_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_10 == 10, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_10 ] = fc::time_point_sec( HIVE_HARDFORK_0_10_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_10 ] = HIVE_HARDFORK_0_10_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_11 == 11, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_11 ] = fc::time_point_sec( HIVE_HARDFORK_0_11_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_11 ] = HIVE_HARDFORK_0_11_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_12 == 12, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_12 ] = fc::time_point_sec( HIVE_HARDFORK_0_12_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_12 ] = HIVE_HARDFORK_0_12_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_13 == 13, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_13 ] = fc::time_point_sec( HIVE_HARDFORK_0_13_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_13 ] = HIVE_HARDFORK_0_13_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_14 == 14, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_14 ] = fc::time_point_sec( HIVE_HARDFORK_0_14_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_14 ] = HIVE_HARDFORK_0_14_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_15 == 15, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_15 ] = fc::time_point_sec( HIVE_HARDFORK_0_15_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_15 ] = HIVE_HARDFORK_0_15_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_16 == 16, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_16 ] = fc::time_point_sec( HIVE_HARDFORK_0_16_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_16 ] = HIVE_HARDFORK_0_16_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_17 == 17, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_17 ] = fc::time_point_sec( HIVE_HARDFORK_0_17_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_17 ] = HIVE_HARDFORK_0_17_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_18 == 18, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_18 ] = fc::time_point_sec( HIVE_HARDFORK_0_18_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_18 ] = HIVE_HARDFORK_0_18_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_19 == 19, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_19 ] = fc::time_point_sec( HIVE_HARDFORK_0_19_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_19 ] = HIVE_HARDFORK_0_19_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_20 == 20, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_20 ] = fc::time_point_sec( HIVE_HARDFORK_0_20_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_20 ] = HIVE_HARDFORK_0_20_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_21 == 21, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_21 ] = fc::time_point_sec( HIVE_HARDFORK_0_21_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_21 ] = HIVE_HARDFORK_0_21_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_22 == 22, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_22 ] = fc::time_point_sec( HIVE_HARDFORK_0_22_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_22 ] = HIVE_HARDFORK_0_22_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_23 == 23, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_23 ] = fc::time_point_sec( HIVE_HARDFORK_0_23_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_23 ] = HIVE_HARDFORK_0_23_VERSION;
  FC_ASSERT( HIVE_HARDFORK_1_24 == 24, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_1_24 ] = fc::time_point_sec( HIVE_HARDFORK_1_24_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_1_24 ] = HIVE_HARDFORK_1_24_VERSION;
  FC_ASSERT( HIVE_HARDFORK_1_25 == 25, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_1_25 ] = fc::time_point_sec( HIVE_HARDFORK_1_25_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_1_25 ] = HIVE_HARDFORK_1_25_VERSION;
  FC_ASSERT( HIVE_HARDFORK_1_26 == 26, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_1_26 ] = fc::time_point_sec( HIVE_HARDFORK_1_26_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_1_26 ] = HIVE_HARDFORK_1_26_VERSION;
  FC_ASSERT( HIVE_HARDFORK_1_27 == 27, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_1_27 ] = fc::time_point_sec( HIVE_HARDFORK_1_27_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_1_27 ] = HIVE_HARDFORK_1_27_VERSION;
#if defined(USE_ALTERNATE_CHAIN_ID)
  FC_ASSERT( HIVE_HARDFORK_1_28 == 28, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_1_28 ] = fc::time_point_sec( HIVE_HARDFORK_1_28_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_1_28 ] = HIVE_HARDFORK_1_28_VERSION;
#if defined(IS_TEST_NET) && defined(HIVE_ENABLE_SMT)
  FC_ASSERT( HIVE_HARDFORK_1_29 == 29, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_1_29 ] = fc::time_point_sec( HIVE_HARDFORK_1_29_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_1_29 ] = HIVE_HARDFORK_1_29_VERSION;
#endif
#endif
}

void database::process_hardforks()
{
  try
  {
    // If there are upcoming hardforks and the next one is later, do nothing
    const auto& hardforks = get_hardfork_property_object();

    if( has_hardfork( HIVE_HARDFORK_0_5__54 ) )
    {
      while( _hardfork_versions.versions[ hardforks.last_hardfork ] < hardforks.next_hardfork
        && hardforks.next_hardfork_time <= head_block_time() )
      {
        if( hardforks.last_hardfork < HIVE_NUM_HARDFORKS ) {
          apply_hardfork( hardforks.last_hardfork + 1 );
        }
        else
          throw unknown_hardfork_exception();
      }
    }
    else
    {
      while( hardforks.last_hardfork < HIVE_NUM_HARDFORKS
          && _hardfork_versions.times[ hardforks.last_hardfork + 1 ] <= head_block_time()
          && hardforks.last_hardfork < HIVE_HARDFORK_0_5__54 )
      {
        apply_hardfork( hardforks.last_hardfork + 1 );
      }
    }
  }
  FC_CAPTURE_AND_RETHROW()
}

bool database::has_hardfork( uint32_t hardfork )const
{
  return get_hardfork_property_object().processed_hardforks.size() > hardfork;
}

uint32_t database::get_hardfork()const
{
  return get_hardfork_property_object().processed_hardforks.size() - 1;
}

void database::set_hardfork( uint32_t hardfork, bool apply_now )
{
  auto const& hardforks = get_hardfork_property_object();

  for( uint32_t i = hardforks.last_hardfork + 1; i <= hardfork && i <= HIVE_NUM_HARDFORKS; i++ )
  {
    if( i <= HIVE_HARDFORK_0_5__54 )
      _hardfork_versions.times[i] = head_block_time();
    else
    {
      modify( hardforks, [&]( hardfork_property_object& hpo )
      {
        hpo.next_hardfork = _hardfork_versions.versions[i];
        hpo.next_hardfork_time = head_block_time();
      } );
    }

    if( apply_now )
      apply_hardfork( i );
  }
}

void database::apply_hardfork( uint32_t hardfork )
{
  if( _log_hardforks )
    elog( "HARDFORK ${hf} at block ${b}", ("hf", hardfork)("b", head_block_num()) );
  operation hardfork_vop = hardfork_operation( hardfork );

  pre_push_virtual_operation( hardfork_vop );
  const auto _op_in_trx = _current_op_in_trx;

  switch( hardfork )
  {
    case HIVE_HARDFORK_0_1:
      perform_vesting_share_split( 1000000 );
      break;
    case HIVE_HARDFORK_0_2:
      retally_witness_votes();
      break;
    case HIVE_HARDFORK_0_3:
      retally_witness_votes();
      break;
    case HIVE_HARDFORK_0_4:
      reset_virtual_schedule_time(*this);
      break;
    case HIVE_HARDFORK_0_5:
      break;
    case HIVE_HARDFORK_0_6:
      retally_witness_vote_counts();
      break;
    case HIVE_HARDFORK_0_7:
      break;
    case HIVE_HARDFORK_0_8:
      retally_witness_vote_counts(true);
      break;
    case HIVE_HARDFORK_0_9:
      {
        for( const std::string& acc : hardfork9::get_compromised_accounts() )
        {
          const account_object* account = find_account( acc );
          if( account == nullptr )
            continue;

          wlog("Setting key: ${k} as an owner authority for account: ${a}", ("k", HIVE_HF_9_COMPROMISED_ACCOUNTS_PUBLIC_KEY_STR)("a", acc));

          update_owner_authority( *account, authority( 1, public_key_type(HIVE_HF_9_COMPROMISED_ACCOUNTS_PUBLIC_KEY_STR), 1 ) );

          auto _modifier = [&]( account_authority_object& auth )
          {
            auth.active  = authority( 1, public_key_type(HIVE_HF_9_COMPROMISED_ACCOUNTS_PUBLIC_KEY_STR), 1 );
            auth.posting = authority( 1, public_key_type(HIVE_HF_9_COMPROMISED_ACCOUNTS_PUBLIC_KEY_STR), 1 );
          };
          update_account_authority( account->get_name(), _modifier );
        }
      }
      break;
    case HIVE_HARDFORK_0_10:
      retally_liquidity_weight();
      break;
    case HIVE_HARDFORK_0_11:
      break;
    case HIVE_HARDFORK_0_12:
      {
        const auto& comment_idx = get_index< comment_cashout_index >().indices();

        for( auto itr = comment_idx.begin(); itr != comment_idx.end(); ++itr )
        {
          // At the hardfork time, all new posts with no votes get their cashout time set to +12 hrs from head block time.
          // All posts with a payout get their cashout time set to +30 days. This hardfork takes place within 30 days
          // initial payout so we don't have to handle the case of posts that should be frozen that aren't
          const comment_object& comment = get_comment( *itr );
          const comment_cashout_ex_object* c_ex = find_comment_cashout_ex( comment );
          if( comment.is_root() )
          {
            // Post has not been paid out and has no votes (cashout_time == 0 === net_rshares == 0, under current semantics)
            if( !c_ex->was_paid() && itr->get_cashout_time() == fc::time_point_sec::maximum() )
            {
              modify( *itr, [&]( comment_cashout_object & c )
              {
                c.set_cashout_time( head_block_time() + HIVE_CASHOUT_WINDOW_SECONDS_PRE_HF17 );
              });
            }
            // Has been paid out, needs to be on second cashout window
            else if( c_ex->was_paid() )
            {
              modify( *itr, [&]( comment_cashout_object& c )
              {
                c.set_cashout_time( c_ex->get_last_payout() + HIVE_SECOND_CASHOUT_WINDOW );
              });
            }
          }
        }

        // liquidity reward mechanism no longer active after HF12
        auto& liquidity_reward_balance_idx = get_mutable_index< liquidity_reward_balance_index >();
        liquidity_reward_balance_idx.clear();
      }
      break;
    case HIVE_HARDFORK_0_13:
      break;
    case HIVE_HARDFORK_0_14:
      break;
    case HIVE_HARDFORK_0_15:
      break;
    case HIVE_HARDFORK_0_16:
      {
        modify( get_feed_history(), [&]( feed_history_object& fho )
        {
          while( fho.price_history.size() > HIVE_FEED_HISTORY_WINDOW )
            fho.price_history.pop_front();
        });
      }
      break;
    case HIVE_HARDFORK_0_17:
      {
        static_assert(
          HIVE_MAX_VOTED_WITNESSES_HF0 + HIVE_MAX_MINER_WITNESSES_HF0 + HIVE_MAX_RUNNER_WITNESSES_HF0 == HIVE_MAX_WITNESSES,
          "HF0 witness counts must add up to HIVE_MAX_WITNESSES" );
        static_assert(
          HIVE_MAX_VOTED_WITNESSES_HF17 + HIVE_MAX_MINER_WITNESSES_HF17 + HIVE_MAX_RUNNER_WITNESSES_HF17 == HIVE_MAX_WITNESSES,
          "HF17 witness counts must add up to HIVE_MAX_WITNESSES" );

        modify( get_witness_schedule_object(), [&]( witness_schedule_object& wso )
        {
          wso.max_voted_witnesses = HIVE_MAX_VOTED_WITNESSES_HF17;
          wso.max_miner_witnesses = HIVE_MAX_MINER_WITNESSES_HF17;
          wso.max_runner_witnesses = HIVE_MAX_RUNNER_WITNESSES_HF17;
        });

        const auto& gpo = get_dynamic_global_properties();

        auto& post_rf = create< reward_fund_object >( HIVE_POST_REWARD_FUND_NAME, gpo.get_total_reward_fund_hive(), head_block_time()
#ifndef IS_TEST_NET
          , HIVE_HF_17_RECENT_CLAIMS
#endif
          );

        // As a shortcut in payout processing, we use the id as an array index.
        // The IDs must be assigned this way. The assertion is a dummy check to ensure this happens.
        FC_ASSERT( post_rf.get_id() == reward_fund_id_type() );

        modify( gpo, [&]( dynamic_global_property_object& g )
        {
          g.total_reward_fund_hive = asset( 0, HIVE_SYMBOL );
          g.total_reward_shares2 = 0;
        });

        /*
        * For all current comments we will either keep their current cashout time, or extend it to 1 week
        * after creation.
        *
        * We cannot do a simple iteration by cashout time because we are editting cashout time.
        * More specifically, we will be adding an explicit cashout time to all comments with parents.
        * To find all discussions that have not been paid out we fir iterate over posts by cashout time.
        * Before the hardfork these are all root posts. Iterate over all of their children, adding each
        * to a specific list. Next, update payout times for all discussions on the root post. This defines
        * the min cashout time for each child in the discussion. Then iterate over the children and set
        * their cashout time in a similar way, grabbing the root post as their inherent cashout time.
        */
        const auto& comment_idx = get_index< comment_cashout_index, by_cashout_time >();
        const auto& by_root_idx = get_index< comment_cashout_ex_index, by_root >();
        vector< const comment_cashout_object* > root_posts;
        root_posts.reserve( HIVE_HF_17_NUM_POSTS );
        vector< const comment_cashout_object* > replies;
        replies.reserve( HIVE_HF_17_NUM_REPLIES );

        for( auto itr = comment_idx.begin(); itr != comment_idx.end() && itr->get_cashout_time() < fc::time_point_sec::maximum(); ++itr )
        {
          root_posts.push_back( &(*itr) );
          auto root_id = itr->get_comment_id();

          for( auto reply_itr = by_root_idx.lower_bound( root_id ); reply_itr != by_root_idx.end() && reply_itr->get_root_id() == root_id; ++reply_itr )
          {
            const comment_cashout_object* comment_cashout = find_comment_cashout( reply_itr->get_comment_id() );
            replies.push_back( comment_cashout );
          }
        }

        for( const auto& itr : root_posts )
        {
          modify( *itr, [&]( comment_cashout_object& c )
          {
            c.set_cashout_time( std::max( c.get_creation_time() + HIVE_CASHOUT_WINDOW_SECONDS, c.get_cashout_time() ) );
          });
        }

        for( const auto& itr : replies )
        {
          modify( *itr, [&]( comment_cashout_object& c )
          {
            c.set_cashout_time( std::max( calculate_discussion_payout_time( get_comment( c ), c ), c.get_creation_time() + HIVE_CASHOUT_WINDOW_SECONDS ) );
          });
        }
      }
      break;
    case HIVE_HARDFORK_0_18:
      break;
    case HIVE_HARDFORK_0_19:
      {
        modify( get_dynamic_global_properties(), [&]( dynamic_global_property_object& gpo )
        {
          gpo.vote_power_reserve_rate = HIVE_REDUCED_VOTE_POWER_RATE;
        });

        modify( get< reward_fund_object, by_name >( HIVE_POST_REWARD_FUND_NAME ), [&]( reward_fund_object &rfo )
        {
#ifndef IS_TEST_NET
          rfo.recent_claims = HIVE_HF_19_RECENT_CLAIMS;
#endif
          rfo.author_reward_curve = curve_id::linear;
          rfo.curation_reward_curve = curve_id::square_root;
        });

        /* Remove all 0 delegation objects */
        vector< const vesting_delegation_object* > to_remove;
        const auto& delegation_idx = get_index< vesting_delegation_index, by_id >();
        auto delegation_itr = delegation_idx.begin();

        while( delegation_itr != delegation_idx.end() )
        {
          if( delegation_itr->get_vesting().amount == 0 )
            to_remove.push_back( &(*delegation_itr) );

          ++delegation_itr;
        }

        for( const vesting_delegation_object* delegation_ptr: to_remove )
        {
          remove( *delegation_ptr );
        }

        remove_old_cashouts();
      }
      break;
    case HIVE_HARDFORK_0_20:
      {
        const auto& dgpo = get_dynamic_global_properties();
        modify( dgpo, [&]( dynamic_global_property_object& gpo )
        {
          gpo.delegation_return_period = HIVE_DELEGATION_RETURN_PERIOD_HF20;
          gpo.reverse_auction_seconds = HIVE_REVERSE_AUCTION_WINDOW_SECONDS_HF20;
          gpo.hbd_stop_percent = HIVE_HBD_STOP_PERCENT_HF20;
          gpo.hbd_start_percent = HIVE_HBD_START_PERCENT_HF20;
          gpo.available_account_subsidies = 0;
        } );

        const auto& wso = get_witness_schedule_object();
        for( const auto& witness : wso.current_shuffled_witnesses )
        {
          // Required check when applying hardfork at genesis
          if( witness != account_name_type() )
          {
            modify( get< witness_object, by_name >( witness ), [&]( witness_object& w )
            {
              w.props.account_creation_fee = asset( w.props.account_creation_fee.amount * HIVE_CREATE_ACCOUNT_WITH_HIVE_MODIFIER, HIVE_SYMBOL );
            } );
          }
        }

        modify( wso, [&]( witness_schedule_object& wso )
        {
          wso.median_props.account_creation_fee = asset( wso.median_props.account_creation_fee.amount * HIVE_CREATE_ACCOUNT_WITH_HIVE_MODIFIER, HIVE_SYMBOL );
        } );

        // Initialize RC:

        // Initial values are located at `libraries/jsonball/data/resource_parameters.json`
        std::string resource_params_json = hive::jsonball::get_resource_parameters();
        fc::variant resource_params_var = fc::json::from_string( resource_params_json, fc::json::format_validation_mode::full, fc::json::strict_parser );
        std::vector< std::pair< fc::variant, std::pair< fc::variant_object, fc::variant_object > > > resource_params_pairs;
        fc::from_variant( resource_params_var, resource_params_pairs );
        fc::time_point_sec now = dgpo.time;

        const auto& rc_params = create< rc_resource_param_object >( [&]( rc_resource_param_object& params_obj )
        {
          for( auto& kv : resource_params_pairs )
          {
            auto k = kv.first.as< rc_resource_types >();
            fc::variant_object& vo = kv.second.first;
            fc::mutable_variant_object mvo( vo );
            fc::from_variant( fc::variant( mvo ), params_obj.resource_param_array[k] );
          }
        } );
        // override value for new account tokens using parameters provided by witnesses
        rc.set_pool_params( wso );
        dlog( "Initial RC params: ${o}", ( "o", rc_params ) );

        // create usage statistics buckets (empty, but with proper timestamps, last bucket has current timestamp)
        time_point_sec timestamp = now - fc::seconds( HIVE_RC_BUCKET_TIME_LENGTH * ( HIVE_RC_WINDOW_BUCKET_COUNT - 1 ) );
        for( int i = 0; i < HIVE_RC_WINDOW_BUCKET_COUNT; ++i )
        {
          create< rc_usage_bucket_object >( timestamp );
          timestamp += fc::seconds( HIVE_RC_BUCKET_TIME_LENGTH );
        }

        const auto& rc_pool = create< rc_pool_object >( rc_params, resource_count_type() );
        ilog( "Initial RC pools: ${o}", ( "o", rc_pool.get_pool() ) );

        create< rc_stats_object >( RC_PENDING_STATS_ID.get_value() );
        create< rc_stats_object >( RC_ARCHIVE_STATS_ID.get_value() );

        const auto& idx = get_index< account_index, by_id >();
        for( auto it = idx.begin(); it != idx.end(); ++it )
        {
          modify( *it, [&]( account_object& account )
          {
            account.rc_adjustment = HIVE_RC_HISTORICAL_ACCOUNT_CREATION_ADJUSTMENT;
            account.rc_manabar.last_update_time = now.sec_since_epoch();
            auto max_rc = account.get_maximum_rc().value;
            account.rc_manabar.current_mana = max_rc;
            account.last_max_rc = max_rc;
          } );
        }

      }
      break;
    case HIVE_HARDFORK_0_21:
    {
      modify( get_dynamic_global_properties(), [&]( dynamic_global_property_object& gpo )
      {
        gpo.proposal_fund_percent = HIVE_PROPOSAL_FUND_PERCENT_HF21;
        gpo.content_reward_percent = HIVE_CONTENT_REWARD_PERCENT_HF21;
        gpo.downvote_pool_percent = HIVE_DOWNVOTE_POOL_PERCENT_HF21;
        gpo.reverse_auction_seconds = HIVE_REVERSE_AUCTION_WINDOW_SECONDS_HF21;
      });

      const auto treasury_name = get_treasury_name();

      // Create the treasury account if it does not exist
      // This may sometimes happen in the mirrornet, when we do not have the account created upon the HF 21 application or any dependent operation
      if( find_account(treasury_name) == nullptr )
        create< account_object >( treasury_name, head_block_time() );

      lock_account( get_treasury() );

      modify( get< reward_fund_object, by_name >( HIVE_POST_REWARD_FUND_NAME ), [&]( reward_fund_object& rfo )
      {
        rfo.percent_curation_rewards = 50 * HIVE_1_PERCENT;
        rfo.author_reward_curve = convergent_linear;
        rfo.curation_reward_curve = convergent_square_root;
        rfo.content_constant = HIVE_CONTENT_CONSTANT_HF21;
#ifndef  IS_TEST_NET
        rfo.recent_claims = HIVE_HF21_CONVERGENT_LINEAR_RECENT_CLAIMS;
#endif
      });
    }
    break;
    case HIVE_HARDFORK_0_22:
      break;
    case HIVE_HARDFORK_0_23:
    {
      clear_accounts( hardforkprotect::get_steemit_accounts() );

      /** Reset TAPOS buffer to avoid replay attack - do it only for mainnet, since is far away from such blocks.
          Skip it now since it is ineffective, but i.e. breaks live 5M mirrornet instance having applied all HFs.
      */
      //auto empty_block_id = block_id_type();
      //const auto& bs_idx = get_index< block_summary_index, by_id >();
      //for( auto itr = bs_idx.begin(); itr != bs_idx.end(); ++itr )
      //{
      //  modify( *itr, [&](block_summary_object& p) {
      //    p.block_id = empty_block_id;
      //  });
      //}
      break;
    }
    case HIVE_HARDFORK_1_24:
    {
      restore_accounts( hardforkprotect::get_restored_accounts() );
#ifdef USE_ALTERNATE_CHAIN_ID
      /// Don't change chain_id in testnet build.
#else
      set_chain_id(HIVE_CHAIN_ID);
#endif /// IS_TEST_NET
      break;
    }
    case HIVE_HARDFORK_1_25:
    {
      modify( get< reward_fund_object, by_name >( HIVE_POST_REWARD_FUND_NAME ), [&]( reward_fund_object& rfo )
      {
        rfo.curation_reward_curve = linear;
        rfo.author_reward_curve   = linear;
      });
      modify( get_dynamic_global_properties(), [&]( dynamic_global_property_object& gpo )
      {
        gpo.reverse_auction_seconds = HIVE_REVERSE_AUCTION_WINDOW_SECONDS_HF25;
        gpo.early_voting_seconds    = HIVE_EARLY_VOTING_SECONDS_HF25;
        gpo.mid_voting_seconds      = HIVE_MID_VOTING_SECONDS_HF25;
      });
      break;
    }
    case HIVE_HARDFORK_1_26:
    {
      modify( get_dynamic_global_properties(), [&]( dynamic_global_property_object& gpo )
      {
        gpo.hbd_stop_percent = HIVE_HBD_STOP_PERCENT_HF26;
        gpo.hbd_start_percent = HIVE_HBD_START_PERCENT_HF26;
      } );
      const auto& fwso = create<witness_schedule_object>( [&]( witness_schedule_object& future_witness_schedule )
      {
        future_witness_schedule.copy_values_from( get_witness_schedule_object() );
      } );
      FC_ASSERT( fwso.get_id() == 1, "Unexpected id allocated to future witness schedule object" );
      break;
    }
    case HIVE_HARDFORK_1_28:
    {
      remove_proposal_votes_for_accounts_without_voting_rights();
      break;
    }
    case HIVE_SMT_HARDFORK:
    {
#ifdef HIVE_ENABLE_SMT
      replenish_nai_pool( *this );
#endif
      break;
    }
    default:
      break;
  }

  modify( get_hardfork_property_object(), [&]( hardfork_property_object& hfp )
  {
    FC_ASSERT( hardfork == hfp.last_hardfork + 1, "Hardfork being applied out of order", ("hardfork",hardfork)("hfp.last_hardfork",hfp.last_hardfork) );
    FC_ASSERT( hfp.processed_hardforks.size() == hardfork, "Hardfork being applied out of order" );
    hfp.processed_hardforks.push_back( _hardfork_versions.times[ hardfork ] );
    hfp.last_hardfork = hardfork;
    hfp.current_hardfork_version = _hardfork_versions.versions[ hardfork ];
    FC_ASSERT( hfp.processed_hardforks[ hfp.last_hardfork ] == _hardfork_versions.times[ hfp.last_hardfork ], "Hardfork processing failed sanity check..." );
  } );

  if( hardfork == HIVE_HARDFORK_1_24_TREASURY_RENAME )
  {
    const auto treasury_name = get_treasury_name();

    if( find_account(treasury_name) == nullptr )
      create< account_object >( treasury_name, head_block_time() );

    lock_account( get_treasury() );
    //the following routine can only be called effectively after hardfork was marked as applied
    //we could wait for regular call in _apply_block(), however it could hinder future changes, most notably use of treasury in future
    //hardfork code with assumption of nonzero balance
    consolidate_treasury_balance();
  }
  // HF 24 updates blockchain configuration.
  if (hardfork == HIVE_HARDFORK_1_24)
  {
    const auto current_blockchain_config = protocol::get_config(get_treasury_name(), get_chain_id());
    fc::variant current_blockchain_config_as_variant;
    fc::to_variant(current_blockchain_config, current_blockchain_config_as_variant);
    set_blockchain_config(fc::json::to_string(current_blockchain_config_as_variant));
  }

  post_push_virtual_operation( hardfork_vop, _op_in_trx );
}

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

    const auto& convert_request_idx = get_index< convert_request_index >().indices();
    for( auto itr = convert_request_idx.begin(); itr != convert_request_idx.end(); ++itr )
    {
      total_hbd += itr->get_convert_amount();
      ++convert_no;
    }

    const auto& collateralized_convert_request_idx = get_index< collateralized_convert_request_index >().indices();
    for( auto itr = collateralized_convert_request_idx.begin(); itr != collateralized_convert_request_idx.end(); ++itr )
    {
      total_supply += itr->get_collateral_amount();
      // don't collect get_converted_amount() - it is not balance object; that tokens are already on owner's balance
      ++collateralized_convert_no;
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

    const auto& escrow_idx = get_index< escrow_index >().indices().get< by_id >();

    for( auto itr = escrow_idx.begin(); itr != escrow_idx.end(); ++itr )
    {
      total_supply += itr->get_hive_balance();
      total_hbd += itr->get_hbd_balance();

      if( itr->get_fee().symbol == HIVE_SYMBOL )
        total_supply += itr->get_fee();
      else if( itr->get_fee().symbol == HBD_SYMBOL )
        total_hbd += itr->get_fee();
      else
        FC_ASSERT( false, "found escrow pending fee that is not HBD or HIVE" );
      ++escrow_no;
    }

    const auto& savings_withdraw_idx = get_index< savings_withdraw_index >().indices().get< by_id >();

    for( auto itr = savings_withdraw_idx.begin(); itr != savings_withdraw_idx.end(); ++itr )
    {
      if( itr->amount.symbol == HIVE_SYMBOL )
        total_supply += itr->amount;
      else if( itr->amount.symbol == HBD_SYMBOL )
        total_hbd += itr->amount;
      else
        FC_ASSERT( false, "found savings withdraw that is not HBD or HIVE" );
      ++withdrawal_no;
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

void database::perform_vesting_share_split( uint32_t magnitude )
{
  try
  {
    modify( get_dynamic_global_properties(), [&]( dynamic_global_property_object& d )
    {
      d.total_vesting_shares.amount *= magnitude;
      d.total_reward_shares2 = 0;
    } );

    // Need to update all VESTS in accounts and the total VESTS in the dgpo
    for( const auto& account : get_index< account_index, by_id >() )
    {
      asset old_vesting_shares = account.get_vesting();
      asset new_vesting_shares = old_vesting_shares;
      modify( account, [&]( account_object& a )
      {
        a.set_vesting( a.get_vesting() * magnitude );
        new_vesting_shares = a.get_vesting();
        a.set_withdrawn( a.get_withdrawn() * magnitude );
        a.set_to_withdraw( a.get_to_withdraw() * magnitude );
        a.set_vesting_withdraw_rate( asset( a.get_to_withdraw().amount / HIVE_VESTING_WITHDRAW_INTERVALS_PRE_HF_16, VESTS_SYMBOL ) );
        if( a.get_vesting_withdraw_rate().amount == 0 )
          a.set_vesting_withdraw_rate( VEST_asset( 1 ) );
        //ABW: above setting to 1 is unnecessary and a bug, but it is also a bug that is very hard to fix;
        //it is a bug, because it sets 1 for all accounts that had no active power down, for whom the original 0
        //was correct value; it is hard to fix, because some of affected accounts still have that 1 to this day
        //and could exploit any lazy fix; the problem lies in "cancel active power down" - since HF5 there really
        //needs to be active power down for it to be acceptable, however for the check code that unfortunate 1 looks
        //like active power down - if we fix it and leave zero, suddenly some historical as well as potentially
        //new operations become invalid
        //TODO: fix after HF28 - see HIVE_HARDFORK_1_28_FIX_POWER_DOWN_CANCEL

        for( uint32_t i = 0; i < HIVE_MAX_PROXY_RECURSION_DEPTH; ++i )
          a.proxied_vsf_votes[i] *= magnitude;
      } );
      if (old_vesting_shares != new_vesting_shares)
        push_virtual_operation( vesting_shares_split_operation(account.get_name(), old_vesting_shares, new_vesting_shares) );
    }

    const auto& comments = get_index< comment_cashout_index >().indices();
    for( const auto& comment : comments )
    {
      modify( comment, [&]( comment_cashout_object& c )
      {
        //c.net_rshares *= magnitude;
        //c.vote_rshares *= magnitude;
        c.accumulate_vote_rshares( c.get_net_rshares() * ( magnitude - 1 ), c.get_vote_rshares() * ( magnitude - 1 ) );
      } );
      modify( *find_comment_cashout_ex( comment.get_comment_id() ), [&]( comment_cashout_ex_object& c_ex )
      {
        //c_ex.abs_rshares *= magnitude;
        c_ex.accumulate_abs_rshares( c_ex.get_abs_rshares() * ( magnitude - 1 ) );
      } );
    }

    for( const auto& c : comments )
    {
      if( c.get_net_rshares() > 0 )
        adjust_rshares2( 0, util::evaluate_reward_curve( c.get_net_rshares() ) );
    }

  }
  FC_CAPTURE_AND_RETHROW()
}

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

