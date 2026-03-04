#pragma once

#include <hive/utilities/benchmark_dumper.hpp>
#include <hive/chain/detail/state/recovery_object.hpp>
#include <hive/chain/detail/state/assets_object.hpp>
#include <hive/chain/detail/state/delayed_votes_object.hpp>

#include <hive/chain/external_storage/allocator_helper.hpp>

namespace hive { namespace chain {

/**
 * RocksDB serialization class for recovery_object
 */
class rocksdb_recovery_object
{
public:
  rocksdb_recovery_object() {}

  rocksdb_recovery_object( const recovery_object& obj )
    : id( obj.get_id() )
    , recovery_account( obj.get_recovery_account() )
    , last_account_recovery( obj.get_last_account_recovery_time() )
    , block_last_account_recovery( obj.get_block_last_account_recovery_time() )
  {}

  template<typename Return_Type>
  Return_Type build( chainbase::database& db )
  {
    if constexpr ( std::is_same_v<Return_Type, const recovery_object*> )
    {
      const auto& obj = db.create_no_undo<recovery_object>(
                      id.get_value(),
                      recovery_account );
      db.modify_no_undo( obj, [this]( recovery_object& o ) { restore_fields( o ); });
      return &obj;
    }
    else
    {
      auto obj = std::make_shared<recovery_object>(
                          allocator_helper::get_allocator<recovery_object, recovery_index>( db ),
                          id.get_value(),
                          recovery_account );
      restore_fields( *obj );
      return Return_Type( obj );
    }
  }

  recovery_id_type      id;
  account_id_type       recovery_account;
  time_point_sec        last_account_recovery;
  time_point_sec        block_last_account_recovery;

private:
  void restore_fields( recovery_object& o ) const
  {
    o.set_last_account_recovery_time( last_account_recovery );
    o.set_block_last_account_recovery_time( block_last_account_recovery );
  }
};

/**
 * RocksDB serialization class for assets_object
 */
class rocksdb_assets_object
{
public:
  rocksdb_assets_object() {}

  rocksdb_assets_object( const assets_object& obj )
    : id( obj.get_id() )
    , name( obj.get_name() )
    , hbd_balance( obj.get_hbd_balance() )
    , savings_hbd_balance( obj.get_hbd_savings() )
    , reward_hbd_balance( obj.get_hbd_rewards() )
    , reward_hive_balance( obj.get_rewards() )
    , reward_vesting_hive( obj.get_vest_rewards_as_hive() )
    , balance( obj.get_balance() )
    , savings_balance( obj.get_savings() )
    , reward_vesting_balance( obj.get_vest_rewards() )
    , vesting_shares( obj.get_vesting() )
    , delegated_vesting_shares( obj.get_delegated_vesting() )
    , received_vesting_shares( obj.get_received_vesting() )
    , vesting_withdraw_rate( obj.get_vesting_withdraw_rate() )
    , curation_rewards( obj.get_curation_rewards() )
    , posting_rewards( obj.get_posting_rewards() )
    , withdrawn( obj.get_withdrawn() )
    , to_withdraw( obj.get_to_withdraw() )
    , savings_hbd_seconds( obj.get_savings_hbd_seconds() )
    , savings_hbd_seconds_last_update( obj.get_savings_hbd_seconds_last_update() )
    , savings_hbd_last_interest_payment( obj.get_savings_hbd_last_interest_payment() )
    , hbd_seconds( obj.get_hbd_seconds() )
    , hbd_seconds_last_update( obj.get_hbd_seconds_last_update() )
    , hbd_last_interest_payment( obj.get_hbd_last_interest_payment() )
    , last_account_update( obj.get_last_account_update() )
    , last_post( obj.get_last_post() )
    , last_root_post( obj.get_last_root_post() )
    , last_post_edit( obj.get_last_post_edit() )
    , last_vote_time( obj.get_last_vote_time() )
    , next_vesting_withdrawal( obj.get_next_vesting_withdrawal() )
    , voting_manabar( obj.get_voting_manabar() )
    , downvote_manabar( obj.get_downvote_manabar() )
    , rc_manabar( obj.get_rc_manabar() )
    , rc_adjustment( obj.get_rc_adjustment() )
    , delegated_rc( obj.get_delegated_rc() )
    , received_rc( obj.get_received_rc() )
    , last_max_rc( obj.get_last_max_rc() )
    , post_count( obj.get_post_count() )
    , post_bandwidth( obj.get_post_bandwidth() )
  {}

  template<typename Return_Type>
  Return_Type build( chainbase::database& db )
  {
    if constexpr ( std::is_same_v<Return_Type, const assets_object*> )
    {
      const auto& obj = db.create_no_undo<assets_object>(
                      id.get_value(),
                      name );
      db.modify_no_undo( obj, [this]( assets_object& o ) { restore_fields( o ); });
      return &obj;
    }
    else
    {
      auto obj = std::make_shared<assets_object>(
                          allocator_helper::get_allocator<assets_object, assets_index>( db ),
                          id.get_value(),
                          name );
      restore_fields( *obj );
      return Return_Type( obj );
    }
  }

  assets_id_type        id;
  account_name_type     name;
  HBD_asset             hbd_balance;
  HBD_asset             savings_hbd_balance;
  HBD_asset             reward_hbd_balance;
  HIVE_asset            reward_hive_balance;
  HIVE_asset            reward_vesting_hive;
  HIVE_asset            balance;
  HIVE_asset            savings_balance;
  VEST_asset            reward_vesting_balance;
  VEST_asset            vesting_shares;
  VEST_asset            delegated_vesting_shares;
  VEST_asset            received_vesting_shares;
  VEST_asset            vesting_withdraw_rate;
  HIVE_asset            curation_rewards;
  HIVE_asset            posting_rewards;
  VEST_asset            withdrawn;
  VEST_asset            to_withdraw;
  uint128_t             savings_hbd_seconds = 0;
  time_point_sec        savings_hbd_seconds_last_update;
  time_point_sec        savings_hbd_last_interest_payment;
  uint128_t             hbd_seconds = 0;
  time_point_sec        hbd_seconds_last_update;
  time_point_sec        hbd_last_interest_payment;
  time_point_sec        last_account_update;
  time_point_sec        last_post;
  time_point_sec        last_root_post;
  time_point_sec        last_post_edit;
  time_point_sec        last_vote_time;
  time_point_sec        next_vesting_withdrawal = fc::time_point_sec::maximum();
  util::manabar         voting_manabar;
  util::manabar         downvote_manabar;
  util::manabar         rc_manabar;
  share_type            rc_adjustment;
  share_type            delegated_rc;
  share_type            received_rc;
  share_type            last_max_rc;
  uint32_t              post_count = 0;
  uint32_t              post_bandwidth = 0;

private:
  void restore_fields( assets_object& o ) const
  {
    o.set_hbd_balance( hbd_balance );
    o.set_hbd_savings( savings_hbd_balance );
    o.set_hbd_rewards( reward_hbd_balance );
    o.set_rewards( reward_hive_balance );
    o.set_vest_rewards_as_hive( reward_vesting_hive );
    o.set_balance( balance );
    o.set_savings( savings_balance );
    o.set_vest_rewards( reward_vesting_balance );
    o.set_vesting( vesting_shares );
    o.set_delegated_vesting( delegated_vesting_shares );
    o.set_received_vesting( received_vesting_shares );
    o.set_vesting_withdraw_rate( vesting_withdraw_rate );
    o.set_curation_rewards( curation_rewards );
    o.set_posting_rewards( posting_rewards );
    o.set_withdrawn( withdrawn );
    o.set_to_withdraw( to_withdraw );
    o.set_savings_hbd_seconds( savings_hbd_seconds );
    o.set_savings_hbd_seconds_last_update( savings_hbd_seconds_last_update );
    o.set_savings_hbd_last_interest_payment( savings_hbd_last_interest_payment );
    o.set_hbd_seconds( hbd_seconds );
    o.set_hbd_seconds_last_update( hbd_seconds_last_update );
    o.set_hbd_last_interest_payment( hbd_last_interest_payment );
    o.set_last_account_update( last_account_update );
    o.set_last_post( last_post );
    o.set_last_root_post( last_root_post );
    o.set_last_post_edit( last_post_edit );
    o.set_last_vote_time( last_vote_time );
    o.set_next_vesting_withdrawal( next_vesting_withdrawal );
    o.get_voting_manabar() = voting_manabar;
    o.get_downvote_manabar() = downvote_manabar;
    o.get_rc_manabar() = rc_manabar;
    o.set_rc_adjustment( rc_adjustment );
    o.set_delegated_rc( delegated_rc );
    o.set_received_rc( received_rc );
    o.set_last_max_rc( last_max_rc );
    o.set_post_count( post_count );
    o.set_post_bandwidth( post_bandwidth );
  }
};

/**
 * RocksDB serialization class for delayed_votes_object
 */
class rocksdb_delayed_votes_object
{
public:
  rocksdb_delayed_votes_object() {}

  rocksdb_delayed_votes_object( const delayed_votes_object& obj )
    : id( obj.get_id() )
    , sum_delayed_votes( obj.get_sum_delayed_votes() )
    , delayed_votes( delayed_votes_object::convert( obj.get_delayed_votes() ) )
  {}

  template<typename Return_Type>
  Return_Type build( chainbase::database& db )
  {
    if constexpr ( std::is_same_v<Return_Type, const delayed_votes_object*> )
    {
      const auto& obj = db.create_no_undo<delayed_votes_object>(
                      id.get_value() );
      db.modify_no_undo( obj, [this]( delayed_votes_object& o ) { restore_fields( o ); });
      return &obj;
    }
    else
    {
      auto obj = std::make_shared<delayed_votes_object>(
                          allocator_helper::get_allocator<delayed_votes_object, delayed_votes_index>( db ),
                          id.get_value() );
      restore_fields( *obj );
      return Return_Type( obj );
    }
  }

  delayed_votes_id_type id;
  ushare_type           sum_delayed_votes = 0;
  std::vector<delayed_votes_data> delayed_votes;

private:
  void restore_fields( delayed_votes_object& o ) const
  {
    o.set_sum_delayed_votes( sum_delayed_votes );
    o.clone( delayed_votes );
  }
};

} } // hive::chain

FC_REFLECT( hive::chain::rocksdb_recovery_object,
          (id)(recovery_account)(last_account_recovery)(block_last_account_recovery)
        )

FC_REFLECT( hive::chain::rocksdb_assets_object,
          (id)(name)
          (hbd_balance)(savings_hbd_balance)(reward_hbd_balance)
          (reward_hive_balance)(reward_vesting_hive)(balance)(savings_balance)
          (reward_vesting_balance)(vesting_shares)(delegated_vesting_shares)
          (received_vesting_shares)(vesting_withdraw_rate)
          (curation_rewards)(posting_rewards)
          (withdrawn)(to_withdraw)
          (savings_hbd_seconds)(savings_hbd_seconds_last_update)
          (savings_hbd_last_interest_payment)
          (hbd_seconds)
          (hbd_seconds_last_update)(hbd_last_interest_payment)
          (last_account_update)(last_post)(last_root_post)
          (last_post_edit)(last_vote_time)(next_vesting_withdrawal)
          (voting_manabar)(downvote_manabar)(rc_manabar)
          (rc_adjustment)(delegated_rc)(received_rc)(last_max_rc)
          (post_count)(post_bandwidth)
        )

FC_REFLECT( hive::chain::rocksdb_delayed_votes_object,
          (id)(sum_delayed_votes)(delayed_votes)
        )
