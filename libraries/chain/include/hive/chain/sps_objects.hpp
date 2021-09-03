#pragma once
#include <chainbase/forward_declarations.hpp>

#include <hive/chain/hive_object_types.hpp>
#include <hive/chain/hive_objects.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <hive/protocol/asset.hpp>

namespace hive { namespace chain {

using hive::protocol::asset;

class proposal_object : public object< proposal_object_type, proposal_object >
{
  CHAINBASE_OBJECT( proposal_object );
  public:
    CHAINBASE_DEFAULT_CONSTRUCTOR( proposal_object, (subject)(permlink) )

    //additional key, at this moment has the same value as `id` member
    uint32_t proposal_id;

    // account that created the proposal
    account_name_type creator;

    // account_being_funded
    account_name_type receiver;

    // start_date (when the proposal will begin paying out if it gets enough vote weight)
    time_point_sec start_date;

    // end_date (when the proposal expires and can no longer pay out)
    time_point_sec end_date;

    //daily_pay (the amount of HBD that is being requested to be paid out daily)
    asset daily_pay;

    //subject (a very brief description or title for the proposal)
    shared_string subject;

    //permlink (a link to a page describing the work proposal in depth, generally this will probably be to a Hive post).
    shared_string permlink;

    //This will be calculate every maintenance period
    uint64_t total_votes = 0;

    bool removed = false;

    time_point_sec get_end_date_with_delay() const
    {
      time_point_sec ret = end_date;
      ret += HIVE_PROPOSAL_MAINTENANCE_CLEANUP;

      return ret;
    }

  CHAINBASE_UNPACK_CONSTRUCTOR(proposal_object, (subject)(permlink));
};

class proposal_vote_object : public object< proposal_vote_object_type, proposal_vote_object>
{
  CHAINBASE_OBJECT( proposal_vote_object );
  public:
    CHAINBASE_DEFAULT_CONSTRUCTOR( proposal_vote_object )

    //account that made voting
    account_name_type voter;

    //the voter voted for this proposal number
    uint32_t proposal_id; //note: it cannot be proposal_id_type because we are searching using proposal_object::proposal_id, not proposal_object::id
  
  CHAINBASE_UNPACK_CONSTRUCTOR(proposal_vote_object);
};

struct by_proposal_id;
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
  allocator< proposal_object >
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
  allocator< proposal_vote_object >
> proposal_vote_index;

} } // hive::chain

FC_REFLECT( hive::chain::proposal_object, (id)(proposal_id)(creator)(receiver)(start_date)(end_date)(daily_pay)(subject)(permlink)(total_votes)(removed) )
CHAINBASE_SET_INDEX_TYPE( hive::chain::proposal_object, hive::chain::proposal_index )

FC_REFLECT( hive::chain::proposal_vote_object, (id)(voter)(proposal_id) )
CHAINBASE_SET_INDEX_TYPE( hive::chain::proposal_vote_object, hive::chain::proposal_vote_index )
