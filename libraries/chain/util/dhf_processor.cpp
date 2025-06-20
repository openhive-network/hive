#include <hive/chain/util/dhf_processor.hpp>

namespace hive { namespace chain {

using hive::protocol::asset;
using hive::protocol::dhf_conversion_operation;
using hive::protocol::dhf_funding_operation;
using hive::protocol::operation;

using hive::chain::proposal_object;
using hive::chain::by_start_date;
using hive::chain::by_end_date;
using hive::chain::proposal_index;
using hive::chain::proposal_id_type;
using hive::chain::proposal_vote_index;
using hive::chain::by_proposal_voter;
using hive::protocol::proposal_pay_operation;
using hive::chain::dhf_helper;
using hive::chain::dynamic_global_property_object;
using hive::chain::block_notification;

const std::string dhf_processor::removing_name = "dhf_processor_remove";
const std::string dhf_processor::calculating_name = "dhf_processor_calculate";

bool dhf_processor::is_maintenance_period( const time_point_sec& head_time ) const
{
  auto due_time = db.get_dynamic_global_properties().next_maintenance_time;
  return due_time <= head_time;
}

bool dhf_processor::is_daily_maintenance_period( const time_point_sec& head_time ) const
{
  /// No DHF conversion until HF24 !
  if( !db.has_hardfork( HIVE_HARDFORK_1_24 ) )
    return false;
  auto due_time = db.get_dynamic_global_properties().next_daily_maintenance_time;
  return due_time <= head_time;
}

void dhf_processor::remove_proposals( const time_point_sec& head_time )
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
      dhf_helper::remove_proposal( proposal, byVoterIdx, db, obj_perf );
    if( obj_perf.done() )
      break;
  }
}

void dhf_processor::find_proposals( const time_point_sec& head_time, t_proposals& active_proposals, t_proposals& no_active_yet_proposals )
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

uint64_t dhf_processor::calculate_votes( uint32_t pid )
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
      auto sum = _voter.get_governance_vote_power();
      ret += sum.value;
    }

    ++found;
  }

  return ret;
}

void dhf_processor::calculate_votes( const t_proposals& proposals )
{
  // Use weighted calculation if hardfork is active
  // TODO: Replace with actual hardfork constant when defined
  if( false ) // db.has_hardfork( HIVE_HARDFORK_DHF_VOTE_WEIGHTING )
  {
    calculate_votes_with_weighting( proposals );
  }
  else
  {
    // Legacy calculation for backwards compatibility
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
}

void dhf_processor::sort_by_votes( t_proposals& proposals )
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

asset dhf_processor::get_treasury_fund()
{
  auto& treasury_account = db.get_treasury();

  return treasury_account.get_hbd_balance();
}

asset dhf_processor::calculate_maintenance_budget( const time_point_sec& head_time )
{
  //Get funds from 'treasury' account ( treasury_fund )
  asset treasury_fund = get_treasury_fund();

  //Calculate budget for given maintenance period
  uint32_t passed_time_seconds = ( head_time - db.get_dynamic_global_properties().last_budget_time ).to_seconds();

  //Calculate daily_budget_limit
  int64_t daily_budget_limit = treasury_fund.amount.value / total_amount_divider;

  daily_budget_limit = fc::uint128_to_uint64( ( uint128_t( passed_time_seconds ) * daily_budget_limit ) / daily_seconds );

  return asset( daily_budget_limit, treasury_fund.symbol );
}

void dhf_processor::transfer_payments( const time_point_sec& head_time, asset& maintenance_budget_limit, const t_proposals& proposals )
{
  if( maintenance_budget_limit.amount.value == 0 )
    return;

  const auto& treasury_account = db.get_treasury();

  uint32_t passed_time_seconds = ( head_time - db.get_dynamic_global_properties().last_budget_time ).to_seconds();

  auto processing = [this, &treasury_account]( const proposal_object& _item, const asset& payment )
  {
    const auto& receiver_account = db.get_account( _item.receiver );

    operation vop = proposal_pay_operation( _item.proposal_id, _item.receiver, db.get_treasury_name(), payment );
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
      period_pay = asset( fc::uint128_to_uint64( ratio * _item.daily_pay.amount.value ) / HIVE_100_PERCENT, _item.daily_pay.symbol );
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

void dhf_processor::update_settings( const time_point_sec& head_time )
{
  db.modify( db.get_dynamic_global_properties(), [&]( dynamic_global_property_object& _dgpo )
  {
    //shifting from current time instead of proper maintenance time causes drift
    //when maintenance block was missed, but the fix is problematic - see MR!168 for details
    _dgpo.next_maintenance_time = head_time + fc::seconds( HIVE_PROPOSAL_MAINTENANCE_PERIOD );
    _dgpo.last_budget_time = head_time;
  } );
}

void dhf_processor::remove_old_proposals( const block_notification& note )
{
  auto head_time = note.get_block_timestamp();

  if( db.get_benchmark_dumper().is_enabled() )
    db.get_benchmark_dumper().begin();

  remove_proposals( head_time );

  if( db.get_benchmark_dumper().is_enabled() ) //we can't count it per proposal so it belongs to block context measurements
    db.get_benchmark_dumper().end( "block", dhf_processor::removing_name );
}

void dhf_processor::make_payments( const block_notification& note )
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
      db.get_benchmark_dumper().end( "block", dhf_processor::calculating_name );
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
    db.get_benchmark_dumper().end( "block", dhf_processor::calculating_name );
}

const std::string& dhf_processor::get_removing_name()
{
  return removing_name;
}

const std::string& dhf_processor::get_calculating_name()
{
  return calculating_name;
}

void dhf_processor::run( const block_notification& note )
{
  remove_old_proposals( note );
  record_funding( note );
  convert_funds( note );
  make_payments( note );
}

void dhf_processor::record_funding( const block_notification& note )
{
  if( !is_maintenance_period( note.get_block_timestamp() ) )
    return;

  const auto& props = db.get_dynamic_global_properties();

  if ( props.dhf_interval_ledger.amount.value <= 0 )
    return;

  operation vop = dhf_funding_operation( db.get_treasury_name(), props.dhf_interval_ledger );
  db.push_virtual_operation( vop );

  // Track daily inflows before resetting ledger
  update_daily_inflow_tracker( props.dhf_interval_ledger );

  db.modify( props, []( dynamic_global_property_object& dgpo )
  {
    dgpo.dhf_interval_ledger = asset( 0, HBD_SYMBOL );
  });
}

void dhf_processor::convert_funds( const block_notification& note )
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

  operation vop = dhf_conversion_operation( treasury_account.get_name(), to_convert, converted_hbd );
  db.push_virtual_operation( vop );
}

asset dhf_processor::get_daily_dhf_inflow()
{
  const auto& props = db.get_dynamic_global_properties();
  return props.dhf_cached_daily_total;
}

void dhf_processor::update_daily_inflow_tracker( const asset& hourly_amount )
{
  db.modify( db.get_dynamic_global_properties(), [&]( dynamic_global_property_object& dgpo )
  {
    // Advance to next hour slot
    dgpo.dhf_current_hour_index = (dgpo.dhf_current_hour_index + 1) % 24;
    
    // Subtract old value from cached total
    dgpo.dhf_cached_daily_total -= dgpo.dhf_hourly_inflows[dgpo.dhf_current_hour_index];
    
    // Add new hourly amount
    dgpo.dhf_hourly_inflows[dgpo.dhf_current_hour_index] = hourly_amount;
    dgpo.dhf_cached_daily_total += hourly_amount;
    
    dgpo.dhf_inflow_last_update = db.head_block_time();
  });
}

uint32_t dhf_processor::calculate_vote_weight_multiplier( const account_name_type& voter, const asset& total_commitment, const asset& daily_inflow, uint32_t minimum_weight )
{
  if( total_commitment.amount <= daily_inflow.amount )
    return HIVE_100_PERCENT; // Full weight if under or at budget
  
  // Calculate proportional reduction if over budget
  uint32_t proportional_weight = fc::uint128_to_uint64( 
    ( uint128_t( daily_inflow.amount.value ) * HIVE_100_PERCENT ) / total_commitment.amount.value 
  );
  
  // Return the higher of proportional weight or minimum weight
  return std::max( proportional_weight, minimum_weight );
}

void dhf_processor::update_voter_dhf_commitment( const account_name_type& voter )
{
  const auto& voter_account = db.get_account( voter );
  const auto& pvidx = db.get_index< proposal_vote_index >().indices().get< by_voter_proposal >();
  const auto& pidx = db.get_index< proposal_index >().indices().get< by_proposal_id >();
  
  asset total_commitment( 0, HBD_SYMBOL );
  uint16_t active_count = 0;
  auto daily_inflow = get_daily_dhf_inflow();
  auto treasury_fund = get_treasury_fund();
  auto sustainable_daily_rate = treasury_fund.amount > 0 ? treasury_fund.amount / 100 : 0;
  
  auto vote_range = pvidx.equal_range( voter );
  for( auto vote_itr = vote_range.first; vote_itr != vote_range.second; ++vote_itr )
  {
    auto proposal_itr = pidx.find( vote_itr->proposal_id );
    if( proposal_itr != pidx.end() && 
        db.head_block_time() >= proposal_itr->start_date && 
        db.head_block_time() <= proposal_itr->end_date )
    {
      active_count++;
      
      // Cap large proposals at sustainable daily rate (fund/100)
      if( proposal_itr->daily_pay.amount > sustainable_daily_rate ) {
        // Large proposals are capped at sustainable daily rate (fund/100)
        total_commitment += asset( sustainable_daily_rate, HBD_SYMBOL );
      } else {
        total_commitment += proposal_itr->daily_pay;
      }
    }
  }
  
  db.modify( voter_account, [&]( account_object& a )
  {
    a.dhf_total_daily_commitment = total_commitment;
    a.dhf_active_proposal_count = active_count;
    a.dhf_commitment_last_update = db.head_block_time();
  });
}

void dhf_processor::calculate_votes_with_weighting( const t_proposals& proposals )
{
  auto daily_inflow = get_daily_dhf_inflow();
  auto treasury_fund = get_treasury_fund();
  auto sustainable_daily_rate = treasury_fund.amount > 0 ? treasury_fund.amount / 100 : 0;

  // Track voter commitments during vote calculation
  std::unordered_map<account_name_type, asset> voter_commitments;
  std::unordered_set<account_name_type> flagged_voters;
  std::unordered_set<account_name_type> voters_with_large_proposal_counted;
  
  // Track highest raw vote total to calculate minimum weight floor
  uint64_t highest_raw_vote_total = 0;

  // Phase 1: Calculate raw votes + track commitments
  for( auto& item : proposals )
  {
    const proposal_object& _item = item;
    uint64_t raw_total_votes = 0;

    // Optimization: determine proposal's commitment value once
    asset proposal_commitment;
    bool is_large_proposal = _item.daily_pay.amount > sustainable_daily_rate;
    if( is_large_proposal ) {
      proposal_commitment = asset( sustainable_daily_rate, HBD_SYMBOL );
    } else {
      proposal_commitment = asset( _item.daily_pay.amount, HBD_SYMBOL );
    }

    const auto& pvidx = db.get_index< proposal_vote_index >().indices().get< by_proposal_voter >();
    auto found = pvidx.find( _item.proposal_id );

    while( found != pvidx.end() && found->proposal_id == _item.proposal_id )
    {
      const auto& _voter = db.get_account( found->voter );

      if( !_voter.has_proxy() )
      {
        auto vote_power = _voter.get_governance_vote_power();
        raw_total_votes += vote_power.value;

        // A voter's commitment only includes one large proposal
        asset commitment_to_add = proposal_commitment;
        if( is_large_proposal ) {
          if( voters_with_large_proposal_counted.count( found->voter ) ) {
            commitment_to_add.amount = 0; // Already counted a large one, add 0
          } else {
            voters_with_large_proposal_counted.insert( found->voter ); // Counted it, mark them
          }
        }

        if (commitment_to_add.amount > 0) {
          voter_commitments[found->voter] += commitment_to_add;
        }

        // Flag if over budget
        if( voter_commitments.count( found->voter ) && voter_commitments[found->voter] > daily_inflow ) {
          flagged_voters.insert(found->voter);
        }
      }
      ++found;
    }
    
    // Track highest raw vote total for minimum weight calculation
    highest_raw_vote_total = std::max( highest_raw_vote_total, raw_total_votes );

    // Store raw votes temporarily
    db.modify( _item, [&]( auto& proposal ) {
      proposal.total_votes = raw_total_votes;
    });
  }
  
  // Calculate minimum weight floor based on highest raw vote total and total vesting shares
  uint32_t minimum_weight = 0;
  if( flagged_voters.size() > 0 && highest_raw_vote_total > 0 ) {
    const auto& props = db.get_dynamic_global_properties();
    auto total_vesting_shares = props.get_total_vesting_shares();
    
    if( total_vesting_shares.amount.value > 0 ) {
      // Minimum weight = (highest_raw_vote_total / total_vesting_shares) * HIVE_100_PERCENT
      // This ensures if everyone votes, the minimum weight approaches 1
      minimum_weight = fc::uint128_to_uint64( 
        ( uint128_t( highest_raw_vote_total ) * HIVE_100_PERCENT ) / total_vesting_shares.amount.value 
      );
    }
  }

  if( flagged_voters.empty() ) {
    // No reweighting needed. Raw votes are final. We are done.
  } else {
    // OPTIMIZATION: Prune the list of proposals to only those affected by flagged voters.
    std::unordered_set<proposal_id_type> proposals_with_flagged_voters;
    const auto& pvidx_by_voter = db.get_index< proposal_vote_index >().indices().get< by_voter_proposal >();
    for( const auto& voter : flagged_voters )
    {
      auto vote_range = pvidx_by_voter.equal_range( voter );
      for( auto vote_itr = vote_range.first; vote_itr != vote_range.second; ++vote_itr )
      {
        proposals_with_flagged_voters.insert( vote_itr->proposal_id );
      }
    }

    // --- PHASE 2: EFFICIENT "DELTA" REWEIGHTING ---
    for( const auto& item : proposals )
    {
      // This is the core of the optimization: skip any proposal with no flagged voters.
      if( proposals_with_flagged_voters.find( item.get().proposal_id ) == proposals_with_flagged_voters.end() )
          continue;

      const proposal_object& _item = item;
      uint64_t total_reduction = 0;

      const auto& pvidx = db.get_index< proposal_vote_index >().indices().get< by_proposal_voter >();
      auto found = pvidx.find( _item.proposal_id );

      while( found != pvidx.end() && found->proposal_id == _item.proposal_id )
      {
        // We only care about voters that were flagged as over-committed
        if( flagged_voters.count(found->voter) )
        {
            const auto& _voter = db.get_account( found->voter );
            if ( !_voter.has_proxy() )
            {
                auto base_vote_power = _voter.get_governance_vote_power();
                auto voter_commitment = voter_commitments.at(found->voter);

                FC_ASSERT( voter_commitment.amount.value > 0, "Voter commitment must be positive to calculate weight" );
                uint32_t weight_multiplier = calculate_vote_weight_multiplier(found->voter, voter_commitment, daily_inflow, minimum_weight);

                // This is the key part: calculate how much vote power is *lost*
                uint128_t reduction_multiplier = HIVE_100_PERCENT - weight_multiplier;
                uint64_t vote_reduction = fc::uint128_to_uint64(
                  ( uint128_t(base_vote_power.value) * reduction_multiplier ) / HIVE_100_PERCENT
                );
                total_reduction += vote_reduction;
            }
        }
        ++found;
      }

      // If any voters were flagged, apply the total reduction to the raw sum.
      if( total_reduction > 0 ) {
        db.modify( _item, [&]( auto& proposal ) {
          // proposal.total_votes contains the raw vote sum from Phase 1
          if( proposal.total_votes >= total_reduction )
            proposal.total_votes -= total_reduction;
          else
            proposal.total_votes = 0; // Safety check for underflow
        });
      }
    }
  }
  
  // Update voter commitment stats for all voters with changed votes
  for( const auto& voter_commitment : voter_commitments )
  {
    const auto& voter_account = db.get_account( voter_commitment.first );
    db.modify( voter_account, [&]( account_object& a )
    {
      a.dhf_total_daily_commitment = voter_commitment.second;
      a.dhf_commitment_last_update = db.head_block_time();
    });
  }
}

} } // namespace hive::chain
