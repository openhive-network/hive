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

    dtds.get_decoded_type_checksum<hive::chain::account_object>();
    dtds.get_decoded_type_checksum<hive::chain::account_metadata_object>();
    dtds.get_decoded_type_checksum<hive::chain::account_authority_object>();
    dtds.get_decoded_type_checksum<hive::chain::vesting_delegation_object>();
    dtds.get_decoded_type_checksum<hive::chain::vesting_delegation_expiration_object>();
    dtds.get_decoded_type_checksum<hive::chain::owner_authority_history_object>();
    dtds.get_decoded_type_checksum<hive::chain::account_recovery_request_object>();
    dtds.get_decoded_type_checksum<hive::chain::change_recovery_account_request_object>();
    dtds.get_decoded_type_checksum<hive::chain::block_summary_object>();
    dtds.get_decoded_type_checksum<hive::chain::comment_object>();
    dtds.get_decoded_type_checksum<hive::chain::comment_cashout_object>();
    dtds.get_decoded_type_checksum<hive::chain::comment_cashout_ex_object>();
    dtds.get_decoded_type_checksum<hive::chain::comment_vote_object>();
    dtds.get_decoded_type_checksum<hive::chain::proposal_object>();
    dtds.get_decoded_type_checksum<hive::chain::proposal_vote_object>();
    dtds.get_decoded_type_checksum<hive::chain::dynamic_global_property_object>();
    dtds.get_decoded_type_checksum<hive::chain::hardfork_property_object>();
    dtds.get_decoded_type_checksum<hive::chain::convert_request_object>();
    dtds.get_decoded_type_checksum<hive::chain::collateralized_convert_request_object>();
    dtds.get_decoded_type_checksum<hive::chain::escrow_object>();
    dtds.get_decoded_type_checksum<hive::chain::savings_withdraw_object>();
    dtds.get_decoded_type_checksum<hive::chain::liquidity_reward_balance_object>();
    dtds.get_decoded_type_checksum<hive::chain::feed_history_object>();
    dtds.get_decoded_type_checksum<hive::chain::limit_order_object>();
    dtds.get_decoded_type_checksum<hive::chain::withdraw_vesting_route_object>();
    dtds.get_decoded_type_checksum<hive::chain::decline_voting_rights_request_object>();
    dtds.get_decoded_type_checksum<hive::chain::reward_fund_object>();
    dtds.get_decoded_type_checksum<hive::chain::recurrent_transfer_object>();
    dtds.get_decoded_type_checksum<hive::chain::pending_required_action_object>();
    dtds.get_decoded_type_checksum<hive::chain::pending_optional_action_object>();
    dtds.get_decoded_type_checksum<hive::chain::transaction_object>();
    dtds.get_decoded_type_checksum<hive::chain::witness_vote_object>();
    dtds.get_decoded_type_checksum<hive::chain::witness_schedule_object>();
    dtds.get_decoded_type_checksum<hive::chain::witness_object>();
    dtds.get_decoded_type_checksum<hive::plugins::account_by_key::key_lookup_object>();
    dtds.get_decoded_type_checksum<hive::plugins::account_history_rocksdb::volatile_operation_object>();
    dtds.get_decoded_type_checksum<hive::plugins::block_log_info::block_log_hash_state_object>();
    dtds.get_decoded_type_checksum<hive::plugins::block_log_info::block_log_pending_message_object>();
    dtds.get_decoded_type_checksum<hive::plugins::follow::follow_object>();
    dtds.get_decoded_type_checksum<hive::plugins::follow::feed_object>();
    dtds.get_decoded_type_checksum<hive::plugins::follow::blog_object>();
    dtds.get_decoded_type_checksum<hive::plugins::follow::blog_author_stats_object>();
    dtds.get_decoded_type_checksum<hive::plugins::follow::reputation_object>();
    dtds.get_decoded_type_checksum<hive::plugins::follow::follow_count_object>();
    dtds.get_decoded_type_checksum<hive::plugins::market_history::bucket_object>();
    dtds.get_decoded_type_checksum<hive::plugins::market_history::order_history_object>();
    dtds.get_decoded_type_checksum<hive::plugins::rc::rc_resource_param_object>();
    dtds.get_decoded_type_checksum<hive::plugins::rc::rc_pool_object>();
    dtds.get_decoded_type_checksum<hive::plugins::rc::rc_stats_object>();
    dtds.get_decoded_type_checksum<hive::plugins::rc::rc_pending_data>();
    dtds.get_decoded_type_checksum<hive::plugins::rc::rc_account_object>();
    dtds.get_decoded_type_checksum<hive::plugins::rc::rc_direct_delegation_object>();
    dtds.get_decoded_type_checksum<hive::plugins::rc::rc_usage_bucket_object>();
    dtds.get_decoded_type_checksum<hive::plugins::reputation::reputation_object>();
    dtds.get_decoded_type_checksum<hive::plugins::tags::tag_object>();
    dtds.get_decoded_type_checksum<hive::plugins::tags::tag_stats_object>();
    dtds.get_decoded_type_checksum<hive::plugins::tags::author_tag_stats_object>();
    dtds.get_decoded_type_checksum<hive::plugins::transaction_status::transaction_status_object>();
    dtds.get_decoded_type_checksum<hive::plugins::witness::witness_custom_op_object>();

    #ifdef HIVE_ENABLE_SMT
    dtds.get_decoded_type_checksum<hive::chain::smt_token_object>();
    dtds.get_decoded_type_checksum<hive::chain::account_regular_balance_object>();
    dtds.get_decoded_type_checksum<hive::chain::account_rewards_balance_object>();
    dtds.get_decoded_type_checksum<hive::chain::nai_pool_object>();
    dtds.get_decoded_type_checksum<hive::chain::smt_token_emissions_object>();
    dtds.get_decoded_type_checksum<hive::chain::smt_contribution_object>();
    dtds.get_decoded_type_checksum<hive::chain::smt_ico_object>();
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
