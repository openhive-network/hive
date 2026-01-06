#include <hive/plugins/database_api/database_api_impl.hpp>

#include <hive/chain/comment_object.hpp>
#include <hive/chain/dhf_objects.hpp>
#include <hive/chain/dhf_objects_multiindex.hpp>

namespace hive { namespace plugins { namespace database_api {

//////////////////////////////////////////////////////////////////////
//                                                                  //
// Comments                                                         //
//                                                                  //
//////////////////////////////////////////////////////////////////////

DEFINE_API_IMPL(database_api_impl, get_comment_pending_payouts)
{
  FC_ASSERT( 0 < args.comments.size() && args.comments.size() <= DATABASE_API_SINGLE_QUERY_LIMIT,
    "list of comments to find not filled or too big" );

  get_comment_pending_payouts_return retval;
  retval.cashout_infos.reserve(args.comments.size());

  for(const auto& key : args.comments)
  {
    auto comment = _db.find_comment(key.first, key.second);
    if(comment)
    {
      retval.cashout_infos.emplace_back();
      comment_pending_payout_info& info = retval.cashout_infos.back();
      info.author = key.first;
      info.permlink = key.second;

      const comment_cashout_object* cc = _db.find_comment_cashout(*comment);
      if(cc != nullptr)
        info.cashout_info = api_commment_cashout_info(*cc, _db);
    }
  }

  return retval;
}

/* Comments */
DEFINE_API_IMPL( database_api_impl, find_comments )
{
  FC_ASSERT( false, "Supported by Hivemind" );
}

/* Votes */

DEFINE_API_IMPL( database_api_impl, list_votes )
{
  FC_ASSERT( false, "Supported by Hivemind" );
}

DEFINE_API_IMPL( database_api_impl, find_votes )
{
    FC_ASSERT( false, "Supported by Hivemind" );
}


//////////////////////////////////////////////////////////////////////
//                                                                  //
// DHF                                                              //
//                                                                  //
//////////////////////////////////////////////////////////////////////

// Implementation of get_proposal_status declared in database_api_impl.hpp
proposal_status get_proposal_status( const chain::proposal_object& po, const fc::time_point_sec current_time )
{
  if( current_time >= po.start_date && current_time <= po.end_date )
  {
    return proposal_status::active;
  }

  if( current_time < po.start_date )
  {
    return proposal_status::inactive;
  }

  if( current_time > po.end_date )
  {
    return proposal_status::expired;
  }

  FC_THROW("Unexpected status of the proposal");
}

namespace {

bool filter_proposal_status( const proposal_object& po, proposal_status filter, time_point_sec current_time )
{
  if( po.removed )
    return false;

  if( filter == all )
    return true;

  auto prop_status = get_proposal_status( po, current_time );
  return filter == prop_status ||
    ( filter == votable && ( prop_status == active || prop_status == inactive ) );
}

} // anonymous namespace

/* Proposals */

DEFINE_API_IMPL( database_api_impl, list_proposals )
{
  using variants = std::vector< fc::variant >;
  FC_ASSERT( 0 < args.limit && args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT, "limit not set or too big" );

  list_proposals_return result;
  result.proposals.reserve( args.limit );

  const auto current_time = _db.head_block_time();

  constexpr auto LOWEST_PROPOSAL_ID = 0;
  constexpr auto GREATEST_PROPOSAL_ID = std::numeric_limits<api_id_type>::max();
  switch( args.order )
  {
    case by_creator:
    {
      auto start_parameters = args.start.as< variants >();

      if ( start_parameters.empty() )
      {
        iterate_results_no_start< hive::chain::proposal_index, hive::chain::by_creator >(
          result.proposals,
          args.limit,
          [&]( const proposal_object& po, const database& db ){ return api_proposal_object( po, current_time ); },
          [&]( const proposal_object& po ){ return filter_proposal_status( po, args.status, current_time ); },
          args.order_direction,
          args.last_id
        );
        break;
      }

      // Workaround: at the moment there is an assumption, that no more than one start parameter is passed, more are ignored
      auto start_creator = start_parameters.front().as< account_name_type >();
      iterate_results< hive::chain::proposal_index, hive::chain::by_creator >(
        boost::make_tuple( start_creator, args.order_direction == ascending ? LOWEST_PROPOSAL_ID : GREATEST_PROPOSAL_ID ),
        result.proposals,
        args.limit,
        [&]( const proposal_object& po, const database& db ){ return api_proposal_object( po, current_time ); },
        [&]( const proposal_object& po ){ return filter_proposal_status( po, args.status, current_time ); },
        args.order_direction,
        args.last_id
      );
      break;
    }
    case by_start_date:
    {
      auto start_parameters = args.start.as< variants >();

      if ( start_parameters.empty() )
      {
        iterate_results_no_start< hive::chain::proposal_index, hive::chain::by_start_date >(
          result.proposals,
          args.limit,
          [&]( const proposal_object& po, const database& db ){ return api_proposal_object( po, current_time ); },
          [&]( const proposal_object& po ){ return filter_proposal_status( po, args.status, current_time ); },
          args.order_direction,
          args.last_id
        );
        break;
      }

      auto start_date_string = start_parameters.front().as< std::string >();
      // check if empty string was passed as the time
      auto time =  start_date_string.empty() || start_parameters.empty()
        ? time_point_sec( args.order_direction == ascending ? fc::time_point::min() : fc::time_point::maximum() )
        : start_parameters.front().as< time_point_sec >();

      iterate_results< hive::chain::proposal_index, hive::chain::by_start_date >(
        boost::make_tuple( time, args.order_direction == ascending ? LOWEST_PROPOSAL_ID : GREATEST_PROPOSAL_ID ),
        result.proposals,
        args.limit,
        [&]( const proposal_object& po, const database& db ){ return api_proposal_object( po, current_time ); },
        [&]( const proposal_object& po ){ return filter_proposal_status( po, args.status, current_time ); },
        args.order_direction,
        args.last_id
      );
      break;
    }
    case by_end_date:
    {
      auto start_parameters = args.start.as< variants >();

      if ( start_parameters.empty() )
      {
        iterate_results_no_start< hive::chain::proposal_index, hive::chain::by_end_date >(
          result.proposals,
          args.limit,
          [&]( const proposal_object& po, const database& db ){ return api_proposal_object( po, current_time ); },
          [&]( const proposal_object& po ){ return filter_proposal_status( po, args.status, current_time ); },
          args.order_direction,
          args.last_id
        );
        break;
      }

      // Workaround: at the moment there is assumption, that no more than one start parameter is passed, more are ignored
      auto end_date_string = start_parameters.empty()
        ? std::string()
        : start_parameters.front().as< std::string >();

      // check if empty string was passed as the time
      auto time = end_date_string.empty() || start_parameters.empty()
        ? time_point_sec( args.order_direction == ascending ? fc::time_point::min() : fc::time_point::maximum() )
        : start_parameters.front().as< time_point_sec >();

      iterate_results< hive::chain::proposal_index, hive::chain::by_end_date >(
        boost::make_tuple( time, args.order_direction == ascending ? LOWEST_PROPOSAL_ID : GREATEST_PROPOSAL_ID ),
        result.proposals,
        args.limit,
        [&]( const proposal_object& po, const database& db ){ return api_proposal_object( po, current_time ); },
        [&]( const proposal_object& po ){ return filter_proposal_status( po, args.status, current_time ); },
        args.order_direction,
        args.last_id
      );
      break;
    }
    case by_total_votes:
    {
      auto start_parameters = args.start.as< variants >();

      if( start_parameters.empty() )
      {
        iterate_results_no_start< hive::chain::proposal_index, hive::chain::by_total_votes >(
          result.proposals,
          args.limit,
          [&]( const proposal_object& po, const database& db ){ return api_proposal_object( po, current_time ); },
          [&]( const proposal_object& po ){ return filter_proposal_status( po, args.status, current_time ); },
          args.order_direction,
          args.last_id
        );
        break;
      }

      // Workaround: at the moment there is assumption, that no more than one start parameter is passed, more are ignored
      auto votes = start_parameters.front().as< uint64_t >();

      iterate_results< hive::chain::proposal_index, hive::chain::by_total_votes >(
        boost::make_tuple( votes, args.order_direction == ascending ? LOWEST_PROPOSAL_ID : GREATEST_PROPOSAL_ID ),
        result.proposals,
        args.limit,
        [&]( const proposal_object& po, const database& db ){ return api_proposal_object( po, current_time ); },
        [&]( const proposal_object& po ){ return filter_proposal_status( po, args.status, current_time ); },
        args.order_direction,
        args.last_id
      );
      break;
    }
    default:
      FC_ASSERT( false, "Unknown or unsupported sort order '${o}'", ( "o", args.order ) );
  }

  return result;
}

DEFINE_API_IMPL( database_api_impl, find_proposals )
{
  FC_ASSERT( 0 < args.proposal_ids.size() && args.proposal_ids.size() <= DATABASE_API_SINGLE_QUERY_LIMIT,
    "list of proposals to find not filled or too big" );
  std::for_each( args.proposal_ids.begin(), args.proposal_ids.end(), [&](auto& id)
  {
    FC_ASSERT( id >= 0, "The proposal id can't be negative" );
  });

  find_proposals_return result;
  result.proposals.reserve( args.proposal_ids.size() );

  auto currentTime = _db.head_block_time();

  std::for_each( args.proposal_ids.begin(), args.proposal_ids.end(), [&](auto& id)
  {
    auto po = _db.find< hive::chain::proposal_object, hive::chain::by_proposal_id >( static_cast<api_id_type>( id ) );
    if( po != nullptr && !po->removed )
    {
      result.proposals.emplace_back( api_proposal_object( *po, currentTime ) );
    }
  });

  return result;
}


/* Proposal Votes */

DEFINE_API_IMPL( database_api_impl, list_proposal_votes )
{
  FC_ASSERT( 0 < args.limit && args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT, "limit not set or too big" );

  auto get_proposal_id = [&](const fc::optional<int64_t>& obj) -> api_id_type
  {
    if(obj.valid())
    {
      FC_ASSERT( *obj >= 0, "The proposal id can't be negative" );
      return *obj;
    }
    else
      return ( args.order_direction == ascending ? 0 : std::numeric_limits<api_id_type>::max());
  };

  auto get_account_name = [&](const fc::optional<account_name_type>& obj) -> account_name_type
  {
    if(obj.valid()) return *obj;
    else return ( args.order_direction == ascending ? "" : _db.get_index<hive::chain::proposal_vote_index, hive::chain::by_voter_proposal>().rbegin()->voter  );
  };

  list_proposal_votes_return result;
  result.proposal_votes.reserve( args.limit );

  const auto current_time = _db.head_block_time();

  switch( args.order )
  {
    case by_voter_proposal:
    {
      auto key = args.start.as< std::pair< fc::optional<account_name_type>, fc::optional<api_id_type> > >();
      iterate_results< hive::chain::proposal_vote_index, hive::chain::by_voter_proposal >(
        boost::make_tuple( get_account_name( key.first ), get_proposal_id( key.second ) ),
        result.proposal_votes,
        args.limit,
        &database_api_impl::on_push_default< api_proposal_vote_object, proposal_vote_object >,
        [&]( const proposal_vote_object& po )
        {
          auto* proposal = _db.find< hive::chain::proposal_object, hive::chain::by_proposal_id >( po.proposal_id );
          return proposal != nullptr && filter_proposal_status( *proposal, args.status, current_time );
        },
        args.order_direction
      );
      break;
    }
    case by_proposal_voter:
    {
      auto key = args.start.as< std::pair< fc::optional<api_id_type>, fc::optional<account_name_type> > >();
      iterate_results< hive::chain::proposal_vote_index, hive::chain::by_proposal_voter >(
        boost::make_tuple( get_proposal_id( key.first ), get_account_name( key.second) ),
        result.proposal_votes,
        args.limit,
        &database_api_impl::on_push_default< api_proposal_vote_object, proposal_vote_object >,
        [&]( const proposal_vote_object& po )
        {
          auto* proposal = _db.find< hive::chain::proposal_object, hive::chain::by_proposal_id >( po.proposal_id );
          return proposal != nullptr && filter_proposal_status( *proposal, args.status, current_time );
        },
        args.order_direction
      );
      break;
    }
    default:
      FC_ASSERT( false, "Unknown or unsupported sort order '${o}'", ( "o", args.order ) );
  }

  return result;
}

} } } // hive::plugins::database_api
