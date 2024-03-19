#include <hive/chain/rc/rc_utility.hpp>
#include <hive/chain/rc/rc_objects.hpp>
#include <hive/chain/rc/rc_curve.hpp>
#include <hive/chain/database.hpp>
#include <hive/chain/database_exceptions.hpp>
#include <hive/chain/generic_custom_operation_interpreter.hpp>
#include <hive/chain/util/remove_guard.hpp>

#include <appbase/application.hpp>

#include <fc/reflect/variant.hpp>
#include <fc/uint128.hpp>

namespace hive { namespace chain {

using fc::uint128_t;

resource_credits::report_type resource_credits::auto_report_type = resource_credits::report_type::REGULAR;
resource_credits::report_output resource_credits::auto_report_output = resource_credits::report_output::ILOG;

void resource_credits::set_auto_report( const std::string& _option_type, const std::string& _option_output )
{
  report_type rt = report_type::REGULAR;
  report_output ro = report_output::ILOG;

  if( _option_type == "NONE" )
    rt = report_type::NONE;
  else if( _option_type == "MINIMAL" )
    rt = report_type::MINIMAL;
  else if( _option_type == "REGULAR" )
    rt = report_type::REGULAR;
  else if( _option_type == "FULL" )
    rt = report_type::FULL;
  else
    FC_THROW_EXCEPTION( fc::parse_error_exception, "Unknown RC stats report type" );

  if( _option_output == "LOG_NOTIFY" || _option_output == "BOTH" )
    ro = report_output::BOTH;
  else if( _option_output == "NOTIFY" )
    ro = report_output::NOTIFY;
  else if( _option_output == "ILOG" )
    ro = report_output::ILOG;
  else if( _option_output == "DLOG" )
    ro = report_output::DLOG;
  else
    FC_THROW_EXCEPTION( fc::parse_error_exception, "Unknown RC stats report output" );

  set_auto_report( rt, ro );
}

fc::variant_object resource_credits::get_report( report_type rt, const rc_stats_object& stats )
{
  if( rt == report_type::NONE )
    return fc::variant_object();

  fc::variant_object_builder report;
  report
    ( "block", stats.get_starting_block() )
    ( "stamp", stats.get_stamp() )
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
        hive::protocol::operation _op(static_cast<int64_t>(i));
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

int64_t resource_credits::compute_cost(
  const rc_price_curve_params& curve_params,
  int64_t current_pool,
  int64_t resource_count,
  int64_t rc_regen
  )
{
  /*
  ilog( "resource_credits::compute_cost( ${params}, ${pool}, ${res}, ${reg} )",
    ("params", curve_params)
    ("pool", current_pool)
    ("res", resource_count)
    ("reg", rc_regen) );
  */
  FC_ASSERT( rc_regen > 0 );

  if( resource_count <= 0 )
  {
    if( resource_count < 0 )
      return -compute_cost( curve_params, current_pool, -resource_count, rc_regen );
    return 0;
  }
  uint128_t num = uint128_t( rc_regen );
  num *= curve_params.coeff_a;
  // rc_regen * coeff_a is already risking overflowing 128 bits,
  //   so shift it immediately before multiplying by resource_count
  num >>= curve_params.shift;
  // err on the side of rounding not in the user's favor
  num += 1;
  num *= uint64_t( resource_count );

  uint128_t denom = uint128_t( curve_params.coeff_b );

  // Negative pool doesn't increase price beyond p_max
  //   i.e. define p(x) = p(0) for all x < 0
  denom += (current_pool > 0) ? uint64_t(current_pool) : uint64_t(0);
  uint128_t num_denom = num / denom;
  // Add 1 to avoid 0 result in case of various rounding issues,
  // err on the side of rounding not in the user's favor
  // ilog( "result: ${r}", ("r", num_denom.to_uint64()+1) );
  return fc::uint128_to_uint64(num_denom)+1;
}

int64_t resource_credits::compute_cost( rc_transaction_info* usage_info ) const
{
  const dynamic_global_property_object& dgpo = db.get_dynamic_global_properties();
  const rc_resource_param_object& params_obj = db.get< rc_resource_param_object, by_id >( rc_resource_param_id_type() );
  const rc_pool_object& pool_obj = db.get< rc_pool_object, by_id >( rc_pool_id_type() );

  int64_t total_vests = dgpo.get_total_vesting_shares().amount.value;
  int64_t rc_regen = ( total_vests / ( HIVE_RC_REGEN_TIME / HIVE_BLOCK_INTERVAL ) );

  int64_t total_cost = 0;

  // When rc_regen is 0, everything is free
  if( rc_regen > 0 )
  {
    for( size_t i = 0; i < HIVE_RC_NUM_RESOURCE_TYPES; ++i )
    {
      const rc_resource_params& params = params_obj.resource_param_array[i];
      int64_t pool = pool_obj.get_pool(i);

      usage_info->usage[i] *= int64_t( params.resource_dynamics_params.resource_unit );
      int64_t pool_regen_share = fc::uint128_to_int64( ( uint128_t( rc_regen ) * pool_obj.get_weight(i) ) / pool_obj.get_weight_divisor() );
      if( pool_regen_share > 0 )
      {
        usage_info->cost[i] = compute_cost( params.price_curve_params, pool, usage_info->usage[i], pool_regen_share );
        total_cost += usage_info->cost[i];
      }
    }
  }

  return total_cost;
}

void resource_credits::regenerate_rc_mana( const account_object& account, const fc::time_point_sec now ) const
{
  // Since RC tracking is non-consensus, we must rely on consensus to forbid
  // transferring / delegating VESTS that haven't regenerated voting power.
  static_assert( HIVE_RC_REGEN_TIME <= HIVE_VOTING_MANA_REGENERATION_SECONDS,
    "RC regen time must be smaller than vote regen time" );

  util::manabar_params mbparams;
  mbparams.max_mana = account.get_maximum_rc().value;
  mbparams.regen_time = HIVE_RC_REGEN_TIME;

  try
  {
    if( mbparams.max_mana != account.last_max_rc )
    {
#ifdef USE_ALTERNATE_CHAIN_ID
      // this situation indicates a bug in RC code, most likely some operation that affects RC was not
      // properly handled by setting new value for last_max_rc after RC changed
      HIVE_ASSERT( false, plugin_exception,
        "Account ${a} max RC changed from ${old} to ${new} without triggering an op, noticed on block ${b}",
        ( "a", account.get_name() )( "old", account.last_max_rc )( "new", mbparams.max_mana )( "b", db.head_block_num() ) );
#else
      wlog( "NOTIFYALERT! Account ${a} max RC changed from ${old} to ${new} without triggering an op, noticed on block ${b}",
        ( "a", account.get_name() )( "old", account.last_max_rc )( "new", mbparams.max_mana )( "b", db.head_block_num() ) );
#endif
    }

    db.modify( account, [&]( account_object& acc )
    {
      acc.rc_manabar.regenerate_mana< true >( mbparams, now );
    } );
  } FC_CAPTURE_AND_RETHROW( (account)(mbparams.max_mana) )
}

void resource_credits::update_account_after_rc_delegation( const account_object& account,
  const fc::time_point_sec now, int64_t delta, bool regenerate_mana ) const
{
  db.modify< account_object >( account, [&]( account_object& acc )
  {
    auto max_rc = acc.get_maximum_rc().value;
    util::manabar_params manabar_params( max_rc, HIVE_RC_REGEN_TIME );
    if( regenerate_mana )
    {
      acc.rc_manabar.regenerate_mana< true >( manabar_params, now );
    }
    else if( acc.rc_manabar.last_update_time != now.sec_since_epoch() )
    {
      //most likely cause: there is no regenerate_rc_mana() call before operation changing vests
      wlog( "NOTIFYALERT! Account ${a} not regenerated prior to RC change, noticed on block ${b}",
        ( "a", acc.get_name() )( "b", db.head_block_num() ) );
    }
    //rc delegation changes are immediately reflected in current_mana in both directions;
    //if negative delta was not taking away delegated mana it would be easy to pump RC;
    //note that it is different when actual VESTs are involved
    acc.rc_manabar.current_mana = std::max( acc.rc_manabar.current_mana + delta, int64_t( 0 ) );
    acc.last_max_rc = max_rc + delta;
    acc.received_rc += delta;
  } );
}

void resource_credits::update_account_after_vest_change( const account_object& account,
  const fc::time_point_sec now, bool _fill_new_mana, bool _check_for_rc_delegation_overflow ) const
{
  if( account.rc_manabar.last_update_time != now.sec_since_epoch() )
  {
    //most likely cause: there is no regenerate_rc_mana() call before operation changing vests
    wlog( "NOTIFYALERT! Account ${a} not regenerated prior to VEST change, noticed on block ${b}",
      ( "a", account.get_name() )( "b", db.head_block_num() ) );
  }

  if( _check_for_rc_delegation_overflow && account.get_delegated_rc() > 0 )
  {
    //the following action is needed when given account lost VESTs and it now might have less
    //than it already delegated with rc delegations (second part of condition is a quick exit
    //check for times before RC delegations are activated (HF26))
    int64_t delegation_overflow = -account.get_maximum_rc( true ).value;
    int64_t initial_overflow = delegation_overflow;

    if( delegation_overflow > 0 )
    {
      int16_t remove_limit = db.get_remove_threshold();
      remove_guard obj_perf( remove_limit );
      remove_delegations( delegation_overflow, account.get_id(), now, obj_perf );

      if( delegation_overflow > 0 )
      {
        // there are still active delegations that need to be cleared, but we've already exhausted the object removal limit;
        // set an object to continue removals in following blocks while blocking explicit changes in RC delegations on account
        auto* expired = db.find< rc_expired_delegation_object, by_account >( account.get_id() );
        if( expired != nullptr )
        {
          // supplement existing object from still unfinished previous delegation removal...
          db.modify( *expired, [&]( rc_expired_delegation_object& e )
          {
            e.expired_delegation += delegation_overflow;
          } );
        }
        else
        {
          // ...or create new one
          db.create< rc_expired_delegation_object >( account, delegation_overflow );
        }
      }

      // We don't update last_max_rc and the manabars here because it happens below
      db.modify( account, [&]( account_object& acc )
      {
        acc.delegated_rc -= initial_overflow;
      } );
    }
  }

  int64_t new_last_max_rc = account.get_maximum_rc().value;
  int64_t drc = new_last_max_rc - account.last_max_rc.value;
  drc = _fill_new_mana ? drc : 0;

  if( new_last_max_rc != account.last_max_rc )
  {
    db.modify( account, [&]( account_object& acc )
    {
      //note: rc delegations behave differently because if they behaved the following way there would
      //be possible to easily fill up mana through giving and immediately taking away rc delegations
      acc.last_max_rc = new_last_max_rc;
      //only positive delta is applied directly to mana, so receiver can immediately start using it;
      //negative delta is caused by either power down or reduced incoming delegation; in both cases
      //there is a delay that makes transfered vests unusable for long time (not shorter than full
      //rc regeneration) therefore we can skip applying negative delta without risk of "pumping" rc;
      //by not applying negative delta we preserve mana that affected account "earned" so far...
      acc.rc_manabar.current_mana += std::max( drc, int64_t( 0 ) );
      //...however we have to reduce it when its current level would exceed new maximum (issue #191)
      if( acc.rc_manabar.current_mana > acc.last_max_rc )
        acc.rc_manabar.current_mana = acc.last_max_rc.value;
    } );
  }
}

void resource_credits::update_rc_for_custom_action( std::function<void()>&& callback,
  const account_object& account ) const
{
  auto now = db.head_block_time();
  regenerate_rc_mana( account, now );
  callback();
  update_account_after_vest_change( account, now );
}

void resource_credits::use_account_rcs( int64_t rc )
{
  const account_name_type& account_name = tx_info.payer;
  if( account_name == account_name_type() )
  {
    if( db.is_in_control() )
      HIVE_ASSERT( false, plugin_exception, "Tried to execute transaction with no resource user", );
    return;
  }

  const account_object& account = db.get_account( account_name );
  const dynamic_global_property_object& dgpo = db.get_dynamic_global_properties();

  util::manabar_params mbparams;
  auto max_mana = account.get_maximum_rc().value;
  mbparams.max_mana = max_mana;
  tx_info.max = max_mana;
  tx_info.rc = account.rc_manabar.current_mana; // initialize before regen in case of exception
  mbparams.regen_time = HIVE_RC_REGEN_TIME;

  try
  {

    db.modify( account, [&]( account_object& acc )
    {
      acc.rc_manabar.regenerate_mana< true >( mbparams, dgpo.time.sec_since_epoch() );
      tx_info.rc = acc.rc_manabar.current_mana; // update after regeneration
      bool has_mana = acc.rc_manabar.has_mana( rc );

#ifdef USE_ALTERNATE_CHAIN_ID
      if( configuration_data.allow_not_enough_rc == false )
#endif
      {
        FC_TODO( "Add || db.has_hardfork( X ) to make RC part of consensus since X" );
          //we should also replace all NOTIFYALERT warnings in RC with assertions, since they can't ever happen
        if( db.is_in_control() )
        {
          HIVE_ASSERT( has_mana, not_enough_rc_exception,
            "Account: ${account} has ${rc_current} RC, needs ${rc_needed} RC. Please wait to transact, or power up HIVE.",
            ( "account", account_name )
            ( "rc_needed", rc )
            ( "rc_current", tx_info.rc )
          );
        }
        else
        {
          if( !has_mana && db.is_processing_block() && db.has_hardfork( HIVE_HARDFORK_1_26 ) )
          {
            //when we didn't have is_processing_block as part of condition the messages below would also
            //be produced when pending transactions were reapplied after new block arrived even though
            //they are not part of any block yet;
            //if we put that part of condition as alternative for db.is_in_control() above it would mean
            //the transactions that were validated before but started to lack RC after new block arrived,
            //would be dropped from mempool (note the difference: when user lacks RC while sending transaction
            //he can notice and react to it; however when his transaction is rejected after validation due
            //to changes in RC, it seems better to retry tx execution like we currently do)
            ilog( "Accepting transaction by ${account}, has ${rc_current} RC, needs ${rc_needed} RC, block ${b}, witness ${w}.",
              ( "account", account_name )
              ( "rc_needed", rc )
              ( "rc_current", tx_info.rc )
              ( "b", dgpo.head_block_number )
              ( "w", dgpo.current_witness )
            );
          }
        }
      }

      acc.rc_manabar.use_mana( rc );
      tx_info.rc = acc.rc_manabar.current_mana;
    } );
  } FC_CAPTURE_AND_RETHROW( (tx_info) )
}

bool resource_credits::has_expired_delegation( const account_object& account ) const
{
  auto* expired = db.find< rc_expired_delegation_object, by_account >( account.get_id() );
  return expired != nullptr;
}

void resource_credits::handle_expired_delegations() const
{
  if( !db.has_hardfork( HIVE_HARDFORK_1_26 ) )
    return;

  // clear as many delegations as possible within limit starting from oldest ones (smallest id)
  const auto& expired_idx = db.get_index<rc_expired_delegation_index, by_id>();
  auto expired_it = expired_idx.begin();
  if( expired_it == expired_idx.end() )
    return;

  auto now = db.head_block_time();
  int16_t remove_limit = db.get_remove_threshold();
  remove_guard obj_perf( remove_limit );

  do
  {
    const auto& expired = *expired_it;
    int64_t delegation_overflow = expired.expired_delegation;
    remove_delegations( delegation_overflow, expired.from, now, obj_perf );

    if( delegation_overflow > 0 )
    {
      // still some delegations remain for next block cycle
      db.modify( expired, [&]( rc_expired_delegation_object& e )
      {
        e.expired_delegation = delegation_overflow;
      } );
      break;
    }
    else
    {
      // no more delegations to clear for user related to this particular delegation overflow
      db.remove( expired ); //remove even if we've hit the removal limit - that overflow is empty already
    }

    expired_it = expired_idx.begin();
  }
  while( !obj_perf.done() && expired_it != expired_idx.end() );
}

void resource_credits::remove_delegations( int64_t& delegation_overflow, account_id_type delegator_id,
  const fc::time_point_sec now, remove_guard& obj_perf ) const
{
  const auto& rc_del_idx = db.get_index<rc_direct_delegation_index, by_from_to>();
  // Maybe add a new index to sort by from / amount delegated so it's always the bigger delegations that is modified first instead of the id order ?
  // start_id just means we iterate over all the rc delegations
  auto rc_del_itr = rc_del_idx.lower_bound( boost::make_tuple( delegator_id, account_id_type::start_id() ) );

  while( delegation_overflow > 0 && rc_del_itr != rc_del_idx.end() && rc_del_itr->from == delegator_id )
  {
    const auto& rc_delegation = *rc_del_itr;
    ++rc_del_itr;

    int64_t delta_rc = std::min( delegation_overflow, int64_t( rc_delegation.delegated_rc ) );
    account_id_type to_id = rc_delegation.to;

    // If the needed RC allow us to leave the delegation untouched, we just change the delegation to take it into account
    if( rc_delegation.delegated_rc > ( uint64_t ) delta_rc )
    {
      db.modify( rc_delegation, [&]( rc_direct_delegation_object& rc_del )
      {
        rc_del.delegated_rc -= delta_rc;
      } );
    }
    else
    {
      // Otherwise, we remove it
      if( !obj_perf.remove( db, rc_delegation ) )
        break;
    }

    const auto& to_account = db.get_account( to_id );
    //since to_account was not originally expected to be affected by operation that is being
    //processed, we need to regenerate its mana before rc delegation is modified
    update_account_after_rc_delegation( to_account, now, -delta_rc, true );

    delegation_overflow -= delta_rc;
  }
}

void resource_credits::handle_auto_report( uint32_t block_num, int64_t global_regen,
  const rc_pool_object& rc_pool ) const
{
  if( ( block_num % HIVE_RC_STATS_REPORT_FREQUENCY ) != 0 )
    return;

  const auto& new_stats_obj = db.get< rc_stats_object >( RC_PENDING_STATS_ID );
  if( auto_report_type != report_type::NONE && new_stats_obj.get_starting_block() )
  {
    fc::variant_object report = get_report( auto_report_type, new_stats_obj );
    switch( auto_report_output )
    {
    case report_output::BOTH:
    case report_output::NOTIFY:
      db.get_app().notify( "RC stats", "rc_stats", report );
      if( auto_report_output != report_output::BOTH )
        break;
      //else
      //  continue to ILOG
    case report_output::ILOG:
      ilog( "RC stats:${report}", ( report ) );
      break;
    default:
      dlog( "RC stats:${report}", ( report ) );
      break;
    };
  }

  const auto& old_stats_obj = db.get< rc_stats_object, by_id >( RC_ARCHIVE_STATS_ID );
  db.modify( old_stats_obj, [&]( rc_stats_object& old_stats )
  {
    db.modify( new_stats_obj, [&]( rc_stats_object& new_stats )
    {
      new_stats.archive_and_reset_stats( old_stats, rc_pool, block_num, global_regen );
    } );
  } );
}

void resource_credits::initialize_evaluators()
{
  // we only need to register evaluator once, when database is reopened in the same 'database' object
  // we already have the evaluator (by the way, that is also true from custom evaluators from plugins)
  if( _custom_operation_interpreter.get() == nullptr )
  {
    _custom_operation_interpreter = std::make_shared<
      generic_custom_operation_interpreter< rc_custom_operation > >( db, HIVE_RC_CUSTOM_OPERATION_ID );

    // register evaluator for each custom operation (we have just one at the moment)
    _custom_operation_interpreter->register_evaluator< delegate_rc_evaluator >( nullptr );

    // register interpreter in database so it can delegate handling of custom operations to it
    db.register_custom_operation_interpreter( _custom_operation_interpreter );
  }
}

// equivalent of previous call through signal
#define ISOLATE_RC_CALL( _context, _name, _call, ... )   \
{                                                        \
  if( !db.has_hardfork( HIVE_HARDFORK_0_20 ) )           \
    return;                                              \
                                                         \
  std::string name;                                      \
                                                         \
  auto& benchmark = db.get_benchmark_dumper();           \
  if( benchmark.is_enabled() )                           \
  {                                                      \
    name = _name;                                        \
    benchmark.begin();                                   \
  }                                                      \
                                                         \
  HIVE_TRY_NOTIFY( _call, __VA_ARGS__ );                 \
                                                         \
  if( benchmark.is_enabled() )                           \
    benchmark.end( _context, name );                     \
}

void resource_credits::finalize_block() const
{
  if( !db.has_hardfork( HIVE_HARDFORK_0_20 ) )
    return;

  const auto& dgpo = db.get_dynamic_global_properties();
  auto now = dgpo.time;
  auto block_num = dgpo.head_block_number;
  int64_t regen = ( dgpo.total_vesting_shares.amount.value / ( HIVE_RC_REGEN_TIME / HIVE_BLOCK_INTERVAL ) );

  const auto& params_obj = db.get< rc_resource_param_object, by_id >( rc_resource_param_id_type() );

  const auto& bucket_idx = db.get_index< rc_usage_bucket_index, by_timestamp >();
  const auto* active_bucket = &( *bucket_idx.rbegin() );
  bool reset_bucket = time_point_sec( active_bucket->get_timestamp() + fc::seconds( HIVE_RC_BUCKET_TIME_LENGTH ) ) <= now;
  if( reset_bucket )
    active_bucket = &( *bucket_idx.begin() );

  const auto& rc_pool = db.get< rc_pool_object, by_id >( rc_pool_id_type() );
  db.modify( rc_pool, [&]( rc_pool_object& pool_obj )
  {
    for( size_t i = 0; i < HIVE_RC_NUM_RESOURCE_TYPES; ++i )
    {
      const rd_dynamics_params& params = params_obj.resource_param_array[i].resource_dynamics_params;
      int64_t pool = pool_obj.get_pool(i);

      pool_obj.set_budget( i, params.budget_per_time_unit );
      int64_t decay = rd_compute_pool_decay( params.decay_params, pool - block_info.usage[i], 1 );

      // update global usage statistics
      if( reset_bucket )
        pool_obj.add_usage( i, -active_bucket->get_usage(i) );
      pool_obj.add_usage( i, block_info.usage[i] );

      int64_t new_pool = pool - decay + params.budget_per_time_unit - block_info.usage[i];

      if( i == resource_new_accounts )
      {
        int64_t new_consensus_pool = dgpo.available_account_subsidies;
        if( new_consensus_pool != new_pool )
        {
          ilog( "resource_new_accounts adjustment on block ${b}: ${a}",
            ( "a", new_consensus_pool - new_pool )( "b", block_num ) );
          new_pool = new_consensus_pool;
        }
      }

      pool_obj.set_pool( i, new_pool );
    }
    pool_obj.recalculate_resource_weights( params_obj );
  } );

  handle_auto_report( block_num, regen, rc_pool );

  db.modify( *active_bucket, [&]( rc_usage_bucket_object& bucket )
  {
    if( reset_bucket )
      bucket.reset( now ); //contents of bucket being reset was already subtracted from globals above
    for( int i = 0; i < HIVE_RC_NUM_RESOURCE_TYPES; ++i )
      bucket.add_usage( i, block_info.usage[i] );
  } );
}

void resource_credits::finalize_transaction( const full_transaction_type& full_tx )
{
  if( !db.has_hardfork( HIVE_HARDFORK_0_20 ) )
    return;

  const signed_transaction& tx = full_tx.get_transaction();

  // How many resources does the transaction use?
  // note: tx_info.usage might already contain state discount for selected operations and extra usage for custom ops
  // note: while we could calculate most of used resources before transaction is executed, doing so for
  // custom operations would be troublesome
  count_resources( tx, full_tx.get_transaction_size(), tx_info.usage, db.head_block_time() );

  // How many RC does this transaction cost?
  int64_t total_cost = compute_cost( &tx_info );
  full_tx.set_rc_cost( total_cost );

  // note: since transaction can influence amount of RC the payer has, we can't check if the payer has
  // enough RC mana prior to actual execution of transaction
  use_account_rcs( total_cost );

  if( db.is_validating_block() || db.is_replaying_block() )
  {
    block_info.add( tx_info );

    const auto& rc_stats = db.get< rc_stats_object >( RC_PENDING_STATS_ID );
    db.modify( rc_stats, [&]( rc_stats_object& stats_obj )
    {
      stats_obj.add_stats( tx_info );
    } );
  }
}

void resource_credits::set_pool_params( const witness_schedule_object& wso ) const
{
  //hardfork needs to be tested outside because the routine is actually also used right before HF20 activates

  const auto& rc_params = db.get< rc_resource_param_object, by_id >( rc_resource_param_id_type() );
  if( rc_params.resource_param_array[ resource_new_accounts ].resource_dynamics_params != wso.account_subsidy_rd )
  {
    dlog( "Activating new account subsidy params in block ${b}", ( "b", db.head_block_num() ) );
    db.modify( rc_params, [&]( rc_resource_param_object& p )
    {
      p.resource_param_array[ resource_new_accounts ].resource_dynamics_params = wso.account_subsidy_rd;
      // Hardcoded to use default values of rc_curve_gen_params() fields for now
      generate_rc_curve_params(
        p.resource_param_array[ resource_new_accounts ].price_curve_params,
        p.resource_param_array[ resource_new_accounts ].resource_dynamics_params,
        rc_curve_gen_params() );
    } );
  }
}

void resource_credits::reset_tx_info( const protocol::signed_transaction& tx )
{
  tx_info = rc_transaction_info{};
  tx_info.payer = get_resource_user( tx );
  if( tx.operations.size() == 1 )
    tx_info.op = tx.operations.front().which();
}

void resource_credits::reset_block_info()
{
  block_info = rc_block_info{};
}

} }
