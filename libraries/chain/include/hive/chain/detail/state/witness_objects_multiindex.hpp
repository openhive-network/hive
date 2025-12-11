#pragma once
#include <hive/chain/detail/state/witness_objects.hpp>

namespace hive { namespace chain {

  struct by_vote_name;
  struct by_pow;
  struct by_work;
  struct by_schedule_time;
  /**
    * @ingroup object_index
    */
  typedef multi_index_container<
    witness_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< witness_object, witness_object::id_type, &witness_object::get_id > >,
      ordered_unique< tag< by_work >,
        composite_key< witness_object,
          member< witness_object, digest_type, &witness_object::last_work >,
          const_mem_fun< witness_object, witness_object::id_type, &witness_object::get_id >
        >
      >,
      ordered_unique< tag< by_name >,
        member< witness_object, account_name_type, &witness_object::owner > >,
      ordered_unique< tag< by_pow >,
        composite_key< witness_object,
          member< witness_object, uint64_t, &witness_object::pow_worker >,
          const_mem_fun< witness_object, witness_object::id_type, &witness_object::get_id >
        >
      >,
      ordered_unique< tag< by_vote_name >,
        composite_key< witness_object,
          member< witness_object, share_type, &witness_object::votes >,
          member< witness_object, account_name_type, &witness_object::owner >
        >,
        composite_key_compare< std::greater< share_type >, std::less< account_name_type > >
      >,
      ordered_unique< tag< by_schedule_time >,
        composite_key< witness_object,
          member< witness_object, fc::uint128, &witness_object::virtual_scheduled_time >,
          const_mem_fun< witness_object, witness_object::id_type, &witness_object::get_id >
        >
      >
    >,
    multi_index_allocator< witness_object >
  > witness_index;

  struct by_account_witness;
  struct by_witness_account;
  typedef multi_index_container<
    witness_vote_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< witness_vote_object, witness_vote_object::id_type, &witness_vote_object::get_id > >,
      ordered_unique< tag< by_account_witness >,
        composite_key< witness_vote_object,
          member< witness_vote_object, account_name_type, &witness_vote_object::account >,
          member< witness_vote_object, account_name_type, &witness_vote_object::witness >
        >,
        composite_key_compare< std::less< account_name_type >, std::less< account_name_type > >
      >,
      ordered_unique< tag< by_witness_account >,
        composite_key< witness_vote_object,
          member< witness_vote_object, account_name_type, &witness_vote_object::witness >,
          member< witness_vote_object, account_name_type, &witness_vote_object::account >
        >,
        composite_key_compare< std::less< account_name_type >, std::less< account_name_type > >
      >
    >, // indexed_by
    multi_index_allocator< witness_vote_object >
  > witness_vote_index;

  typedef multi_index_container<
    witness_schedule_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< witness_schedule_object, witness_schedule_object::id_type, &witness_schedule_object::get_id > >
    >,
    multi_index_allocator< witness_schedule_object, 4 > // dubleton (plus one internal)
  > witness_schedule_index;

} } // hive::chain

CHAINBASE_SET_INDEX_TYPE( hive::chain::witness_object, hive::chain::witness_index )
CHAINBASE_SET_INDEX_TYPE( hive::chain::witness_vote_object, hive::chain::witness_vote_index )
CHAINBASE_SET_INDEX_TYPE( hive::chain::witness_schedule_object, hive::chain::witness_schedule_index )
