#include <hive/chain/util/decoded_types_data_storage.hpp>

#include <hive/chain/hive_object_types.hpp>
#include <hive/plugins/account_by_key/account_by_key_objects.hpp>
#include <hive/plugins/account_history_rocksdb/account_history_rocksdb_objects.hpp>
#include <hive/plugins/block_log_info/block_log_info_objects.hpp>
#include <hive/plugins/follow/follow_objects.hpp>
#include <hive/plugins/market_history/market_history_plugin.hpp>
#include <hive/plugins/rc/rc_objects.hpp>
#include <hive/plugins/reputation/reputation_objects.hpp>
#include <hive/plugins/tags/tags_plugin.hpp>
#include <hive/plugins/transaction_status/transaction_status_objects.hpp>
#include <hive/plugins/witness/witness_plugin_objects.hpp>
#include <hive/chain/block_summary_object.hpp>
#include <hive/chain/dhf_objects.hpp>
#include <hive/chain/pending_required_action_object.hpp>
#include <hive/chain/pending_optional_action_object.hpp>
#include <hive/chain/transaction_object.hpp>

#ifdef HIVE_ENABLE_SMT

#include <hive/chain/smt_objects/smt_token_object.hpp>
#include <hive/chain/smt_objects/account_balance_object.hpp>
#include <hive/chain/smt_objects/nai_pool_object.hpp>
#include <hive/chain/smt_objects/smt_token_object.hpp>

#endif

#include <iostream>
#include <sstream>
#include <unordered_map>
#include <chrono>
#include <fc/io/json.hpp>

void do_job()
{
  try
  {
    hive::chain::util::decoded_types_data_storage dtds;

    auto begin = std::chrono::steady_clock::now();

    dtds.register_new_type<hive::chain::account_object>();
    dtds.register_new_type<hive::chain::account_metadata_object>();
    dtds.register_new_type<hive::chain::account_authority_object>();
    dtds.register_new_type<hive::chain::vesting_delegation_object>();
    dtds.register_new_type<hive::chain::vesting_delegation_expiration_object>();
    dtds.register_new_type<hive::chain::owner_authority_history_object>();
    dtds.register_new_type<hive::chain::account_recovery_request_object>();
    dtds.register_new_type<hive::chain::change_recovery_account_request_object>();
    dtds.register_new_type<hive::chain::block_summary_object>();
    dtds.register_new_type<hive::chain::comment_object>();
    dtds.register_new_type<hive::chain::comment_cashout_object>();
    dtds.register_new_type<hive::chain::comment_cashout_ex_object>();
    dtds.register_new_type<hive::chain::comment_vote_object>();
    dtds.register_new_type<hive::chain::proposal_object>();
    dtds.register_new_type<hive::chain::proposal_vote_object>();
    dtds.register_new_type<hive::chain::dynamic_global_property_object>();
    dtds.register_new_type<hive::chain::hardfork_property_object>();
    dtds.register_new_type<hive::chain::convert_request_object>();
    dtds.register_new_type<hive::chain::collateralized_convert_request_object>();
    dtds.register_new_type<hive::chain::escrow_object>();
    dtds.register_new_type<hive::chain::savings_withdraw_object>();
    dtds.register_new_type<hive::chain::liquidity_reward_balance_object>();
    dtds.register_new_type<hive::chain::feed_history_object>();
    dtds.register_new_type<hive::chain::limit_order_object>();
    dtds.register_new_type<hive::chain::withdraw_vesting_route_object>();
    dtds.register_new_type<hive::chain::decline_voting_rights_request_object>();
    dtds.register_new_type<hive::chain::reward_fund_object>();
    dtds.register_new_type<hive::chain::recurrent_transfer_object>();
    dtds.register_new_type<hive::chain::pending_required_action_object>();
    dtds.register_new_type<hive::chain::pending_optional_action_object>();
    dtds.register_new_type<hive::chain::transaction_object>();
    dtds.register_new_type<hive::chain::witness_vote_object>();
    dtds.register_new_type<hive::chain::witness_schedule_object>();
    dtds.register_new_type<hive::chain::witness_object>();
    dtds.register_new_type<hive::plugins::account_by_key::key_lookup_object>();
    dtds.register_new_type<hive::plugins::account_history_rocksdb::volatile_operation_object>();
    dtds.register_new_type<hive::plugins::block_log_info::block_log_hash_state_object>();
    dtds.register_new_type<hive::plugins::block_log_info::block_log_pending_message_object>();
    dtds.register_new_type<hive::plugins::follow::follow_object>();
    dtds.register_new_type<hive::plugins::follow::feed_object>();
    dtds.register_new_type<hive::plugins::follow::blog_object>();
    dtds.register_new_type<hive::plugins::follow::blog_author_stats_object>();
    dtds.register_new_type<hive::plugins::follow::reputation_object>();
    dtds.register_new_type<hive::plugins::follow::follow_count_object>();
    dtds.register_new_type<hive::plugins::market_history::bucket_object>();
    dtds.register_new_type<hive::plugins::market_history::order_history_object>();
    dtds.register_new_type<hive::plugins::rc::rc_resource_param_object>();
    dtds.register_new_type<hive::plugins::rc::rc_pool_object>();
    dtds.register_new_type<hive::plugins::rc::rc_stats_object>();
    dtds.register_new_type<hive::plugins::rc::rc_pending_data>();
    dtds.register_new_type<hive::plugins::rc::rc_account_object>();
    dtds.register_new_type<hive::plugins::rc::rc_direct_delegation_object>();
    dtds.register_new_type<hive::plugins::rc::rc_usage_bucket_object>();
    dtds.register_new_type<hive::plugins::reputation::reputation_object>();
    dtds.register_new_type<hive::plugins::tags::tag_object>();
    dtds.register_new_type<hive::plugins::tags::tag_stats_object>();
    dtds.register_new_type<hive::plugins::tags::author_tag_stats_object>();
    dtds.register_new_type<hive::plugins::transaction_status::transaction_status_object>();
    dtds.register_new_type<hive::plugins::witness::witness_custom_op_object>();

    #ifdef HIVE_ENABLE_SMT
    dtds.register_new_type<hive::chain::smt_token_object>();
    dtds.register_new_type<hive::chain::account_regular_balance_object>();
    dtds.register_new_type<hive::chain::account_rewards_balance_object>();
    dtds.register_new_type<hive::chain::nai_pool_object>();
    dtds.register_new_type<hive::chain::smt_token_emissions_object>();
    dtds.register_new_type<hive::chain::smt_contribution_object>();
    dtds.register_new_type<hive::chain::smt_ico_object>();
    #endif

    auto end = std::chrono::steady_clock::now();

    std::cerr << "\n----- Results: ----- \n\n";
    std::cerr << dtds.generate_decoded_types_data_pretty_string() << "\n";

    const std::string json = dtds.generate_decoded_types_data_json_string();
    std::cerr << "\nFINAL JSON:\n" << json << "\n\n";
    std::cerr << "size of json: " << json.size() << "\n";

    std::cerr << "\n\nTotal amount of decoded types: " << dtds.get_decoded_types_data_map().size() << "\n";
    std::cerr << "Decoding time: " << std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() << " microseconds.\n";
  }
  catch ( const fc::exception& e )
  {
    edump((e.to_detail_string()));
  }
}

int main( int argc, char** argv )
{
  do_job();
  return 0;
}
