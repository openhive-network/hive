#include <hive/plugins/database_api/database_api_impl.hpp>

#include <hive/chain/comment_object.hpp>
#include <hive/chain/global_property_object.hpp>
#include <hive/chain/detail/state/feed_history_object_multiindex.hpp>
#include <hive/chain/detail/state/reward_fund_object_multiindex.hpp>
#include <hive/chain/witness_objects.hpp>

#include <hive/utilities/git_revision.hpp>

namespace hive { namespace plugins { namespace database_api {

api_commment_cashout_info::api_commment_cashout_info(const comment_cashout_object& cc, const database&)
{
  total_vote_weight = cc.get_total_vote_weight();
  reward_weight = HIVE_100_PERCENT; // since HF17 reward is not limited if posts are too frequent
  total_payout_value = HBD_asset(); // since HF19 it was either default 0 or cc did not exist
  curator_payout_value = HBD_asset(); // since HF19 it was either default 0 or cc did not exist
  author_rewards = 0; // since HF19 author_rewards was either default 0 or cc did not exist
  net_votes = cc.get_net_votes();
  last_payout = time_point_sec::min(); // since HF17 there is only one payout and cc does not exist after HF19
  net_rshares = cc.get_net_rshares();
  abs_rshares = 0; // value was only used for comments created before HF6 (and to recognize that there are votes)
  vote_rshares = cc.get_vote_rshares();
  children_abs_rshares = 0; // value not accumulated after HF17
  cashout_time = cc.get_cashout_time();
  max_cashout_time = time_point_sec::maximum(); // since HF17 it is the only possible value
  max_accepted_payout = cc.get_max_accepted_payout();
  percent_hbd = cc.get_percent_hbd();
  allow_votes = cc.allows_votes();
  allow_curation_rewards = cc.allows_curation_rewards();
  was_voted_on = cc.has_votes();
}


//////////////////////////////////////////////////////////////////////
//                                                                  //
// Constructors                                                     //
//                                                                  //
//////////////////////////////////////////////////////////////////////

database_api::database_api( appbase::application& app )
  : my( new database_api_impl( app ) )
{
  JSON_RPC_REGISTER_API( HIVE_DATABASE_API_PLUGIN_NAME );
}

database_api::~database_api() {}

database_api_impl::database_api_impl( appbase::application& app )
  : _db( app.get_plugin< hive::plugins::chain::chain_plugin >().db() ) {}

database_api_impl::~database_api_impl() {}

//////////////////////////////////////////////////////////////////////
//                                                                  //
// Globals                                                          //
//                                                                  //
//////////////////////////////////////////////////////////////////////


DEFINE_API_IMPL( database_api_impl, get_config )
{
  return hive::protocol::get_config( _db.get_treasury_name(), _db.get_chain_id() );
}

DEFINE_API_IMPL( database_api_impl, get_version )
{
  fc::mutable_variant_object version_storage;
  hive::utilities::build_version_info(&version_storage);

  version_storage.set("chain_id", std::string(_db.get_chain_id()));

  return version_storage;
}

DEFINE_API_IMPL( database_api_impl, get_dynamic_global_properties )
{
  return api_dynamic_global_property_object( _db.get_dynamic_global_properties(), _db );
}

#define FILL_FIELD(field) if( active.field != future.field ) { filled = true; field = future.field; }

bool future_chain_properties::fill( const chain_properties& active, const chain_properties& future )
{
  bool filled = false;
  FILL_FIELD( account_creation_fee );
  FILL_FIELD( maximum_block_size );
  FILL_FIELD( hbd_interest_rate );
  FILL_FIELD( account_subsidy_budget );
  FILL_FIELD( account_subsidy_decay );
  return filled;
}

bool future_witness_schedule::fill( const witness_schedule_object& active, const witness_schedule_object& future )
{
  bool filled = false;
  FILL_FIELD( num_scheduled_witnesses );
  FILL_FIELD( elected_weight );
  FILL_FIELD( timeshare_weight );
  FILL_FIELD( miner_weight );
  FILL_FIELD( witness_pay_normalization_factor );
  median_props = future_chain_properties();
  if( !median_props->fill( active.median_props, future.median_props ) )
    median_props.reset();
  else
    filled = true;
  FILL_FIELD( majority_version );
  FILL_FIELD( max_voted_witnesses );
  FILL_FIELD( max_miner_witnesses );
  FILL_FIELD( max_runner_witnesses );
  FILL_FIELD( hardfork_required_witnesses );
  FILL_FIELD( account_subsidy_rd );
  FILL_FIELD( account_subsidy_witness_rd );
  FILL_FIELD( min_witness_account_subsidy_decay );
  return filled;
}

#undef FILL_FIELD

DEFINE_API_IMPL( database_api_impl, get_witness_schedule )
{
  FC_ASSERT( _db.has_hardfork( HIVE_HARDFORK_1_26 ) || !args.include_future, "Future witnesses only become available after HF26" );
  const auto& wso = _db.get_witness_schedule_object();
  const auto& future_wso = _db.has_hardfork( HIVE_HARDFORK_1_26 ) ? _db.get_future_witness_schedule_object() : wso;
  get_witness_schedule_return result( wso, future_wso, args.include_future, _db );
  return result;
}

DEFINE_API_IMPL( database_api_impl, get_hardfork_properties )
{
  return _db.get_hardfork_property_object();
}

DEFINE_API_IMPL( database_api_impl, get_reward_funds )
{
  get_reward_funds_return result;

  const auto& rf_idx = _db.get_index< reward_fund_index, by_id >();
  auto itr = rf_idx.begin();

  while( itr != rf_idx.end() )
  {
    result.funds.emplace_back( *itr, _db );
    ++itr;
  }

  return result;
}

DEFINE_API_IMPL( database_api_impl, get_current_price_feed )
{
  return _db.get_feed_history().current_median_history;
}

DEFINE_API_IMPL( database_api_impl, get_feed_history )
{
  return _db.get_feed_history();
}


//////////////////////////////////////////////////////////////////////
//                                                                  //
// API Registration Macros                                          //
//                                                                  //
//////////////////////////////////////////////////////////////////////

DEFINE_LOCKLESS_APIS( database_api, (get_config)(get_version) )

DEFINE_READ_APIS( database_api,
  (get_dynamic_global_properties)
  (get_witness_schedule)
  (get_hardfork_properties)
  (get_reward_funds)
  (get_current_price_feed)
  (get_feed_history)
  (list_witnesses)
  (find_witnesses)
  (list_witness_votes)
  (get_active_witnesses)
  (list_accounts)
  (find_accounts)
  (list_owner_histories)
  (find_owner_histories)
  (list_account_recovery_requests)
  (find_account_recovery_requests)
  (list_change_recovery_account_requests)
  (find_change_recovery_account_requests)
  (list_escrows)
  (find_escrows)
  (list_withdraw_vesting_routes)
  (find_withdraw_vesting_routes)
  (list_savings_withdrawals)
  (find_savings_withdrawals)
  (list_vesting_delegations)
  (find_vesting_delegations)
  (list_vesting_delegation_expirations)
  (find_vesting_delegation_expirations)
  (list_hbd_conversion_requests)
  (find_hbd_conversion_requests)
  (list_collateralized_conversion_requests)
  (find_collateralized_conversion_requests)
  (list_decline_voting_rights_requests)
  (find_decline_voting_rights_requests)
  (get_comment_pending_payouts)
  (list_limit_orders)
  (find_limit_orders)
  (list_proposals)
  (find_proposals)
  (list_proposal_votes)
  (get_order_book)
  (find_recurrent_transfers)
  (get_transaction_hex)
  (get_required_signatures)
  (get_potential_signatures)
  (verify_authority)
  (verify_account_authority)
  (verify_signatures)
#ifdef HIVE_ENABLE_SMT
  (get_nai_pool)
  (list_smt_contributions)
  (find_smt_contributions)
  (list_smt_tokens)
  (find_smt_tokens)
  (list_smt_token_emissions)
  (find_smt_token_emissions)
#endif
  (is_known_transaction)
)

} } } // hive::plugins::database_api
