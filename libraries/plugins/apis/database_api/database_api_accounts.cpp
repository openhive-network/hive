#include <hive/plugins/database_api/database_api_impl.hpp>

#include <hive/chain/account_object.hpp>
#include <hive/chain/detail/state/time_object.hpp>
#include <hive/chain/witness_objects.hpp>
#include <hive/chain/witness_objects_multiindex.hpp>

namespace hive { namespace plugins { namespace database_api {

//////////////////////////////////////////////////////////////////////
//                                                                  //
// Witnesses                                                        //
//                                                                  //
//////////////////////////////////////////////////////////////////////

DEFINE_API_IMPL( database_api_impl, list_witnesses )
{
  FC_ASSERT( 0 < args.limit && args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT, "limit not set or too big" );

  list_witnesses_return result;
  result.witnesses.reserve( args.limit );

  switch( args.order )
  {
    case( by_name ):
    {
      std::optional< protocol::account_name_type > start;
      if( !args.start.is_null() )
      {
        start = args.start.as< protocol::account_name_type >();
      }
      iterate_results< chain::witness_index, chain::by_name >(
        start,
        result.witnesses,
        args.limit,
        &database_api_impl::on_push_default< api_witness_object, witness_object >,
        &database_api_impl::filter_default< witness_object > );
      break;
    }
    case( by_vote_name ):
    {
      std::optional< boost::tuple< share_type, account_name_type > > start;
      if( !args.start.is_null() )
      {
        auto key = args.start.as< std::pair< share_type, account_name_type > >();
        start = boost::make_tuple( key.first, key.second );
      }
      iterate_results< chain::witness_index, chain::by_vote_name >(
        start,
        result.witnesses,
        args.limit,
        &database_api_impl::on_push_default< api_witness_object, witness_object >,
        &database_api_impl::filter_default< witness_object > );
      break;
    }
    case( by_schedule_time ):
    {
      std::optional< boost::tuple< fc::uint128, witness_id_type > > start;
      if( !args.start.is_null() )
      {
        auto key = args.start.as< std::pair< fc::uint128, account_name_type > >();
        auto wit_id = _db.get< chain::witness_object, chain::by_name >( key.second ).get_id();
        start = boost::make_tuple( key.first, wit_id );
      }
      iterate_results< chain::witness_index, chain::by_schedule_time >(
        start,
        result.witnesses,
        args.limit,
        &database_api_impl::on_push_default< api_witness_object, witness_object >,
        &database_api_impl::filter_default< witness_object > );
      break;
    }
    default:
      FC_ASSERT( false, "Unknown or unsupported sort order '${o}'", ( "o", args.order ) );
  }

  return result;
}

DEFINE_API_IMPL( database_api_impl, find_witnesses )
{
  FC_ASSERT( 0 < args.owners.size() && args.owners.size() <= DATABASE_API_SINGLE_QUERY_LIMIT,
    "list of witnesses to find not filled or too big" );

  find_witnesses_return result;

  for( auto& o : args.owners )
  {
    auto witness = _db.find< chain::witness_object, chain::by_name >( o );

    if( witness != nullptr )
      result.witnesses.emplace_back( *witness, _db );
  }

  return result;
}

DEFINE_API_IMPL( database_api_impl, list_witness_votes )
{
  FC_ASSERT( 0 < args.limit && args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT, "limit not set or too big" );

  list_witness_votes_return result;
  result.votes.reserve( args.limit );

  switch( args.order )
  {
    case( by_account_witness ):
    {
      std::optional< boost::tuple< account_name_type, account_name_type > > start;
      if( !args.start.is_null() )
      {
        auto key = args.start.as< std::pair< account_name_type, account_name_type > >();
        start = boost::make_tuple( key.first, key.second );
      }
      iterate_results< chain::witness_vote_index, chain::by_account_witness >(
        start,
        result.votes,
        args.limit,
        &database_api_impl::on_push_default< api_witness_vote_object, witness_vote_object >,
        &database_api_impl::filter_default< witness_vote_object > );
      break;
    }
    case( by_witness_account ):
    {
      std::optional< boost::tuple< account_name_type, account_name_type > > start;
      if( !args.start.is_null() )
      {
        auto key = args.start.as< std::pair< account_name_type, account_name_type > >();
        start = boost::make_tuple( key.first, key.second );
      }
      iterate_results< chain::witness_vote_index, chain::by_witness_account >(
        start,
        result.votes,
        args.limit,
        &database_api_impl::on_push_default< api_witness_vote_object, witness_vote_object >,
        &database_api_impl::filter_default< witness_vote_object > );
      break;
    }
    default:
      FC_ASSERT( false, "Unknown or unsupported sort order '${o}'", ( "o", args.order ) );
  }

  return result;
}

DEFINE_API_IMPL( database_api_impl, get_active_witnesses )
{
  FC_ASSERT( _db.has_hardfork( HIVE_HARDFORK_1_26 ) || !args.include_future, "Future witnesses only become available after HF26" );
  const auto& wso = _db.get_witness_schedule_object();
  get_active_witnesses_return result;
  result.witnesses.assign( wso.current_shuffled_witnesses.begin(),
    wso.current_shuffled_witnesses.begin() + wso.num_scheduled_witnesses );
  if( args.include_future )
  {
    const auto& future_wso = _db.get_future_witness_schedule_object();
    result.future_witnesses = vector< account_name_type >( future_wso.current_shuffled_witnesses.begin(),
      future_wso.current_shuffled_witnesses.begin() + future_wso.num_scheduled_witnesses );
  }
  return result;
}


//////////////////////////////////////////////////////////////////////
//                                                                  //
// Accounts                                                         //
//                                                                  //
//////////////////////////////////////////////////////////////////////

/* Accounts */

DEFINE_API_IMPL( database_api_impl, list_accounts )
{
  FC_ASSERT( 0 < args.limit && args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT, "limit not set or too big" );

  list_accounts_return result;
  result.accounts.reserve( args.limit );

  switch( args.order )
  {
    case( by_name ):
    {
      std::optional< protocol::account_name_type > start;
      if( !args.start.is_null() )
      {
        start = args.start.as< protocol::account_name_type >();
      }
      iterate_results< chain::account_index, chain::by_name >(
        start,
        result.accounts,
        args.limit,
        [&]( const account_object& a, const database& db ){ return api_account_object( a, db, get_metadata_plugin(), args.delayed_votes_active ); },
        &database_api_impl::filter_default< account_object > );
      break;
    }
    case( by_proxy ):
    {
      std::optional< boost::tuple< account_id_type, account_name_type > > start;
      if( !args.start.is_null() )
      {
        auto key = args.start.as< std::pair< account_name_type, account_name_type > >();
        account_id_type proxy_id;
        if( key.first != HIVE_PROXY_TO_SELF_ACCOUNT )
        {
          const auto* proxy = _db.find_account( key.first );
          FC_ASSERT( proxy != nullptr, "Given proxy account does not exist." );
          proxy_id = proxy->get_id();
        }
        start = boost::make_tuple( proxy_id, key.second );
      }
      iterate_results< chain::account_index, chain::by_proxy >(
        start,
        result.accounts,
        args.limit,
        [&]( const account_object& a, const database& db ){ return api_account_object( a, db, get_metadata_plugin(), args.delayed_votes_active ); },
        &database_api_impl::filter_default< account_object > );
      break;
    }
    case( by_next_vesting_withdrawal ):
    {
      std::optional< boost::tuple< fc::time_point_sec, account_name_type > > start;
      if( !args.start.is_null() )
      {
        auto key = args.start.as< std::pair< fc::time_point_sec, account_name_type > >();
        start = boost::make_tuple( key.first, key.second );
      }
      iterate_results< chain::account_index, chain::by_next_vesting_withdrawal >(
        start,
        result.accounts,
        args.limit,
        [&]( const account_object& a, const database& db ){ return api_account_object( a, db, get_metadata_plugin(), args.delayed_votes_active ); },
        &database_api_impl::filter_default< account_object > );
      break;
    }
    default:
      FC_ASSERT( false, "Unknown or unsupported sort order '${o}'", ( "o", args.order ) );
  }

  return result;
}

DEFINE_API_IMPL( database_api_impl, find_accounts )
{
  find_accounts_return result;
  FC_ASSERT( 0 < args.accounts.size() && args.accounts.size() <= DATABASE_API_SINGLE_QUERY_LIMIT,
    "list of accounts to find not filled or too big" );

  for( auto& a : args.accounts )
  {
    auto acct = _db.find< chain::account_object, chain::by_name >( a );
    if( acct != nullptr )
      result.accounts.emplace_back( *acct, _db, get_metadata_plugin(), args.delayed_votes_active );
  }

  return result;
}


/* Owner Auth Histories */

DEFINE_API_IMPL( database_api_impl, list_owner_histories )
{
  FC_ASSERT( 0 < args.limit && args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT, "limit not set or too big" );

  list_owner_histories_return result;
  result.owner_auths.reserve( args.limit );

  std::optional< boost::tuple< account_name_type, fc::time_point_sec > > start;
  if( !args.start.is_null() )
  {
    auto key = args.start.as< std::pair< account_name_type, fc::time_point_sec > >();
    start = boost::make_tuple( key.first, key.second );
  }
  iterate_results< chain::owner_authority_history_index, chain::by_account >(
    start,
    result.owner_auths,
    args.limit,
    &database_api_impl::on_push_default< api_owner_authority_history_object, owner_authority_history_object >,
    &database_api_impl::filter_default< owner_authority_history_object > );

  return result;
}

DEFINE_API_IMPL( database_api_impl, find_owner_histories )
{
  find_owner_histories_return result;

  const auto& hist_idx = _db.get_index< chain::owner_authority_history_index, chain::by_account >();
  auto itr = hist_idx.lower_bound( args.owner );

  while( itr != hist_idx.end() && itr->account == args.owner && result.owner_auths.size() <= DATABASE_API_SINGLE_QUERY_LIMIT )
  {
    result.owner_auths.emplace_back( *itr, _db );
    ++itr;
  }

  return result;
}


/* Account Recovery Requests */

DEFINE_API_IMPL( database_api_impl, list_account_recovery_requests )
{
  FC_ASSERT( 0 < args.limit && args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT, "limit not set or too big" );

  list_account_recovery_requests_return result;
  result.requests.reserve( args.limit );

  switch( args.order )
  {
    case( by_account ):
    {
      std::optional< account_name_type > start;
      if( !args.start.is_null() )
      {
        start = args.start.as< account_name_type >();
      }
      iterate_results< chain::account_recovery_request_index, chain::by_account >(
        start,
        result.requests,
        args.limit,
        &database_api_impl::on_push_default< api_account_recovery_request_object, account_recovery_request_object >,
        &database_api_impl::filter_default< account_recovery_request_object > );
      break;
    }
    case( by_expiration ):
    {
      std::optional< boost::tuple< fc::time_point_sec, account_name_type > > start;
      if( !args.start.is_null() )
      {
        auto key = args.start.as< std::pair< fc::time_point_sec, account_name_type > >();
        start = boost::make_tuple( key.first, key.second );
      }
      iterate_results< chain::account_recovery_request_index, chain::by_expiration >(
        start,
        result.requests,
        args.limit,
        &database_api_impl::on_push_default< api_account_recovery_request_object, account_recovery_request_object >,
        &database_api_impl::filter_default< account_recovery_request_object > );
      break;
    }
    default:
      FC_ASSERT( false, "Unknown or unsupported sort order '${o}'", ( "o", args.order ) );
  }

  return result;
}

DEFINE_API_IMPL( database_api_impl, find_account_recovery_requests )
{
  find_account_recovery_requests_return result;
  FC_ASSERT( 0 < args.accounts.size() && args.accounts.size() <= DATABASE_API_SINGLE_QUERY_LIMIT,
    "list of affected accounts to find not filled or too big" );

  for( auto& a : args.accounts )
  {
    auto request = _db.find< chain::account_recovery_request_object, chain::by_account >( a );

    if( request != nullptr )
      result.requests.emplace_back( *request, _db );
  }

  return result;
}


/* Change Recovery Account Requests */

DEFINE_API_IMPL( database_api_impl, list_change_recovery_account_requests )
{
  FC_ASSERT( 0 < args.limit && args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT, "limit not set or too big" );

  list_change_recovery_account_requests_return result;
  result.requests.reserve( args.limit );

  switch( args.order )
  {
    case( by_account ):
    {
      std::optional< account_name_type > start;
      if( !args.start.is_null() )
      {
        start = args.start.as< account_name_type >();
      }
      iterate_results< chain::change_recovery_account_request_index, chain::by_account >(
        start,
        result.requests,
        args.limit,
        &database_api_impl::on_push_default< api_change_recovery_account_request_object, change_recovery_account_request_object >,
        &database_api_impl::filter_default< change_recovery_account_request_object > );
      break;
    }
    case( by_effective_date ):
    {
      std::optional< boost::tuple< fc::time_point_sec, account_name_type > > start;
      if( !args.start.is_null() )
      {
        auto key = args.start.as< std::pair< fc::time_point_sec, account_name_type > >();
        start = boost::make_tuple( key.first, key.second );
      }
      iterate_results< chain::change_recovery_account_request_index, chain::by_effective_date >(
        start,
        result.requests,
        args.limit,
        &database_api_impl::on_push_default< api_change_recovery_account_request_object, change_recovery_account_request_object >,
        &database_api_impl::filter_default< change_recovery_account_request_object > );
      break;
    }
    default:
      FC_ASSERT( false, "Unknown or unsupported sort order '${o}'", ( "o", args.order ) );
  }

  return result;
}

DEFINE_API_IMPL( database_api_impl, find_change_recovery_account_requests )
{
  find_change_recovery_account_requests_return result;
  FC_ASSERT( 0 < args.accounts.size() && args.accounts.size() <= DATABASE_API_SINGLE_QUERY_LIMIT,
    "list of affected accounts to find not filled or too big");

  for( auto& a : args.accounts )
  {
    auto request = _db.find< chain::change_recovery_account_request_object, chain::by_account >( a );

    if( request != nullptr )
      result.requests.emplace_back( *request, _db );
  }

  return result;
}

} } } // hive::plugins::database_api
