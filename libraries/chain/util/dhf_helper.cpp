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

  auto _iter_pid = proposal_ids.begin();
  while( _iter_pid != proposal_ids.end() )
  {
    auto _pid = *_iter_pid;

    ++_iter_pid;

    auto foundPosI = byPropIdIdx.find( _pid );

    if(foundPosI == byPropIdIdx.end())
    {
      if( db.is_in_control() || db.has_hardfork( HIVE_HARDFORK_1_28_DONT_TRY_REMOVE_NONEXISTENT_PROPOSAL ) )
        FC_ASSERT( false, "Can't remove nonexistent proposal with id: ${pid}", ("pid", _pid) );
      continue;
    }

    FC_ASSERT(foundPosI->creator == proposal_owner, "Only the proposal owner can remove it.");

    remove_proposal( *foundPosI, byVoterIdx, db, obj_perf );
    if( obj_perf.done() )
      break;
  }

  if( db.is_in_control() || db.has_hardfork( HIVE_HARDFORK_1_28_DONT_TRY_REMOVE_NONEXISTENT_PROPOSAL ) )
  {
    while( _iter_pid != proposal_ids.end() )
    {
      auto foundPosI = byPropIdIdx.find( *_iter_pid );

      if(foundPosI == byPropIdIdx.end())
      {
        FC_ASSERT( false, "Can't remove nonexistent proposal with id: ${pid}", ("pid", *_iter_pid) );
      }

      ++_iter_pid;
    }
  }
}

} } // hive::chain
