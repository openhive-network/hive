#include <hive/protocol/blockchain_configuration.hpp>

#include <hive/protocol/types.hpp>
#include <hive/protocol/asset.hpp>

namespace hive
{
  namespace protocol
  {
    namespace blockchain_configuration
    {
      blockchain_configuration_t::blockchain_configuration_t() 
        :
#ifdef IS_TEST_NET
        hive_cashout_window_seconds_pre_hf12{hive_cashout_window_seconds.get()},
        hive_cashout_window_seconds_pre_hf17{hive_cashout_window_seconds.get()},
#else
        hive_daily_proposal_maintenance_period{hive_one_day_seconds.get()},
#endif

        hive_blockchain_hardfork_version{hive_blockchain_version.get()},
        hive_mining_reward{1000, this->hive_symbol.get()},
        hive_min_liquidity_reward{1000 * hive_liquidity_reward_blocks.get(), hive_symbol.get()},
        hive_min_content_reward{hive_mining_reward.get()},
        hive_min_curate_reward{hive_mining_reward.get()},
        hive_min_producer_reward{hive_mining_reward.get()},
        hive_min_pow_reward{hive_mining_reward.get()},
        hive_active_challenge_fee{2000, hive_symbol.get()},
        hive_owner_challenge_fee{30000, hive_symbol.get()},
        hive_min_payout_hbd{20, hbd_symbol.get()},
        hive_min_block_size_limit{static_cast<uint32_t>(hive_max_transaction_size.get())},
        hive_max_block_size{hive_max_transaction_size.get() * hive_block_interval * 2000u},
        hive_feed_interval_blocks{hive_blocks_per_hour.get()},
        hive_conversion_delay{fc::hours(hive_feed_history_window.get())},
        hive_delegation_return_period_hf0{hive_cashout_window_seconds.get()},
        hive_root_post_parent{ hive::protocol::account_name_type{} },
        hive_treasury_fee{10 * hive_blockchain_precision.get()},
        hive_proposal_fee_increase_days_sec{60u * 60 * 24 * hive_proposal_fee_increase_days.get()},
        hive_proposal_fee_increase_amount{1 * hive_blockchain_precision.get()},
        days_for_delayed_voting{hive_delayed_voting_total_interval_seconds.get() / hive_delayed_voting_interval_seconds.get()}

#ifdef HIVE_ENABLE_SMT
      , smt_max_votes_per_regeneration {(smt_max_nominal_votes_per_day.get() * smt_vesting_withdraw_interval_seconds.get()) / 86400 }
#endif
      {
      }

      blockchain_configuration_t g_blockchain_configuration{};
    }
  }

  const typename hive::protocol::blockchain_configuration::blockchain_configuration_t &get_blockchain_configuration() { return hive::protocol::blockchain_configuration::g_blockchain_configuration; }
  void set_blockchain_configuration(const typename hive::protocol::blockchain_configuration::blockchain_configuration_t &new_conf) { hive::protocol::blockchain_configuration::g_blockchain_configuration = new_conf; }
  void restore_blockchain_configuration() { set_blockchain_configuration(hive::protocol::blockchain_configuration::blockchain_configuration_t{}); }

} // hive::protocol::blockchain_configuration
