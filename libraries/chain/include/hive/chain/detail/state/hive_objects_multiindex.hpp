#pragma once
#include <hive/chain/detail/state/hive_objects.hpp>

namespace hive { namespace chain {

  struct by_price;
  struct by_expiration;
  struct by_account;
  typedef multi_index_container<
    limit_order_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< limit_order_object, limit_order_object::id_type, &limit_order_object::get_id > >,
      ordered_unique< tag< by_expiration >,
        composite_key< limit_order_object,
          member< limit_order_object, time_point_sec, &limit_order_object::expiration >,
          const_mem_fun< limit_order_object, limit_order_object::id_type, &limit_order_object::get_id >
        >
      >,
      ordered_unique< tag< by_price >,
        composite_key< limit_order_object,
          member< limit_order_object, price, &limit_order_object::sell_price >,
          const_mem_fun< limit_order_object, limit_order_object::id_type, &limit_order_object::get_id >
        >,
        composite_key_compare< std::greater< price >, std::less< limit_order_id_type > >
      >,
      ordered_unique< tag< by_account >,
        composite_key< limit_order_object,
          member< limit_order_object, account_name_type, &limit_order_object::seller >,
          member< limit_order_object, uint32_t, &limit_order_object::orderid >
        >
      >
    >,
    multi_index_allocator< limit_order_object >
  > limit_order_index;

  struct by_owner;
  struct by_conversion_date;
  typedef multi_index_container<
    convert_request_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< convert_request_object, convert_request_object::id_type, &convert_request_object::get_id > >,
      ordered_unique< tag< by_conversion_date >,
        composite_key< convert_request_object,
          const_mem_fun< convert_request_object, time_point_sec, &convert_request_object::get_conversion_date >,
          const_mem_fun< convert_request_object, convert_request_object::id_type, &convert_request_object::get_id >
        >
      >,
      ordered_unique< tag< by_owner >,
        composite_key< convert_request_object,
          const_mem_fun< convert_request_object, account_id_type, &convert_request_object::get_owner >,
          const_mem_fun< convert_request_object, uint32_t, &convert_request_object::get_request_id >
        >
      >
    >,
    multi_index_allocator< convert_request_object >
  > convert_request_index;

  struct by_owner;
  struct by_conversion_date;
  typedef multi_index_container<
    collateralized_convert_request_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< collateralized_convert_request_object, collateralized_convert_request_object::id_type, &collateralized_convert_request_object::get_id > >,
      ordered_unique< tag< by_conversion_date >,
        composite_key< collateralized_convert_request_object,
          const_mem_fun< collateralized_convert_request_object, time_point_sec, &collateralized_convert_request_object::get_conversion_date >,
          const_mem_fun< collateralized_convert_request_object, collateralized_convert_request_object::id_type, &collateralized_convert_request_object::get_id >
        >
      >,
      ordered_unique< tag< by_owner >,
        composite_key< collateralized_convert_request_object,
          const_mem_fun< collateralized_convert_request_object, account_id_type, &collateralized_convert_request_object::get_owner >,
          const_mem_fun< collateralized_convert_request_object, uint32_t, &collateralized_convert_request_object::get_request_id >
        >
      >
    >,
    multi_index_allocator< collateralized_convert_request_object >
  > collateralized_convert_request_index;

  struct by_owner;
  struct by_volume_weight;

  typedef multi_index_container<
    liquidity_reward_balance_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< liquidity_reward_balance_object, liquidity_reward_balance_object::id_type, &liquidity_reward_balance_object::get_id > >,
      ordered_unique< tag< by_owner >,
        member< liquidity_reward_balance_object, account_id_type, &liquidity_reward_balance_object::owner > >,
      ordered_unique< tag< by_volume_weight >,
        composite_key< liquidity_reward_balance_object,
            member< liquidity_reward_balance_object, fc::uint128, &liquidity_reward_balance_object::weight >,
            member< liquidity_reward_balance_object, account_id_type, &liquidity_reward_balance_object::owner >
        >,
        composite_key_compare< std::greater< fc::uint128 >, std::less< account_id_type > >
      >
    >,
    multi_index_allocator< liquidity_reward_balance_object, 1024 > // not used after HF12
  > liquidity_reward_balance_index;

  typedef multi_index_container<
    feed_history_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< feed_history_object, feed_history_object::id_type, &feed_history_object::get_id > >
    >,
    multi_index_allocator< feed_history_object, 2 > // singleton (plus one internal)
  > feed_history_index;

  struct by_withdraw_route;
  struct by_destination;
  typedef multi_index_container<
    withdraw_vesting_route_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< withdraw_vesting_route_object, withdraw_vesting_route_object::id_type, &withdraw_vesting_route_object::get_id > >,
      ordered_unique< tag< by_withdraw_route >,
        composite_key< withdraw_vesting_route_object,
          member< withdraw_vesting_route_object, account_name_type, &withdraw_vesting_route_object::from_account >,
          member< withdraw_vesting_route_object, account_name_type, &withdraw_vesting_route_object::to_account >
        >,
        composite_key_compare< std::less< account_name_type >, std::less< account_name_type > >
      >,
      ordered_unique< tag< by_destination >,
        composite_key< withdraw_vesting_route_object,
          member< withdraw_vesting_route_object, account_name_type, &withdraw_vesting_route_object::to_account >,
          const_mem_fun< withdraw_vesting_route_object, withdraw_vesting_route_object::id_type, &withdraw_vesting_route_object::get_id >
        >
      >
    >,
    multi_index_allocator< withdraw_vesting_route_object >
  > withdraw_vesting_route_index;

  struct by_from_id {};
  struct by_ratification_deadline;
  typedef multi_index_container<
    escrow_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< escrow_object, escrow_object::id_type, &escrow_object::get_id > >,
      ordered_unique< tag< by_from_id >,
        composite_key< escrow_object,
          member< escrow_object, account_name_type, &escrow_object::from >,
          member< escrow_object, uint32_t, &escrow_object::escrow_id >
        >
      >,
      ordered_unique< tag< by_ratification_deadline >,
        composite_key< escrow_object,
          const_mem_fun< escrow_object, bool, &escrow_object::is_approved >,
          member< escrow_object, time_point_sec, &escrow_object::ratification_deadline >,
          const_mem_fun< escrow_object, escrow_object::id_type, &escrow_object::get_id >
        >,
        composite_key_compare< std::less< bool >, std::less< time_point_sec >, std::less< escrow_id_type > >
      >
    >,
    multi_index_allocator< escrow_object >
  > escrow_index;

  struct by_from_rid {};
  struct by_to_complete;
  struct by_complete_from_rid;
  typedef multi_index_container<
    savings_withdraw_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< savings_withdraw_object, savings_withdraw_object::id_type, &savings_withdraw_object::get_id > >,
      ordered_unique< tag< by_from_rid >,
        composite_key< savings_withdraw_object,
          member< savings_withdraw_object, account_name_type, &savings_withdraw_object::from >,
          member< savings_withdraw_object, uint32_t, &savings_withdraw_object::request_id >
        >
      >,
      ordered_unique< tag< by_complete_from_rid >,
        composite_key< savings_withdraw_object,
          member< savings_withdraw_object, time_point_sec, &savings_withdraw_object::complete >,
          member< savings_withdraw_object, account_name_type, &savings_withdraw_object::from >,
          member< savings_withdraw_object, uint32_t, &savings_withdraw_object::request_id >
        >
      >,
      ordered_unique< tag< by_to_complete >,
        composite_key< savings_withdraw_object,
          member< savings_withdraw_object, account_name_type, &savings_withdraw_object::to >,
          member< savings_withdraw_object, time_point_sec, &savings_withdraw_object::complete >,
          const_mem_fun< savings_withdraw_object, savings_withdraw_object::id_type, &savings_withdraw_object::get_id >
        >
      >
    >,
    multi_index_allocator< savings_withdraw_object >
  > savings_withdraw_index;

  struct by_account;
  struct by_effective_date;
  typedef multi_index_container<
    decline_voting_rights_request_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< decline_voting_rights_request_object, decline_voting_rights_request_object::id_type, &decline_voting_rights_request_object::get_id > >,
      ordered_unique< tag< by_account >,
        member< decline_voting_rights_request_object, account_name_type, &decline_voting_rights_request_object::account >
      >,
      ordered_unique< tag< by_effective_date >,
        composite_key< decline_voting_rights_request_object,
          member< decline_voting_rights_request_object, time_point_sec, &decline_voting_rights_request_object::effective_date >,
          member< decline_voting_rights_request_object, account_name_type, &decline_voting_rights_request_object::account >
        >,
        composite_key_compare< std::less< time_point_sec >, std::less< account_name_type > >
      >
    >,
    multi_index_allocator< decline_voting_rights_request_object >
  > decline_voting_rights_request_index;

  typedef multi_index_container<
    reward_fund_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< reward_fund_object, reward_fund_object::id_type, &reward_fund_object::get_id > >,
      ordered_unique< tag< by_name >,
        member< reward_fund_object, reward_fund_name_type, &reward_fund_object::name > >
    >,
    multi_index_allocator< reward_fund_object, 2 > // singleton (plus one internal)
  > reward_fund_index;

  struct by_from_to_id;
  struct by_from_id;
  struct by_trigger_date;
  typedef multi_index_container<
    recurrent_transfer_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< recurrent_transfer_object, recurrent_transfer_object::id_type, &recurrent_transfer_object::get_id > >,
      ordered_unique< tag< by_trigger_date >,
        composite_key< recurrent_transfer_object,
          const_mem_fun< recurrent_transfer_object, time_point_sec, &recurrent_transfer_object::get_trigger_date >,
          const_mem_fun< recurrent_transfer_object, recurrent_transfer_object::id_type, &recurrent_transfer_object::get_id >
        >
      >,
      ordered_unique< tag< by_from_id >,
        composite_key< recurrent_transfer_object,
          member< recurrent_transfer_object, account_id_type, &recurrent_transfer_object::from_id >,
          const_mem_fun< recurrent_transfer_object, recurrent_transfer_object::id_type, &recurrent_transfer_object::get_id >
        >
      >,
      ordered_unique< tag< by_from_to_id >,
        composite_key< recurrent_transfer_object,
          member< recurrent_transfer_object, account_id_type, &recurrent_transfer_object::from_id >,
          member< recurrent_transfer_object, account_id_type, &recurrent_transfer_object::to_id >,
          member< recurrent_transfer_object, uint8_t, &recurrent_transfer_object::pair_id >
        >,
        composite_key_compare< std::less< account_id_type >, std::less< account_id_type >, std::less< uint8_t > >
      >
    >,
    multi_index_allocator< recurrent_transfer_object >
  > recurrent_transfer_index;

} } // hive::chain

CHAINBASE_SET_INDEX_TYPE( hive::chain::limit_order_object, hive::chain::limit_order_index )
CHAINBASE_SET_INDEX_TYPE( hive::chain::feed_history_object, hive::chain::feed_history_index )
CHAINBASE_SET_INDEX_TYPE( hive::chain::convert_request_object, hive::chain::convert_request_index )
CHAINBASE_SET_INDEX_TYPE( hive::chain::collateralized_convert_request_object, hive::chain::collateralized_convert_request_index )
CHAINBASE_SET_INDEX_TYPE( hive::chain::liquidity_reward_balance_object, hive::chain::liquidity_reward_balance_index )
CHAINBASE_SET_INDEX_TYPE( hive::chain::withdraw_vesting_route_object, hive::chain::withdraw_vesting_route_index )
CHAINBASE_SET_INDEX_TYPE( hive::chain::savings_withdraw_object, hive::chain::savings_withdraw_index )
CHAINBASE_SET_INDEX_TYPE( hive::chain::escrow_object, hive::chain::escrow_index )
CHAINBASE_SET_INDEX_TYPE( hive::chain::decline_voting_rights_request_object, hive::chain::decline_voting_rights_request_index )
CHAINBASE_SET_INDEX_TYPE( hive::chain::reward_fund_object, hive::chain::reward_fund_index )
CHAINBASE_SET_INDEX_TYPE( hive::chain::recurrent_transfer_object, hive::chain::recurrent_transfer_index )
