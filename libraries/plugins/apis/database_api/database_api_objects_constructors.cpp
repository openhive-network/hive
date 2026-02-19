#include <hive/plugins/database_api/database_api_objects.hpp>
#include <hive/plugins/metadata_api/metadata_api.hpp>

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
#include <hive/chain/smt_objects.hpp>
#include <hive/chain/dhf_objects.hpp>
#include <hive/chain/dhf_objects_multiindex.hpp>
#include <hive/chain/transaction_object.hpp>
#include <hive/chain/witness_objects.hpp>
#include <hive/chain/database.hpp>
#include <hive/chain/notifications.hpp>
#include <hive/chain/detail/state/assets_object.hpp>
#include <hive/chain/detail/state/manabars_rc_object.hpp>
#include <hive/chain/detail/state/time_object.hpp>
#include <hive/chain/detail/state/delayed_votes_object.hpp>
#include <hive/chain/detail/state/recovery_object.hpp>

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
  reward_balance( o.reward_balance ),
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
  escrow_id( o.escrow_id ),
  from( o.from ),
  to( o.to ),
  agent( o.agent ),
  ratification_deadline( o.ratification_deadline ),
  escrow_expiration( o.escrow_expiration ),
  hbd_balance( o.hbd_balance ),
  hive_balance( o.hive_balance ),
  pending_fee( o.pending_fee ),
  to_approved( o.to_approved ),
  agent_approved( o.agent_approved ),
  disputed( o.disputed )
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
  vesting_shares( o.get_vesting() ),
  min_delegation_time( o.get_min_delegation_time() )
{}

// api_vesting_delegation_expiration_object constructor
api_vesting_delegation_expiration_object::api_vesting_delegation_expiration_object( const vesting_delegation_expiration_object& o, const database& db ):
  id( o.get_id() ),
  delegator( db.get_account( o.get_delegator() ).get_name() ),
  vesting_shares( o.get_vesting() ),
  expiration( o.get_expiration_time() )
{}

// api_convert_request_object constructors

api_convert_request_object::api_convert_request_object( const convert_request_object& o, const database& db ):
  id( o.get_id() ),
  owner( db.get_account( o.get_owner() ).get_name() ),
  requestid( o.get_request_id() ),
  amount( o.get_convert_amount() ),
  conversion_date( o.get_conversion_date() )
{}

// api_collateralized_convert_request_object constructors

api_collateralized_convert_request_object::api_collateralized_convert_request_object( const collateralized_convert_request_object& o, const database& db ) :
  id( o.get_id() ),
  owner( db.get_account( o.get_owner() ).get_name() ),
  requestid( o.get_request_id() ),
  collateral_amount( o.get_collateral_amount() ),
  converted_amount( o.get_converted_amount() ),
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
  for_sale( o.for_sale ),
  sell_price( o.sell_price )
{}


// api_dynamic_global_property_object constructors
api_dynamic_global_property_object::api_dynamic_global_property_object( const dynamic_global_property_object& o, const database& db ) :
  id( o.get_id() ),
  head_block_number( o.head_block_number ),
  head_block_id( o.head_block_id ),
  time( o.time ),
  current_witness( o.current_witness ),
  total_pow( o.total_pow ),
  num_pow_witnesses( o.num_pow_witnesses ),
  virtual_supply( o.virtual_supply ),
  current_supply( o.current_supply ),
  init_hbd_supply( o.init_hbd_supply ),
  current_hbd_supply( o.current_hbd_supply ),
  total_vesting_fund_hive( o.total_vesting_fund_hive ),
  total_vesting_shares( o.total_vesting_shares ),
  total_reward_fund_hive( o.total_reward_fund_hive ),
  total_reward_shares2( o.total_reward_shares2 ),
  pending_rewarded_vesting_shares( o.pending_rewarded_vesting_shares ),
  pending_rewarded_vesting_hive( o.pending_rewarded_vesting_hive ),
  hbd_interest_rate( o.hbd_interest_rate ),
  hbd_print_rate( o.hbd_print_rate ),
  maximum_block_size( o.maximum_block_size ),
  current_aslot( o.current_aslot ),
  recent_slots_filled( o.recent_slots_filled ),
  participation_count( o.participation_count ),
  last_irreversible_block_num( db.get_last_irreversible_block_num()),
  vote_power_reserve_rate( o.vote_power_reserve_rate ),
  delegation_return_period( o.delegation_return_period ),
  reverse_auction_seconds( o.reverse_auction_seconds ),
  available_account_subsidies( o.available_account_subsidies ),
  hbd_stop_percent( o.hbd_stop_percent ),
  hbd_start_percent( o.hbd_start_percent ),
  next_maintenance_time( o.next_maintenance_time ),
  last_budget_time( o.last_budget_time ),
  next_daily_maintenance_time( o.next_daily_maintenance_time ),
  content_reward_percent( o.content_reward_percent ),
  vesting_reward_percent( o.vesting_reward_percent ),
  proposal_fund_percent( o.proposal_fund_percent ),
  dhf_interval_ledger( o.dhf_interval_ledger ),
  downvote_pool_percent( o.downvote_pool_percent ),
  current_remove_threshold( o.current_remove_threshold ),
  early_voting_seconds( o.early_voting_seconds ),
  mid_voting_seconds( o.mid_voting_seconds ),
  max_consecutive_recurrent_transfer_failures( o.max_consecutive_recurrent_transfer_failures ),
  max_recurrent_transfer_end_date( o.max_recurrent_transfer_end_date ),
  min_recurrent_transfers_recurrence( o.min_recurrent_transfers_recurrence ),
  max_open_recurrent_transfers( o.max_open_recurrent_transfers )
#ifdef HIVE_ENABLE_SMT
  , smt_creation_fee( o.smt_creation_fee )
#endif
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
  memo_key( a.get_memo_key() ),
  proxy( HIVE_PROXY_TO_SELF_ACCOUNT ),
  last_account_update(),
  created( a.get_block_creation_time() ),
  mined( a.was_mined() ),
  reset_account( HIVE_NULL_ACCOUNT ),
  last_account_recovery(),
  post_count( a.get_post_count() ),
  can_vote( a.can_vote() ),
  withdraw_routes( a.get_withdraw_routes() ),
  pending_transfers( a.get_pending_escrow_transfers() ),
  witnesses_voted_for( a.get_witnesses_voted_for() ),
  last_post(),
  last_root_post(),
  last_post_edit(),
  post_bandwidth( a.get_post_bandwidth() ),
  pending_claimed_accounts( a.get_pending_claimed_accounts() ),
  open_recurrent_transfers( a.get_open_recurrent_transfers() ),
  governance_vote_expiration_ts( a.get_governance_vote_expiration_ts())
{
  // Get split objects
  const auto& assets = db.get< assets_object, by_account_id >( a.get_id() );
  const auto& mrc = db.get< manabars_rc_object, by_account_id >( a.get_id() );
  const auto& time_obj = db.get< time_object, by_account_id >( a.get_id() );
  const auto& dvotes = db.get< delayed_votes_object, by_account_id >( a.get_id() );
  const auto& recovery = db.get< recovery_object, by_account_id >( a.get_id() );

  // From manabars_rc_object
  voting_manabar = mrc.get_voting_manabar();
  downvote_manabar = mrc.get_downvote_manabar();

  // From time_object
  last_vote_time = time_obj.get_last_vote_time();
  last_account_update = time_obj.get_last_account_update();
  last_post = time_obj.get_last_post();
  last_root_post = time_obj.get_last_root_post();
  last_post_edit = time_obj.get_last_post_edit();
  next_vesting_withdrawal = time_obj.get_next_vesting_withdrawal();
  last_account_recovery = recovery.get_block_last_account_recovery_time();

  // From assets_object
  balance = assets.get_balance();
  savings_balance = assets.get_savings();
  hbd_balance = assets.get_hbd_balance();
  hbd_seconds = time_obj.get_hbd_seconds();
  hbd_seconds_last_update = time_obj.get_hbd_seconds_last_update();
  hbd_last_interest_payment = time_obj.get_hbd_last_interest_payment();
  savings_hbd_balance = assets.get_hbd_savings();
  savings_hbd_seconds = assets.get_savings_hbd_seconds();
  savings_hbd_seconds_last_update = assets.get_savings_hbd_seconds_last_update();
  savings_hbd_last_interest_payment = assets.get_savings_hbd_last_interest_payment();
  savings_withdraw_requests = a.get_savings_withdraw_requests();
  reward_hbd_balance = assets.get_hbd_rewards();
  reward_hive_balance = assets.get_rewards();
  reward_vesting_balance = assets.get_vest_rewards();
  reward_vesting_hive = assets.get_vest_rewards_as_hive();
  curation_rewards = assets.get_curation_rewards().amount;
  posting_rewards = assets.get_posting_rewards().amount;
  vesting_shares = assets.get_vesting();
  delegated_vesting_shares = assets.get_delegated_vesting();
  received_vesting_shares = assets.get_received_vesting();
  vesting_withdraw_rate = assets.get_vesting_withdraw_rate();
  withdrawn = assets.get_withdrawn().amount;
  to_withdraw = assets.get_to_withdraw().amount;

  if( a.has_proxy() )
    proxy = db.get_account( a.get_proxy() ).get_name();
  if( recovery.has_recovery_account() )
    recovery_account = db.get_account( recovery.get_recovery_account() ).get_name();

  size_t n = a.get_proxied_vsf_votes().size();
  proxied_vsf_votes.reserve( n );
  for( size_t i=0; i<n; i++ )
    proxied_vsf_votes.push_back( a.get_proxied_vsf_votes()[i] );

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

#ifdef HIVE_ENABLE_SMT
  const auto& by_control_account_index = db.get_index<smt_token_index>().indices().get<by_control_account>();
  auto smt_obj_itr = by_control_account_index.find( name );
  is_smt = smt_obj_itr != by_control_account_index.end();
#endif

  if( delayed_votes_active )
    delayed_votes = vector< delayed_votes_data >{ dvotes.get_delayed_votes().begin(), dvotes.get_delayed_votes().end() };

  post_voting_power = VEST_asset(a.get_effective_vesting_shares( assets, time_obj ));
}


// api_comment_object constructors
api_comment_object::api_comment_object( const comment_object& o, const database& db ):
  id( o.get_id() ),
  depth( o.get_depth() )
{
  const comment_cashout_object* cc = db.find_comment_cashout( o );
  if( cc )
  {
    total_vote_weight       = cc->get_total_vote_weight();
    reward_weight           = HIVE_100_PERCENT; // since HF17 reward is not limited if posts are too frequent
    author_rewards          = 0; // since HF19 it was always 0 or cc did not exist
    net_votes               = cc->get_net_votes();
    last_payout             = time_point_sec::min(); // since HF19 it is the only value possible
    children                = cc->get_number_of_replies();
    net_rshares             = cc->get_net_rshares();
    abs_rshares             = 0; // value was only used for comments created before HF6
    vote_rshares            = cc->get_vote_rshares();
    children_abs_rshares    = 0; // value not accumulated after HF17
    created                 = cc->get_creation_time();
    last_update             = created; // edit time not available here (Hivemind has it)
    cashout_time            = cc->get_cashout_time();
    max_cashout_time        = time_point_sec::maximum(); // since HF17 it is the only possible value
    max_accepted_payout     = cc->get_max_accepted_payout();
    percent_hbd             = cc->get_percent_hbd();
    allow_votes             = cc->allows_votes();
    allow_curation_rewards  = cc->allows_curation_rewards();
    was_voted_on            = cc->has_votes();

    for( auto& route : cc->get_beneficiaries() )
    {
      beneficiaries.emplace_back( db.get_account( route.account_id ).get_name(), route.weight );
    }
  }
}


// api_comment_vote_object constructor
api_comment_vote_object::api_comment_vote_object( const comment_vote_object& cv, const database& db ) :
  id( cv.get_id() ),
  weight( cv.get_weight() ),
  rshares( cv.get_rshares() ),
  vote_percent( cv.get_vote_percent() ),
  last_update( cv.get_last_update() )
{
  voter = db.get( cv.get_voter() ).get_name();
  const comment_cashout_object* cc = db.find_comment_cashout( cv.get_comment() );
  assert( cc != nullptr ); //votes should not exist after cashout
  author = db.get_account( cc->get_author_id() ).get_name();
  permlink = to_string( cc->get_permlink() );
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
  current_median_history( f.current_median_history ),
  market_median_history( f.market_median_history ),
  current_min_history( f.current_min_history ),
  current_max_history( f.current_max_history ),
  price_history( f.price_history.begin(), f.price_history.end() )
{}


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

#ifdef HIVE_ENABLE_SMT
// api_smt_token_object constructor
api_smt_token_object::api_smt_token_object( const smt_token_object& token, const database& db )
  : token( token.copy_chain_object() ) //FIXME: exposes internal chain object as API result
{
  const smt_ico_object* ico = db.find< smt_ico_object, by_symbol >( token.liquid_symbol );
  if ( ico != nullptr )
    this->ico = ico->copy_chain_object(); //FIXME: exposes internal chain object as API result
}

// api_smt_token_emissions_object constructor
api_smt_token_emissions_object::api_smt_token_emissions_object( const smt_token_emissions_object& o, const database& db ):
  id( o.get_id() ),
  symbol( o.symbol ),
  schedule_time( o.schedule_time ),
  emissions_unit( o.emissions_unit ),
  interval_seconds( o.interval_seconds ),
  interval_count( o.interval_count ),
  lep_time( o.lep_time ),
  rep_time( o.rep_time ),
  lep_abs_amount( o.lep_abs_amount ),
  rep_abs_amount( o.rep_abs_amount ),
  lep_rel_amount_numerator( o.lep_rel_amount_numerator ),
  rep_rel_amount_numerator( o.rep_rel_amount_numerator ),
  rel_amount_denom_bits( o.rel_amount_denom_bits )
{}

// api_smt_contribution_object constructor
api_smt_contribution_object::api_smt_contribution_object( const smt_contribution_object& o, const database& db ):
  id( o.get_id() ),
  symbol( o.symbol ),
  contributor( o.contributor ),
  contribution_id( o.contribution_id ),
  contribution( o.contribution )
{}
#endif

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
  daily_pay(po.daily_pay),
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
