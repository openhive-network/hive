#pragma once

#ifdef USE_ALTERNATE_CHAIN_ID

#include <hive/protocol/types.hpp>
#include <hive/protocol/hardfork.hpp>

#include <fc/optional.hpp>
#include <fc/time.hpp>

#include <cstdint>
#include <string>
#include <array>
#include <vector>

#include <assert.h>

namespace hive { namespace protocol { namespace testnet_blockchain_configuration {
  struct hardfork_schedule_item_t
  {
    uint32_t hardfork;
    uint32_t block_num;
  };

  class configuration
  {
    // time after comment creation when voter receives penalty on effective weight;
    // the more severe penalty the closer to comment creation the vote is;
    // takes precedence over other vote windows; value applied during HF25 - later needs to be set in dgpo
    uint32_t hive_reverse_auction_window_seconds = 0; // disabled
    // time after comment creation when votes stop getting full weight and start to get only half
    // if zero it also disables mid voting window; value applied during HF25 - later needs to be set in dgpo
    uint32_t hive_early_voting_seconds = 60 * 10; // 10 minutes
    // time after end of early voting window when votes stop getting half of weight and start getting 1/8;
    // value applied during HF25 - later needs to be set in dgpo
    uint32_t hive_mid_voting_seconds = 60 * 20; // 20 minutes
    // time after comment creation when cashout occurs
    uint32_t hive_cashout_window_seconds = 60 * 60; // 1 hr
    // time before cashout when vote uses less power;
    // the more severe penalty the closer to comment cashout the vote is
    uint32_t hive_upvote_lockout_seconds = 60 * 5; // 5 minutes
    // number of blocks between feed updates
    uint32_t hive_feed_interval_blocks = 20; // blocks, originally 60*60/3
    // time before feed expires
    uint32_t hive_max_feed_age_seconds = 60*24*7; // originally 60*60*24*7, see comment to set_feed_related_values
    // time before savings are withdrawn
    uint32_t hive_savings_withdraw_time = 60*60*24*3; // seconds, originally 3 days
    // Minimum number of hours between recurrent transfers.
    uint16_t hive_min_recurrent_transfers_recurrence = 24; // hours, originally 24
    // Delay between funds vesting and its usage in governance voting.
    uint64_t hive_delayed_voting_total_interval_seconds = 60*60*24*1; // 1 day, originally 30 days
    // Time before governance votes expire.
    int64_t hive_governance_vote_expiration_period = 60*60*24*5; // seconds, originally 5 days
    // How often proposals are taken care of.
    uint32_t hive_proposal_maintenance_period = 60*60; // 1 hour
    
    std::string hive_hf9_compromised_key;
    hive::protocol::private_key_type hive_initminer_key;

    std::vector<std::string> init_witnesses;
    fc::time_point_sec     genesis_time;
    // Hardfork times expressed in seconds since epoch.
    std::array< uint32_t, HIVE_NUM_HARDFORKS + 1> hf_times = {};
    fc::optional<uint64_t> init_supply, hbd_init_supply;
    // Whether we want producer_missed_operation & shutdown_witness_operation vops generated.
    bool generate_missed_block_operations = 
#ifdef IS_TEST_NET
      false; // Don't generate for testnet node (unless overridden in e.g. unit test).
#else
      true; // Do generate for other non-mainnet nodes.
#endif // IS_TEST_NET
    // How many blocks is witness allowed to miss before it is being shut down.
    uint16_t witness_shutdown_threshold = 28800; // aka HIVE_BLOCKS_PER_DAY

    public:
      configuration();

      void set_init_supply( uint64_t init_supply );
      void set_hbd_init_supply( uint64_t hbd_init_supply );
      uint64_t get_init_supply( uint64_t default_value )const;
      uint64_t get_hbd_init_supply( uint64_t default_value )const;

      void set_init_witnesses( const std::vector<std::string>& init_witnesses );
      const std::vector<std::string>& get_init_witnesses()const;
      typedef std::vector<hardfork_schedule_item_t> hardfork_schedule_t;
      /**
       * Sets genesis time and consecutive hardfork times to be retrieved by get_hf_time later.
       * 
       * @param genesis_time - when genesis should happen.
       * @param hardfork_schedule - when hardforks following genesis should happen. No need to specify all of them, the function
       * will fill in the gaps while get_hf_time will handle the tail ones (see implementation).
       */
      void set_hardfork_schedule( const fc::time_point_sec& genesis_time, const hardfork_schedule_t& hardfork_schedule );
      void reset_hardfork_schedule();
      uint32_t get_hf_time(uint32_t hf_num, uint32_t default_time_sec)const;
      bool get_generate_missed_block_operations() const { return generate_missed_block_operations; }
      uint16_t get_witness_shutdown_threshold() const { return witness_shutdown_threshold; }

      uint32_t get_hive_reverse_auction_window_seconds() const { return hive_reverse_auction_window_seconds; }
      uint32_t get_hive_early_voting_seconds() const { return hive_early_voting_seconds; }
      uint32_t get_hive_mid_voting_seconds() const { return hive_mid_voting_seconds; }
      uint32_t get_hive_cashout_window_seconds() const { return hive_cashout_window_seconds; }
      uint32_t get_hive_upvote_lockout_seconds() const { return hive_upvote_lockout_seconds; }

      uint32_t get_hive_feed_interval_blocks() const { return hive_feed_interval_blocks; }
      uint32_t get_hive_max_feed_age_seconds() const { return hive_max_feed_age_seconds; }

      uint32_t get_hive_savings_withdraw_time() const { return hive_savings_withdraw_time; }
      uint16_t get_hive_min_recurrent_transfers_recurrence() const { return hive_min_recurrent_transfers_recurrence; }

      uint32_t get_hive_delayed_voting_total_interval_seconds() const { return hive_delayed_voting_total_interval_seconds; }
      uint32_t get_hive_delayed_voting_interval_seconds() const { return get_hive_delayed_voting_total_interval_seconds() / 30; }
      int64_t  get_hive_governance_vote_expiration_period() const { return hive_governance_vote_expiration_period; }

      uint32_t get_hive_proposal_maintenance_period() const { return hive_proposal_maintenance_period; }

      std::string get_HF9_compromised_accounts_key() const { return hive_hf9_compromised_key; }

      hive::protocol::private_key_type get_default_initminer_private_key() const;

      hive::protocol::private_key_type get_initminer_private_key() const { return hive_initminer_key; }
      hive::protocol::public_key_type get_initminer_public_key() const;

      /** Sets cashout related values (relative to comment creation time):
      * [0 .. reverse auction)
      * [reverse auction .. early voting)
      * [early voting .. early voting + mid voting)
      * [early voting + mid voting .. cashout - upvote lockout)
      * [cashout - upvote lockout .. cashout)
      */
      void set_cashout_related_values( uint32_t reverse_auction, uint32_t early_voting,
        uint32_t mid_voting, uint32_t cashout, uint32_t upvote_lockout )
      {
        assert( reverse_auction <= cashout && early_voting + mid_voting <= cashout && upvote_lockout <= cashout );
        assert( early_voting > 0 || mid_voting == 0 );
        hive_reverse_auction_window_seconds = reverse_auction;
        hive_early_voting_seconds = early_voting;
        hive_mid_voting_seconds = mid_voting;
        hive_cashout_window_seconds = cashout;
        hive_upvote_lockout_seconds = upvote_lockout;
      }

      void reset_cashout_values() 
      { 
        configuration clear;
        set_cashout_related_values( clear.get_hive_reverse_auction_window_seconds(), clear.get_hive_early_voting_seconds(),
                                    clear.get_hive_mid_voting_seconds(), clear.get_hive_cashout_window_seconds(),
                                    clear.get_hive_upvote_lockout_seconds() );
      }

      /** Sets wintess feed related values:
       * [1 .. HIVE_BLOCKS_PER_HOUR]
       * [3*24*7 .. 60*60*24*7] recommended to be in 504:1 ratio with feed_interval_blocks
       */
      void set_feed_related_values( uint32_t feed_interval_blocks, uint32_t max_feed_age_seconds )
      {
        hive_feed_interval_blocks = feed_interval_blocks;
        hive_max_feed_age_seconds = max_feed_age_seconds;
      }

      void reset_feed_values()
      {
        configuration clear;
        set_feed_related_values( clear.get_hive_feed_interval_blocks(), clear.get_hive_max_feed_age_seconds() );
      }

      /**
       * [1 .. 60*60*24*3]
       */
      void set_savings_related_values( uint32_t savings_withdraw_time )
      {
        hive_savings_withdraw_time = savings_withdraw_time;
      }

      void reset_savings_values()
      {
        configuration clear;
        set_savings_related_values( clear.get_hive_savings_withdraw_time() );
      }

      /**
       * [1 .. 24]
       */
      void set_min_recurrent_transfers_recurrence( uint16_t min_recurrent_transfers_recurrence )
      {
        hive_min_recurrent_transfers_recurrence = min_recurrent_transfers_recurrence;
      }

      void reset_recurrent_transfers_values()
      {
        configuration clear;
        set_min_recurrent_transfers_recurrence( clear.get_hive_min_recurrent_transfers_recurrence() );
      }

      /**
       * [30*3 .. 60*60*24*30]
       * [15 .. 60*60*24*5 ]
       */
      void set_governance_voting_related_values( uint64_t delayed_voting_total_interval_seconds, int64_t governance_vote_expiration_period )
      {
        hive_delayed_voting_total_interval_seconds = delayed_voting_total_interval_seconds;
        hive_governance_vote_expiration_period = governance_vote_expiration_period;
      }

      void reset_governance_voting_values()
      {
        configuration clear;
        set_governance_voting_related_values( clear.get_hive_delayed_voting_total_interval_seconds(),
                                              clear.get_hive_governance_vote_expiration_period() );
      }

      /**
       * [3 .. 3600]
       */
      void set_proposal_related_values( uint32_t proposal_maintenance_period )
      {
        hive_proposal_maintenance_period = proposal_maintenance_period;
      }

      void reset_proposal_values()
      {
        configuration clear;
        set_proposal_related_values( clear.get_hive_proposal_maintenance_period() );
      }

      void set_skeleton_key(const hive::protocol::private_key_type& private_key);

      void set_generate_missed_block_operations( bool generate )
      {
        generate_missed_block_operations = generate;
      }

      /**
       * [0 .. HIVE_BLOCKS_PER_DAY]
       */
      void set_witness_shutdown_threshold( uint16_t threshold )
      {
         witness_shutdown_threshold = threshold;
      }
  };

  extern configuration configuration_data;

} } }// hive::protocol::testnet_blockchain_configuration

FC_REFLECT( hive::protocol::testnet_blockchain_configuration::hardfork_schedule_item_t, (hardfork)(block_num) )

#endif // USE_ALTERNATE_CHAIN_ID