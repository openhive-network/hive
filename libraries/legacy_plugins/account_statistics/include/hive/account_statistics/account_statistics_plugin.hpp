#include <hive/app/plugin.hpp>

#include <boost/multi_index/composite_key.hpp>

//
// Plugins should #define their SPACE_ID's so plugins with
// conflicting SPACE_ID assignments can be compiled into the
// same binary (by simply re-assigning some of the conflicting #defined
// SPACE_ID's in a build script).
//
// Assignment of SPACE_ID's cannot be done at run-time because
// various template automagic depends on them being known at compile
// time.
//
#ifndef HIVE_ACCOUNT_STATISTICS_SPACE_ID
#define HIVE_ACCOUNT_STATISTICS_SPACE_ID 10
#endif

#ifndef HIVE_ACCOUNT_STATISTICS_PLUGIN_NAME
#define HIVE_ACCOUNT_STATISTICS_PLUGIN_NAME "account_stats"
#endif

namespace hive { namespace account_statistics {

using namespace chain;
using app::application;

enum account_statistics_plugin_object_types
{
  account_stats_bucket_object_type    = ( HIVE_ACCOUNT_STATISTICS_SPACE_ID << 8 ),
  account_activity_bucket_object_type = ( HIVE_ACCOUNT_STATISTICS_SPACE_ID << 8 ) + 1
};

struct account_stats_bucket_object : public object< account_stats_bucket_object_type, account_stats_bucket_object >
{
  CHAINBASE_OBJECT( account_stats_bucket_object );

public:
  CHAINBASE_DEFAULT_CONSTRUCTOR( account_stats_bucket_object )

  fc::time_point_sec   open;                                     ///< Open time of the bucket
  uint32_t             seconds = 0;                              ///< Seconds accounted for in the bucket
  account_name_type    name;                                     ///< Account name
  uint32_t             transactions = 0;                         ///< Transactions this account signed
  uint32_t             market_bandwidth = 0;                     ///< Charged bandwidth for market transactions
  uint32_t             non_market_bandwidth = 0;                 ///< Charged bandwidth for non-market transactions
  uint32_t             total_ops = 0;                            ///< Ops this account was an authority on
  uint32_t             market_ops = 0;                           ///< Market operations
  uint32_t             forum_ops = 0;                            ///< Forum operations
  uint32_t             root_comments = 0;                        ///< Top level root comments
  uint32_t             root_comment_edits = 0;                   ///< Edits to root comments
  uint32_t             root_comments_deleted = 0;                ///< Root comments deleted
  uint32_t             replies = 0;                              ///< Replies to comments
  uint32_t             reply_edits = 0;                          ///< Edits to replies
  uint32_t             replies_deleted = 0;                      ///< Replies deleted
  uint32_t             new_root_votes = 0;                       ///< New votes on root comments
  uint32_t             changed_root_votes = 0;                   ///< Changed votes for root comments
  uint32_t             new_reply_votes = 0;                      ///< New votes on replies
  uint32_t             changed_reply_votes = 0;                  ///< Changed votes for replies
  uint32_t             author_reward_payouts = 0;                ///< Number of author reward payouts
  share_type           author_rewards_hbd = 0;                   ///< HBD paid for author rewards
  share_type           author_rewards_vests = 0;                 ///< VESTS paid for author rewards
  share_type           author_rewards_total_hive_value = 0;      ///< HIVE Value of author rewards
  share_type           author_rewards_payout_hbd_value = 0;      ///< HBD Value of author rewards at time of payout
  uint32_t             curation_reward_payouts = 0;              ///< Number of curation reward payouts.
  share_type           curation_rewards_vests = 0;               ///< VESTS paid for curation rewards
  share_type           curation_rewards_hive_value = 0;          ///< HIVE Value of curation rewards
  share_type           curation_rewards_payout_hbd_value = 0;    ///< HBD Value of curation rewards at time of payout
  uint32_t             liquidity_reward_payouts = 0;             ///< Number of liquidity reward payouts
  share_type           liquidity_rewards = 0;                    ///< Amount of HIVE paid as liquidity rewards
  uint32_t             transfers_to = 0;                         ///< Account to account transfers to this account
  uint32_t             transfers_from = 0;                       ///< Account to account transfers from this account
  share_type           hive_sent = 0;                            ///< HIVE sent from this account
  share_type           hive_received = 0;                        ///< HIVE received by this account
  share_type           hbd_sent = 0;                             ///< HBD sent from this account
  share_type           hbd_received = 0;                         ///< HBD received by this account
  uint32_t             hbd_interest_payments = 0;                ///< Number of times interest was paid to HBD
  share_type           hbd_paid_as_interest = 0;                 ///< Amount of HBD paid as interest
  uint32_t             transfers_to_vesting = 0;                 ///< Transfers to vesting by this account. Note: Transfer to vesting from A to B counts as a transfer from A to B followed by a vesting deposit by B.
  share_type           hive_vested = 0;                          ///< HIVE vested by the account
  share_type           new_vests = 0;                            ///< New VESTS by vesting transfers
  uint32_t             new_vesting_withdrawal_requests = 0;      ///< New vesting withdrawal requests
  uint32_t             modified_vesting_withdrawal_requests = 0; ///< Changes to vesting withdraw requests
  uint32_t             vesting_withdrawals_processed = 0;        ///< Vesting withdrawals processed for this account
  uint32_t             finished_vesting_withdrawals = 0;         ///< Processed vesting withdrawals that are now finished
  share_type           vests_withdrawn = 0;                      ///< VESTS withdrawn from the account
  share_type           hive_received_from_withdrawls = 0;        ///< HIVE received from this account's vesting withdrawals
  share_type           hive_received_from_routes = 0;            ///< HIVE received from another account's vesting withdrawals
  share_type           vests_received_from_routes = 0;           ///< VESTS received from another account's vesting withdrawals
  uint32_t             hbd_conversion_requests_created = 0;      ///< HBD conversion requests created
  share_type           hbd_to_be_converted = 0;                  ///< Amount of HBD to be converted
  uint32_t             hbd_conversion_requests_filled = 0;       ///< HBD conversion requests filled
  share_type           hive_converted = 0;                       ///< Amount of HIVE that was converted
  //note: collateralized convert requests not handled here - plugin is assumed obsolete
  uint32_t             limit_orders_created = 0;                 ///< Limit orders created by this account
  uint32_t             limit_orders_filled = 0;                  ///< Limit orders filled by this account
  uint32_t             limit_orders_cancelled = 0;               ///< Limit orders cancelled by this account
  share_type           limit_order_hive_paid = 0;                ///< HIVE paid by limit orders
  share_type           limit_order_hive_received = 0;            ///< HIVE received from limit orders
  share_type           limit_order_hbd_paid = 0;                 ///< HBD paid by limit orders
  share_type           limit_order_hbd_received = 0;             ///< HBD received by limit orders
  uint32_t             total_pow = 0;                            ///< POW completed
  uint128_t            estimated_hashpower = 0;                  ///< Estimated hashpower
};

typedef account_stats_bucket_object::id_type account_stats_bucket_id_type;

struct account_activity_bucket_object : public object< account_activity_bucket_object_type, account_activity_bucket_object >
{
  CHAINBASE_OBJECT( account_activity_bucket_object );

public:
  CHAINBASE_DEFAULT_CONSTRUCTOR( account_activity_bucket_object )

  fc::time_point_sec   open;                                  ///< Open time for the bucket
  uint32_t             seconds = 0;                           ///< Seconds accounted for in the bucket
  uint32_t             active_market_accounts = 0;            ///< Active market accounts in the bucket
  uint32_t             active_forum_accounts = 0;             ///< Active forum accounts in the bucket
  uint32_t             active_market_and_forum_accounts = 0;  ///< Active accounts in both the market and the forum
};

typedef account_activity_bucket_object::id_type account_activity_bucket_id_type;

namespace detail
{
  class account_statistics_plugin_impl;
}

class account_statistics_plugin : public hive::app::plugin
{
  public:
    account_statistics_plugin( application* app );
    virtual ~account_statistics_plugin();

    virtual std::string plugin_name()const override { return HIVE_ACCOUNT_STATISTICS_PLUGIN_NAME; }
    virtual void plugin_set_program_options(
      boost::program_options::options_description& cli,
      boost::program_options::options_description& cfg ) override;
    virtual void plugin_initialize( const boost::program_options::variables_map& options ) override;
    virtual void plugin_startup() override;

    const flat_set< uint32_t >& get_tracked_buckets() const;
    uint32_t get_max_history_per_bucket() const;
    const flat_set< std::string >& get_tracked_accounts() const;

  private:
    friend class detail::account_statistics_plugin_impl;
    std::unique_ptr< detail::account_statistics_plugin_impl > _my;
};

} } // hive::account_statistics

FC_REFLECT( hive::account_statistics::account_stats_bucket_object,
  (id)
  (open)
  (seconds)
  (name)
  (transactions)
  (market_bandwidth)
  (non_market_bandwidth)
  (total_ops)
  (market_ops)
  (forum_ops)
  (root_comments)
  (root_comment_edits)
  (root_comments_deleted)
  (replies)
  (reply_edits)
  (replies_deleted)
  (new_root_votes)
  (changed_root_votes)
  (new_reply_votes)
  (changed_reply_votes)
  (author_reward_payouts)
  (author_rewards_hbd)
  (author_rewards_vests)
  (author_rewards_total_hive_value)
  (author_rewards_payout_hbd_value)
  (curation_reward_payouts)
  (curation_rewards_vests)
  (curation_rewards_hive_value)
  (curation_rewards_payout_hbd_value)
  (liquidity_reward_payouts)
  (liquidity_rewards)
  (transfers_to)
  (transfers_from)
  (hive_sent)
  (hive_received)
  (hbd_sent)
  (hbd_received)
  (hbd_interest_payments)
  (hbd_paid_as_interest)
  (transfers_to_vesting)
  (hive_vested)
  (new_vests)
  (new_vesting_withdrawal_requests)
  (modified_vesting_withdrawal_requests)
  (vesting_withdrawals_processed)
  (finished_vesting_withdrawals)
  (vests_withdrawn)
  (hive_received_from_withdrawls)
  (hive_received_from_routes)
  (vests_received_from_routes)
  (hbd_conversion_requests_created)
  (hbd_to_be_converted)
  (hbd_conversion_requests_filled)
  (hive_converted)
  (limit_orders_created)
  (limit_orders_filled)
  (limit_orders_cancelled)
  (limit_order_hive_paid)
  (limit_order_hive_received)
  (limit_order_hbd_paid)
  (limit_order_hbd_received)
  (total_pow)
  (estimated_hashpower)
)
//SET_INDEX_TYPE( hive::account_statistics::account_stats_bucket_object,)

FC_REFLECT(
  hive::account_statistics::account_activity_bucket_object,
  (id)
  (open)
  (seconds)
  (active_market_accounts)
  (active_forum_accounts)
  (active_market_and_forum_accounts)
)
