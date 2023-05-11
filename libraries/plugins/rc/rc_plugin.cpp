
#include <hive/chain/hive_fwd.hpp>

#include <hive/plugins/block_data_export/block_data_export_plugin.hpp>
#include <hive/plugins/block_data_export/exportable_block_data.hpp>

#include <hive/protocol/config.hpp>
#include <hive/chain/rc/rc_curve.hpp>
#include <hive/chain/rc/rc_export_objects.hpp>
#include <hive/plugins/rc/rc_plugin.hpp>
#include <hive/chain/rc/rc_objects.hpp>
#include <hive/chain/rc/rc_operations.hpp>

#include <hive/chain/rc/resource_count.hpp>
#include <hive/chain/rc/resource_user.hpp>

#include <hive/chain/account_object.hpp>
#include <hive/chain/database.hpp>
#include <hive/chain/database_exceptions.hpp>
#include <hive/chain/generic_custom_operation_interpreter.hpp>
#include <hive/chain/index.hpp>
#include <hive/chain/util/type_registrar_definition.hpp>

#include <hive/chain/util/remove_guard.hpp>

#include <hive/jsonball/jsonball.hpp>

#include <boost/algorithm/string.hpp>
#include <cmath>

#ifndef IS_TEST_NET
#define HIVE_HF20_BLOCK_NUM 26256743
#endif

namespace hive { namespace plugins { namespace rc {

using hive::plugins::block_data_export::block_data_export_plugin;
using hive::plugins::block_data_export::exportable_block_data;
using namespace hive::chain;

struct exp_rc_data : public exportable_block_data
{
  exp_rc_data() {}
  virtual ~exp_rc_data() {}

  virtual void to_variant( fc::variant& v )const override
  {
    fc::to_variant( *this, v );
  }

  void add_tx_info( const rc_transaction_info& tx_info )
  {
    if( !txs.valid() )
      txs = std::vector< rc_transaction_info >();
    txs->emplace_back( tx_info );
  }
  void add_opt_action_info( const rc_optional_action_info& action_info )
  {
    if( !opt_actions.valid() )
      opt_actions = std::vector< rc_optional_action_info >();
    opt_actions->emplace_back( action_info );
  }

  rc_block_info                                      block;
  optional< std::vector< rc_transaction_info > >     txs;
  optional< std::vector< rc_optional_action_info > > opt_actions;
};

} } } // namespace hive::plugins::rc

FC_REFLECT( hive::plugins::rc::exp_rc_data,
  ( block )
  ( txs )
  ( opt_actions )
)

namespace hive { namespace plugins { namespace rc {

namespace detail {

using chain::plugin_exception;
using hive::chain::util::manabar_params;

class rc_plugin_impl
{
  public:
    typedef rc_plugin::report_type report_type;
    enum class report_output { DLOG, ILOG, NOTIFY };

    static void set_auto_report( const std::string& _option_type, const std::string& _option_output );
    static void set_auto_report( report_type _type, report_output _output )
    {
      auto_report_type = _type;
      auto_report_output = _output;
    }

    rc_plugin_impl( rc_plugin& _plugin ) :
      _db( appbase::app().get_plugin< hive::plugins::chain::chain_plugin >().db() ),
      _self( _plugin )
    {}

    void on_pre_reindex( const reindex_notification& node );
    void on_post_reindex( const reindex_notification& note );
    void on_pre_apply_block( const block_notification& note );
    void on_post_apply_block( const block_notification& note );
    void on_pre_apply_transaction( const transaction_notification& note );
    void on_post_apply_transaction( const transaction_notification& note );
    void on_pre_apply_operation( const operation_notification& note );
    void on_post_apply_operation( const operation_notification& note );
    void on_pre_apply_optional_action( const optional_action_notification& note );
    void on_post_apply_optional_action( const optional_action_notification& note );
    void on_pre_apply_custom_operation( const custom_operation_notification& note );
    void on_post_apply_custom_operation( const custom_operation_notification& note );

    template< typename OpType >
    void pre_apply_custom_op_type( const custom_operation_notification& note );
    template< typename OpType >
    void post_apply_custom_op_type( const custom_operation_notification& note );

    void on_first_block();
    void validate_database();

    bool before_first_block()
    {
      return (_db.count< rc_resource_param_object >() == 0);
    }

    fc::variant_object get_report( report_type rt, const rc_stats_object& stats ) const;

    void update_rc_for_custom_action( std::function<void()>&& callback, const account_name_type& account_name ) const;

    database&                     _db;
    rc_plugin&                    _self;

    std::map< account_name_type, int64_t > _account_to_max_rc;
    uint32_t                      _enable_at_block = 1;
    bool                          _enable_rc_stats = false; //needs to be false by default

    std::shared_ptr< generic_custom_operation_interpreter< hive::chain::rc_custom_operation > > _custom_operation_interpreter;

    boost::signals2::connection   _pre_reindex_conn;
    boost::signals2::connection   _post_reindex_conn;
    boost::signals2::connection   _pre_apply_block_conn;
    boost::signals2::connection   _post_apply_block_conn;
    boost::signals2::connection   _pre_apply_transaction_conn;
    boost::signals2::connection   _post_apply_transaction_conn;
    boost::signals2::connection   _pre_apply_operation_conn;
    boost::signals2::connection   _post_apply_operation_conn;
    boost::signals2::connection   _pre_apply_optional_action_conn;
    boost::signals2::connection   _post_apply_optional_action_conn;
    boost::signals2::connection   _pre_apply_custom_operation_conn;
    boost::signals2::connection   _post_apply_custom_operation_conn;

    static report_type auto_report_type; //type of automatic daily rc stats reports
    static report_output auto_report_output; //output of automatic daily rc stat reports
};

rc_plugin_impl::report_type rc_plugin_impl::auto_report_type = rc_plugin_impl::report_type::REGULAR;
rc_plugin_impl::report_output rc_plugin_impl::auto_report_output = rc_plugin_impl::report_output::ILOG;

void rc_plugin_impl::set_auto_report( const std::string& _option_type, const std::string& _option_output )
{
  if( _option_type == "NONE" )
    auto_report_type = report_type::NONE;
  else if( _option_type == "MINIMAL" )
    auto_report_type = report_type::MINIMAL;
  else if( _option_type == "REGULAR" )
    auto_report_type = report_type::REGULAR;
  else if( _option_type == "FULL" )
    auto_report_type = report_type::FULL;
  else
    FC_THROW_EXCEPTION( fc::parse_error_exception, "Unknown RC stats report type" );

  if( _option_output == "NOTIFY" )
    auto_report_output = report_output::NOTIFY;
  else if( _option_output == "ILOG" )
    auto_report_output = report_output::ILOG;
  else if( _option_output == "DLOG" )
    auto_report_output = report_output::DLOG;
  else
    FC_THROW_EXCEPTION( fc::parse_error_exception, "Unknown RC stats report output" );
}

void rc_plugin_impl::on_pre_apply_transaction( const transaction_notification& note )
{
  if( before_first_block() )
    return;

  _db.modify( _db.get< rc_pending_data, by_id >( rc_pending_data_id_type() ), [&]( rc_pending_data& data )
  {
    data.reset_differential_usage();
  } );
}

void rc_plugin_impl::on_post_apply_transaction( const transaction_notification& note )
{ try {
  if( before_first_block() )
    return;

  resource_credits rc( _db );
  const dynamic_global_property_object& gpo = _db.get_dynamic_global_properties();
  const auto& pending_data = _db.get< rc_pending_data, by_id >( rc_pending_data_id_type() );

  rc_transaction_info tx_info;
  // Initialize with (negative) usage for state that was updated by transaction
  tx_info.usage = pending_data.get_differential_usage();

  // How many resources does the transaction use?
  count_resources( note.transaction, note.full_transaction->get_transaction_size(), tx_info.usage, _db.head_block_time() );
  if( note.transaction.operations.size() == 1 )
    tx_info.op = note.transaction.operations.front().which();

  // How many RC does this transaction cost?
  int64_t total_cost = rc.compute_cost( &tx_info );
  note.full_transaction->set_rc_cost( total_cost );

  _db.modify( pending_data, [&]( rc_pending_data& data )
  {
    data.add_pending_usage( tx_info.usage, tx_info.cost );
  } );

  // Who pays the cost?
  tx_info.payer = get_resource_user( note.transaction );
  rc.use_account_rcs( &tx_info, total_cost );

  if( _enable_rc_stats && ( _db.is_validating_block() || _db.is_replaying_block() ) )
  {
    _db.modify( _db.get< rc_stats_object, by_id >( RC_PENDING_STATS_ID ), [&]( rc_stats_object& stats_obj )
    {
      stats_obj.add_stats( tx_info );
    } );
  }

  std::shared_ptr< exp_rc_data > export_data =
    hive::plugins::block_data_export::find_export_data< exp_rc_data >( HIVE_RC_PLUGIN_NAME );
  if( export_data )
    export_data->add_tx_info( tx_info );
} FC_CAPTURE_AND_RETHROW( (note.transaction) ) }

void rc_plugin_impl::on_pre_apply_block( const block_notification& note )
{
  if( before_first_block() )
    return;

  _db.modify( _db.get< rc_pending_data, by_id >( rc_pending_data_id_type() ), [&]( rc_pending_data& data )
  {
    data.reset_pending_usage();
  } );
}

void rc_plugin_impl::on_post_apply_block( const block_notification& note )
{ try{
  if( before_first_block() )
  {
    if( note.block_num >= _enable_at_block )
      on_first_block(); //all state will be ready for full processing cycle in next block
    return;
  }

  if( _db.has_hardfork( HIVE_HARDFORK_1_26 ) )
  {
    // delegations were introduced in HF26, so there is no point in checking them earlier;
    // also we are doing it in post apply block and not in pre, because otherwise transactions run
    // during block production would have different environment than when the block was applied
    resource_credits( _db ).handle_expired_delegations();
  }

  const dynamic_global_property_object& gpo = _db.get_dynamic_global_properties();
  if( gpo.total_vesting_shares.amount <= 0 )
  {
    return;
  }

  auto now = _db.head_block_time();
  const auto& pending_data = _db.get< rc_pending_data, by_id >( rc_pending_data_id_type() );
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

  const auto& rc_pool = _db.get< rc_pool_object, by_id >( rc_pool_id_type() );
  _db.modify( rc_pool, [&]( rc_pool_object& pool_obj )
  {
    bool budget_adjustment = false;
    for( size_t i=0; i<HIVE_RC_NUM_RESOURCE_TYPES; i++ )
    {
      const rd_dynamics_params& params = params_obj.resource_param_array[i].resource_dynamics_params;
      int64_t pool = pool_obj.get_pool(i);

      block_info.pool[i] = pool;
      block_info.share[i] = pool_obj.count_share(i);
      if( pool_obj.set_budget( i, params.budget_per_time_unit ) )
        budget_adjustment = true;
      block_info.usage[i] = pending_data.get_pending_usage()[i];
      block_info.cost[i] = pending_data.get_pending_cost()[i];
      block_info.decay[i] = rd_compute_pool_decay( params.decay_params, pool - block_info.usage[i], 1 );

      //update global usage statistics
      if( reset_bucket )
        pool_obj.add_usage( i, -active_bucket->get_usage(i) );
      pool_obj.add_usage( i, block_info.usage[i] );

      int64_t new_pool = pool - block_info.decay[i] + params.budget_per_time_unit - block_info.usage[i];

      if( i == resource_new_accounts )
      {
        int64_t new_consensus_pool = gpo.available_account_subsidies;
        if( new_consensus_pool != new_pool )
        {
          block_info.new_accounts_adjustment = new_consensus_pool - new_pool;
          ilog( "resource_new_accounts adjustment on block ${b}: ${a}",
            ("a", block_info.new_accounts_adjustment)("b", gpo.head_block_number) );
          new_pool = new_consensus_pool;
        }
      }

      pool_obj.set_pool( i, new_pool );
    }
    pool_obj.recalculate_resource_weights( params_obj );
    if( budget_adjustment )
      block_info.budget = pool_obj.get_last_known_budget();
  } );

  if( _enable_rc_stats && ( note.block_num % HIVE_BLOCKS_PER_DAY ) == 0 )
  {
    const auto& new_stats_obj = _db.get< rc_stats_object, by_id >( RC_PENDING_STATS_ID );
    if( auto_report_type != report_type::NONE && new_stats_obj.get_starting_block() )
    {
      fc::variant_object report = get_report( auto_report_type, new_stats_obj );
      switch( auto_report_output )
      {
      case report_output::NOTIFY:
        hive::notify( "RC stats", "rc_stats", report );
        break;
      case report_output::ILOG:
        ilog( "RC stats:${report}", ( report ) );
        break;
      default:
        dlog( "RC stats:${report}", ( report ) );
        break;
      };
    }
    const auto& old_stats_obj = _db.get< rc_stats_object, by_id >( RC_ARCHIVE_STATS_ID );
    _db.modify( old_stats_obj, [&]( rc_stats_object& old_stats )
    {
      _db.modify( new_stats_obj, [&]( rc_stats_object& new_stats )
      {
        new_stats.archive_and_reset_stats( old_stats, rc_pool, note.block_num, block_info.regen );
      } );
    } );
  }

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
    export_data->block = block_info;
} FC_CAPTURE_AND_RETHROW( (note.full_block->get_block()) ) }

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
  ilog( "Genesis pool is ${o}", ( "o", pool_obj.get_pool() ) );
  _db.create< rc_stats_object >( RC_PENDING_STATS_ID.get_value() );
  _db.create< rc_stats_object >( RC_ARCHIVE_STATS_ID.get_value() );
  _db.create< rc_pending_data >();

  const auto& idx = _db.get_index< account_index >().indices().get< by_id >();
  for( auto it=idx.begin(); it!=idx.end(); ++it )
  {
    _db.modify( *it, [&]( account_object& account )
    {
      account.rc_adjustment = HIVE_RC_HISTORICAL_ACCOUNT_CREATION_ADJUSTMENT;
      account.rc_manabar.last_update_time = now.sec_since_epoch();
      auto max_rc = account.get_maximum_rc().value;
      account.rc_manabar.current_mana = max_rc;
      account.last_max_rc = max_rc;
    } );
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

  pre_apply_operation_visitor( database& db ) : _db(db)
  {
    const auto& gpo = _db.get_dynamic_global_properties();
    _current_time = gpo.time.sec_since_epoch();
    _current_block_number = gpo.head_block_number;
  }

  void regenerate( const account_object& account )const
  {
    //
    // Since RC tracking is non-consensus, we must rely on consensus to forbid
    // transferring / delegating VESTS that haven't regenerated voting power.
    //
    // TODO:  Issue number
    //
    static_assert( HIVE_RC_REGEN_TIME <= HIVE_VOTING_MANA_REGENERATION_SECONDS, "RC regen time must be smaller than vote regen time" );

    // ilog( "regenerate(${a})", ("a", account.get_name()) );

    manabar_params mbparams;
    mbparams.max_mana = account.get_maximum_rc().value;
    mbparams.regen_time = HIVE_RC_REGEN_TIME;

    try {

    if( mbparams.max_mana != account.last_max_rc )
    {
#ifdef USE_ALTERNATE_CHAIN_ID
      // this situation indicates a bug in RC code, most likely some operation that affects RC was not
      // properly handled by setting new value for last_max_rc after RC changed
      HIVE_ASSERT( false, plugin_exception,
        "Account ${a} max RC changed from ${old} to ${new} without triggering an op, noticed on block ${b}",
        ("a", account.get_name())("old", account.last_max_rc)("new", mbparams.max_mana)("b", _db.head_block_num()) );
#else
      wlog( "NOTIFYALERT! Account ${a} max RC changed from ${old} to ${new} without triggering an op, noticed on block ${b}",
        ("a", account.get_name())("old", account.last_max_rc)("new", mbparams.max_mana)("b", _db.head_block_num()) );
#endif
    }

    _db.modify( account, [&]( account_object& acc )
    {
      acc.rc_manabar.regenerate_mana< true >( mbparams, _current_time );
    } );
    } FC_CAPTURE_AND_RETHROW( (account)(mbparams.max_mana) )
  }

  template< bool account_may_not_exist = false >
  void regenerate( const account_name_type& name )const
  {
    const account_object* account = _db.find_account( name );
    if( account_may_not_exist )
    {
      if( account == nullptr )
        return;
    }
    else
    {
      FC_ASSERT( account != nullptr, "Unexpectedly, account ${a} does not exist", ("a", name) );
    }

    regenerate( *account );
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
        regenerate( it->get_name() );
      }
    }
  }

  void operator()( const hardfork_hive_operation& op )const
  {
    regenerate( op.account );
    for( auto& account : op.other_affected_accounts )
      regenerate( account );
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

  //void operator()( const delayed_voting_operation& op )const //not needed, only voting power changes, not amount of VESTs

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

  void operator()( const delegate_rc_operation& op )const
  {
    regenerate( op.from );
    for( const auto& delegatee : op.delegatees )
      regenerate( delegatee );
  }

  template< typename Op >
  void operator()( const Op& op )const {}
};

typedef pre_apply_operation_visitor pre_apply_optional_action_vistor;

struct post_apply_operation_visitor
{
  typedef void result_type;

  database&                                _db;
  uint32_t                                 _current_time = 0;
  uint32_t                                 _current_block_number = 0;
  account_name_type                        _current_witness;

  post_apply_operation_visitor( database& db ) : _db(db)
  {
    const auto& dgpo = _db.get_dynamic_global_properties();
    _current_time = dgpo.time.sec_since_epoch();
    _current_block_number = dgpo.head_block_number;
    _current_witness = dgpo.current_witness;
  }

  void update_after_vest_change( const account_name_type& account_name,
    bool _fill_new_mana = true, bool _check_for_rc_delegation_overflow = false ) const
  {
    const account_object& account = _db.get_account( account_name );
    resource_credits( _db ).update_account_after_vest_change( account,
      _current_time, _fill_new_mana, _check_for_rc_delegation_overflow );
  }
  void operator()( const account_create_with_delegation_operation& op )const
  {
    update_after_vest_change( op.creator );
  }

  void operator()( const pow_operation& op )const
  {
    // ilog( "handling post-apply pow_operation" );
    update_after_vest_change( op.worker_account );
    update_after_vest_change( _current_witness );
  }

  void operator()( const pow2_operation& op )const
  {
    auto worker_name = get_worker_name( op.work );
    update_after_vest_change( worker_name );
    update_after_vest_change( _current_witness );
  }

  void operator()( const transfer_to_vesting_operation& op )
  {
    account_name_type target = op.to.size() ? op.to : op.from;
    update_after_vest_change( target );
  }

  void operator()( const withdraw_vesting_operation& op )const
  {
    update_after_vest_change( op.account, false, true );
  }

  void operator()( const delegate_vesting_shares_operation& op )const
  {
    update_after_vest_change( op.delegator, true, true );
    update_after_vest_change( op.delegatee, true, true );
  }

  void operator()( const author_reward_operation& op )const
  {
    update_after_vest_change( op.author );
      //not needed, since HF17 rewards need to be claimed before they affect vest balance
  }

  void operator()( const curation_reward_operation& op )const
  {
    update_after_vest_change( op.curator );
      //not needed, since HF17 rewards need to be claimed before they affect vest balance
  }

  void operator()( const fill_vesting_withdraw_operation& op )const
  {
    update_after_vest_change( op.from_account, true, true );
    update_after_vest_change( op.to_account );
  }

  void operator()( const claim_reward_balance_operation& op )const
  {
    update_after_vest_change( op.account );
  }

#ifdef HIVE_ENABLE_SMT
  void operator()( const claim_reward_balance2_operation& op )const
  {
    update_after_vest_change( op.account );
  }
#endif

  void operator()( const hardfork_operation& op )const
  {
    if( op.hardfork_id == HIVE_HARDFORK_0_1 )
    {
      const auto& idx = _db.get_index< account_index, by_id >();
      for( auto it=idx.begin(); it!=idx.end(); ++it )
        update_after_vest_change( it->get_name() );
    }

    if( op.hardfork_id == HIVE_HARDFORK_0_20 )
    {
      const auto& params = _db.get< rc_resource_param_object, by_id >( rc_resource_param_id_type() );

      _db.modify( _db.get< rc_pool_object, by_id >( rc_pool_id_type() ), [&]( rc_pool_object& p )
      {
        for( size_t i = 0; i < HIVE_RC_NUM_RESOURCE_TYPES; i++ )
        {
          p.set_pool( i, int64_t( params.resource_param_array[ i ].resource_dynamics_params.max_pool_size ) );
        }

        p.set_pool( resource_new_accounts, 0 );
      });
    }
  }

  void operator()( const hardfork_hive_operation& op )const
  {
    update_after_vest_change( op.account, true, true );
    for( auto& account : op.other_affected_accounts )
      update_after_vest_change( account, true, true );
  }

  void operator()( const return_vesting_delegation_operation& op )const
  {
    update_after_vest_change( op.account );
  }

  void operator()( const comment_benefactor_reward_operation& op )const
  {
    update_after_vest_change( op.benefactor );
      //not needed, since HF17 rewards need to be claimed before they affect vest balance
  }

  void operator()( const producer_reward_operation& op )const
  {
    update_after_vest_change( op.producer );
  }

  void operator()( const clear_null_account_balance_operation& op )const
  {
    update_after_vest_change( HIVE_NULL_ACCOUNT );
  }

  //not needed for treasury accounts (?)
  //void operator()( const consolidate_treasury_balance_operation& op )const

  //changes power of governance vote, not vest balance
  //void operator()( const delayed_voting_operation& op )const

  //pays fee in hbd, vest balance not touched
  //void operator()( const create_proposal_operation& op )const

  //no change in vest balance
  //void operator()( const update_proposal_votes_operation& op )const

  //no change in vest balance
  //void operator()( const remove_proposal_operation& op )const

  //nothing to do, all necessary RC updates are handled by the operation itself
  //void operator()( const delegate_rc_operation& op )const

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

  // ilog( "Calling pre-vtor on ${op}", ("op", note.op) );
  note.op.visit( vtor );

  count_resources_result differential_usage;
  if( prepare_differential_usage( note.op, _db, differential_usage ) )
  {
    _db.modify( _db.get< rc_pending_data, by_id >( rc_pending_data_id_type() ), [&]( rc_pending_data& data )
    {
      data.add_differential_usage( differential_usage );
    } );
  }
  } FC_CAPTURE_AND_RETHROW( (note.op) )
}


template< typename OpType >
void rc_plugin_impl::pre_apply_custom_op_type( const custom_operation_notification& note )
{
  const OpType* op = note.find_get_op< OpType >();
  if( !op )
    return;

  const dynamic_global_property_object& gpo = _db.get_dynamic_global_properties();
  pre_apply_operation_visitor vtor( _db );

  if( _db.has_hardfork( HIVE_HARDFORK_0_20 ) )
    vtor._vesting_share_price = gpo.get_vesting_share_price();

  vtor._current_witness = gpo.current_witness;

  op->visit( vtor );

  count_resources_result differential_usage;
  if( prepare_differential_usage( *op, _db, differential_usage ) )
  {
    _db.modify( _db.get< rc_pending_data, by_id >( rc_pending_data_id_type() ), [&]( rc_pending_data& data )
    {
      data.add_differential_usage( differential_usage );
    } );
  }
}

void rc_plugin_impl::on_pre_apply_custom_operation( const custom_operation_notification& note )
{
  pre_apply_custom_op_type< rc_custom_operation >( note );
  // If we wanted to pre-handle other plugin operations, we could put pre_apply_custom_op_type< other_plugin_operation >( note )
}

void rc_plugin_impl::on_post_apply_operation( const operation_notification& note )
{ try {
  if( before_first_block() )
    return;
  // dlog( "Calling post-vtor on ${op}", ("op", note.op) );
  post_apply_operation_visitor vtor( _db );
  note.op.visit( vtor );
} FC_CAPTURE_AND_RETHROW( (note.op) ) }

template< typename OpType >
void rc_plugin_impl::post_apply_custom_op_type( const custom_operation_notification& note )
{
  const OpType* op = note.find_get_op< OpType >();
  if( !op )
    return;
  // dlog( "Calling post-vtor on ${op}", ("op", note.op) );
  post_apply_operation_visitor vtor( _db );
  op->visit( vtor );

  count_resources_result extra_usage;
  count_resources( *op, extra_usage, _db.head_block_time() );
  _db.modify( _db.get< rc_pending_data, by_id >( rc_pending_data_id_type() ), [&]( rc_pending_data& data )
  {
    //the extra cost is stored on the same counters as differential usage (but as positive values);
    //we have to handle it here because later we'd have to reinterpret json into concrete custom op
    //just to collect correct cost
    data.add_custom_op_usage( extra_usage );
  } );
}

void rc_plugin_impl::on_post_apply_custom_operation( const custom_operation_notification& note )
{
  post_apply_custom_op_type< rc_custom_operation >( note );
}

void rc_plugin_impl::on_pre_apply_optional_action( const optional_action_notification& note )
{
  if( before_first_block() )
    return;

  const dynamic_global_property_object& gpo = _db.get_dynamic_global_properties();
  pre_apply_optional_action_vistor vtor( _db );

  vtor._current_witness = gpo.current_witness;

  note.action.visit( vtor );

  _db.modify( _db.get< rc_pending_data, by_id >( rc_pending_data_id_type() ), [&]( rc_pending_data& data )
  {
    data.reset_differential_usage();

    count_resources_result differential_usage;
    if( prepare_differential_usage( note.action, _db, differential_usage ) )
      data.add_differential_usage( differential_usage );
  } );
}

void rc_plugin_impl::on_post_apply_optional_action( const optional_action_notification& note )
{
  if( before_first_block() )
    return;

  resource_credits rc( _db );
  const dynamic_global_property_object& gpo = _db.get_dynamic_global_properties();
  const auto& pending_data = _db.get< rc_pending_data, by_id >( rc_pending_data_id_type() );

  // There is no transaction equivalent for actions, so post apply transaction logic for actions goes here.
  post_apply_optional_action_visitor vtor( _db );
  note.action.visit( vtor );

  rc_optional_action_info opt_action_info;
  // Initialize with (negative) usage for state that was updated by action
  opt_action_info.usage = pending_data.get_differential_usage();

  // How many resources do the optional actions use?
  count_resources( note.action, fc::raw::pack_size( note.action ), opt_action_info.usage, _db.head_block_time() );

  // How many RC do these actions cost?
  int64_t total_cost = rc.compute_cost( &opt_action_info );

  _db.modify( pending_data, [&]( rc_pending_data& data )
  {
    data.add_pending_usage( opt_action_info.usage, opt_action_info.cost );
  } );

  // Who pays the cost?
  opt_action_info.payer = get_resource_user( note.action );
  rc.use_account_rcs( &opt_action_info, total_cost );

  if( _enable_rc_stats && ( _db.is_validating_block() || _db.is_replaying_block() ) )
  {
    _db.modify( _db.get< rc_stats_object, by_id >( RC_PENDING_STATS_ID ), [&]( rc_stats_object& stats_obj )
    {
      stats_obj.add_stats( opt_action_info );
    } );
  }

  std::shared_ptr< exp_rc_data > export_data =
    hive::plugins::block_data_export::find_export_data< exp_rc_data >( HIVE_RC_PLUGIN_NAME );
  if( export_data )
    export_data->add_opt_action_info( opt_action_info );
}

void rc_plugin_impl::validate_database()
{
  const auto& idx = _db.get_index< account_index >().indices().get< by_name >();

  for( const account_object& account : idx )
  {
    int64_t max_rc = account.get_maximum_rc().value;
    FC_ASSERT( max_rc == account.last_max_rc,
      "Account ${a} max RC changed from ${old} to ${new} without triggering an op, noticed on block ${b} in validate_database()",
      ("a", account.get_name())("old", account.last_max_rc)("new", max_rc)("b", _db.head_block_num()) );
  }
}

fc::variant_object rc_plugin_impl::get_report( report_type rt, const rc_stats_object& stats ) const
{
  if( rt == report_type::NONE )
    return fc::variant_object();

  fc::variant_object_builder report;
  report
    ( "block", stats.get_starting_block() )
    ( "regen", stats.get_global_regen() )
    ( "budget", stats.get_budget() )
    ( "pool", stats.get_pool() )
    ( "share", stats.get_share() )
    // note: these are average costs from the start of current set of blocks (so from
    // previous set); they might be different from costs calculated for current set
    ( "vote", stats.get_archive_average_cost(0) )
    ( "comment", stats.get_archive_average_cost(1) )
    ( "transfer", stats.get_archive_average_cost(2) );
  if( rt != report_type::MINIMAL )
  {
    fc::variant_object_builder ops;
    for( int i = 0; i <= HIVE_RC_NUM_OPERATIONS; ++i )
    {
      const auto& op_stats = stats.get_op_stats(i);
      if( op_stats.count == 0 )
        continue;
      fc::variant_object_builder op;
      op( "count", op_stats.count );
      if( rt != report_type::FULL )
      {
        op( "avg_cost", op_stats.average_cost() );
      }
      else
      {
        op
          ( "cost", op_stats.cost )
          ( "usage", op_stats.usage );
      }
      if( i == HIVE_RC_NUM_OPERATIONS )
      {
        ops( "multiop", op.get() );
      }
      else
      {
        hive::protocol::operation _op;
        _op.set_which(i);
        std::string op_name = _op.get_stored_type_name( true );
        ops( op_name, op.get() );
      }
    }
    report( "ops", ops.get() );

    fc::variants payers;
    for( int i = 0; i < HIVE_RC_NUM_PAYER_RANKS; ++i )
    {
      const auto& payer_stats = stats.get_payer_stats(i);
      fc::variant_object_builder payer;
      payer
        ( "rank", i )
        ( "count", payer_stats.count );
      if( rt == report_type::FULL )
      {
        payer
          ( "cost", payer_stats.cost )
          ( "usage", payer_stats.usage );
      }
      if( payer_stats.less_than_5_percent )
        payer( "lt5", payer_stats.less_than_5_percent );
      if( payer_stats.less_than_10_percent )
        payer( "lt10", payer_stats.less_than_10_percent );
      if( payer_stats.less_than_20_percent )
        payer( "lt20", payer_stats.less_than_20_percent );
      if( payer_stats.was_dry() )
      {
        fc::variant_object_builder dry;
        dry
          ( "vote", payer_stats.cant_afford[0] )
          ( "comment", payer_stats.cant_afford[1] )
          ( "transfer", payer_stats.cant_afford[2] );
        payer( "cant_afford", dry.get() );
      }
      payers.emplace_back( payer.get() );
    }
    report( "payers", payers );
  }
  return report.get();
}

void rc_plugin_impl::update_rc_for_custom_action( std::function<void()>&& callback, const protocol::account_name_type& account_name ) const
{
  pre_apply_operation_visitor _pre_vtor( _db );
  _pre_vtor.regenerate( account_name );

  callback();

  post_apply_operation_visitor _post_vtor( _db );
  _post_vtor.update_after_vest_change( account_name );
}

} // detail

rc_plugin::rc_plugin() {}
rc_plugin::~rc_plugin() {}

void rc_plugin::set_program_options( options_description& cli, options_description& cfg )
{
  cfg.add_options()
    ("rc-stats-report-type", bpo::value<string>()->default_value("REGULAR"), "Level of detail of daily RC stat reports: NONE, MINIMAL, REGULAR, FULL. Default REGULAR." )
    ("rc-stats-report-output", bpo::value<string>()->default_value("ILOG"), "Where to put daily RC stat reports: DLOG, ILOG, NOTIFY. Default ILOG." )
    ;
}

void rc_plugin::plugin_initialize( const boost::program_options::variables_map& options )
{
  ilog( "Initializing resource credit plugin" );

  my = std::make_unique< detail::rc_plugin_impl >( *this );

  try
  {
    chain::database& db = appbase::app().get_plugin< hive::plugins::chain::chain_plugin >().db();

    my->_pre_apply_block_conn = db.add_pre_apply_block_handler( [&]( const block_notification& note )
      { try { my->on_pre_apply_block( note ); } FC_LOG_AND_RETHROW() }, *this, 0 );
    my->_post_apply_block_conn = db.add_post_apply_block_handler( [&]( const block_notification& note )
      { try { my->on_post_apply_block( note ); } FC_LOG_AND_RETHROW() }, *this, 0 );
    my->_pre_apply_transaction_conn = db.add_pre_apply_transaction_handler( [&]( const transaction_notification& note )
      { try { my->on_pre_apply_transaction( note ); } FC_LOG_AND_RETHROW() }, *this, 0 );
    my->_post_apply_transaction_conn = db.add_post_apply_transaction_handler( [&]( const transaction_notification& note )
    {
      try
      {
        my->on_post_apply_transaction( note );
      }
      catch( not_enough_rc_exception& ex )
      {
        throw;
      }
      FC_LOG_AND_RETHROW()
    }, *this, 0 );
    my->_pre_apply_operation_conn = db.add_pre_apply_operation_handler( [&]( const operation_notification& note )
      { try { my->on_pre_apply_operation( note ); } FC_LOG_AND_RETHROW() }, *this, 0 );
    my->_post_apply_operation_conn = db.add_post_apply_operation_handler( [&]( const operation_notification& note )
      { try { my->on_post_apply_operation( note ); } FC_LOG_AND_RETHROW() }, *this, 0 );
    my->_pre_apply_optional_action_conn = db.add_pre_apply_optional_action_handler( [&]( const optional_action_notification& note )
      { try { my->on_pre_apply_optional_action( note ); } FC_LOG_AND_RETHROW() }, *this, 0 );
    my->_post_apply_optional_action_conn = db.add_post_apply_optional_action_handler( [&]( const optional_action_notification& note )
      { try { my->on_post_apply_optional_action( note ); } FC_LOG_AND_RETHROW() }, *this, 0 );
    my->_pre_apply_custom_operation_conn = db.add_pre_apply_custom_operation_handler( [&]( const custom_operation_notification& note )
      { try { my->on_pre_apply_custom_operation( note ); } FC_LOG_AND_RETHROW() }, *this, 0 );
    my->_post_apply_custom_operation_conn = db.add_post_apply_custom_operation_handler( [&]( const custom_operation_notification& note )
      { try { my->on_post_apply_custom_operation( note ); } FC_LOG_AND_RETHROW() }, *this, 0 );

    HIVE_ADD_PLUGIN_INDEX(db, rc_resource_param_index);
    HIVE_ADD_PLUGIN_INDEX(db, rc_pool_index);
    HIVE_ADD_PLUGIN_INDEX(db, rc_stats_index);
    HIVE_ADD_PLUGIN_INDEX(db, rc_pending_data_index);
    HIVE_ADD_PLUGIN_INDEX(db, rc_direct_delegation_index);
    HIVE_ADD_PLUGIN_INDEX(db, rc_usage_bucket_index);
    HIVE_ADD_PLUGIN_INDEX(db, rc_expired_delegation_index);

    fc::mutable_variant_object state_opts;

#ifndef IS_TEST_NET
    my->_enable_at_block = HIVE_HF20_BLOCK_NUM; // testnet starts RC at 1
    my->_enable_rc_stats = true; // testnet rarely has enough useful RC data to collect and report
#endif

    appbase::app().get_plugin< chain::chain_plugin >().report_state_options( name(), state_opts );

    // Each plugin needs its own evaluator registry.
    my->_custom_operation_interpreter = std::make_shared< generic_custom_operation_interpreter< hive::chain::rc_custom_operation > >( my->_db, name() );

    // Add each operation evaluator to the registry
    my->_custom_operation_interpreter->register_evaluator< delegate_rc_evaluator >( this );

    // Add the registry to the database so the database can delegate custom ops to the plugin
    my->_db.register_custom_operation_interpreter( my->_custom_operation_interpreter );

    my->set_auto_report( options.at( "rc-stats-report-type" ).as<std::string>(),
      options.at( "rc-stats-report-output" ).as<std::string>() );

    ilog( "RC's will be computed starting at block ${b}", ("b", my->_enable_at_block) );
  }
  FC_CAPTURE_AND_RETHROW()
}

void rc_plugin::plugin_startup() {}

void rc_plugin::plugin_shutdown()
{
  chain::util::disconnect_signal( my->_pre_reindex_conn );
  chain::util::disconnect_signal( my->_post_reindex_conn );
  chain::util::disconnect_signal( my->_pre_apply_block_conn );
  chain::util::disconnect_signal( my->_post_apply_block_conn );
  chain::util::disconnect_signal( my->_pre_apply_transaction_conn );
  chain::util::disconnect_signal( my->_post_apply_transaction_conn );
  chain::util::disconnect_signal( my->_pre_apply_operation_conn );
  chain::util::disconnect_signal( my->_post_apply_operation_conn );
  chain::util::disconnect_signal( my->_pre_apply_optional_action_conn );
  chain::util::disconnect_signal( my->_post_apply_optional_action_conn );
  chain::util::disconnect_signal( my->_pre_apply_custom_operation_conn );

}

bool rc_plugin::is_active() const
{
  return !my->before_first_block();
}

void rc_plugin::set_enable_rc_stats( bool enable )
{
  my->_enable_rc_stats = enable;
}

bool rc_plugin::is_rc_stats_enabled() const
{
  return my->_enable_rc_stats;
}

void rc_plugin::validate_database()
{
  my->validate_database();
}

void rc_plugin::update_rc_for_custom_action( std::function<void()>&& callback, const protocol::account_name_type& account_name ) const
{
  my->update_rc_for_custom_action( std::move( callback ), account_name );
}

fc::variant_object rc_plugin::get_report( report_type rt, bool pending ) const
{
  const rc_stats_object& stats = my->_db.get< rc_stats_object >( pending ? RC_PENDING_STATS_ID : RC_ARCHIVE_STATS_ID );
  return my->get_report( rt, stats );
}

} } } // hive::plugins::rc
