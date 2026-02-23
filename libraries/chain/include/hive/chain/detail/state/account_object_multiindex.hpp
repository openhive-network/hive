#pragma once
#include <hive/chain/detail/state/account_object.hpp>

namespace hive { namespace chain {

  struct by_block {};  // Index tag for RocksDB archiving (by last_access_block)
  /**
    * @ingroup object_index
    */
  typedef multi_index_container<
    account_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< account_object, account_object::id_type, &account_object::get_id > >,
      ordered_unique< tag< by_name >,
        const_mem_fun< account_object, const account_name_type&, &account_object::get_name > >,
      ordered_unique< tag< by_block >,
        composite_key< account_object,
          const_mem_fun< account_object, uint32_t, &account_object::get_last_access_block >,
          const_mem_fun< account_object, account_object::id_type, &account_object::get_id >
        >
      >
    >,
    multi_index_allocator< account_object >
  > account_index;

  struct by_account {};

  typedef multi_index_container <
    owner_authority_history_object,
    indexed_by <
      ordered_unique< tag< by_id >,
        const_mem_fun< owner_authority_history_object, owner_authority_history_object::id_type, &owner_authority_history_object::get_id > >,
      ordered_unique< tag< by_account >,
        composite_key< owner_authority_history_object,
          member< owner_authority_history_object, account_name_type, &owner_authority_history_object::account >,
          member< owner_authority_history_object, time_point_sec, &owner_authority_history_object::last_valid_time >,
          const_mem_fun< owner_authority_history_object, owner_authority_history_object::id_type, &owner_authority_history_object::get_id >
        >,
        composite_key_compare< std::less< account_name_type >, std::less< time_point_sec >, std::less< owner_authority_history_id_type > >
      >
    >,
    multi_index_allocator< owner_authority_history_object >
  > owner_authority_history_index;

  typedef multi_index_container <
    account_metadata_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< account_metadata_object, account_metadata_object::id_type, &account_metadata_object::get_id > >,
      ordered_unique< tag< by_account >,
        member< account_metadata_object, account_id_type, &account_metadata_object::account > >,
      ordered_unique< tag< by_block >,
        composite_key< account_metadata_object,
          const_mem_fun< account_metadata_object, uint32_t, &account_metadata_object::get_last_access_block >,
          const_mem_fun< account_metadata_object, account_metadata_object::id_type, &account_metadata_object::get_id >
        >
      >
    >,
    multi_index_allocator< account_metadata_object >
  > account_metadata_index;

  typedef multi_index_container <
    account_authority_object,
    indexed_by <
      ordered_unique< tag< by_id >,
        const_mem_fun< account_authority_object, account_authority_object::id_type, &account_authority_object::get_id > >,
      ordered_unique< tag< by_account >,
        composite_key< account_authority_object,
          member< account_authority_object, account_name_type, &account_authority_object::account >,
          const_mem_fun< account_authority_object, account_authority_object::id_type, &account_authority_object::get_id >
        >,
        composite_key_compare< std::less< account_name_type >, std::less< account_authority_id_type > >
      >,
      ordered_unique< tag< by_block >,
        composite_key< account_authority_object,
          const_mem_fun< account_authority_object, uint32_t, &account_authority_object::get_last_access_block >,
          const_mem_fun< account_authority_object, account_authority_object::id_type, &account_authority_object::get_id >
        >
      >
    >,
    multi_index_allocator< account_authority_object >
  > account_authority_index;

  struct by_delegation {};

  typedef multi_index_container <
    vesting_delegation_object,
    indexed_by <
      ordered_unique< tag< by_id >,
        const_mem_fun< vesting_delegation_object, vesting_delegation_object::id_type, &vesting_delegation_object::get_id > >,
      ordered_unique< tag< by_delegation >,
        composite_key< vesting_delegation_object,
          const_mem_fun< vesting_delegation_object, account_id_type, &vesting_delegation_object::get_delegator >,
          const_mem_fun< vesting_delegation_object, account_id_type, &vesting_delegation_object::get_delegatee >
        >
      >
    >,
    multi_index_allocator< vesting_delegation_object >
  > vesting_delegation_index;

  struct by_expiration {};
  struct by_account_expiration {};

  typedef multi_index_container <
    vesting_delegation_expiration_object,
    indexed_by <
      ordered_unique< tag< by_id >,
        const_mem_fun< vesting_delegation_expiration_object, vesting_delegation_expiration_object::id_type, &vesting_delegation_expiration_object::get_id > >,
      ordered_unique< tag< by_expiration >,
        composite_key< vesting_delegation_expiration_object,
          const_mem_fun< vesting_delegation_expiration_object, time_point_sec, &vesting_delegation_expiration_object::get_expiration_time >,
          const_mem_fun< vesting_delegation_expiration_object, vesting_delegation_expiration_object::id_type, &vesting_delegation_expiration_object::get_id >
        >
      >,
      ordered_unique< tag< by_account_expiration >,
        composite_key< vesting_delegation_expiration_object,
          const_mem_fun< vesting_delegation_expiration_object, account_id_type, &vesting_delegation_expiration_object::get_delegator >,
          const_mem_fun< vesting_delegation_expiration_object, time_point_sec, &vesting_delegation_expiration_object::get_expiration_time >,
          const_mem_fun< vesting_delegation_expiration_object, vesting_delegation_expiration_object::id_type, &vesting_delegation_expiration_object::get_id >
        >
      >
    >,
    multi_index_allocator< vesting_delegation_expiration_object >
  > vesting_delegation_expiration_index;

  typedef multi_index_container <
    account_recovery_request_object,
    indexed_by <
      ordered_unique< tag< by_id >,
        const_mem_fun< account_recovery_request_object, account_recovery_request_object::id_type, &account_recovery_request_object::get_id > >,
      ordered_unique< tag< by_account >,
        const_mem_fun< account_recovery_request_object, const account_name_type&, &account_recovery_request_object::get_account_to_recover >
      >,
      ordered_unique< tag< by_expiration >,
        composite_key< account_recovery_request_object,
          const_mem_fun< account_recovery_request_object, time_point_sec, &account_recovery_request_object::get_expiration_time >,
          const_mem_fun< account_recovery_request_object, const account_name_type&, &account_recovery_request_object::get_account_to_recover >
        >,
        composite_key_compare< std::less< time_point_sec >, std::less< const account_name_type& > >
      >
    >,
    multi_index_allocator< account_recovery_request_object >
  > account_recovery_request_index;

  struct by_effective_date {};

  typedef multi_index_container <
    change_recovery_account_request_object,
    indexed_by <
      ordered_unique< tag< by_id >,
        const_mem_fun< change_recovery_account_request_object, change_recovery_account_request_object::id_type, &change_recovery_account_request_object::get_id > >,
      ordered_unique< tag< by_account >,
        const_mem_fun< change_recovery_account_request_object, const account_name_type&, &change_recovery_account_request_object::get_account_to_recover >
      >,
      ordered_unique< tag< by_effective_date >,
        composite_key< change_recovery_account_request_object,
          const_mem_fun< change_recovery_account_request_object, time_point_sec, &change_recovery_account_request_object::get_execution_time >,
          const_mem_fun< change_recovery_account_request_object, const account_name_type&, &change_recovery_account_request_object::get_account_to_recover >
        >,
        composite_key_compare< std::less< time_point_sec >, std::less< const account_name_type& > >
      >
    >,
    multi_index_allocator< change_recovery_account_request_object >
  > change_recovery_account_request_index;
} }

CHAINBASE_SET_INDEX_TYPE( hive::chain::account_object, hive::chain::account_index )
CHAINBASE_SET_INDEX_TYPE( hive::chain::account_metadata_object, hive::chain::account_metadata_index )
CHAINBASE_SET_INDEX_TYPE( hive::chain::account_authority_object, hive::chain::account_authority_index )
CHAINBASE_SET_INDEX_TYPE( hive::chain::vesting_delegation_object, hive::chain::vesting_delegation_index )
CHAINBASE_SET_INDEX_TYPE( hive::chain::vesting_delegation_expiration_object, hive::chain::vesting_delegation_expiration_index )
CHAINBASE_SET_INDEX_TYPE( hive::chain::owner_authority_history_object, hive::chain::owner_authority_history_index )
CHAINBASE_SET_INDEX_TYPE( hive::chain::account_recovery_request_object, hive::chain::account_recovery_request_index )
CHAINBASE_SET_INDEX_TYPE( hive::chain::change_recovery_account_request_object, hive::chain::change_recovery_account_request_index )
