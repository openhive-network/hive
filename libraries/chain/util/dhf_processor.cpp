#include <hive/chain/util/sps_processor.hpp>

namespace hive { namespace chain {

using hive::protocol::asset;
using hive::protocol::operation;

using hive::chain::proposal_object;
using hive::chain::by_start_date;
using hive::chain::by_end_date;
using hive::chain::proposal_index;
using hive::chain::proposal_id_type;
using hive::chain::proposal_vote_index;
using hive::chain::by_proposal_voter;
using hive::protocol::proposal_pay_operation;
using hive::chain::sps_helper;
using hive::chain::dynamic_global_property_object;
using hive::chain::block_notification;

const std::string sps_processor::removing_name = "sps_processor_remove";
const std::string sps_processor::calculating_name = "sps_processor_calculate";

bool sps_processor::is_maintenance_period( const time_point_sec& head_time ) const
{
  auto due_time = db.get_dynamic_global_properties().next_maintenance_time;
  return due_time <= head_time;
}

bool sps_processor::is_daily_maintenance_period( const time_point_sec& head_time ) const
{
  /// No DHF conversion until HF24 !
  if( !db.has_hardfork( HIVE_HARDFORK_1_24 ) )
    return false;
  auto due_time = db.get_dynamic_global_properties().next_daily_maintenance_time;
  return due_time <= head_time;
}

void sps_processor::remove_proposals( const time_point_sec& head_time )
{
  FC_TODO("implement proposal removal based on automatic actions")

  auto& byEndDateIdx = db.get_index< proposal_index, by_end_date >();
  auto& byVoterIdx = db.get_index< proposal_vote_index, by_proposal_voter >();

  auto end = byEndDateIdx.upper_bound( head_time );
  auto itr = byEndDateIdx.begin();

  remove_guard obj_perf( db.get_remove_threshold() );

  while( itr != end )
  {
    const auto& proposal = *itr;
    ++itr;
    /*
      It was decided that automatic removing of old proposals is to be blocked in order to allow looking for
      expired proposals (by API call `list_proposals` with `expired` flag). Maybe in the future removing will be re-enabled.
      For now proposals can be removed only by explicit call of `remove_proposal_operation` - that operation removes some
      data and if threshold is reached, remaining proposals are marked as `removed` and are removed in regular per-block
      cycles here.
    */
    if( proposal.removed )
      sps_helper::remove_proposal( proposal, byVoterIdx, db, obj_perf );
    if( obj_perf.done() )
      break;
  }
}

void sps_processor::find_proposals( const time_point_sec& head_time, t_proposals& active_proposals, t_proposals& no_active_yet_proposals )
{
  const auto& pidx = db.get_index< proposal_index >().indices().get< by_start_date >();

  FC_TODO("avoid scanning all proposals by use of end_date index, don't list future proposals - use index directly");
  std::for_each( pidx.begin(), pidx.upper_bound( head_time ), [&]( auto& proposal )
  {
    if( head_time >= proposal.start_date && head_time <= proposal.end_date )
      active_proposals.emplace_back( proposal );
  } );

  std::for_each( pidx.upper_bound( head_time ), pidx.end(), [&]( auto& proposal )
  {
    no_active_yet_proposals.emplace_back( proposal );
  } );
}

uint64_t sps_processor::calculate_votes( uint32_t pid )
{
  uint64_t ret = 0;

  const auto& pvidx = db.get_index< proposal_vote_index >().indices().get< by_proposal_voter >();
  auto found = pvidx.find( pid );

  while( found != pvidx.end() && found->proposal_id == pid )
  {
    const auto& _voter = db.get_account( found->voter );

    //If _voter has set proxy, then his votes aren't taken into consideration
    if( !_voter.has_proxy() )
    {
      auto sum = _voter.witness_vote_weight();
      ret += sum.value;
    }

    ++found;
  }

  return ret;
}

void sps_processor::calculate_votes( const t_proposals& proposals )
{
  for( auto& item : proposals )
  {
    const proposal_object& _item = item;
    auto total_votes = calculate_votes( _item.proposal_id );

    db.modify( _item, [&]( auto& proposal )
    {
      proposal.total_votes = total_votes;
    } );
  }
}

void sps_processor::sort_by_votes( t_proposals& proposals )
{
  std::sort( proposals.begin(), proposals.end(), []( const proposal_object& a, const proposal_object& b )
  {
    if (a.total_votes == b.total_votes)
    {
      return a.proposal_id < b.proposal_id;
    }
    return a.total_votes > b.total_votes;
  } );
}

asset sps_processor::get_treasury_fund()
{
  auto& treasury_account = db.get_treasury();

  return treasury_account.get_hbd_balance();
}

asset sps_processor::calculate_maintenance_budget( const time_point_sec& head_time )
{
  //Get funds from 'treasury' account ( treasury_fund )
  asset treasury_fund = get_treasury_fund();

  //Calculate budget for given maintenance period
  uint32_t passed_time_seconds = ( head_time - db.get_dynamic_global_properties().last_budget_time ).to_seconds();

  //Calculate daily_budget_limit
  int64_t daily_budget_limit = treasury_fund.amount.value / total_amount_divider;

  daily_budget_limit = ( ( uint128_t( passed_time_seconds ) * daily_budget_limit ) / daily_seconds ).to_uint64();

  return asset( daily_budget_limit, treasury_fund.symbol );
}

void sps_processor::transfer_payments( const time_point_sec& head_time, asset& maintenance_budget_limit, const t_proposals& proposals )
{
  if( maintenance_budget_limit.amount.value == 0 )
    return;

  const auto& treasury_account = db.get_treasury();

  uint32_t passed_time_seconds = ( head_time - db.get_dynamic_global_properties().last_budget_time ).to_seconds();

  auto processing = [this, &treasury_account]( const proposal_object& _item, const asset& payment )
  {
    const auto& receiver_account = db.get_account( _item.receiver );

    operation vop = proposal_pay_operation( _item.proposal_id, _item.receiver, db.get_treasury_name(), payment, db.get_current_trx(), db.get_current_op_in_trx() );
    /// Push vop to be recorded by other parts (like AH plugin etc.)
    db.push_virtual_operation(vop);
    /// Virtual ops have no evaluators, so operation must be immediately "evaluated"
    db.adjust_balance( treasury_account, -payment );
    db.adjust_balance( receiver_account, payment );
  };

  for( auto& item : proposals )
  {
    const proposal_object& _item = item;

    //Proposals without any votes shouldn't be treated as active
    if( _item.total_votes == 0 )
      break;

    asset period_pay;
    if( db.has_hardfork(HIVE_HARDFORK_1_25) )
    {
      period_pay = asset((( passed_time_seconds * _item.daily_pay.amount.value ) / daily_seconds ), _item.daily_pay.symbol );
    }
    else
    {
      uint128_t ratio = ( passed_time_seconds * HIVE_100_PERCENT ) / daily_seconds;
      period_pay = asset( ( ratio * _item.daily_pay.amount.value ).to_uint64() / HIVE_100_PERCENT, _item.daily_pay.symbol );
    }

    if( period_pay >= maintenance_budget_limit )
    {
      processing( _item, maintenance_budget_limit );
      break;
    }
    else
    {
      processing( _item, period_pay );
      maintenance_budget_limit -= period_pay;
    }
  }
}

void sps_processor::update_settings( const time_point_sec& head_time )
{
  db.modify( db.get_dynamic_global_properties(), [&]( dynamic_global_property_object& _dgpo )
  {
    //shifting from current time instead of proper maintenance time causes drift
    //when maintenance block was missed, but the fix is problematic - see MR!168 for details
    _dgpo.next_maintenance_time = head_time + fc::seconds( HIVE_PROPOSAL_MAINTENANCE_PERIOD );
    _dgpo.last_budget_time = head_time;
  } );
}

void sps_processor::remove_old_proposals( const block_notification& note )
{
  auto head_time = note.get_block_timestamp();

  if( db.get_benchmark_dumper().is_enabled() )
    db.get_benchmark_dumper().begin();

  remove_proposals( head_time );

  if( db.get_benchmark_dumper().is_enabled() ) //we can't count it per proposal so it belongs to block context measurements
    db.get_benchmark_dumper().end( "block", sps_processor::removing_name );
}

void sps_processor::make_payments( const block_notification& note )
{
  auto head_time = note.get_block_timestamp();

  //Check maintenance period
  if( !is_maintenance_period( head_time ) )
    return;

  if( db.get_benchmark_dumper().is_enabled() )
    db.get_benchmark_dumper().begin();

  t_proposals active_proposals;
  t_proposals no_active_yet_proposals;

  //Find all active proposals, where actual_time >= start_date and actual_time <= end_date
  find_proposals( head_time, active_proposals, no_active_yet_proposals );
  if( active_proposals.empty() )
  {
    calculate_votes( no_active_yet_proposals );

    //Set `new maintenance time` and `last budget time`
    update_settings( head_time );

    if( db.get_benchmark_dumper().is_enabled() ) //we can't count it per proposal so it belongs to block context measurements
      db.get_benchmark_dumper().end( "block", sps_processor::calculating_name );
    return;
  }

  //Calculate total_votes for every active proposal
  calculate_votes( active_proposals );

  //Calculate total_votes for every proposal that isn't active yet. It's only for presentation/statistics
  calculate_votes( no_active_yet_proposals );

  //Sort all active proposals by total_votes
  sort_by_votes( active_proposals );

  //Calculate budget for given maintenance period
  asset maintenance_budget_limit = calculate_maintenance_budget( head_time );

  //Execute transfer for every active proposal
  transfer_payments( head_time, maintenance_budget_limit, active_proposals );

  //Set `new maintenance time` and `last budget time`
  update_settings( head_time );

  if( db.get_benchmark_dumper().is_enabled() ) //we can't count it per proposal so it belongs to block context measurements
    db.get_benchmark_dumper().end( "block", sps_processor::calculating_name );
}

const std::string& sps_processor::get_removing_name()
{
  return removing_name;
}

const std::string& sps_processor::get_calculating_name()
{
  return calculating_name;
}

void sps_processor::run( const block_notification& note )
{
  remove_old_proposals( note );
  record_funding( note );
  convert_funds( note );
  make_payments( note );
}

void sps_processor::record_funding( const block_notification& note )
{
  if( !is_maintenance_period( note.get_block_timestamp() ) )
    return;

  const auto& props = db.get_dynamic_global_properties();

  if ( props.sps_interval_ledger.amount.value <= 0 )
    return;

  operation vop = dhf_funding_operation( db.get_treasury_name(), props.sps_interval_ledger );
  db.push_virtual_operation( vop );

  db.modify( props, []( dynamic_global_property_object& dgpo )
  {
    dgpo.sps_interval_ledger = asset( 0, HBD_SYMBOL );
  });
}

void sps_processor::convert_funds( const block_notification& note )
{
  auto block_ts = note.get_block_timestamp();
  if( !is_daily_maintenance_period( block_ts ))
    return;

  db.modify( db.get_dynamic_global_properties(), [&]( dynamic_global_property_object& _dgpo )
  {
    //shifting from current time instead of proper maintenance time causes drift
    //when maintenance block was missed, but the fix is problematic - see MR!168 for details
    _dgpo.next_daily_maintenance_time = block_ts + fc::seconds( HIVE_DAILY_PROPOSAL_MAINTENANCE_PERIOD );
  } );

  const auto &treasury_account = db.get_treasury();
  if (treasury_account.balance.amount == 0)
    return;

  const auto to_convert = asset(HIVE_PROPOSAL_CONVERSION_RATE * treasury_account.balance.amount / HIVE_100_PERCENT, HIVE_SYMBOL);

  const feed_history_object& fhistory = db.get_feed_history();
  if( fhistory.current_median_history.is_null() )
    return;

  auto converted_hbd = to_convert * fhistory.current_median_history;
  // Don't convert if the conversion would result in an amount lower than the dust threshold
  if(converted_hbd < HIVE_MIN_PAYOUT_HBD )
    return;

  db.adjust_balance( treasury_account, -to_convert );
  db.adjust_balance( treasury_account, converted_hbd );

  db.adjust_supply( -to_convert );
  db.adjust_supply( converted_hbd );

  operation vop = dhf_conversion_operation( treasury_account.name, to_convert, converted_hbd );
  db.push_virtual_operation( vop );
}

} } // namespace hive::chain
