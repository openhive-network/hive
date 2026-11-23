#pragma once

#include <hive/chain/detail/state/dhf_objects_multiindex.hpp>
#include <hive/chain/util/remove_guard.hpp>

namespace hive { namespace chain {

class account_object;
class database;

class dhf_helper
{
  public:
    // removes votes cast for proposals by given account (as long as we are within limit), returns if the process was successful
    static bool remove_proposal_votes( const account_object& voter, const proposal_vote_index::index<by_voter_proposal>::type& proposal_votes,
      database& db, remove_guard& obj_perf );

    // removes votes cast for given proposal (as long as we are within limit), returns if the process was successful
    static bool remove_proposal_votes( const proposal_object& proposal, const proposal_vote_index::index<by_proposal_voter>::type& proposal_votes,
      database& db, remove_guard& obj_perf );

    // removes given proposal with all related votes (as long as we are within limit), returns if the process was successful
    static bool remove_proposal( const proposal_object& proposal, const proposal_vote_index::index<by_proposal_voter>::type& proposal_votes,
      database& db, remove_guard& obj_perf )
    {
      remove_proposal_votes( proposal, proposal_votes, db, obj_perf );
      return obj_perf.remove( db, proposal );
    }

    // marks given proposal `removed` for the per-block sweep, moving its `end_date` to given time; throws unless
    // that time is not after head block time
    static void mark_proposal_removed( const proposal_object& proposal, const time_point_sec& end_date, database& db );
};

} } // hive::chain
