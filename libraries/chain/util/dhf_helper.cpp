#include <hive/chain/hive_fwd.hpp>

#include <hive/chain/util/dhf_helper.hpp>

#include <hive/chain/database.hpp>
#include <hive/chain/detail/state/account_object.hpp>

namespace hive { namespace chain {

bool dhf_helper::remove_proposal_votes( const account_object& voter, const proposal_vote_index::index<by_voter_proposal>::type& proposal_votes,
  database& db, remove_guard& obj_perf )
{
  auto pVoteI = proposal_votes.lower_bound( boost::make_tuple( voter.get_name(), 0 ) );
  while( pVoteI != proposal_votes.end() && pVoteI->voter == voter.get_name() )
  {
    const auto& vote = *pVoteI;
    ++pVoteI;
    if( !obj_perf.remove( db, vote ) )
      return false;
  }
  return true;
}

bool dhf_helper::remove_proposal_votes( const proposal_object& proposal, const proposal_vote_index::index<by_proposal_voter>::type& proposal_votes,
  database& db, remove_guard& obj_perf )
{
  auto pVoteI = proposal_votes.lower_bound( boost::make_tuple( proposal.proposal_id, account_name_type() ) );
  while( pVoteI != proposal_votes.end() && pVoteI->proposal_id == proposal.proposal_id )
  {
    const auto& vote = *pVoteI;
    ++pVoteI;
    if( !obj_perf.remove( db, vote ) )
      return false;
  }
  return true;
}

void dhf_helper::mark_proposal_removed( const proposal_object& proposal, const time_point_sec& end_date, database& db )
{
  // the sweep in dhf_processor::remove_proposals only walks proposals that already ended, and the hint that lets it
  // skip the walk (database::may_have_removed_proposals) relies on finding every proposal marked `removed` there
  FC_ASSERT( end_date <= db.head_block_time(), "Proposal ${pid} marked removed must end by head block time ${h}, not at ${e}",
    ("pid", proposal.proposal_id)("h", db.head_block_time())("e", end_date) );
  db.modify( proposal, [&]( proposal_object& p )
  {
    p.removed = true;
    p.end_date = end_date;
  } );
  db.set_may_have_removed_proposals();
}

} } // hive::chain
