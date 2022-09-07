#include <hive/chain/hive_fwd.hpp>

#include <hive/chain/util/dhf_helper.hpp>

namespace hive { namespace chain {

void dhf_helper::remove_proposals( database& db, const flat_set<int64_t>& proposal_ids, const account_name_type& proposal_owner )
{
  if( proposal_ids.empty() )
    return;

  auto& byPropIdIdx = db.get_index< proposal_index, by_proposal_id >();
  auto& byVoterIdx = db.get_index< proposal_vote_index, by_proposal_voter >();

  remove_guard obj_perf( db.get_remove_threshold() );

  for(auto pid : proposal_ids)
  {
    auto foundPosI = byPropIdIdx.find( pid );

    if(foundPosI == byPropIdIdx.end())
      continue;

    FC_ASSERT(foundPosI->creator == proposal_owner, "Only proposal owner can remove it...");

    remove_proposal( *foundPosI, byVoterIdx, db, obj_perf );
    if( obj_perf.done() )
      break;
  }

}

} } // hive::chain
