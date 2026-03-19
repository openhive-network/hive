#include <hive/plugins/database_api/database_api_impl.hpp>

#include <hive/chain/detail/state/escrow_object_multiindex.hpp>
#include <hive/chain/detail/state/withdraw_vesting_route_object_multiindex.hpp>
#include <hive/chain/detail/state/savings_withdraw_object_multiindex.hpp>
#include <hive/chain/detail/state/convert_request_object_multiindex.hpp>
#include <hive/chain/detail/state/collateralized_convert_request_object_multiindex.hpp>
#include <hive/chain/detail/state/decline_voting_rights_request_object_multiindex.hpp>
#include <hive/chain/detail/state/limit_order_object_multiindex.hpp>
#include <hive/chain/detail/state/recurrent_transfer_object_multiindex.hpp>

namespace hive { namespace plugins { namespace database_api {

//////////////////////////////////////////////////////////////////////
//                                                                  //
// Financial - Escrows, Vesting, Savings, Conversions, Orders       //
//                                                                  //
//////////////////////////////////////////////////////////////////////

/* Escrows */

DEFINE_API_IMPL( database_api_impl, list_escrows )
{
  FC_ASSERT( 0 < args.limit && args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT, "limit not set or too big" );

  list_escrows_return result;
  result.escrows.reserve( args.limit );

  switch( args.order )
  {
    case( by_from_id ):
    {
      std::optional< boost::tuple< account_name_type, uint32_t > > start;
      if( !args.start.is_null() )
      {
        auto key = args.start.as< std::pair< account_name_type, uint32_t > >();
        start = boost::make_tuple( key.first, key.second );
      }
      iterate_results< chain::escrow_index, chain::by_from_id >(
        start,
        result.escrows,
        args.limit,
        &database_api_impl::on_push_default< api_escrow_object, escrow_object >,
        &database_api_impl::filter_default< escrow_object > );
      break;
    }
    case( by_ratification_deadline ):
    {
      std::optional< boost::tuple< bool, fc::time_point_sec, escrow_id_type > > start;
      if( !args.start.is_null() )
      {
        auto key = args.start.as< std::vector< fc::variant > >();
        FC_ASSERT( key.size() == 3, "by_ratification_deadline start requires 3 values. (bool, time_point_sec, escrow_id_type)" );
        start = boost::make_tuple( key[0].as< bool >(), key[1].as< fc::time_point_sec >(), key[2].as< escrow_id_type >() );
      }
      iterate_results< chain::escrow_index, chain::by_ratification_deadline >(
        start,
        result.escrows,
        args.limit,
        &database_api_impl::on_push_default< api_escrow_object, escrow_object >,
        &database_api_impl::filter_default< escrow_object > );
      break;
    }
    default:
      FC_ASSERT( false, "Unknown or unsupported sort order '${o}'", ( "o", args.order ) );
  }

  return result;
}

DEFINE_API_IMPL( database_api_impl, find_escrows )
{
  find_escrows_return result;

  const auto& escrow_idx = _db.get_index< chain::escrow_index, chain::by_from_id >();
  auto itr = escrow_idx.lower_bound( args.from );

  while( itr != escrow_idx.end() && itr->get_from() == args.from && result.escrows.size() <= DATABASE_API_SINGLE_QUERY_LIMIT )
  {
    result.escrows.emplace_back( *itr, _db );
    ++itr;
  }

  return result;
}


/* Withdraw Vesting Routes */

DEFINE_API_IMPL( database_api_impl, list_withdraw_vesting_routes )
{
  FC_ASSERT( 0 < args.limit && args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT, "limit not set or too big" );

  list_withdraw_vesting_routes_return result;
  result.routes.reserve( args.limit );

  switch( args.order )
  {
    case( by_withdraw_route ):
    {
      std::optional< boost::tuple< account_name_type, account_name_type > > start;
      if( !args.start.is_null() )
      {
        auto key = args.start.as< std::pair< account_name_type, account_name_type > >();
        start = boost::make_tuple( key.first, key.second );
      }
      iterate_results< chain::withdraw_vesting_route_index, chain::by_withdraw_route >(
        start,
        result.routes,
        args.limit,
        &database_api_impl::on_push_default< api_withdraw_vesting_route_object, withdraw_vesting_route_object >,
        &database_api_impl::filter_default< withdraw_vesting_route_object > );
      break;
    }
    case( by_destination ):
    {
      std::optional< boost::tuple< account_name_type, withdraw_vesting_route_id_type > > start;
      if( !args.start.is_null() )
      {
        auto key = args.start.as< std::pair< account_name_type, withdraw_vesting_route_id_type > >();
        start = boost::make_tuple( key.first, key.second );
      }
      iterate_results< chain::withdraw_vesting_route_index, chain::by_destination >(
        start,
        result.routes,
        args.limit,
        &database_api_impl::on_push_default< api_withdraw_vesting_route_object, withdraw_vesting_route_object >,
        &database_api_impl::filter_default< withdraw_vesting_route_object > );
      break;
    }
    default:
      FC_ASSERT( false, "Unknown or unsupported sort order '${o}'", ( "o", args.order ) );
  }

  return result;
}

DEFINE_API_IMPL( database_api_impl, find_withdraw_vesting_routes )
{
  find_withdraw_vesting_routes_return result;

  switch( args.order )
  {
    case( by_withdraw_route ):
    {
      const auto& route_idx = _db.get_index< chain::withdraw_vesting_route_index, chain::by_withdraw_route >();
      auto itr = route_idx.lower_bound( args.account );

      while( itr != route_idx.end() && itr->from_account == args.account && result.routes.size() <= DATABASE_API_SINGLE_QUERY_LIMIT )
      {
        result.routes.emplace_back( *itr, _db );
        ++itr;
      }

      break;
    }
    case( by_destination ):
    {
      const auto& route_idx = _db.get_index< chain::withdraw_vesting_route_index, chain::by_destination >();
      auto itr = route_idx.lower_bound( args.account );

      while( itr != route_idx.end() && itr->to_account == args.account && result.routes.size() <= DATABASE_API_SINGLE_QUERY_LIMIT )
      {
        result.routes.emplace_back( *itr, _db );
        ++itr;
      }

      break;
    }
    default:
      FC_ASSERT( false, "Unknown or unsupported sort order '${o}'", ( "o", args.order ) );
  }

  return result;
}


/* Savings Withdrawals */

DEFINE_API_IMPL( database_api_impl, list_savings_withdrawals )
{
  FC_ASSERT( 0 < args.limit && args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT, "limit not set or too big" );

  list_savings_withdrawals_return result;
  result.withdrawals.reserve( args.limit );

  switch( args.order )
  {
    case( by_from_id ):
    {
      std::optional< boost::tuple< account_name_type, uint32_t > > start;
      if( !args.start.is_null() )
      {
        auto key = args.start.as< std::pair< account_name_type, uint32_t > >();
        start = boost::make_tuple( key.first, key.second );
      }
      iterate_results< chain::savings_withdraw_index, chain::by_from_rid >(
        start,
        result.withdrawals,
        args.limit,
        &database_api_impl::on_push_default< api_savings_withdraw_object, savings_withdraw_object >,
        &database_api_impl::filter_default< savings_withdraw_object > );
      break;
    }
    case( by_complete_from_id ):
    {
      std::optional< boost::tuple< fc::time_point_sec, account_name_type, uint32_t > > start;
      if( !args.start.is_null() )
      {
        auto key = args.start.as< std::vector< fc::variant > >();
        FC_ASSERT( key.size() == 3, "by_complete_from_id start requires 3 values. (time_point_sec, account_name_type, uint32_t)" );
        start = boost::make_tuple( key[0].as< fc::time_point_sec >(), key[1].as< account_name_type >(), key[2].as< uint32_t >() );
      }
      iterate_results< chain::savings_withdraw_index, chain::by_complete_from_rid >(
        start,
        result.withdrawals,
        args.limit,
        &database_api_impl::on_push_default< api_savings_withdraw_object, savings_withdraw_object >,
        &database_api_impl::filter_default< savings_withdraw_object > );
      break;
    }
    case( by_to_complete ):
    {
      std::optional< boost::tuple< account_name_type, fc::time_point_sec, savings_withdraw_id_type > > start;
      if( !args.start.is_null() )
      {
        auto key = args.start.as< std::vector< fc::variant > >();
        FC_ASSERT( key.size() == 3, "by_to_complete start requires 3 values. (account_name_type, time_point_sec, savings_withdraw_id_type" );
        start = boost::make_tuple( key[0].as< account_name_type >(), key[1].as< fc::time_point_sec >(), key[2].as< savings_withdraw_id_type >() );
      }
      iterate_results< chain::savings_withdraw_index, chain::by_to_complete >(
        start,
        result.withdrawals,
        args.limit,
        &database_api_impl::on_push_default< api_savings_withdraw_object, savings_withdraw_object >,
        &database_api_impl::filter_default< savings_withdraw_object > );
      break;
    }
    default:
      FC_ASSERT( false, "Unknown or unsupported sort order '${o}'", ( "o", args.order ) );
  }

  return result;
}

DEFINE_API_IMPL( database_api_impl, find_savings_withdrawals )
{
  find_savings_withdrawals_return result;
  const auto& withdraw_idx = _db.get_index< chain::savings_withdraw_index, chain::by_from_rid >();
  auto itr = withdraw_idx.lower_bound( args.account );

  while( itr != withdraw_idx.end() && itr->get_from() == args.account && result.withdrawals.size() <= DATABASE_API_SINGLE_QUERY_LIMIT )
  {
    result.withdrawals.emplace_back( *itr, _db );
    ++itr;
  }

  return result;
}


/* Vesting Delegations */

DEFINE_API_IMPL( database_api_impl, list_vesting_delegations )
{
  FC_ASSERT( 0 < args.limit && args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT, "limit not set or too big" );

  list_vesting_delegations_return result;
  result.delegations.reserve( args.limit );

  switch( args.order )
  {
    case( by_delegation ):
    {
      std::optional< boost::tuple< account_id_type, account_id_type > > start;
      if( !args.start.is_null() )
      {
        auto key = args.start.as< std::pair< account_name_type, account_name_type > >();
        const auto* delegator = _db.find_account( key.first );
        FC_ASSERT( delegator != nullptr, "Given account does not exist." );
        account_id_type delegatee_id;
        if( key.second != "" )
        {
          const auto* delegatee = _db.find_account( key.second );
          FC_ASSERT( delegatee != nullptr, "Given account does not exist." );
          delegatee_id = delegatee->get_id();
        }
        start = boost::make_tuple( delegator->get_id(), delegatee_id );
      }
      iterate_results< chain::vesting_delegation_index, chain::by_delegation >(
        start,
        result.delegations,
        args.limit,
        &database_api_impl::on_push_default< api_vesting_delegation_object, vesting_delegation_object >,
        &database_api_impl::filter_default< vesting_delegation_object > );
      break;
    }
    default:
      FC_ASSERT( false, "Unknown or unsupported sort order '${o}'", ( "o", args.order ) );
  }

  return result;
}

DEFINE_API_IMPL( database_api_impl, find_vesting_delegations )
{
  find_vesting_delegations_return result;
  const auto& delegation_idx = _db.get_index< chain::vesting_delegation_index, chain::by_delegation >();
  const auto* delegator = _db.find_account( args.account );
  FC_ASSERT( delegator != nullptr, "Given account does not exist." );
  account_id_type delegator_id = delegator->get_id();
  auto itr = delegation_idx.lower_bound( delegator_id );

  while( itr != delegation_idx.end() && itr->get_delegator() == delegator_id && result.delegations.size() <= DATABASE_API_SINGLE_QUERY_LIMIT )
  {
    result.delegations.emplace_back( *itr, _db );
    ++itr;
  }

  return result;
}


/* Vesting Delegation Expirations */

DEFINE_API_IMPL( database_api_impl, list_vesting_delegation_expirations )
{
  FC_ASSERT( 0 < args.limit && args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT, "limit not set or too big" );

  list_vesting_delegation_expirations_return result;
  result.delegations.reserve( args.limit );

  switch( args.order )
  {
    case( by_expiration ):
    {
      std::optional< boost::tuple< time_point_sec, vesting_delegation_expiration_id_type > > start;
      if( !args.start.is_null() )
      {
        auto key = args.start.as< std::pair< time_point_sec, vesting_delegation_expiration_id_type > >();
        start = boost::make_tuple( key.first, key.second );
      }
      iterate_results< chain::vesting_delegation_expiration_index, chain::by_expiration >(
        start,
        result.delegations,
        args.limit,
        &database_api_impl::on_push_default< api_vesting_delegation_expiration_object, vesting_delegation_expiration_object >,
        &database_api_impl::filter_default< vesting_delegation_expiration_object > );
      break;
    }
    case( by_account_expiration ):
    {
      std::optional< boost::tuple< account_id_type, time_point_sec, vesting_delegation_expiration_id_type > > start;
      if( !args.start.is_null() )
      {
        auto key = args.start.as< std::vector< fc::variant > >();
        FC_ASSERT( key.size() == 3, "by_account_expiration start requires 3 values. (account_name_type, time_point_sec, vesting_delegation_expiration_id_type" );
        account_name_type delegator_name = key[0].as< account_name_type >();
        account_id_type delegator_id;
        if( delegator_name != "" )
        {
          const auto* delegator = _db.find_account( delegator_name );
          FC_ASSERT( delegator != nullptr, "Given account does not exist." );
          delegator_id = delegator->get_id();
        }
        start = boost::make_tuple( delegator_id, key[1].as< time_point_sec >(), key[2].as< vesting_delegation_expiration_id_type >() );
      }
      iterate_results< chain::vesting_delegation_expiration_index, chain::by_account_expiration >(
        start,
        result.delegations,
        args.limit,
        &database_api_impl::on_push_default< api_vesting_delegation_expiration_object, vesting_delegation_expiration_object >,
        &database_api_impl::filter_default< vesting_delegation_expiration_object > );
      break;
    }
    default:
      FC_ASSERT( false, "Unknown or unsupported sort order '${o}'", ( "o", args.order ) );
  }

  return result;
}

DEFINE_API_IMPL( database_api_impl, find_vesting_delegation_expirations )
{
  find_vesting_delegation_expirations_return result;
  const auto& del_exp_idx = _db.get_index< chain::vesting_delegation_expiration_index, chain::by_account_expiration >();
  const auto* delegator = _db.find_account( args.account );
  FC_ASSERT( delegator != nullptr, "Given account does not exist." );
  account_id_type delegator_id = delegator->get_id();
  auto itr = del_exp_idx.lower_bound( delegator_id );

  while( itr != del_exp_idx.end() && itr->get_delegator() == delegator_id && result.delegations.size() <= DATABASE_API_SINGLE_QUERY_LIMIT )
  {
    result.delegations.emplace_back( *itr, _db );
    ++itr;
  }

  return result;
}


/* HBD Conversion Requests */

DEFINE_API_IMPL( database_api_impl, list_hbd_conversion_requests )
{
  FC_ASSERT( 0 < args.limit && args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT, "limit not set or too big" );

  list_hbd_conversion_requests_return result;
  result.requests.reserve( args.limit );

  switch( args.order )
  {
    case( by_conversion_date ):
    {
      std::optional< boost::tuple< time_point_sec, convert_request_id_type > > start;
      if( !args.start.is_null() )
      {
        auto key = args.start.as< std::pair< time_point_sec, convert_request_id_type > >();
        start = boost::make_tuple( key.first, key.second );
      }
      iterate_results< chain::convert_request_index, chain::by_conversion_date >(
        start,
        result.requests,
        args.limit,
        &database_api_impl::on_push_default< api_convert_request_object, convert_request_object >,
        &database_api_impl::filter_default< convert_request_object > );
      break;
    }
    case( by_account ):
    {
      std::optional< boost::tuple< account_id_type, uint32_t > > start;
      if( !args.start.is_null() )
      {
        auto key = args.start.as< std::pair< account_name_type, uint32_t > >();
        account_id_type owner_id;
        if( key.first != "" )
        {
          const auto* owner = _db.find_account( key.first );
          FC_ASSERT( owner != nullptr, "Given account does not exist." );
          owner_id = owner->get_id();
        }
        start = boost::make_tuple( owner_id, key.second );
      }
      iterate_results< chain::convert_request_index, chain::by_owner >(
        start,
        result.requests,
        args.limit,
        &database_api_impl::on_push_default< api_convert_request_object, convert_request_object >,
        &database_api_impl::filter_default< convert_request_object > );
      break;
    }
    default:
      FC_ASSERT( false, "Unknown or unsupported sort order '${o}'", ( "o", args.order ) );
  }

  return result;
}

DEFINE_API_IMPL( database_api_impl, find_hbd_conversion_requests )
{
  find_hbd_conversion_requests_return result;

  const auto* owner = _db.find_account( args.account );
  FC_ASSERT( owner != nullptr, "Given account does not exist." );
  account_id_type owner_id = owner->get_id();

  const auto& convert_idx = _db.get_index< chain::convert_request_index, chain::by_owner >();
  auto itr = convert_idx.lower_bound( owner_id );

  while( itr != convert_idx.end() && itr->get_owner() == owner_id && result.requests.size() <= DATABASE_API_SINGLE_QUERY_LIMIT )
  {
    result.requests.emplace_back( *itr, _db );
    ++itr;
  }

  return result;
}

/* Collateralized Conversion Requests */

DEFINE_API_IMPL( database_api_impl, list_collateralized_conversion_requests )
{
  FC_ASSERT( 0 < args.limit && args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT, "limit not set or too big" );

  list_collateralized_conversion_requests_return result;
  result.requests.reserve( args.limit );

  switch( args.order )
  {
    case( by_conversion_date ):
    {
      std::optional< boost::tuple< time_point_sec, collateralized_convert_request_id_type > > start;
      if( !args.start.is_null() )
      {
        auto key = args.start.as< std::pair< time_point_sec, collateralized_convert_request_id_type > >();
        start = boost::make_tuple( key.first, key.second );
      }
      iterate_results< chain::collateralized_convert_request_index, chain::by_conversion_date >(
        start,
        result.requests,
        args.limit,
        &database_api_impl::on_push_default< api_collateralized_convert_request_object, collateralized_convert_request_object >,
        &database_api_impl::filter_default< collateralized_convert_request_object > );
      break;
    }
    case( by_account ):
    {
      std::optional< boost::tuple< account_id_type, uint32_t > > start;
      if( !args.start.is_null() )
      {
        auto key = args.start.as< std::pair< account_name_type, uint32_t > >();
        account_id_type owner_id;
        if( key.first != "" )
        {
          const auto* owner = _db.find_account( key.first );
          FC_ASSERT( owner != nullptr, "Given account does not exist." );
          owner_id = owner->get_id();
        }
        start = boost::make_tuple( owner_id, key.second );
      }
      iterate_results< chain::collateralized_convert_request_index, chain::by_owner >(
        start,
        result.requests,
        args.limit,
        &database_api_impl::on_push_default< api_collateralized_convert_request_object, collateralized_convert_request_object >,
        &database_api_impl::filter_default< collateralized_convert_request_object > );
      break;
    }
    default:
      FC_ASSERT( false, "Unknown or unsupported sort order '${o}'", ( "o", args.order ) );
  }

  return result;
}

DEFINE_API_IMPL( database_api_impl, find_collateralized_conversion_requests )
{
  find_collateralized_conversion_requests_return result;

  const auto* owner = _db.find_account( args.account );
  FC_ASSERT( owner != nullptr, "Given account does not exist." );
  account_id_type owner_id = owner->get_id();

  const auto& convert_idx = _db.get_index< chain::collateralized_convert_request_index, chain::by_owner >();
  auto itr = convert_idx.lower_bound( owner_id );

  while( itr != convert_idx.end() && itr->get_owner() == owner_id && result.requests.size() <= DATABASE_API_SINGLE_QUERY_LIMIT )
  {
    result.requests.emplace_back( *itr, _db );
    ++itr;
  }

  return result;
}

/* Decline Voting Rights Requests */

DEFINE_API_IMPL( database_api_impl, list_decline_voting_rights_requests )
{
  FC_ASSERT( 0 < args.limit && args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT, "limit not set or too big" );

  list_decline_voting_rights_requests_return result;
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
      iterate_results< chain::decline_voting_rights_request_index, chain::by_account >(
        start,
        result.requests,
        args.limit,
        &database_api_impl::on_push_default< api_decline_voting_rights_request_object, decline_voting_rights_request_object >,
        &database_api_impl::filter_default< decline_voting_rights_request_object > );
      break;
    }
    case( by_effective_date ):
    {
      std::optional< boost::tuple< time_point_sec, account_name_type > > start;
      if( !args.start.is_null() )
      {
        auto key = args.start.as< std::pair< time_point_sec, account_name_type > >();
        start = boost::make_tuple( key.first, key.second );
      }
      iterate_results< chain::decline_voting_rights_request_index, chain::by_effective_date >(
        start,
        result.requests,
        args.limit,
        &database_api_impl::on_push_default< api_decline_voting_rights_request_object, decline_voting_rights_request_object >,
        &database_api_impl::filter_default< decline_voting_rights_request_object > );
      break;
    }
    default:
      FC_ASSERT( false, "Unknown or unsupported sort order '${o}'", ( "o", args.order ) );
  }

  return result;
}

DEFINE_API_IMPL( database_api_impl, find_decline_voting_rights_requests )
{
  FC_ASSERT( 0 < args.accounts.size() && args.accounts.size() <= DATABASE_API_SINGLE_QUERY_LIMIT,
    "list of affected accounts to find not filled or too big");
  find_decline_voting_rights_requests_return result;

  for( auto& a : args.accounts )
  {
    auto request = _db.find< chain::decline_voting_rights_request_object, chain::by_account >( a );

    if( request != nullptr )
      result.requests.emplace_back( *request, _db );
  }

  return result;
}


/* Limit Orders */

DEFINE_API_IMPL( database_api_impl, list_limit_orders )
{
  FC_ASSERT( 0 < args.limit && args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT, "limit not set or too big" );

  list_limit_orders_return result;
  result.orders.reserve( args.limit );

  switch( args.order )
  {
    case( by_price ):
    {
      std::optional< boost::tuple< price, limit_order_id_type > > start;
      if( !args.start.is_null() )
      {
        auto key = args.start.as< std::pair< price, limit_order_id_type > >();
        start = boost::make_tuple( key.first, key.second );
      }
      iterate_results< chain::limit_order_index, chain::by_price >(
        start,
        result.orders,
        args.limit,
        &database_api_impl::on_push_default< api_limit_order_object, limit_order_object >,
        &database_api_impl::filter_default< limit_order_object > );
      break;
    }
    case( by_account ):
    {
      std::optional< boost::tuple< account_name_type, uint32_t > > start;
      if( !args.start.is_null() )
      {
        auto key = args.start.as< std::pair< account_name_type, uint32_t > >();
        start = boost::make_tuple( key.first, key.second );
      }
      iterate_results< chain::limit_order_index, chain::by_account >(
        start,
        result.orders,
        args.limit,
        &database_api_impl::on_push_default< api_limit_order_object, limit_order_object >,
        &database_api_impl::filter_default< limit_order_object > );
      break;
    }
    default:
      FC_ASSERT( false, "Unknown or unsupported sort order '${o}'", ( "o", args.order ) );
  }

  return result;
}

DEFINE_API_IMPL( database_api_impl, find_limit_orders )
{
  find_limit_orders_return result;
  const auto& order_idx = _db.get_index< chain::limit_order_index, chain::by_account >();
  auto itr = order_idx.lower_bound( args.account );

  while( itr != order_idx.end() && itr->get_seller() == args.account && result.orders.size() <= DATABASE_API_SINGLE_QUERY_LIMIT )
  {
    result.orders.emplace_back( *itr, _db );
    ++itr;
  }

  return result;
}


/* Order Book */

DEFINE_API_IMPL( database_api_impl, get_order_book )
{
  FC_ASSERT( 0 < args.limit && args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT, "limit not set or too big" );
  get_order_book_return result;

  auto max_sell = price::max( HBD_SYMBOL, HIVE_SYMBOL );
  auto max_buy = price::max( HIVE_SYMBOL, HBD_SYMBOL );

  const auto& limit_price_idx = _db.get_index< chain::limit_order_index >().indices().get< chain::by_price >();
  auto sell_itr = limit_price_idx.lower_bound( max_sell );
  auto buy_itr  = limit_price_idx.lower_bound( max_buy );
  auto end = limit_price_idx.end();

  while( sell_itr != end && sell_itr->get_sell_price().base.symbol == HBD_SYMBOL && result.bids.size() < args.limit )
  {
    auto itr = sell_itr;
    order cur;
    cur.order_price = itr->get_sell_price();
    cur.real_price  = 0.0;
    // cur.real_price  = (cur.order_price).to_real();
    cur.hbd = itr->amount_for_sale().amount;
    cur.hive = ( itr->amount_for_sale() * cur.order_price ).amount;
    cur.created = itr->get_created();
    result.bids.emplace_back( std::move( cur ) );
    ++sell_itr;
  }
  while( buy_itr != end && buy_itr->get_sell_price().base.symbol == HIVE_SYMBOL && result.asks.size() < args.limit )
  {
    auto itr = buy_itr;
    order cur;
    cur.order_price = itr->get_sell_price();
    cur.real_price = 0.0;
    // cur.real_price  = (~cur.order_price).to_real();
    cur.hive    = itr->amount_for_sale().amount;
    cur.hbd     = ( itr->amount_for_sale() * cur.order_price ).amount;
    cur.created = itr->get_created();
    result.asks.emplace_back( std::move( cur ) );
    ++buy_itr;
  }

  return result;
}

/* Find recurrent transfers */

DEFINE_API_IMPL( database_api_impl, find_recurrent_transfers ) {
  find_recurrent_transfers_return result;

  const auto* from_account = _db.find_account( args.from );
  FC_ASSERT( from_account != nullptr, "Given 'from' account does not exist." );
  auto from_account_id = from_account->get_id();

  const auto &idx = _db.get_index<chain::recurrent_transfer_index, chain::by_from_id>();
  auto itr = idx.find(from_account_id);

  while (itr != idx.end() && itr->from_id == from_account_id && result.recurrent_transfers.size() <= DATABASE_API_SINGLE_QUERY_LIMIT)
  {
    const auto& to_account = _db.get_account( itr->to_id );
    result.recurrent_transfers.emplace_back( *itr, from_account->get_name(), to_account.get_name() );
    ++itr;
  }

  return result;
}

} } } // hive::plugins::database_api
