#pragma once
#include <hive/chain/hive_fwd.hpp>

#include <hive/chain/hive_object_types.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <hive/protocol/asset.hpp>
#include <hive/protocol/config.hpp>

namespace hive { namespace chain {

using hive::protocol::asset;
using hive::protocol::HBD_asset;

class proposal_object : public object< proposal_object_type, proposal_object, std::true_type >
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
    HBD_asset daily_pay;

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

    size_t get_dynamic_alloc() const
    {
      size_t size = 0;
      size += subject.capacity() * sizeof( decltype( subject )::value_type );
      size += permlink.capacity() * sizeof( decltype( permlink )::value_type );
      return size;
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

} } // hive::chain

FC_REFLECT( hive::chain::proposal_object, (id)(proposal_id)(creator)(receiver)(start_date)(end_date)(daily_pay)(subject)(permlink)(total_votes)(removed) )
FC_REFLECT( hive::chain::proposal_vote_object, (id)(voter)(proposal_id) )
