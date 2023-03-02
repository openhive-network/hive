#pragma once

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

    std::string hive_hf9_compromised_key;
    hive::protocol::private_key_type hive_initminer_key;

    std::vector<std::string> init_witnesses;
    fc::time_point_sec     genesis_time;
    std::array<fc::optional<uint32_t>, HIVE_NUM_HARDFORKS + 1> blocks = {};

    public:
      configuration();

      void set_init_witnesses( const std::vector<std::string>& init_witnesses );
      const std::vector<std::string>& get_init_witnesses()const;
      void set_genesis_time( const fc::time_point_sec& genesis_time );
      void set_hardfork_schedule( const std::vector<hardfork_schedule_item_t>& hardfork_schedule );
      uint32_t get_hf_time(uint32_t hf_num, uint32_t default_time_sec)const;

      uint32_t get_hive_reverse_auction_window_seconds() const { return hive_reverse_auction_window_seconds; }
      uint32_t get_hive_early_voting_seconds() const { return hive_early_voting_seconds; }
      uint32_t get_hive_mid_voting_seconds() const { return hive_mid_voting_seconds; }
      uint32_t get_hive_cashout_window_seconds() const { return hive_cashout_window_seconds; }
      uint32_t get_hive_upvote_lockout_seconds() const { return hive_upvote_lockout_seconds; }

      uint32_t get_hive_feed_interval_blocks() const { return hive_feed_interval_blocks; }
      uint32_t get_hive_max_feed_age_seconds() const { return hive_max_feed_age_seconds; }

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

      void set_skeleton_key(const hive::protocol::private_key_type& private_key);
  };

  extern configuration configuration_data;

} } }// hive::protocol::testnet_blockchain_configuration

FC_REFLECT( hive::protocol::testnet_blockchain_configuration::hardfork_schedule_item_t, (hardfork)(block_num) )
