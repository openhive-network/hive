#pragma once

#include <hive/plugins/wallet_bridge_api/wallet_bridge_api_args.hpp>
#include <hive/plugins/json_rpc/utility.hpp>
#include <fc/api.hpp>

namespace hive { namespace plugins { namespace wallet_bridge_api {

class wallet_bridge_api_impl;

class wallet_bridge_api
{
  public:
    wallet_bridge_api();
    ~wallet_bridge_api();

    DECLARE_API(
        (get_version)
        (get_witness_schedule)
        (get_current_median_history_price)
        (get_hardfork_version)
        (get_block)
        (get_chain_properties)
        (get_ops_in_block)
        (get_feed_history)
        (get_active_witnesses)
        (get_withdraw_routes)
        (list_my_accounts)
        (list_accounts)
        (get_dynamic_global_properties)
        (get_account)
        (get_accounts)
        (get_transaction)
        (list_witnesses)
        (get_witness)
        (get_conversion_requests)
        (get_order_book)
        (get_open_orders)
        (get_owner_history)
        (get_account_history)
        (list_proposals)
        (find_proposals)
        (list_proposal_votes)
        (get_reward_fund)
        (broadcast_transaction_synchronous)
    )

    void api_startup();
    void api_shutdown();

  private:
    std::unique_ptr< wallet_bridge_api_impl > my;
};

} } } //hive::plugins::wallet_bridge_api

FC_API(
  hive::plugins::wallet_bridge_api::wallet_bridge_api,
  (get_version)
  (get_witness_schedule)
  (get_current_median_history_price)
  (get_hardfork_version)
  (get_block)
  (get_chain_properties)
  (get_ops_in_block)
  (get_feed_history)
  (get_active_witnesses)
  (get_withdraw_routes)
  (list_my_accounts)
  (list_accounts)
  (get_dynamic_global_properties)
  (get_account)
  (get_accounts)
  (get_transaction)
  (list_witnesses)
  (get_witness)
  (get_conversion_requests)
  (get_order_book)
  (get_open_orders)
  (get_owner_history)
  (get_account_history)
  (list_proposals)
  (find_proposals)
  (list_proposal_votes)
  (get_reward_fund)
  (broadcast_transaction_synchronous)
)