#pragma once

#include <hive/chain/database.hpp>
#include <hive/chain/sps_objects.hpp>
#include <hive/chain/util/remove_guard.hpp>

#include <boost/container/flat_set.hpp>

namespace hive { namespace chain {

using boost::container::flat_set;

class sps_helper
{
  public:
    static void remove_proposal_votes( const proposal_object& proposal, const proposal_vote_index::index<by_proposal_voter>::type& proposal_votes,
      database& db, remove_guard& obj_perf )
    {
      auto pVoteI = proposal_votes.lower_bound( boost::make_tuple( proposal.proposal_id, account_name_type() ) );
      while( pVoteI != proposal_votes.end() && pVoteI->proposal_id == proposal.proposal_id )
      {
        const auto& vote = *pVoteI;
        ++pVoteI;
        if( !obj_perf.remove( db, vote ) )
          break;
      }
    }

    static void remove_proposal( const proposal_object& proposal, const proposal_vote_index::index<by_proposal_voter>::type& proposal_votes,
      database& db, remove_guard& obj_perf )
    {
      remove_proposal_votes( proposal, proposal_votes, db, obj_perf );
      obj_perf.remove( db, proposal );
    }

    static void remove_proposals( database& db, const flat_set<int64_t>& proposal_ids, const account_name_type& proposal_owner );
};

} } // hive::chain
