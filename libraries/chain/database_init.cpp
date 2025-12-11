#include <hive/chain/hive_fwd.hpp>

#include <hive/chain/database.hpp>
#include <hive/chain/hive_evaluator.hpp>
#include <hive/chain/evaluator_registry.hpp>
#include <hive/chain/custom_operation_interpreter.hpp>
#include <hive/chain/witness_schedule.hpp>
#include <hive/chain/account_object_multiindex.hpp>
#include <hive/chain/block_summary_object_multiindex.hpp>
#include <hive/chain/hive_objects_multiindex.hpp>
#include <hive/chain/witness_objects_multiindex.hpp>
#include <hive/chain/smt_objects.hpp>

#include <hive/chain/util/rd_setup.hpp>
#include <hive/chain/util/state_checker_tools.hpp>

#include <hive/protocol/get_config.hpp>

#include "database_impl.hpp"

namespace hive { namespace chain {

void database::initialize_evaluators()
{
  register_social_evaluators( _my->_evaluator_registry );
  register_transfer_evaluators( _my->_evaluator_registry );
  register_account_evaluators( _my->_evaluator_registry );
  register_witness_evaluators( _my->_evaluator_registry );
  register_dhf_evaluators( _my->_evaluator_registry );
#ifdef HIVE_ENABLE_SMT
  register_smt_evaluators( _my->_evaluator_registry );
#endif

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
  FC_ASSERT(_my->_decoded_types_data_storage && "No storage - no types");
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

    create< account_object >( HIVE_NULL_ACCOUNT, HIVE_GENESIS_TIME );
    create< account_authority_object >( [&]( account_authority_object& auth )
    {
      auth.account = HIVE_NULL_ACCOUNT;
      auth.owner.weight_threshold = 1;
      auth.active.weight_threshold = 1;
      auth.posting.weight_threshold = 1;
    });

#if defined(IS_TEST_NET) || defined(HIVE_CONVERTER_ICEBERG_PLUGIN_ENABLED)
    create< account_object >( OBSOLETE_TREASURY_ACCOUNT, HIVE_GENESIS_TIME );
    create< account_authority_object >([&](account_authority_object& auth)
    {
      auth.account = OBSOLETE_TREASURY_ACCOUNT;
      auth.owner.weight_threshold = 1;
      auth.active.weight_threshold = 1;
      auth.posting.weight_threshold = 1;
    } );
    create< account_object >( NEW_HIVE_TREASURY_ACCOUNT, HIVE_GENESIS_TIME );
    create< account_authority_object >([&](account_authority_object& auth)
    {
      auth.account = NEW_HIVE_TREASURY_ACCOUNT;
      auth.owner.weight_threshold = 1;
      auth.active.weight_threshold = 1;
      auth.posting.weight_threshold = 1;
    } );
#endif

    create< account_object >( HIVE_TEMP_ACCOUNT, HIVE_GENESIS_TIME );
    create< account_authority_object >( [&]( account_authority_object& auth )
    {
      auth.account = HIVE_TEMP_ACCOUNT;
      auth.owner.weight_threshold = 0;
      auth.active.weight_threshold = 0;
      auth.posting.weight_threshold = 0;
    });

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
        a.balance = HIVE_asset( HIVE_INIT_SUPPLY ) - to_vest;
        a.hbd_balance = HBD_asset( HIVE_HBD_INIT_SUPPLY );
        a.vesting_shares = initial_vests;
        FC_ASSERT( a.balance.amount >= 0 && a.hbd_balance.amount >= 0 && a.vesting_shares.amount >= 0, "Invalid testnet configuration" );
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

} } // hive::chain
