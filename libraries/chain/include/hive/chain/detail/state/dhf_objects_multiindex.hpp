#pragma once
#include <hive/chain/detail/state/dhf_objects.hpp>

namespace hive { namespace chain {

  struct by_proposal_id {};
  struct by_start_date;
  struct by_end_date;
  struct by_creator;
  struct by_total_votes;

  typedef multi_index_container<
    proposal_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< proposal_object, proposal_object::id_type, &proposal_object::get_id > >,
      ordered_unique< tag< by_proposal_id >,
        member< proposal_object, uint32_t, &proposal_object::proposal_id > >,
      ordered_unique< tag< by_start_date >,
        composite_key< proposal_object,
          member< proposal_object, time_point_sec, &proposal_object::start_date >,
          member< proposal_object, uint32_t, &proposal_object::proposal_id >
        >,
        composite_key_compare< std::less< time_point_sec >, std::less< uint32_t > >
      >,
      ordered_unique< tag< by_end_date >,
        composite_key< proposal_object,
          const_mem_fun< proposal_object, time_point_sec, &proposal_object::get_end_date_with_delay >,
          member< proposal_object, uint32_t, &proposal_object::proposal_id >
        >,
        composite_key_compare< std::less< time_point_sec >, std::less< uint32_t > >
      >,
      ordered_unique< tag< by_creator >,
        composite_key< proposal_object,
          member< proposal_object, account_name_type, &proposal_object::creator >,
          member< proposal_object, uint32_t, &proposal_object::proposal_id >
        >,
        composite_key_compare< std::less< account_name_type >, std::less< uint32_t > >
      >,
      ordered_unique< tag< by_total_votes >,
        composite_key< proposal_object,
          member< proposal_object, uint64_t, &proposal_object::total_votes >,
          member< proposal_object, uint32_t, &proposal_object::proposal_id >
        >,
        composite_key_compare< std::less< uint64_t >, std::less< uint32_t > >
      >
    >,
    multi_index_allocator< proposal_object >
  > proposal_index;

  struct by_voter_proposal;
  struct by_proposal_voter;

  typedef multi_index_container<
    proposal_vote_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< proposal_vote_object, proposal_vote_object::id_type, &proposal_vote_object::get_id > >,
      ordered_unique< tag< by_voter_proposal >,
        composite_key< proposal_vote_object,
          member< proposal_vote_object, account_name_type, &proposal_vote_object::voter >,
          member< proposal_vote_object, uint32_t, &proposal_vote_object::proposal_id >
        >
      >,
      ordered_unique< tag< by_proposal_voter >,
        composite_key< proposal_vote_object,
          member< proposal_vote_object, uint32_t, &proposal_vote_object::proposal_id >,
          member< proposal_vote_object, account_name_type, &proposal_vote_object::voter >
        >
      >
    >,
    multi_index_allocator< proposal_vote_object >
  > proposal_vote_index;

} } // hive::chain

CHAINBASE_SET_INDEX_TYPE( hive::chain::proposal_object, hive::chain::proposal_index )
CHAINBASE_SET_INDEX_TYPE( hive::chain::proposal_vote_object, hive::chain::proposal_vote_index )
