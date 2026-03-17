#include <hive/plugins/database_api/database_api_objects.hpp>

#include <hive/chain/account_object.hpp>
#include <hive/chain/block_summary_object.hpp>
#include <hive/chain/hardfork_property_object.hpp>
#include <hive/chain/comment_object.hpp>
#include <hive/chain/global_property_object.hpp>
#include <hive/chain/detail/state/convert_request_object.hpp>
#include <hive/chain/detail/state/collateralized_convert_request_object.hpp>
#include <hive/chain/detail/state/escrow_object.hpp>
#include <hive/chain/detail/state/savings_withdraw_object.hpp>
#include <hive/chain/detail/state/feed_history_object.hpp>
#include <hive/chain/detail/state/limit_order_object.hpp>
#include <hive/chain/detail/state/withdraw_vesting_route_object.hpp>
#include <hive/chain/detail/state/decline_voting_rights_request_object.hpp>
#include <hive/chain/detail/state/reward_fund_object.hpp>
#include <hive/chain/detail/state/recurrent_transfer_object.hpp>
#include <hive/chain/dhf_objects.hpp>
#include <hive/chain/dhf_objects_multiindex.hpp>
#include <hive/chain/transaction_object.hpp>
#include <hive/chain/witness_objects.hpp>
#include <hive/chain/database.hpp>
#include <hive/chain/notifications.hpp>

namespace hive { namespace plugins { namespace database_api {

using namespace hive::chain;
using namespace hive::protocol;

//////////////////////////////////////////////////////////////////////
//                                                                  //
// Constructor implementations for database_api_objects            //
//                                                                  //
//////////////////////////////////////////////////////////////////////

// api_reward_fund_object constructor
api_reward_fund_object::api_reward_fund_object( const reward_fund_object& o, const database& db ):
  id( o.get_id() ),
  name( o.name ),
  reward_balance( o.reward_balance.to_asset() ),
  recent_claims( o.recent_claims ),
  last_update( o.last_update ),
  content_constant( o.content_constant ),
  percent_curation_rewards( o.percent_curation_rewards ),
  percent_content_rewards( o.percent_content_rewards ),
  author_reward_curve( o.author_reward_curve ),
  curation_reward_curve( o.curation_reward_curve )
{}

// api_witness_vote_object constructor
api_witness_vote_object::api_witness_vote_object( const witness_vote_object& o, const database& db ):
  id( o.get_id() ),
  witness( o.witness ),
  account( o.account )
{}

// api_escrow_object constructor
api_escrow_object::api_escrow_object( const escrow_object& o, const database& db ):
  id( o.get_id() ),
  escrow_id( o.get_escrow_id() ),
  from( o.get_from() ),
  to( o.get_to() ),
  agent( o.get_agent() ),
  ratification_deadline( o.get_ratification_deadline() ),
  escrow_expiration( o.get_escrow_expiration() ),
  hbd_balance( o.get_hbd_balance().to_asset() ),
  hive_balance( o.get_hive_balance().to_asset() ),
  pending_fee( o.get_fee() ),
  to_approved( o.is_to_approved() ),
  agent_approved( o.is_agent_approved() ),
  disputed( o.is_disputed() )
{}

// api_withdraw_vesting_route_object constructors
api_withdraw_vesting_route_object::api_withdraw_vesting_route_object( const withdraw_vesting_route_object& o, const database& db ):
  id( o.get_id() ),
  from_account( o.from_account ),
  to_account( o.to_account ),
  percent( o.percent ),
  auto_vest( o.auto_vest )
{}


// api_vesting_delegation_object constructor
api_vesting_delegation_object::api_vesting_delegation_object( const vesting_delegation_object& o, const database& db ):
  id( o.get_id() ),
  delegator( db.get_account( o.get_delegator() ).get_name() ),
  delegatee( db.get_account( o.get_delegatee() ).get_name() ),
  vesting_shares( o.get_vesting().to_asset() ),
  min_delegation_time( o.get_min_delegation_time() )
{}

// api_vesting_delegation_expiration_object constructor
api_vesting_delegation_expiration_object::api_vesting_delegation_expiration_object( const vesting_delegation_expiration_object& o, const database& db ):
  id( o.get_id() ),
  delegator( db.get_account( o.get_delegator() ).get_name() ),
  vesting_shares( o.get_vesting().to_asset() ),
  expiration( o.get_expiration_time() )
{}

// api_convert_request_object constructors

api_convert_request_object::api_convert_request_object( const convert_request_object& o, const database& db ):
  id( o.get_id() ),
  owner( db.get_account( o.get_owner() ).get_name() ),
  requestid( o.get_request_id() ),
  amount( o.get_convert_amount().to_asset() ),
  conversion_date( o.get_conversion_date() )
{}

// api_collateralized_convert_request_object constructors

api_collateralized_convert_request_object::api_collateralized_convert_request_object( const collateralized_convert_request_object& o, const database& db ) :
  id( o.get_id() ),
  owner( db.get_account( o.get_owner() ).get_name() ),
  requestid( o.get_request_id() ),
  collateral_amount( o.get_collateral_amount().to_asset() ),
  converted_amount( o.get_converted_amount().to_asset() ),
  conversion_date( o.get_conversion_date() )
{}

// api_decline_voting_rights_request_object constructor
api_decline_voting_rights_request_object::api_decline_voting_rights_request_object( const decline_voting_rights_request_object& o, const database& db ):
  id( o.get_id() ),
  account( o.account ),
  effective_date( o.effective_date )
{}

// api_limit_order_object constructors
api_limit_order_object::api_limit_order_object( const limit_order_object& o, const database& db ):
  id( o.get_id() ),
  created( o.created ),
  expiration( o.expiration ),
  seller( o.seller ),
  orderid( o.orderid ),
  for_sale( o.for_sale.amount ),
  sell_price( o.sell_price )
{}


// api_dynamic_global_property_object constructors
api_dynamic_global_property_object::api_dynamic_global_property_object( const dynamic_global_property_object& o, const database& db ) :
  id( o.get_id() ),
  head_block_number( o.get_head_block_number() ),
  head_block_id( o.get_head_block_id() ),
  time( o.get_head_block_time() ),
  current_witness( o.get_current_witness() ),
  total_pow( o.get_total_pow() ),
  num_pow_witnesses( o.get_current_pow_witnesses() ),
  virtual_supply( o.get_virtual_supply().to_asset() ),
  current_supply( o.get_current_supply().to_asset() ),
  init_hbd_supply( o.get_initial_hbd_supply().to_asset() ),
  current_hbd_supply( o.get_current_hbd_supply().to_asset() ),
  total_vesting_fund_hive( o.get_total_vesting_fund_hive().to_asset() ),
  total_vesting_shares( o.get_total_vesting_shares().to_asset() ),
  total_reward_fund_hive( o.get_total_reward_fund_hive().to_asset() ),
  total_reward_shares2( o.get_total_reward_shares2() ),
  pending_rewarded_vesting_shares( o.get_pending_rewarded_vesting_shares().to_asset() ),
  pending_rewarded_vesting_hive( o.get_pending_rewarded_vesting_hive().to_asset() ),
  hbd_interest_rate( o.get_hbd_interest_rate() ),
  hbd_print_rate( o.get_hbd_print_rate() ),
  maximum_block_size( o.get_maximum_block_size() ),
  current_aslot( o.get_current_aslot() ),
  recent_slots_filled( o.get_recent_slots_filled() ),
  participation_count( o.get_participation_count() ),
  last_irreversible_block_num( db.get_last_irreversible_block_num()),
  vote_power_reserve_rate( o.get_vote_power_reserve_rate() ),
  delegation_return_period( o.get_delegation_return_period() ),
  reverse_auction_seconds( o.get_reverse_auction_seconds() ),
  available_account_subsidies( o.get_available_account_subsidies() ),
  hbd_stop_percent( o.get_hbd_stop_percent() ),
  hbd_start_percent( o.get_hbd_start_percent() ),
  next_maintenance_time( o.get_next_maintenance_time() ),
  last_budget_time( o.get_last_budget_time() ),
  next_daily_maintenance_time( o.get_next_daily_maintenance_time() ),
  content_reward_percent( o.get_content_reward_percent() ),
  vesting_reward_percent( o.get_vesting_reward_percent() ),
  proposal_fund_percent( o.get_proposal_fund_percent() ),
  dhf_interval_ledger( o.get_dhf_interval_ledger().to_asset() ),
  downvote_pool_percent( o.get_downvote_pool_percent() ),
  current_remove_threshold( o.get_current_remove_threshold() ),
  early_voting_seconds( o.get_early_voting_seconds() ),
  mid_voting_seconds( o.get_mid_voting_seconds() ),
  max_consecutive_recurrent_transfer_failures( o.get_max_consecutive_recurrent_transfer_failures() ),
  max_recurrent_transfer_end_date( o.get_max_recurrent_transfer_end_date() ),
  min_recurrent_transfers_recurrence( o.get_min_recurrent_transfers_recurrence() ),
  max_open_recurrent_transfers( o.get_max_open_recurrent_transfers() )
{}


// api_change_recovery_account_request_object constructor
api_change_recovery_account_request_object::api_change_recovery_account_request_object( const change_recovery_account_request_object& o, const database& db ):
  id( o.get_id() ),
  account_to_recover( o.get_account_to_recover() ),
  recovery_account( o.get_recovery_account() ),
  effective_on( o.get_execution_time() )
{}

// api_account_object constructors
api_account_object::api_account_object( const account_object& a, const database& db, const metadata::metadata_plugin* metadata_plugin, bool delayed_votes_active ) :
  id( a.get_id() ),
  name( a.get_name() ),
  memo_key( a.memo_key ),
  proxy( HIVE_PROXY_TO_SELF_ACCOUNT ),
  last_account_update( a.last_account_update ),
  created( a.get_block_creation_time() ),
  mined( a.was_mined() ),
  reset_account( HIVE_NULL_ACCOUNT ),
  last_account_recovery( a.get_block_last_account_recovery_time() ),
  post_count( a.post_count ),
  can_vote( a.can_vote ),
  voting_manabar( a.voting_manabar ),
  downvote_manabar( a.downvote_manabar ),
  balance( a.get_hive_balance().to_asset() ),
  savings_balance( a.get_hive_savings().to_asset() ),
  hbd_balance( a.hbd_balance.to_asset() ),
  hbd_seconds( a.hbd_seconds ),
  hbd_seconds_last_update( a.hbd_seconds_last_update ),
  hbd_last_interest_payment( a.hbd_last_interest_payment ),
  savings_hbd_balance( a.get_hbd_savings().to_asset() ),
  savings_hbd_seconds( a.savings_hbd_seconds ),
  savings_hbd_seconds_last_update( a.savings_hbd_seconds_last_update ),
  savings_hbd_last_interest_payment( a.savings_hbd_last_interest_payment ),
  savings_withdraw_requests( a.savings_withdraw_requests ),
  reward_hbd_balance( a.get_hbd_rewards().to_asset() ),
  reward_hive_balance( a.get_hive_rewards().to_asset() ),
  reward_vesting_balance( a.get_vest_rewards().to_asset() ),
  reward_vesting_hive( a.get_vest_rewards_as_hive().to_asset() ),
  curation_rewards( a.curation_rewards.amount ),
  posting_rewards( a.posting_rewards.amount ),
  vesting_shares( a.vesting_shares.to_asset() ),
  delegated_vesting_shares( a.delegated_vesting_shares.to_asset() ),
  received_vesting_shares( a.received_vesting_shares.to_asset() ),
  vesting_withdraw_rate( a.vesting_withdraw_rate.to_asset() ),
  next_vesting_withdrawal( a.next_vesting_withdrawal ),
  withdrawn( a.withdrawn.amount ),
  to_withdraw( a.to_withdraw.amount ),
  withdraw_routes( a.withdraw_routes ),
  pending_transfers( a.pending_escrow_transfers ),
  witnesses_voted_for( a.witnesses_voted_for ),
  last_post( a.last_post ),
  last_root_post( a.last_root_post ),
  last_post_edit( a.last_post_edit ),
  last_vote_time( a.last_vote_time ),
  post_bandwidth( a.post_bandwidth ),
  pending_claimed_accounts( a.pending_claimed_accounts ),
  open_recurrent_transfers( a.open_recurrent_transfers ),
  governance_vote_expiration_ts( a.get_governance_vote_expiration_ts())
{
  if( a.has_proxy() )
    proxy = db.get_account( a.get_proxy() ).get_name();
  if( a.has_recovery_account() )
    recovery_account = db.get_account( a.get_recovery_account() ).get_name();

  size_t n = a.proxied_vsf_votes.size();
  proxied_vsf_votes.reserve( n );
  for( size_t i=0; i<n; i++ )
    proxied_vsf_votes.push_back( a.proxied_vsf_votes[i] );

  const auto& auth = db.get< account_authority_object, by_account >( name );
  owner = authority( auth.owner );
  active = authority( auth.active );
  posting = authority( auth.posting );
  previous_owner_update = auth.previous_owner_update;
  last_owner_update = auth.last_owner_update;
  if( metadata_plugin )
  {
    auto meta = metadata_plugin->get_account_metadata( a );
    json_metadata = meta.json_metadata;
    posting_json_metadata = meta.posting_json_metadata;
  }

  if( delayed_votes_active )
    delayed_votes = vector< delayed_votes_data >{ a.delayed_votes.begin(), a.delayed_votes.end() };

  post_voting_power = a.get_effective_vesting_shares().to_asset();
}


// api_owner_authority_history_object constructors
api_owner_authority_history_object::api_owner_authority_history_object( const owner_authority_history_object& o, const database& db ) :
  id( o.get_id() ),
  account( o.account ),
  previous_owner_authority( authority( o.previous_owner_authority ) ),
  last_valid_time( o.last_valid_time )
{}


// api_account_recovery_request_object constructors
api_account_recovery_request_object::api_account_recovery_request_object( const account_recovery_request_object& o, const database& db ) :
  id( o.get_id() ),
  account_to_recover( o.get_account_to_recover() ),
  new_owner_authority( authority( o.get_new_owner_authority() ) ),
  expires( o.get_expiration_time() )
{}


// api_savings_withdraw_object constructors
api_savings_withdraw_object::api_savings_withdraw_object( const savings_withdraw_object& o, const database& db ) :
  id( o.get_id() ),
  from( o.from ),
  to( o.to ),
  memo( to_string( o.memo ) ),
  request_id( o.request_id ),
  amount( o.amount ),
  complete( o.complete )
{}


// api_feed_history_object constructors
api_feed_history_object::api_feed_history_object( const feed_history_object& f ) :
  id( f.get_id() ),
  current_median_history( f.current_median_history.to_price() ),
  market_median_history( f.market_median_history.to_price() ),
  current_min_history( f.current_min_history.to_price() ),
  current_max_history( f.current_max_history.to_price() )
{
  for( const auto& p : f.price_history )
    price_history.push_back( p.to_price() );
}


// api_witness_object constructors
api_witness_object::api_witness_object( const witness_object& w, const database& db ) :
  id( w.get_id() ),
  owner( w.owner ),
  created( w.created ),
  url( to_string( w.url ) ),
  total_missed( w.total_missed ),
  last_aslot( w.last_aslot ),
  last_confirmed_block_num( w.last_confirmed_block_num ),
  pow_worker( w.pow_worker ),
  signing_key( w.signing_key ),
  props( w.props ),
  hbd_exchange_rate( w.get_hbd_exchange_rate() ),
  last_hbd_exchange_update( w.get_last_hbd_exchange_update() ),
  votes( w.votes ),
  virtual_last_update( w.virtual_last_update ),
  virtual_position( w.virtual_position ),
  virtual_scheduled_time( w.virtual_scheduled_time ),
  last_work( w.last_work ),
  running_version( w.running_version ),
  hardfork_version_vote( w.hardfork_version_vote ),
  hardfork_time_vote( w.hardfork_time_vote ),
  available_witness_account_subsidies( w.available_witness_account_subsidies )
{}

// api_witness_schedule_object constructor
api_witness_schedule_object::api_witness_schedule_object( const witness_schedule_object& wso,
  const witness_schedule_object& future_wso, bool include_future, const database& db ) :
  id( wso.get_id() ),
  current_virtual_time( future_wso.current_virtual_time ),
  next_shuffle_block_num( future_wso.next_shuffle_block_num ),
  num_scheduled_witnesses( wso.num_scheduled_witnesses ),
  elected_weight( wso.elected_weight ),
  timeshare_weight( wso.timeshare_weight ),
  miner_weight( wso.miner_weight ),
  witness_pay_normalization_factor( wso.witness_pay_normalization_factor ),
  median_props( wso.median_props ),
  majority_version( wso.majority_version ),
  max_voted_witnesses( wso.max_voted_witnesses ),
  max_miner_witnesses( wso.max_miner_witnesses ),
  max_runner_witnesses( wso.max_runner_witnesses ),
  hardfork_required_witnesses( wso.hardfork_required_witnesses ),
  account_subsidy_rd( wso.account_subsidy_rd ),
  account_subsidy_witness_rd( wso.account_subsidy_witness_rd ),
  min_witness_account_subsidy_decay( wso.min_witness_account_subsidy_decay )
{
  size_t n = wso.current_shuffled_witnesses.size();
  current_shuffled_witnesses.reserve( n );
  std::transform(wso.current_shuffled_witnesses.begin(), wso.current_shuffled_witnesses.end(),
            std::back_inserter(current_shuffled_witnesses),
            [](const account_name_type& s) -> std::string { return s; } );

  if( include_future )
  {
    n = future_wso.current_shuffled_witnesses.size();
    future_shuffled_witnesses = vector<string>();
    future_shuffled_witnesses->reserve( n );

    std::transform( future_wso.current_shuffled_witnesses.begin(), future_wso.current_shuffled_witnesses.end(),
      std::back_inserter( future_shuffled_witnesses.value() ),
      []( const account_name_type& s ) -> std::string { return s; } );

    future_changes = future_witness_schedule();
    if( !future_changes->fill( wso, future_wso ) )
      future_changes.reset();
  }
}

// api_signed_block_object constructors
api_signed_block_object::api_signed_block_object(const std::shared_ptr<full_block_type>& full_block) :
  signed_block(full_block->get_block()),
  block_id(full_block->get_block_id()),
  signing_key(full_block->get_signing_key())
{
  const std::vector<std::shared_ptr<full_transaction_type>>& full_transactions = full_block->get_full_transactions();
  transaction_ids.reserve(transactions.size());
  for (const std::shared_ptr<full_transaction_type>& full_transaction : full_transactions)
    transaction_ids.push_back(full_transaction->get_transaction_id());
}

// api_hardfork_property_object constructor
api_hardfork_property_object::api_hardfork_property_object( const hardfork_property_object& h ) :
  id( h.get_id() ),
  last_hardfork( h.last_hardfork ),
  current_hardfork_version( h.current_hardfork_version ),
  next_hardfork( h.next_hardfork ),
  next_hardfork_time( h.next_hardfork_time )
{
  size_t n = h.processed_hardforks.size();
  processed_hardforks.reserve( n );

  for( size_t i = 0; i < n; i++ )
    processed_hardforks.push_back( h.processed_hardforks[i] );
}

// Helper function for proposal status - defined in database_api_impl.hpp
proposal_status get_proposal_status( const hive::chain::proposal_object& po, const fc::time_point_sec current_time );

// api_proposal_object constructor
api_proposal_object::api_proposal_object(const proposal_object& po, const time_point_sec& current_time) :
  id(po.get_id()),
  proposal_id(po.proposal_id),
  creator(po.creator),
  receiver(po.receiver),
  start_date(po.start_date),
  end_date(po.end_date),
  daily_pay(po.daily_pay.to_asset()),
  subject(to_string(po.subject)),
  permlink(to_string(po.permlink)),
  total_votes(po.total_votes),
  status(get_proposal_status(po,current_time))
{}

// api_proposal_vote_object constructor
api_proposal_vote_object::api_proposal_vote_object( const proposal_vote_object& pvo, const database& db ) :
  id( pvo.get_id() ),
  voter( pvo.voter ),
  proposal( db.get< proposal_object, by_proposal_id >( pvo.proposal_id ), db.head_block_time() )
{}

// api_recurrent_transfer_object constructor
api_recurrent_transfer_object::api_recurrent_transfer_object( const recurrent_transfer_object& o, const account_name_type from_name, const account_name_type to_name ):
  id( o.get_id() ),
  trigger_date( o.get_trigger_date() ),
  from( from_name ),
  to( to_name ),
  amount( o.amount ),
  memo( to_string(o.memo) ),
  recurrence( o.recurrence ),
  consecutive_failures( o.consecutive_failures ),
  remaining_executions( o.remaining_executions ),
  pair_id( o.pair_id )
{}

} } } // hive::plugins::database_api
