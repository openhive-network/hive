#pragma once

#include <hive/chain/hive_objects.hpp>
#include <hive/plugins/account_by_key_api/account_by_key_api.hpp>
#include <hive/plugins/account_history_api/account_history_api_plugin.hpp>
#include <hive/plugins/account_history_api/account_history_api.hpp>
#include <hive/plugins/block_api/block_api.hpp>
#include <hive/plugins/database_api/database_api.hpp>
#include <hive/plugins/market_history_api/market_history_api.hpp>
#include <hive/plugins/rc_api/rc_api.hpp>

namespace hive { namespace plugins { namespace wallet_bridge_api {

/* get_version */
typedef variant                            get_version_args;
typedef database_api::get_version_return   get_version_return;

/* get_witness_schedule */
typedef variant                                     get_witness_schedule_args;
typedef database_api::get_witness_schedule_return   get_witness_schedule_return;

/* get_current_median_history_price */
typedef variant             get_current_median_history_price_args;
typedef protocol::price     get_current_median_history_price_return;

/* get_hardfork_version */
typedef variant                         get_hardfork_version_args;
typedef protocol::hardfork_version      get_hardfork_version_return;

/* get_block */
typedef variant                       get_block_args;
typedef block_api::get_block_return   get_block_return;

/* get_ops_in_block */
typedef variant                                   get_ops_in_block_args;
typedef account_history::get_ops_in_block_return  get_ops_in_block_return;

/* get_chain_properties */
typedef variant                   get_chain_properties_args;
typedef chain::chain_properties   get_chain_properties_return;

/* get_feed_history */
typedef variant                                 get_feed_history_args;
typedef database_api::api_feed_history_object   get_feed_history_return;

/* get_active_witnesses */
typedef variant                                     get_active_witnesses_args;
typedef database_api::get_active_witnesses_return   get_active_witnesses_return;

/* get_withdraw_routes */
struct find_withdraw_vesting_json_route
{
  protocol::account_name_type from;
  protocol::account_name_type to;
  uint16_t                    percent = 0;
  bool                        auto_vest = false;
};
typedef variant                                                 get_withdraw_routes_args;
typedef vector<database_api::api_withdraw_vesting_route_object> get_withdraw_routes_return;

/* list_my_accounts */
struct list_my_accounts_json_account
{
  protocol::account_name_type name;
  protocol::asset             balance;
  protocol::asset             vesting_shares;
  protocol::asset             hbd_balance;
};
struct list_my_accounts_json_return
{
  vector<list_my_accounts_json_account> accounts;

  protocol::asset total_hive;
  protocol::asset total_vest;
  protocol::asset total_hbd;
};
typedef variant                                   list_my_accounts_args;
typedef vector<database_api::api_account_object>  list_my_accounts_return;

/* list_accounts */
typedef variant                             list_accounts_args;
typedef set<protocol::account_name_type>    list_accounts_return;

/* get_dynamic_global_properties */
typedef variant                                             get_dynamic_global_properties_args;
typedef database_api::api_dynamic_global_property_object    get_dynamic_global_properties_return;

/* get_account */
typedef variant                                           get_account_args;
typedef fc::optional<database_api::api_account_object>    get_account_return;

/* get_accounts */
typedef variant                                         get_accounts_args;
typedef std::vector<database_api::api_account_object>   get_accounts_return;

/* get transaction */
typedef variant                                         get_transaction_args;
typedef hive::protocol::annotated_signed_transaction    get_transaction_return;

/* list_witnesses */
typedef variant                               list_witnesses_args;
typedef database_api::list_witnesses_return   list_witnesses_return;

/* get_witness */

typedef                                                 variant get_witness_args;
typedef fc::optional<database_api::api_witness_object>  get_witness_return;

/* get_conversion_requests */
typedef variant                                             get_conversion_requests_args;
typedef vector< database_api::api_convert_request_object >  get_conversion_requests_return;

/* get_collateralized_conversion_requests */
typedef variant                                                           get_collateralized_conversion_requests_args;
typedef vector< database_api::api_collateralized_convert_request_object > get_collateralized_conversion_requests_return;

/* get_order_book */
struct get_order_book_json_order
{
  protocol::asset   hive;
  protocol::asset   hbd;
  protocol::asset   sum_hbd;
  double            price = 0;
};
struct get_order_book_json_return
{
  vector<get_order_book_json_order> bids;
  vector<get_order_book_json_order> asks;
  protocol::asset   bid_total;
  protocol::asset   ask_total;
};
typedef variant                               get_order_book_args;
typedef market_history::get_order_book_return get_order_book_return;

/* get_open_orders */
struct get_open_orders_json_order
{
  uint32_t        id = 0;
  double          price = 0;
  protocol::asset quantity;
  string          type;
};
struct get_open_orders_json_return
{
  vector<get_open_orders_json_order> orders;
};
typedef variant                                       get_open_orders_args;
typedef vector<database_api::api_limit_order_object>  get_open_orders_return;

/* get_owner_history */
typedef variant                                     get_owner_history_args;
typedef database_api::find_owner_histories_return   get_owner_history_return;

/* get_account_history */
struct get_account_history_json_op
{
  uint32_t                      pos   = 0;
  uint32_t                      block = 0;
  protocol::transaction_id_type id;
  protocol::operation           op;
};
typedef variant                                                   get_account_history_args;
typedef std::map<uint32_t, account_history::api_operation_object> get_account_history_return;

/* list_proposals */
typedef variant                               list_proposals_args;
typedef database_api::list_proposals_return   list_proposals_return;

/* find_proposals */
typedef variant                             find_proposals_args;
typedef list_proposals_return               find_proposals_return;

/* is_known_transaction */
typedef variant                             is_known_transaction_args;
typedef bool                                is_known_transaction_return;

/* list_proposal_votes */
typedef variant                                   list_proposal_votes_args;
typedef database_api::list_proposal_votes_return  list_proposal_votes_return;

/* get_reward_fund */
typedef variant                               get_reward_fund_args;
typedef database_api::api_reward_fund_object  get_reward_fund_return;

/* broadcast_transaction_synchronous */
typedef variant broadcast_transaction_synchronous_args;
struct broadcast_transaction_synchronous_return
{
  protocol::transaction_id_type   id;
  int32_t                         block_num = 0;
  int32_t                         trx_num = 0;
  bool                            expired = false;
};

/* broadcast_transaction */
typedef variant broadcast_transaction_args;
typedef hive::plugins::json_rpc::void_type broadcast_transaction_return;

/* find_recurrent_transfers */
typedef variant                                               find_recurrent_transfers_args;
typedef vector< database_api::api_recurrent_transfer_object > find_recurrent_transfers_return;

/* find_rc_accounts */
typedef variant                                               find_rc_accounts_args;
typedef vector< rc::rc_account_api_object >                   find_rc_accounts_return;

/* list_rc_accounts */
typedef variant                                               list_rc_accounts_args;
typedef vector< rc::rc_account_api_object >                   list_rc_accounts_return;

/* list_rc_direct_delegations */
typedef variant                                               list_rc_direct_delegations_args;
typedef vector< rc::rc_direct_delegation_api_object >         list_rc_direct_delegations_return;

} } } // hive::plugins::wallet_bridge_api

FC_REFLECT( hive::plugins::wallet_bridge_api::broadcast_transaction_synchronous_return, (id)(block_num)(trx_num)(expired))

FC_REFLECT( hive::plugins::wallet_bridge_api::find_withdraw_vesting_json_route, (from)(to)(percent)(auto_vest))

FC_REFLECT( hive::plugins::wallet_bridge_api::list_my_accounts_json_account, (name)(balance)(vesting_shares)(hbd_balance))
FC_REFLECT( hive::plugins::wallet_bridge_api::list_my_accounts_json_return, (accounts)(total_hive)(total_vest)(total_hbd))

FC_REFLECT( hive::plugins::wallet_bridge_api::get_account_history_json_op, (pos)(block)(id)(op))

FC_REFLECT( hive::plugins::wallet_bridge_api::get_open_orders_json_order, (id)(price)(quantity)(type))
FC_REFLECT( hive::plugins::wallet_bridge_api::get_open_orders_json_return, (orders))

FC_REFLECT( hive::plugins::wallet_bridge_api::get_order_book_json_order, (hive)(hbd)(sum_hbd)(price))
FC_REFLECT( hive::plugins::wallet_bridge_api::get_order_book_json_return, (bids)(asks)(bid_total)(ask_total))
