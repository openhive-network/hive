
#include <hive/chain/hive_fwd.hpp>

#include <hive/plugins/block_data_export/block_data_export_plugin.hpp>

#include <hive/plugins/rc/rc_config.hpp>
#include <hive/plugins/rc/rc_curve.hpp>
#include <hive/plugins/rc/rc_export_objects.hpp>
#include <hive/plugins/rc/rc_plugin.hpp>
#include <hive/plugins/rc/rc_objects.hpp>

#include <hive/chain/account_object.hpp>
#include <hive/chain/database.hpp>
#include <hive/chain/database_exceptions.hpp>
#include <hive/chain/index.hpp>

#include <hive/jsonball/jsonball.hpp>

#include <boost/algorithm/string.hpp>

#ifndef IS_TEST_NET
#define HIVE_HF20_BLOCK_NUM 26256743
#endif

namespace hive { namespace plugins { namespace rc {

using hive::plugins::block_data_export::block_data_export_plugin;

namespace detail {

using chain::plugin_exception;
using hive::chain::util::manabar_params;

class rc_plugin_impl
{
  public:
    rc_plugin_impl( rc_plugin& _plugin ) :
      _db( appbase::app().get_plugin< hive::plugins::chain::chain_plugin >().db() ),
      _self( _plugin )
    {
      _skip.skip_reject_not_enough_rc = 0;
      _skip.skip_deduct_rc = 0;
      _skip.skip_negative_rc_balance = 0;
      _skip.skip_reject_unknown_delta_vests = 1;
    }

    void on_pre_reindex( const reindex_notification& node );
    void on_post_reindex( const reindex_notification& note );
    void on_post_apply_block( const block_notification& note );
    //void on_pre_apply_transaction( const transaction_notification& note );
    void on_post_apply_transaction( const transaction_notification& note );
    void on_pre_apply_operation( const operation_notification& note );
    void on_post_apply_operation( const operation_notification& note );
    void on_pre_apply_optional_action( const optional_action_notification& note );
    void on_post_apply_optional_action( const optional_action_notification& note );

    void on_first_block();
    void validate_database();

    bool before_first_block()
    {
      return (_db.count< rc_account_object >() == 0);
    }

    int64_t calculate_cost_of_resources( int64_t total_vests, rc_info& usage_info );

    database&                     _db;
    rc_plugin&                    _self;

    rc_plugin_skip_flags          _skip;
    std::map< account_name_type, int64_t > _account_to_max_rc;
    uint32_t                      _enable_at_block = 1;

#ifdef IS_TEST_NET
    std::set< account_name_type > _whitelist;
#endif

    boost::signals2::connection   _pre_reindex_conn;
    boost::signals2::connection   _post_reindex_conn;
    boost::signals2::connection   _post_apply_block_conn;
    boost::signals2::connection   _pre_apply_transaction_conn;
    boost::signals2::connection   _post_apply_transaction_conn;
    boost::signals2::connection   _pre_apply_operation_conn;
    boost::signals2::connection   _post_apply_operation_conn;
    boost::signals2::connection   _pre_apply_optional_action_conn;
    boost::signals2::connection   _post_apply_optional_action_conn;
};

inline int64_t get_next_vesting_withdrawal( const account_object& account )
{
  int64_t total_left = account.to_withdraw.value - account.withdrawn.value;
  int64_t withdraw_per_period = account.vesting_withdraw_rate.amount.value;
  int64_t next_withdrawal = (withdraw_per_period <= total_left) ? withdraw_per_period : total_left;
  bool is_done = (account.next_vesting_withdrawal == fc::time_point_sec::maximum());
  return is_done ? 0 : next_withdrawal;
}

template< bool account_may_exist = false >
void create_rc_account( database& db, uint32_t now, const account_object& account, asset max_rc_creation_adjustment )
{
  // ilog( "create_rc_account( ${a} )", ("a", account.name) );
  if( account_may_exist )
  {
    const rc_account_object* rc_account = db.find< rc_account_object, by_name >( account.name );
    if( rc_account != nullptr )
      return;
  }

  if( max_rc_creation_adjustment.symbol == HIVE_SYMBOL )
  {
    const dynamic_global_property_object& gpo = db.get_dynamic_global_properties();
    max_rc_creation_adjustment = max_rc_creation_adjustment * gpo.get_vesting_share_price();
  }
  else if( max_rc_creation_adjustment.symbol == VESTS_SYMBOL )
  {
    // This occurs naturally when rc_account is initialized, so don't logspam
    // wlog( "Encountered max_rc_creation_adjustment.symbol == VESTS_SYMBOL creating account ${acct}", ("acct", account.name) );
  }
  else
  {
    elog( "Encountered unknown max_rc_creation_adjustment creating account ${acct}", ("acct", account.name) );
    max_rc_creation_adjustment = asset( 0, VESTS_SYMBOL );
  }

  db.create< rc_account_object >( [&]( rc_account_object& rca )
  {
    rca.account = account.name;
    rca.rc_manabar.last_update_time = now;
    rca.max_rc_creation_adjustment = max_rc_creation_adjustment;
    int64_t max_rc = get_maximum_rc( account, rca );
    rca.rc_manabar.current_mana = max_rc;
    rca.last_max_rc = max_rc;
  } );
}

template< bool account_may_exist = false >
void create_rc_account( database& db, uint32_t now, const account_name_type& account_name, asset max_rc_creation_adjustment )
{
  const account_object& account = db.get< account_object, by_name >( account_name );
  create_rc_account< account_may_exist >( db, now, account, max_rc_creation_adjustment );
}

std::vector< std::pair< int64_t, account_name_type > > dump_all_accounts( const database& db )
{
  std::vector< std::pair< int64_t, account_name_type > > result;
  const auto& idx = db.get_index< account_index >().indices().get< by_id >();
  for( auto it=idx.begin(); it!=idx.end(); ++it )
  {
    result.emplace_back( it->get_id(), it->name );
  }

  return result;
}

std::vector< std::pair< int64_t, account_name_type > > dump_all_rc_accounts( const database& db )
{
  std::vector< std::pair< int64_t, account_name_type > > result;
  const auto& idx = db.get_index< rc_account_index >().indices().get< by_id >();
  for( auto it=idx.begin(); it!=idx.end(); ++it )
  {
    result.emplace_back( it->get_id(), it->account );
  }

  return result;
}

struct get_resource_user_visitor
{
  typedef account_name_type result_type;

  get_resource_user_visitor() {}

  account_name_type operator()( const witness_set_properties_operation& op )const
  {
    return op.owner;
  }

  account_name_type operator()( const recover_account_operation& op )const
  {
    for( const auto& account_weight : op.new_owner_authority.account_auths )
      return account_weight.first;
    for( const auto& account_weight : op.recent_owner_authority.account_auths )
      return account_weight.first;
    return op.account_to_recover;
  }

  template< typename Op >
  account_name_type operator()( const Op& op )const
  {
    flat_set< account_name_type > req;
    op.get_required_active_authorities( req );
    for( const account_name_type& account : req )
      return account;
    op.get_required_owner_authorities( req );
    for( const account_name_type& account : req )
      return account;
    op.get_required_posting_authorities( req );
    for( const account_name_type& account : req )
      return account;
    return account_name_type();
  }
};

account_name_type get_resource_user( const signed_transaction& tx )
{
  get_resource_user_visitor vtor;

  for( const operation& op : tx.operations )
  {
    account_name_type resource_user = op.visit( vtor );
    if( resource_user != account_name_type() )
      return resource_user;
  }
  return account_name_type();
}

account_name_type get_resource_user( const optional_automated_action& action )
{
  get_resource_user_visitor vtor;

  return action.visit( vtor );
}

void use_account_rcs(
  database& db,
  const dynamic_global_property_object& gpo,
  const account_name_type& account_name,
  int64_t rc,
  rc_plugin_skip_flags skip
#ifdef IS_TEST_NET
  ,
  const set< account_name_type >& whitelist
#endif
  )
{

  if( account_name == account_name_type() )
  {
    if( db.is_producing() )
    {
      HIVE_ASSERT( false, plugin_exception,
        "Tried to execute transaction with no resource user",
        );
    }
    return;
  }

#ifdef IS_TEST_NET
  if( whitelist.count( account_name ) ) return;
#endif

  // ilog( "use_account_rcs( ${n}, ${rc} )", ("n", account_name)("rc", rc) );
  const account_object& account = db.get< account_object, by_name >( account_name );
  const rc_account_object& rc_account = db.get< rc_account_object, by_name >( account_name );

  manabar_params mbparams;
  mbparams.max_mana = get_maximum_rc( account, rc_account );
  mbparams.regen_time = HIVE_RC_REGEN_TIME;

  try{

  db.modify( rc_account, [&]( rc_account_object& rca )
  {
    rca.rc_manabar.regenerate_mana< true >( mbparams, gpo.time.sec_since_epoch() );

    bool has_mana = rca.rc_manabar.has_mana( rc );

    if( (!skip.skip_reject_not_enough_rc) && db.has_hardfork( HIVE_HARDFORK_0_20 ) )
    {
      if( db.is_producing() )
      {
        HIVE_ASSERT( has_mana, plugin_exception,
          "Account: ${account} has ${rc_current} RC, needs ${rc_needed} RC. Please wait to transact, or power up HIVE.",
          ("account", account_name)
          ("rc_needed", rc)
          ("rc_current", rca.rc_manabar.current_mana)
          );
      }
      else
      {
        if( !has_mana )
        {
          const dynamic_global_property_object& gpo = db.get_dynamic_global_properties();
          ilog( "Accepting transaction by ${account}, has ${rc_current} RC, needs ${rc_needed} RC, block ${b}, witness ${w}.",
            ("account", account_name)
            ("rc_needed", rc)
            ("rc_current", rca.rc_manabar.current_mana)
            ("b", gpo.head_block_number)
            ("w", gpo.current_witness)
            );
        }
      }
    }

    if( (!has_mana) && ( skip.skip_negative_rc_balance || (gpo.time.sec_since_epoch() <= 1538211600) ) )
      return;

    if( skip.skip_deduct_rc )
      return;

    int64_t min_mana = -1 * ( HIVE_RC_MAX_NEGATIVE_PERCENT * mbparams.max_mana ) / HIVE_100_PERCENT;

    rca.rc_manabar.use_mana( rc, min_mana );
  } );
  }FC_CAPTURE_AND_RETHROW( (account)(rc_account)(mbparams.max_mana) )
}

int64_t rc_plugin_impl::calculate_cost_of_resources( int64_t total_vests, rc_info& usage_info )
{
  // How many RC does this transaction cost?
  const rc_resource_param_object& params_obj = _db.get< rc_resource_param_object, by_id >( rc_resource_param_id_type() );
  const rc_pool_object& pool_obj = _db.get< rc_pool_object, by_id >( rc_pool_id_type() );

  int64_t rc_regen = ( total_vests / ( HIVE_RC_REGEN_TIME / HIVE_BLOCK_INTERVAL ) );

  int64_t total_cost = 0;

  // When rc_regen is 0, everything is free
  if( rc_regen > 0 )
  {
    for( size_t i = 0; i < HIVE_RC_NUM_RESOURCE_TYPES; ++i )
    {
      const rc_resource_params& params = params_obj.resource_param_array[i];
      int64_t pool = pool_obj.pool_array[i];

      usage_info.usage.resource_count[i] *= int64_t( params.resource_dynamics_params.resource_unit );
      usage_info.cost[i] = compute_rc_cost_of_resource( params.price_curve_params, pool, usage_info.usage.resource_count[i], rc_regen );
      total_cost += usage_info.cost[i];
    }
  }

  return total_cost;
}

void rc_plugin_impl::on_post_apply_transaction( const transaction_notification& note )
{ try {
  if( before_first_block() )
    return;

  const dynamic_global_property_object& gpo = _db.get_dynamic_global_properties();

  rc_transaction_info tx_info;

  // How many resources does the transaction use?
  count_resources( note.transaction, tx_info.usage );

  // How many RC does this transaction cost?
  int64_t total_cost = calculate_cost_of_resources( gpo.total_vesting_shares.amount.value, tx_info );

  // Who pays the cost?
  tx_info.resource_user = get_resource_user( note.transaction );
  use_account_rcs( _db, gpo, tx_info.resource_user, total_cost, _skip
#ifdef IS_TEST_NET
  ,
  _whitelist
#endif
  );

  std::shared_ptr< exp_rc_data > export_data =
    hive::plugins::block_data_export::find_export_data< exp_rc_data >( HIVE_RC_PLUGIN_NAME );
  if( export_data )
  {
    export_data->tx_info.push_back( tx_info );
  }
  else if( ( ( gpo.head_block_number + 1 ) % HIVE_BLOCKS_PER_DAY) == 0 )
  {
    //correction for head block number is to counter the fact that transactions are handled before block switches to
    //new one and therefore it is different for transactions and for block they are processed in (the effect was that
    //similar condition for automatic actions/block was triggering debug print for different block than here; now
    //both places print in the same moment, so you can see connection between data from here and there)
    dlog( "${b} : ${i}", ("b", gpo.head_block_number+1)("i", tx_info) );
  }
} FC_CAPTURE_AND_RETHROW( (note.transaction) ) }

struct block_extensions_count_resources_visitor
{
  typedef void result_type;

  count_resources_result& _r;
  block_extensions_count_resources_visitor( count_resources_result& r ) : _r( r ) {}

  // Only optional actions need to be counted. We decided in design that
  // the operation should pay the cost for any required actions created
  // as a result.
  void operator()( const optional_automated_actions& opt_actions )
  {
    for( const auto& a : opt_actions )
    {
      count_resources( a, _r );
    }
  }

  template< typename T >
  void operator()( const T& ) {}
};

void rc_plugin_impl::on_post_apply_block( const block_notification& note )
{ try{
  const dynamic_global_property_object& gpo = _db.get_dynamic_global_properties();
  if( before_first_block() )
  {
    if( gpo.head_block_number == _enable_at_block )
      on_first_block();
    else
      return;
  }

  /*
  const auto& idx = _db.get_index< account_index >().indices().get< by_name >();

  ilog( "\n\n************************************************************************" );
  ilog( "Block ${b}", ("b", note.block_num) );
  for( const account_object& acct : idx )
  {
    auto it = _account_to_max_rc.find( acct.name );
    const rc_account_object& rc_account = _db.get< rc_account_object, by_name >( acct.name );
    int64_t max_rc = get_maximum_rc( acct, rc_account );
    if( it == _account_to_max_rc.end() )
    {
      ilog( "NEW ${n} : ${v}", ("n", acct.name)("v", max_rc) );
      _account_to_max_rc.emplace( acct.name, max_rc );
    }
    else if( max_rc != it->second )
    {
      ilog( "${n} : ${v}", ("n", acct.name)("v", max_rc) );
      _account_to_max_rc[ acct.name ] = max_rc;
    }
  }
  for( const signed_transaction& tx : note.block.transactions )
  {
    ilog( "${tx}", ("tx", tx) );
  }
  */

  if( gpo.total_vesting_shares.amount <= 0 )
  {
    return;
  }

  auto now = _db.head_block_time();

  // How many resources did transactions use?
  count_resources_result count;
  for( const signed_transaction& tx : note.block.transactions )
  {
    count_resources( tx, count );
  }

  block_extensions_count_resources_visitor ext_visitor( count );
  for( const auto& e : note.block.extensions )
  {
    e.visit( ext_visitor );
  }

  const witness_schedule_object& wso = _db.get_witness_schedule_object();
  const rc_resource_param_object& params_obj = _db.get< rc_resource_param_object, by_id >( rc_resource_param_id_type() );

  rc_block_info block_info;
  block_info.regen = ( gpo.total_vesting_shares.amount.value / ( HIVE_RC_REGEN_TIME / HIVE_BLOCK_INTERVAL ) );

  if( params_obj.resource_param_array[ resource_new_accounts ].resource_dynamics_params !=
      wso.account_subsidy_rd )
  {
    dlog( "Copying changed subsidy params from consensus in block ${b}", ("b", gpo.head_block_number) );
    _db.modify( params_obj, [&]( rc_resource_param_object& p )
    {
      p.resource_param_array[ resource_new_accounts ].resource_dynamics_params = wso.account_subsidy_rd;
      // Hardcoded to use default values of rc_curve_gen_params() fields for now
      generate_rc_curve_params(
        p.resource_param_array[ resource_new_accounts ].price_curve_params,
        p.resource_param_array[ resource_new_accounts ].resource_dynamics_params,
        rc_curve_gen_params() );
    } );
  }

  const auto& bucket_idx = _db.get_index< rc_usage_bucket_index >().indices().get< by_timestamp >();
  const auto* active_bucket = &( *bucket_idx.rbegin() );
  bool reset_bucket = time_point_sec( active_bucket->get_timestamp() + fc::seconds( HIVE_RC_BUCKET_TIME_LENGTH ) ) <= now;
  if( reset_bucket )
    active_bucket = &( *bucket_idx.begin() );

  bool debug_print = ( ( gpo.head_block_number % HIVE_BLOCKS_PER_DAY ) == 0 );
  _db.modify( _db.get< rc_pool_object, by_id >( rc_pool_id_type() ), [&]( rc_pool_object& pool_obj )
  {
    for( size_t i=0; i<HIVE_RC_NUM_RESOURCE_TYPES; i++ )
    {
      const rd_dynamics_params& params = params_obj.resource_param_array[i].resource_dynamics_params;
      int64_t& pool = pool_obj.pool_array[i];

      block_info.pool[i] = pool;
      block_info.pool_share[i] = pool_obj.count_share(i);
      block_info.budget[i] = params.budget_per_time_unit;
      block_info.usage[i] = count.resource_count[i]*int64_t( params.resource_unit );
      block_info.decay[i] = rd_compute_pool_decay( params.decay_params, pool - block_info.usage[i], 1 );
      //update global usage statistics
      if( reset_bucket )
        pool_obj.add_usage( i, -active_bucket->get_usage(i) );
      pool_obj.add_usage( i, block_info.usage[i] );

      int64_t new_pool = pool - block_info.decay[i] + block_info.budget[i] - block_info.usage[i];
      pool = new_pool;

      if( i == resource_new_accounts )
      {
        int64_t new_consensus_pool = gpo.available_account_subsidies;
        block_info.new_accounts_adjustment = new_consensus_pool - new_pool;
        if( block_info.new_accounts_adjustment != 0 )
        {
          ilog( "resource_new_accounts adjustment on block ${b}: ${a}",
            ("a", block_info.new_accounts_adjustment)("b", gpo.head_block_number) );
          pool += block_info.new_accounts_adjustment;
        }
      }

      /*
      if( debug_print )
      {
        double k = 27.027027027027028;
        double a = double(params.pool_eq - pool);
        a /= k*double(pool);
        dlog( "a=${a}   aR=${aR}", ("a", a)("aR", a*gpo.total_vesting_shares.amount.value/HIVE_RC_REGEN_TIME) );
      }
      */
    }
    pool_obj.recalculate_resource_weights( params_obj );
  } );

  _db.modify( *active_bucket, [&]( rc_usage_bucket_object& bucket )
  {
    if( reset_bucket )
      bucket.reset( now ); //contents of bucket being reset was already subtracted from globals above
    for( int i = 0; i < HIVE_RC_NUM_RESOURCE_TYPES; ++i )
      bucket.add_usage( i, block_info.usage[i] );
  } );

  std::shared_ptr< exp_rc_data > export_data =
    hive::plugins::block_data_export::find_export_data< exp_rc_data >( HIVE_RC_PLUGIN_NAME );
  if( export_data )
    export_data->block_info = block_info;
  else if( debug_print )
    dlog( "${b} : ${i}", ( "b", gpo.head_block_number )( "i", block_info ) );
} FC_CAPTURE_AND_RETHROW( (note.block) ) }

void rc_plugin_impl::on_first_block()
{
  // Only now enable exporting data - if it was enabled during plugin init it would produce tons of empty data
  block_data_export_plugin* export_plugin = appbase::app().find_plugin< block_data_export_plugin >();
  if( export_plugin != nullptr )
  {
    ilog( "Registering RC export data factory" );
    export_plugin->register_export_data_factory( HIVE_RC_PLUGIN_NAME,
      []() -> std::shared_ptr< exportable_block_data > { return std::make_shared< exp_rc_data >(); } );
  }

  // Initial values are located at `libraries/jsonball/data/resource_parameters.json`
  std::string resource_params_json = hive::jsonball::get_resource_parameters();
  fc::variant resource_params_var = fc::json::from_string( resource_params_json, fc::json::strict_parser );
  std::vector< std::pair< fc::variant, std::pair< fc::variant_object, fc::variant_object > > > resource_params_pairs;
  fc::from_variant( resource_params_var, resource_params_pairs );
  fc::time_point_sec now = _db.get_dynamic_global_properties().time;

  _db.create< rc_resource_param_object >(
    [&]( rc_resource_param_object& params_obj )
    {
      for( auto& kv : resource_params_pairs )
      {
        auto k = kv.first.as< rc_resource_types >();
        fc::variant_object& vo = kv.second.first;
        fc::mutable_variant_object mvo(vo);
        fc::from_variant( fc::variant( mvo ), params_obj.resource_param_array[ k ] );
      }

      dlog( "Genesis params_obj is ${o}", ("o", params_obj) );
    } );

  const rc_resource_param_object& params_obj = _db.get< rc_resource_param_object, by_id >( rc_resource_param_id_type() );

  //create usage statistics buckets (empty, but with proper timestamps, last bucket has current timestamp)
  time_point_sec timestamp = now - fc::seconds( HIVE_RC_BUCKET_TIME_LENGTH * ( HIVE_RC_WINDOW_BUCKET_COUNT - 1 ) );
  for( int i = 0; i < HIVE_RC_WINDOW_BUCKET_COUNT; ++i )
  {
    _db.create< rc_usage_bucket_object >( timestamp );
    timestamp += fc::seconds( HIVE_RC_BUCKET_TIME_LENGTH );
  }

  const auto& pool_obj = _db.create< rc_pool_object >( params_obj, resource_count_type() );
  ilog( "Genesis pool_obj is ${o}", ("o", pool_obj) );

  const auto& idx = _db.get_index< account_index >().indices().get< by_id >();
  for( auto it=idx.begin(); it!=idx.end(); ++it )
  {
    create_rc_account( _db, now.sec_since_epoch(), *it, asset( HIVE_RC_HISTORICAL_ACCOUNT_CREATION_ADJUSTMENT, VESTS_SYMBOL ) );
  }
}

struct get_worker_name_visitor
{
  typedef account_name_type result_type;

  template< typename WorkType >
  account_name_type operator()( const WorkType& work )
  {   return work.input.worker_account;    }
};

account_name_type get_worker_name( const pow2_work& work )
{
  // Even though in both cases the result is work.input.worker_account,
  // we have to use a visitor because pow2_work is a static_variant
  get_worker_name_visitor vtor;
  return work.visit( vtor );
}

//
// This visitor performs the following functions:
//
// - Call regenerate() when an account's vesting shares are about to change
// - Save regenerated account names in a local array for further (post-operation) processing
//
struct pre_apply_operation_visitor
{
  typedef void result_type;

  database&                                _db;
  uint32_t                                 _current_time = 0;
  uint32_t                                 _current_block_number = 0;
  account_name_type                        _current_witness;
  fc::optional< price >                    _vesting_share_price;
  rc_plugin_skip_flags                     _skip;

  pre_apply_operation_visitor( database& db ) : _db(db)
  {
    const auto& gpo = _db.get_dynamic_global_properties();
    _current_time = gpo.time.sec_since_epoch();
    _current_block_number = gpo.head_block_number;
  }

  void regenerate( const account_object& account, const rc_account_object& rc_account )const
  {
    //
    // Since RC tracking is non-consensus, we must rely on consensus to forbid
    // transferring / delegating VESTS that haven't regenerated voting power.
    //
    // TODO:  Issue number
    //
    static_assert( HIVE_RC_REGEN_TIME <= HIVE_VOTING_MANA_REGENERATION_SECONDS, "RC regen time must be smaller than vote regen time" );

    // ilog( "regenerate(${a})", ("a", account.name) );

    manabar_params mbparams;
    mbparams.max_mana = get_maximum_rc( account, rc_account );
    mbparams.regen_time = HIVE_RC_REGEN_TIME;

    try {

    if( mbparams.max_mana != rc_account.last_max_rc )
    {
      if( !_skip.skip_reject_unknown_delta_vests )
      {
        HIVE_ASSERT( false, plugin_exception,
          "Account ${a} max RC changed from ${old} to ${new} without triggering an op, noticed on block ${b}",
          ("a", account.name)("old", rc_account.last_max_rc)("new", mbparams.max_mana)("b", _db.head_block_num()) );
      }
      else
      {
        wlog( "NOTIFYALERT! Account ${a} max RC changed from ${old} to ${new} without triggering an op, noticed on block ${b}",
          ("a", account.name)("old", rc_account.last_max_rc)("new", mbparams.max_mana)("b", _db.head_block_num()) );
      }
    }

    _db.modify( rc_account, [&]( rc_account_object& rca )
    {
      rca.rc_manabar.regenerate_mana< true >( mbparams, _current_time );
    } );
    } FC_CAPTURE_AND_RETHROW( (account)(rc_account)(mbparams.max_mana) )
  }

  template< bool account_may_not_exist = false >
  void regenerate( const account_name_type& name )const
  {
    const account_object* account = _db.find< account_object, by_name >( name );
    if( account_may_not_exist )
    {
      if( account == nullptr )
        return;
    }
    else
    {
      FC_ASSERT( account != nullptr, "Unexpectedly, account ${a} does not exist", ("a", name) );
    }

    const rc_account_object* rc_account = _db.find< rc_account_object, by_name >( name );
    FC_ASSERT( rc_account != nullptr, "Unexpectedly, rc_account ${a} does not exist", ("a", name) );
    try{
    regenerate( *account, *rc_account );
    } FC_CAPTURE_AND_RETHROW( (*account)(*rc_account) )
  }

  void operator()( const account_create_with_delegation_operation& op )const
  {
    regenerate( op.creator );
  }

  void operator()( const transfer_to_vesting_operation& op )const
  {
    account_name_type target = op.to.size() ? op.to : op.from;
    regenerate( target );
  }

  void operator()( const withdraw_vesting_operation& op )const
  {
    regenerate( op.account );
  }

  void operator()( const set_withdraw_vesting_route_operation& op )const
  {
    regenerate( op.from_account );
  }

  void operator()( const delegate_vesting_shares_operation& op )const
  {
    regenerate( op.delegator );
    regenerate( op.delegatee );
  }

  void operator()( const author_reward_operation& op )const
  {
    regenerate( op.author );
  }

  void operator()( const curation_reward_operation& op )const
  {
    regenerate( op.curator );
  }

  // Is this one actually necessary?
  void operator()( const comment_reward_operation& op )const
  {
    regenerate( op.author );
  }

  void operator()( const fill_vesting_withdraw_operation& op )const
  {
    regenerate( op.from_account );
    regenerate( op.to_account );
  }

  void operator()( const claim_reward_balance_operation& op )const
  {
    regenerate( op.account );
  }

#ifdef HIVE_ENABLE_SMT
  void operator()( const claim_reward_balance2_operation& op )const
  {
    regenerate( op.account );
  }
#endif

  void operator()( const hardfork_operation& op )const
  {
    if( op.hardfork_id == HIVE_HARDFORK_0_1 )
    {
      const auto& idx = _db.get_index< account_index >().indices().get< by_id >();
      for( auto it=idx.begin(); it!=idx.end(); ++it )
      {
        regenerate( it->name );
      }
    }
  }

  void operator()( const return_vesting_delegation_operation& op )const
  {
    regenerate( op.account );
  }

  void operator()( const comment_benefactor_reward_operation& op )const
  {
    regenerate( op.benefactor );
  }

  void operator()( const producer_reward_operation& op )const
  {
    regenerate( op.producer );
  }

  void operator()( const clear_null_account_balance_operation& op )const
  {
    regenerate( HIVE_NULL_ACCOUNT );
  }

  //void operator()( const consolidate_treasury_balance_operation& op )const //not needed for treasury accounts, leave default

  void operator()( const delayed_voting_operation& op )const
  {
    regenerate( op.voter );
  }

  void operator()( const pow_operation& op )const
  {
    regenerate< true >( op.worker_account );
    regenerate< false >( _current_witness );
  }

  void operator()( const pow2_operation& op )const
  {
    regenerate< true >( get_worker_name( op.work ) );
    regenerate< false >( _current_witness );
  }

  void operator()( const create_proposal_operation& op )const
  {
    regenerate( op.creator );
    regenerate( op.receiver );
  }

  void operator()( const update_proposal_operation& op )const
  {
    regenerate( op.creator );
  }

  void operator()( const update_proposal_votes_operation& op )const
  {
    regenerate( op.voter );
  }

  void operator()( const remove_proposal_operation& op )const
  {
    regenerate( op.proposal_owner );
  }

  template< typename Op >
  void operator()( const Op& op )const {}
};

typedef pre_apply_operation_visitor pre_apply_optional_action_vistor;

struct account_regen_info
{
  account_regen_info( const account_name_type& a, bool r = true )
    : account_name(a), fill_new_mana(r) {}

  account_name_type         account_name;
  bool                      fill_new_mana = true;
};

struct post_apply_operation_visitor
{
  typedef void result_type;

  vector< account_regen_info >&            _mod_accounts;
  database&                                _db;
  uint32_t                                 _current_time = 0;
  uint32_t                                 _current_block_number = 0;
  account_name_type                        _current_witness;

  post_apply_operation_visitor( vector< account_regen_info >& ma, database& db )
    : _mod_accounts(ma), _db(db)
  {
    const auto& dgpo = _db.get_dynamic_global_properties();
    _current_time = dgpo.time.sec_since_epoch();
    _current_block_number = dgpo.head_block_number;
    _current_witness = dgpo.current_witness;
  }

  void operator()( const account_create_operation& op )const
  {
    create_rc_account( _db, _current_time, op.new_account_name, op.fee );
  }

  void operator()( const account_create_with_delegation_operation& op )const
  {
    create_rc_account( _db, _current_time, op.new_account_name, op.fee );
    _mod_accounts.emplace_back( op.creator );
  }

  void operator()( const create_claimed_account_operation& op )const
  {
    create_rc_account( _db, _current_time, op.new_account_name, _db.get_witness_schedule_object().median_props.account_creation_fee );
  }

  void operator()( const pow_operation& op )const
  {
    // ilog( "handling post-apply pow_operation" );
    create_rc_account< true >( _db, _current_time, op.worker_account, asset( 0, HIVE_SYMBOL ) );
    _mod_accounts.emplace_back( op.worker_account );
    _mod_accounts.emplace_back( _current_witness );
  }

  void operator()( const pow2_operation& op )const
  {
    auto worker_name = get_worker_name( op.work );
    create_rc_account< true >( _db, _current_time, worker_name, asset( 0, HIVE_SYMBOL ) );
    _mod_accounts.emplace_back( worker_name );
    _mod_accounts.emplace_back( _current_witness );
  }

  void operator()( const transfer_to_vesting_operation& op )
  {
    account_name_type target = op.to.size() ? op.to : op.from;
    _mod_accounts.emplace_back( target );
  }

  void operator()( const withdraw_vesting_operation& op )const
  {
    _mod_accounts.emplace_back( op.account, false );
  }

  void operator()( const delegate_vesting_shares_operation& op )const
  {
    _mod_accounts.emplace_back( op.delegator );
    _mod_accounts.emplace_back( op.delegatee );
  }

  void operator()( const author_reward_operation& op )const
  {
    _mod_accounts.emplace_back( op.author );
  }

  void operator()( const curation_reward_operation& op )const
  {
    _mod_accounts.emplace_back( op.curator );
  }

  // Is this one actually necessary?
  void operator()( const comment_reward_operation& op )const
  {
    _mod_accounts.emplace_back( op.author );
  }

  void operator()( const fill_vesting_withdraw_operation& op )const
  {
    _mod_accounts.emplace_back( op.from_account );
    _mod_accounts.emplace_back( op.to_account );
  }

  void operator()( const claim_reward_balance_operation& op )const
  {
    _mod_accounts.emplace_back( op.account );
  }

#ifdef HIVE_ENABLE_SMT
  void operator()( const claim_reward_balance2_operation& op )const
  {
    _mod_accounts.emplace_back( op.account );
  }
#endif

  void operator()( const hardfork_operation& op )const
  {
    if( op.hardfork_id == HIVE_HARDFORK_0_1 )
    {
      const auto& idx = _db.get_index< account_index >().indices().get< by_id >();
      for( auto it=idx.begin(); it!=idx.end(); ++it )
      {
        _mod_accounts.emplace_back( it->name );
      }
    }

    if( op.hardfork_id == HIVE_HARDFORK_0_20 )
    {
      const auto& params = _db.get< rc_resource_param_object, by_id >( rc_resource_param_id_type() );

      _db.modify( _db.get< rc_pool_object, by_id >( rc_pool_id_type() ), [&]( rc_pool_object& p )
      {
        for( size_t i = 0; i < HIVE_RC_NUM_RESOURCE_TYPES; i++ )
        {
          p.pool_array[ i ] = int64_t( params.resource_param_array[ i ].resource_dynamics_params.max_pool_size );
        }

        p.pool_array[ resource_new_accounts ] = 0;
      });
    }
  }

  void operator()( const return_vesting_delegation_operation& op )const
  {
    _mod_accounts.emplace_back( op.account );
  }

  void operator()( const comment_benefactor_reward_operation& op )const
  {
    _mod_accounts.emplace_back( op.benefactor );
  }

  void operator()( const producer_reward_operation& op )const
  {
    _mod_accounts.emplace_back( op.producer );
  }

  void operator()( const clear_null_account_balance_operation& op )const
  {
    _mod_accounts.emplace_back( HIVE_NULL_ACCOUNT );
  }

  //void operator()( const consolidate_treasury_balance_operation& op )const //not needed for treasury accounts, leave default

  void operator()( const delayed_voting_operation& op )const
  {
    _mod_accounts.emplace_back( op.voter );
  }

  void operator()( const create_proposal_operation& op )const
  {
    _mod_accounts.emplace_back( op.creator );
    _mod_accounts.emplace_back( op.receiver );
  }

  void operator()( const update_proposal_votes_operation& op )const
  {
    _mod_accounts.emplace_back( op.voter );
  }

  void operator()( const remove_proposal_operation& op )const
  {
    _mod_accounts.emplace_back( op.proposal_owner );
  }

  template< typename Op >
  void operator()( const Op& op )const
  {
    // ilog( "handling post-apply operation default" );
  }
};

typedef post_apply_operation_visitor post_apply_optional_action_visitor;



void rc_plugin_impl::on_pre_apply_operation( const operation_notification& note )
{ try {
  if( before_first_block() )
    return;

  const dynamic_global_property_object& gpo = _db.get_dynamic_global_properties();
  pre_apply_operation_visitor vtor( _db );

  // TODO: Add issue number to HF constant
  if( _db.has_hardfork( HIVE_HARDFORK_0_20 ) )
    vtor._vesting_share_price = gpo.get_vesting_share_price();

  vtor._current_witness = gpo.current_witness;
  vtor._skip = _skip;

  // ilog( "Calling pre-vtor on ${op}", ("op", note.op) );
  note.op.visit( vtor );
  } FC_CAPTURE_AND_RETHROW( (note.op) )
}

void update_modified_accounts( database& db, const std::vector< account_regen_info >& modified_accounts )
{
  for( const account_regen_info& regen_info : modified_accounts )
  {
    const account_object& account = db.get< account_object, by_name >( regen_info.account_name );
    const rc_account_object& rc_account = db.get< rc_account_object, by_name >( regen_info.account_name );

    int64_t new_last_max_rc = get_maximum_rc( account, rc_account );
    int64_t drc = new_last_max_rc - rc_account.last_max_rc;
    drc = regen_info.fill_new_mana ? drc : 0;

    db.modify( rc_account, [&]( rc_account_object& rca )
    {
      rca.last_max_rc = new_last_max_rc;
      rca.rc_manabar.current_mana += std::max( drc, int64_t( 0 ) );
    } );
  }
}

void rc_plugin_impl::on_post_apply_operation( const operation_notification& note )
{ try {
  if( before_first_block() )
    return;

  vector< account_regen_info > modified_accounts;

  // ilog( "Calling post-vtor on ${op}", ("op", note.op) );
  post_apply_operation_visitor vtor( modified_accounts, _db );
  note.op.visit( vtor );

  update_modified_accounts( _db, modified_accounts );
} FC_CAPTURE_AND_RETHROW( (note.op) ) }

void rc_plugin_impl::on_pre_apply_optional_action( const optional_action_notification& note )
{
  if( before_first_block() )
    return;

  const dynamic_global_property_object& gpo = _db.get_dynamic_global_properties();
  pre_apply_optional_action_vistor vtor( _db );

  vtor._current_witness = gpo.current_witness;
  vtor._skip = _skip;

  note.action.visit( vtor );
}

void rc_plugin_impl::on_post_apply_optional_action( const optional_action_notification& note )
{
  if( before_first_block() )
    return;

  const dynamic_global_property_object& gpo = _db.get_dynamic_global_properties();

  // There is no transaction equivalent for actions, so post apply transaction logic for actions goes here.
  vector< account_regen_info > modified_accounts;

  post_apply_optional_action_visitor vtor( modified_accounts, _db );
  note.action.visit( vtor );

  update_modified_accounts( _db, modified_accounts );

  rc_optional_action_info opt_action_info;

  // How many resources do the optional actions use?
  count_resources( note.action, opt_action_info.usage );

  // How many RC do these actions cost?
  int64_t total_cost = calculate_cost_of_resources( gpo.total_vesting_shares.amount.value, opt_action_info );

  // Who pays the cost?
  opt_action_info.resource_user = get_resource_user( note.action );
  use_account_rcs( _db, gpo, opt_action_info.resource_user, total_cost, _skip
#ifdef IS_TEST_NET
  ,
  _whitelist
#endif
  );

  std::shared_ptr< exp_rc_data > export_data =
    hive::plugins::block_data_export::find_export_data< exp_rc_data >( HIVE_RC_PLUGIN_NAME );
  if( (gpo.head_block_number % HIVE_BLOCKS_PER_DAY) == 0 )
  {
    dlog( "${b} : ${i}", ("b", gpo.head_block_number)("i", opt_action_info) );
  }
  if( export_data )
    export_data->opt_action_info.push_back( opt_action_info );
}

void rc_plugin_impl::validate_database()
{
  const auto& rc_idx = _db.get_index< rc_account_index >().indices().get< by_name >();

  for( const rc_account_object& rc_account : rc_idx )
  {
    const account_object& account = _db.get< account_object, by_name >( rc_account.account );
    int64_t max_rc = get_maximum_rc( account, rc_account );

    assert( max_rc == rc_account.last_max_rc );
    FC_ASSERT( max_rc == rc_account.last_max_rc,
      "Account ${a} max RC changed from ${old} to ${new} without triggering an op, noticed on block ${b} in validate_database()",
      ("a", account.name)("old", rc_account.last_max_rc)("new", max_rc)("b", _db.head_block_num()) );
  }
}

} // detail

rc_plugin::rc_plugin() {}
rc_plugin::~rc_plugin() {}

void rc_plugin::set_program_options( options_description& cli, options_description& cfg )
{
  cfg.add_options()
    ("rc-skip-reject-not-enough-rc", bpo::value<bool>()->default_value( false ), "Skip rejecting transactions when account has insufficient RCs. This is not recommended." )
    ("rc-compute-historical-rc", bpo::value<bool>()->default_value( false ), "Generate historical resource credits" )
#ifdef IS_TEST_NET
    ("rc-start-at-block", bpo::value<uint32_t>()->default_value(0), "Start calculating RCs at a specific block" )
    ("rc-account-whitelist", bpo::value< vector<string> >()->composing(), "Ignore RC calculations for the whitelist" )
#endif
    ;
  cli.add_options()
    ("rc-skip-reject-not-enough-rc", bpo::bool_switch()->default_value( false ), "Skip rejecting transactions when account has insufficient RCs. This is not recommended." )
    ("rc-compute-historical-rc", bpo::bool_switch()->default_value( false ), "Generate historical resource credits" )
#ifdef IS_TEST_NET
    ("rc-start-at-block", bpo::value<uint32_t>()->default_value(0), "Start calculating RCs at a specific block" )
    ("rc-account-whitelist", bpo::value< vector<string> >()->composing(), "Ignore RC calculations for the whitelist" )
#endif
    ;
}

void rc_plugin::plugin_initialize( const boost::program_options::variables_map& options )
{
  ilog( "Initializing resource credit plugin" );

  my = std::make_unique< detail::rc_plugin_impl >( *this );

  try
  {
    chain::database& db = appbase::app().get_plugin< hive::plugins::chain::chain_plugin >().db();

    my->_post_apply_block_conn = db.add_post_apply_block_handler( [&]( const block_notification& note )
      { try { my->on_post_apply_block( note ); } FC_LOG_AND_RETHROW() }, *this, 0 );
    //my->_pre_apply_transaction_conn = db.add_pre_apply_transaction_handler( [&]( const transaction_notification& note )
    //   { try { my->on_pre_apply_transaction( note ); } FC_LOG_AND_RETHROW() }, *this, 0 );
    my->_post_apply_transaction_conn = db.add_post_apply_transaction_handler( [&]( const transaction_notification& note )
      { try { my->on_post_apply_transaction( note ); } FC_LOG_AND_RETHROW() }, *this, 0 );
    my->_pre_apply_operation_conn = db.add_pre_apply_operation_handler( [&]( const operation_notification& note )
      { try { my->on_pre_apply_operation( note ); } FC_LOG_AND_RETHROW() }, *this, 0 );
    my->_post_apply_operation_conn = db.add_post_apply_operation_handler( [&]( const operation_notification& note )
      { try { my->on_post_apply_operation( note ); } FC_LOG_AND_RETHROW() }, *this, 0 );
    my->_pre_apply_optional_action_conn = db.add_pre_apply_optional_action_handler( [&]( const optional_action_notification& note )
      { try { my->on_pre_apply_optional_action( note ); } FC_LOG_AND_RETHROW() }, *this, 0 );
    my->_post_apply_optional_action_conn = db.add_post_apply_optional_action_handler( [&]( const optional_action_notification& note )
      { try { my->on_post_apply_optional_action( note ); } FC_LOG_AND_RETHROW() }, *this, 0 );

    HIVE_ADD_PLUGIN_INDEX(db, rc_resource_param_index);
    HIVE_ADD_PLUGIN_INDEX(db, rc_pool_index);
    HIVE_ADD_PLUGIN_INDEX(db, rc_account_index);
    HIVE_ADD_PLUGIN_INDEX(db, rc_usage_bucket_index);

    fc::mutable_variant_object state_opts;

    my->_skip.skip_reject_not_enough_rc = options.at( "rc-skip-reject-not-enough-rc" ).as< bool >();
    state_opts["rc-compute-historical-rc"] = options.at( "rc-compute-historical-rc" ).as<bool>();
#ifndef IS_TEST_NET
    if( !options.at( "rc-compute-historical-rc" ).as<bool>() )
    {
      my->_enable_at_block = HIVE_HF20_BLOCK_NUM;
    }
#else
    uint32_t start_block = options.at( "rc-start-at-block" ).as<uint32_t>();
    if( start_block > 0 )
    {
      my->_enable_at_block = start_block;
    }

    if( options.count( "rc-account-whitelist" ) > 0 )
    {
      auto accounts = options.at( "rc-account-whitelist" ).as< vector< string > > ();
      for( auto& arg : accounts )
      {
        vector< string > names;
        boost::split( names, arg, boost::is_any_of( " \t" ) );
        for( const std::string& name : names )
          my->_whitelist.insert( account_name_type( name ) );
      }

      ilog( "Ignoring RC's for accounts: ${w}", ("w", my->_whitelist) );
    }
#endif

    appbase::app().get_plugin< chain::chain_plugin >().report_state_options( name(), state_opts );

    ilog( "RC's will be computed starting at block ${b}", ("b", my->_enable_at_block) );
  }
  FC_CAPTURE_AND_RETHROW()
}

void rc_plugin::plugin_startup() {}

void rc_plugin::plugin_shutdown()
{
  chain::util::disconnect_signal( my->_pre_reindex_conn );
  chain::util::disconnect_signal( my->_post_reindex_conn );
  chain::util::disconnect_signal( my->_post_apply_block_conn );
  // chain::util::disconnect_signal( my->_pre_apply_transaction_conn );
  chain::util::disconnect_signal( my->_post_apply_transaction_conn );
  chain::util::disconnect_signal( my->_pre_apply_operation_conn );
  chain::util::disconnect_signal( my->_post_apply_operation_conn );
  chain::util::disconnect_signal( my->_pre_apply_optional_action_conn );
  chain::util::disconnect_signal( my->_post_apply_optional_action_conn );
}

void rc_plugin::set_rc_plugin_skip_flags( rc_plugin_skip_flags skip )
{
  my->_skip = skip;
}

const rc_plugin_skip_flags& rc_plugin::get_rc_plugin_skip_flags() const
{
  return my->_skip;
}

void rc_plugin::validate_database()
{
  my->validate_database();
}

exp_rc_data::exp_rc_data() {}
exp_rc_data::~exp_rc_data() {}

void exp_rc_data::to_variant( fc::variant& v )const
{
  fc::to_variant( *this, v );
}

int64_t get_maximum_rc( const account_object& account, const rc_account_object& rc_account )
{
  int64_t result = account.vesting_shares.amount.value;
  result = fc::signed_sat_sub( result, account.delegated_vesting_shares.amount.value );
  result = fc::signed_sat_add( result, account.received_vesting_shares.amount.value );
  result = fc::signed_sat_add( result, rc_account.max_rc_creation_adjustment.amount.value );
  result = fc::signed_sat_sub( result, detail::get_next_vesting_withdrawal( account ) );
  return result;
}

} } } // hive::plugins::rc
