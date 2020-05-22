#include <hive/chain/hive_fwd.hpp>

#include <appbase/application.hpp>

#include <hive/plugins/database_api/database_api.hpp>
#include <hive/plugins/database_api/database_api_plugin.hpp>

#include <hive/protocol/get_config.hpp>
#include <hive/protocol/exceptions.hpp>
#include <hive/protocol/transaction_util.hpp>

#include <hive/chain/util/smt_token.hpp>

#include <hive/utilities/git_revision.hpp>

#include <fc/git_revision.hpp>

namespace hive { namespace plugins { namespace database_api {



class database_api_impl
{
   public:
      database_api_impl();
      ~database_api_impl();

      DECLARE_API_IMPL
      (
         (get_config)
         (get_version)
         (get_dynamic_global_properties)
         (get_witness_schedule)
         (get_hardfork_properties)
         (get_reward_funds)
         (get_current_price_feed)
         (get_feed_history)
         (list_witnesses)
         (find_witnesses)
         (list_witness_votes)
         (get_active_witnesses)
         (list_accounts)
         (find_accounts)
         (list_owner_histories)
         (find_owner_histories)
         (list_account_recovery_requests)
         (find_account_recovery_requests)
         (list_change_recovery_account_requests)
         (find_change_recovery_account_requests)
         (list_escrows)
         (find_escrows)
         (list_withdraw_vesting_routes)
         (find_withdraw_vesting_routes)
         (list_savings_withdrawals)
         (find_savings_withdrawals)
         (list_vesting_delegations)
         (find_vesting_delegations)
         (list_vesting_delegation_expirations)
         (find_vesting_delegation_expirations)
         (list_hbd_conversion_requests)
         (find_hbd_conversion_requests)
         (list_decline_voting_rights_requests)
         (find_decline_voting_rights_requests)
         (find_comments)
         (list_votes)
         (find_votes)
         (list_limit_orders)
         (find_limit_orders)
         (get_order_book)
         (list_proposals)
         (find_proposals)
         (list_proposal_votes)
         (get_transaction_hex)
         (get_required_signatures)
         (get_potential_signatures)
         (verify_authority)
         (verify_account_authority)
         (verify_signatures)
#ifdef HIVE_ENABLE_SMT
         (get_nai_pool)
         (list_smt_contributions)
         (find_smt_contributions)
         (list_smt_tokens)
         (find_smt_tokens)
         (list_smt_token_emissions)
         (find_smt_token_emissions)
#endif
      )

      template< typename ResultType >
      static ResultType on_push_default( const ResultType& r ) { return r.copy_chain_object(); } //FIXME: exposes internal chain object as API result

      template< typename ValueType >
      static bool filter_default( const ValueType& r ) { return true; }

      template<typename IndexType, typename OrderType, typename StartType, typename ResultType, typename OnPushType, typename FilterType>
      void iterate_results_from_index(
         uint64_t index,
         std::vector<ResultType>& result,
         uint32_t limit,
         OnPushType&& on_push,
         FilterType&& filter,
         order_direction_type direction = ascending )
      {
         const auto& idx = _db.get_index< IndexType, OrderType >();
         typename IndexType::value_type::id_type id( index );
         if( direction == ascending )
         {
            auto itr = idx.iterator_to(*(_db.get_index<IndexType, hive::chain::by_id>().find( id )));
            auto end = idx.end();

            while( result.size() < limit && itr != end )
            {
               if( filter( *itr ) )
                  result.push_back( on_push( *itr ) );
               ++itr;
            }
         }
         else if( direction == descending )
         {
            auto index_it = idx.iterator_to(*(_db.get_index<IndexType, hive::chain::by_id>().upper_bound( id )));
            auto iter  = boost::make_reverse_iterator( index_it );
            auto iter_end = boost::make_reverse_iterator( idx.begin() );

            while( result.size() < limit && iter != iter_end )
            {
               if( filter( *iter ) )
                  result.push_back( on_push( *iter ) );
               ++iter;
            }
         }
      }

      template<typename IndexType, typename OrderType, typename StartType, typename ResultType, typename OnPushType, typename FilterType>
      void iterate_results(
         StartType start,
         std::vector<ResultType>& result,
         uint32_t limit,
         OnPushType&& on_push,
         FilterType&& filter,
         order_direction_type direction = ascending,
         fc::optional<uint64_t> last_index = fc::optional<uint64_t>()
      )
      {
         if ( last_index.valid() ) {
            iterate_results_from_index<IndexType, OrderType, StartType>( *last_index, result, limit, std::move(on_push), std::move(filter), direction );
            return;
         }

         const auto& idx = _db.get_index< IndexType, OrderType >();
         if( direction == ascending )
         {
            auto itr = idx.lower_bound( start );
            auto end = idx.end();

            while( result.size() < limit && itr != end )
            {
               if( filter( *itr ) )
                  result.push_back( on_push( *itr ) );

               ++itr;
            }
         }
         else if( direction == descending )
         {
            auto iter = boost::make_reverse_iterator( idx.upper_bound(start) );
            auto end_iter = boost::make_reverse_iterator( idx.begin() );

            while( result.size() < limit && iter != end_iter )
            {
               if( filter( *iter ) )
                  result.push_back( on_push( *iter ) );
               ++iter;
            }
         }
      }

      chain::database& _db;
};




//////////////////////////////////////////////////////////////////////
//                                                                  //
// Constructors                                                     //
//                                                                  //
//////////////////////////////////////////////////////////////////////

database_api::database_api()
   : my( new database_api_impl() )
{
   JSON_RPC_REGISTER_API( HIVE_DATABASE_API_PLUGIN_NAME );
}

database_api::~database_api() {}

database_api_impl::database_api_impl()
   : _db( appbase::app().get_plugin< hive::plugins::chain::chain_plugin >().db() ) {}

database_api_impl::~database_api_impl() {}

//////////////////////////////////////////////////////////////////////
//                                                                  //
// Globals                                                          //
//                                                                  //
//////////////////////////////////////////////////////////////////////


DEFINE_API_IMPL( database_api_impl, get_config )
{
   return hive::protocol::get_config( _db.get_treasury_name(), _db.get_chain_id() );
}

DEFINE_API_IMPL( database_api_impl, get_version )
{
   return get_version_return
   (
      fc::string( HIVE_BLOCKCHAIN_VERSION ),
      fc::string( hive::utilities::git_revision_sha ),
      fc::string( fc::git_revision_sha ),
      _db.get_chain_id()
   );
}

DEFINE_API_IMPL( database_api_impl, get_dynamic_global_properties )
{
   return _db.get_dynamic_global_properties().copy_chain_object(); //FIXME: exposes internal chain object as API result
}

DEFINE_API_IMPL( database_api_impl, get_witness_schedule )
{
   return api_witness_schedule_object( _db.get_witness_schedule_object() );
}

DEFINE_API_IMPL( database_api_impl, get_hardfork_properties )
{
   return _db.get_hardfork_property_object();
}

DEFINE_API_IMPL( database_api_impl, get_reward_funds )
{
   get_reward_funds_return result;

   const auto& rf_idx = _db.get_index< reward_fund_index, by_id >();
   auto itr = rf_idx.begin();

   while( itr != rf_idx.end() )
   {
      result.funds.push_back( itr->copy_chain_object() ); //FIXME: exposes internal chain object as API result
      ++itr;
   }

   return result;
}

DEFINE_API_IMPL( database_api_impl, get_current_price_feed )
{
   return _db.get_feed_history().current_median_history;;
}

DEFINE_API_IMPL( database_api_impl, get_feed_history )
{
   return _db.get_feed_history();
}


//////////////////////////////////////////////////////////////////////
//                                                                  //
// Witnesses                                                        //
//                                                                  //
//////////////////////////////////////////////////////////////////////

DEFINE_API_IMPL( database_api_impl, list_witnesses )
{
   FC_ASSERT( args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT );

   list_witnesses_return result;
   result.witnesses.reserve( args.limit );

   switch( args.order )
   {
      case( by_name ):
      {
         iterate_results< chain::witness_index, chain::by_name >(
            args.start.as< protocol::account_name_type >(),
            result.witnesses,
            args.limit,
            [&]( const witness_object& w ){ return api_witness_object( w ); },
            &database_api_impl::filter_default< witness_object > );
         break;
      }
      case( by_vote_name ):
      {
         auto key = args.start.as< std::pair< share_type, account_name_type > >();
         iterate_results< chain::witness_index, chain::by_vote_name >(
            boost::make_tuple( key.first, key.second ),
            result.witnesses,
            args.limit,
            [&]( const witness_object& w ){ return api_witness_object( w ); },
            &database_api_impl::filter_default< witness_object > );
         break;
      }
      case( by_schedule_time ):
      {
         auto key = args.start.as< std::pair< fc::uint128, account_name_type > >();
         auto wit_id = _db.get< chain::witness_object, chain::by_name >( key.second ).get_id();
         iterate_results< chain::witness_index, chain::by_schedule_time >(
            boost::make_tuple( key.first, wit_id ),
            result.witnesses,
            args.limit,
            [&]( const witness_object& w ){ return api_witness_object( w ); },
            &database_api_impl::filter_default< witness_object > );
         break;
      }
      default:
         FC_ASSERT( false, "Unknown or unsupported sort order" );
   }

   return result;
}

DEFINE_API_IMPL( database_api_impl, find_witnesses )
{
   FC_ASSERT( args.owners.size() <= DATABASE_API_SINGLE_QUERY_LIMIT );

   find_witnesses_return result;

   for( auto& o : args.owners )
   {
      auto witness = _db.find< chain::witness_object, chain::by_name >( o );

      if( witness != nullptr )
         result.witnesses.push_back( api_witness_object( *witness ) );
   }

   return result;
}

DEFINE_API_IMPL( database_api_impl, list_witness_votes )
{
   FC_ASSERT( args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT );

   list_witness_votes_return result;
   result.votes.reserve( args.limit );

   switch( args.order )
   {
      case( by_account_witness ):
      {
         auto key = args.start.as< std::pair< account_name_type, account_name_type > >();
         iterate_results< chain::witness_vote_index, chain::by_account_witness >(
            boost::make_tuple( key.first, key.second ),
            result.votes,
            args.limit,
            [&]( const witness_vote_object& v ){ return v.copy_chain_object(); }, //FIXME: exposes internal chain object as API result
            &database_api_impl::filter_default< api_witness_vote_object > );
         break;
      }
      case( by_witness_account ):
      {
         auto key = args.start.as< std::pair< account_name_type, account_name_type > >();
         iterate_results< chain::witness_vote_index, chain::by_witness_account >(
            boost::make_tuple( key.first, key.second ),
            result.votes,
            args.limit,
            [&]( const witness_vote_object& v ){ return v.copy_chain_object(); }, //FIXME: exposes internal chain object as API result
            &database_api_impl::filter_default< api_witness_vote_object > );
         break;
      }
      default:
         FC_ASSERT( false, "Unknown or unsupported sort order" );
   }

   return result;
}

DEFINE_API_IMPL( database_api_impl, get_active_witnesses )
{
   const auto& wso = _db.get_witness_schedule_object();
   size_t n = wso.current_shuffled_witnesses.size();
   get_active_witnesses_return result;
   result.witnesses.reserve( n );
   for( size_t i=0; i<n; i++ )
      result.witnesses.push_back( wso.current_shuffled_witnesses[i] );
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
   FC_ASSERT( args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT );

   list_accounts_return result;
   result.accounts.reserve( args.limit );

   switch( args.order )
   {
      case( by_name ):
      {
         iterate_results< chain::account_index, chain::by_name >(
            args.start.as< protocol::account_name_type >(),
            result.accounts,
            args.limit,
            [&]( const account_object& a ){ return api_account_object( a, _db, args.delayed_votes_active ); },
            &database_api_impl::filter_default< account_object > );
         break;
      }
      case( by_proxy ):
      {
         auto key = args.start.as< std::pair< account_name_type, account_name_type > >();
         iterate_results< chain::account_index, chain::by_proxy >(
            boost::make_tuple( key.first, key.second ),
            result.accounts,
            args.limit,
            [&]( const account_object& a ){ return api_account_object( a, _db, args.delayed_votes_active ); },
            &database_api_impl::filter_default< account_object > );
         break;
      }
      case( by_next_vesting_withdrawal ):
      {
         auto key = args.start.as< std::pair< fc::time_point_sec, account_name_type > >();
         iterate_results< chain::account_index, chain::by_next_vesting_withdrawal >(
            boost::make_tuple( key.first, key.second ),
            result.accounts,
            args.limit,
            [&]( const account_object& a ){ return api_account_object( a, _db, args.delayed_votes_active ); },
            &database_api_impl::filter_default< account_object > );
         break;
      }
      default:
         FC_ASSERT( false, "Unknown or unsupported sort order" );
   }

   return result;
}

DEFINE_API_IMPL( database_api_impl, find_accounts )
{
   find_accounts_return result;
   FC_ASSERT( args.accounts.size() <= DATABASE_API_SINGLE_QUERY_LIMIT );

   for( auto& a : args.accounts )
   {
      auto acct = _db.find< chain::account_object, chain::by_name >( a );
      if( acct != nullptr )
         result.accounts.push_back( api_account_object( *acct, _db, args.delayed_votes_active ) );
   }

   return result;
}


/* Owner Auth Histories */

DEFINE_API_IMPL( database_api_impl, list_owner_histories )
{
   FC_ASSERT( args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT );

   list_owner_histories_return result;
   result.owner_auths.reserve( args.limit );

   auto key = args.start.as< std::pair< account_name_type, fc::time_point_sec > >();
   iterate_results< chain::owner_authority_history_index, chain::by_account >(
      boost::make_tuple( key.first, key.second ),
      result.owner_auths,
      args.limit,
      [&]( const owner_authority_history_object& o ){ return api_owner_authority_history_object( o ); },
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
      result.owner_auths.push_back( api_owner_authority_history_object( *itr ) );
      ++itr;
   }

   return result;
}


/* Account Recovery Requests */

DEFINE_API_IMPL( database_api_impl, list_account_recovery_requests )
{
   FC_ASSERT( args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT );

   list_account_recovery_requests_return result;
   result.requests.reserve( args.limit );

   switch( args.order )
   {
      case( by_account ):
      {
         iterate_results< chain::account_recovery_request_index, chain::by_account >(
            args.start.as< account_name_type >(),
            result.requests,
            args.limit,
            [&]( const account_recovery_request_object& a ){ return api_account_recovery_request_object( a ); },
            &database_api_impl::filter_default< api_account_recovery_request_object > );
         break;
      }
      case( by_expiration ):
      {
         auto key = args.start.as< std::pair< fc::time_point_sec, account_name_type > >();
         iterate_results< chain::account_recovery_request_index, chain::by_expiration >(
            boost::make_tuple( key.first, key.second ),
            result.requests,
            args.limit,
            [&]( const account_recovery_request_object& a ){ return api_account_recovery_request_object( a ); },
            &database_api_impl::filter_default< api_account_recovery_request_object > );
         break;
      }
      default:
         FC_ASSERT( false, "Unknown or unsupported sort order" );
   }

   return result;
}

DEFINE_API_IMPL( database_api_impl, find_account_recovery_requests )
{
   find_account_recovery_requests_return result;
   FC_ASSERT( args.accounts.size() <= DATABASE_API_SINGLE_QUERY_LIMIT );

   for( auto& a : args.accounts )
   {
      auto request = _db.find< chain::account_recovery_request_object, chain::by_account >( a );

      if( request != nullptr )
         result.requests.push_back( api_account_recovery_request_object( *request ) );
   }

   return result;
}


/* Change Recovery Account Requests */

DEFINE_API_IMPL( database_api_impl, list_change_recovery_account_requests )
{
   FC_ASSERT( args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT );

   list_change_recovery_account_requests_return result;
   result.requests.reserve( args.limit );

   switch( args.order )
   {
      case( by_account ):
      {
         iterate_results< chain::change_recovery_account_request_index, chain::by_account >(
            args.start.as< account_name_type >(),
            result.requests,
            args.limit,
            &database_api_impl::on_push_default< change_recovery_account_request_object >,
            &database_api_impl::filter_default< change_recovery_account_request_object > );
         break;
      }
      case( by_effective_date ):
      {
         auto key = args.start.as< std::pair< fc::time_point_sec, account_name_type > >();
         iterate_results< chain::change_recovery_account_request_index, chain::by_effective_date >(
            boost::make_tuple( key.first, key.second ),
            result.requests,
            args.limit,
            &database_api_impl::on_push_default< change_recovery_account_request_object >,
            &database_api_impl::filter_default< change_recovery_account_request_object > );
         break;
      }
      default:
         FC_ASSERT( false, "Unknown or unsupported sort order" );
   }

   return result;
}

DEFINE_API_IMPL( database_api_impl, find_change_recovery_account_requests )
{
   find_change_recovery_account_requests_return result;
   FC_ASSERT( args.accounts.size() <= DATABASE_API_SINGLE_QUERY_LIMIT );

   for( auto& a : args.accounts )
   {
      auto request = _db.find< chain::change_recovery_account_request_object, chain::by_account >( a );

      if( request != nullptr )
         result.requests.push_back( request->copy_chain_object() ); //FIXME: exposes internal chain object as API result
   }

   return result;
}


/* Escrows */

DEFINE_API_IMPL( database_api_impl, list_escrows )
{
   FC_ASSERT( args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT );

   list_escrows_return result;
   result.escrows.reserve( args.limit );

   switch( args.order )
   {
      case( by_from_id ):
      {
         auto key = args.start.as< std::pair< account_name_type, uint32_t > >();
         iterate_results< chain::escrow_index, chain::by_from_id >(
            boost::make_tuple( key.first, key.second ),
            result.escrows,
            args.limit,
            &database_api_impl::on_push_default< escrow_object >,
            &database_api_impl::filter_default< escrow_object > );
         break;
      }
      case( by_ratification_deadline ):
      {
         auto key = args.start.as< std::vector< fc::variant > >();
         FC_ASSERT( key.size() == 3, "by_ratification_deadline start requires 3 values. (bool, time_point_sec, escrow_id_type)" );
         iterate_results< chain::escrow_index, chain::by_ratification_deadline >(
            boost::make_tuple( key[0].as< bool >(), key[1].as< fc::time_point_sec >(), key[2].as< escrow_id_type >() ),
            result.escrows,
            args.limit,
            &database_api_impl::on_push_default< escrow_object >,
            &database_api_impl::filter_default< escrow_object > );
         break;
      }
      default:
         FC_ASSERT( false, "Unknown or unsupported sort order" );
   }

   return result;
}

DEFINE_API_IMPL( database_api_impl, find_escrows )
{
   find_escrows_return result;

   const auto& escrow_idx = _db.get_index< chain::escrow_index, chain::by_from_id >();
   auto itr = escrow_idx.lower_bound( args.from );

   while( itr != escrow_idx.end() && itr->from == args.from && result.escrows.size() <= DATABASE_API_SINGLE_QUERY_LIMIT )
   {
      result.escrows.push_back( itr->copy_chain_object() ); //FIXME: exposes internal chain object as API result
      ++itr;
   }

   return result;
}


/* Withdraw Vesting Routes */

DEFINE_API_IMPL( database_api_impl, list_withdraw_vesting_routes )
{
   FC_ASSERT( args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT );

   list_withdraw_vesting_routes_return result;
   result.routes.reserve( args.limit );

   switch( args.order )
   {
      case( by_withdraw_route ):
      {
         auto key = args.start.as< std::pair< account_name_type, account_name_type > >();
         iterate_results< chain::withdraw_vesting_route_index, chain::by_withdraw_route >(
            boost::make_tuple( key.first, key.second ),
            result.routes,
            args.limit,
            &database_api_impl::on_push_default< withdraw_vesting_route_object >,
            &database_api_impl::filter_default< withdraw_vesting_route_object > );
         break;
      }
      case( by_destination ):
      {
         auto key = args.start.as< std::pair< account_name_type, withdraw_vesting_route_id_type > >();
         iterate_results< chain::withdraw_vesting_route_index, chain::by_destination >(
            boost::make_tuple( key.first, key.second ),
            result.routes,
            args.limit,
            &database_api_impl::on_push_default< withdraw_vesting_route_object >,
            &database_api_impl::filter_default< withdraw_vesting_route_object > );
         break;
      }
      default:
         FC_ASSERT( false, "Unknown or unsupported sort order" );
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
            result.routes.push_back( itr->copy_chain_object() ); //FIXME: exposes internal chain object as API result
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
            result.routes.push_back( itr->copy_chain_object() ); //FIXME: exposes internal chain object as API result
            ++itr;
         }

         break;
      }
      default:
         FC_ASSERT( false, "Unknown or unsupported sort order" );
   }

   return result;
}


/* Savings Withdrawals */

DEFINE_API_IMPL( database_api_impl, list_savings_withdrawals )
{
   FC_ASSERT( args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT );

   list_savings_withdrawals_return result;
   result.withdrawals.reserve( args.limit );

   switch( args.order )
   {
      case( by_from_id ):
      {
         auto key = args.start.as< std::pair< account_name_type, uint32_t > >();
         iterate_results< chain::savings_withdraw_index, chain::by_from_rid >(
            boost::make_tuple( key.first, key.second ),
            result.withdrawals,
            args.limit,
            [&]( const savings_withdraw_object& w ){ return api_savings_withdraw_object( w ); },
            &database_api_impl::filter_default< savings_withdraw_object > );
         break;
      }
      case( by_complete_from_id ):
      {
         auto key = args.start.as< std::vector< fc::variant > >();
         FC_ASSERT( key.size() == 3, "by_complete_from_id start requires 3 values. (time_point_sec, account_name_type, uint32_t)" );
         iterate_results< chain::savings_withdraw_index, chain::by_complete_from_rid >(
            boost::make_tuple( key[0].as< fc::time_point_sec >(), key[1].as< account_name_type >(), key[2].as< uint32_t >() ),
            result.withdrawals,
            args.limit,
            [&]( const savings_withdraw_object& w ){ return api_savings_withdraw_object( w ); },
            &database_api_impl::filter_default< savings_withdraw_object > );
         break;
      }
      case( by_to_complete ):
      {
         auto key = args.start.as< std::vector< fc::variant > >();
         FC_ASSERT( key.size() == 3, "by_to_complete start requires 3 values. (account_name_type, time_point_sec, savings_withdraw_id_type" );
         iterate_results< chain::savings_withdraw_index, chain::by_to_complete >(
            boost::make_tuple( key[0].as< account_name_type >(), key[1].as< fc::time_point_sec >(), key[2].as< savings_withdraw_id_type >() ),
            result.withdrawals,
            args.limit,
            [&]( const savings_withdraw_object& w ){ return api_savings_withdraw_object( w ); },
            &database_api_impl::filter_default< savings_withdraw_object > );
         break;
      }
      default:
         FC_ASSERT( false, "Unknown or unsupported sort order" );
   }

   return result;
}

DEFINE_API_IMPL( database_api_impl, find_savings_withdrawals )
{
   find_savings_withdrawals_return result;
   const auto& withdraw_idx = _db.get_index< chain::savings_withdraw_index, chain::by_from_rid >();
   auto itr = withdraw_idx.lower_bound( args.account );

   while( itr != withdraw_idx.end() && itr->from == args.account && result.withdrawals.size() <= DATABASE_API_SINGLE_QUERY_LIMIT )
   {
      result.withdrawals.push_back( api_savings_withdraw_object( *itr ) );
      ++itr;
   }

   return result;
}


/* Vesting Delegations */

DEFINE_API_IMPL( database_api_impl, list_vesting_delegations )
{
   FC_ASSERT( args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT );

   list_vesting_delegations_return result;
   result.delegations.reserve( args.limit );

   switch( args.order )
   {
      case( by_delegation ):
      {
         auto key = args.start.as< std::pair< account_name_type, account_name_type > >();
         iterate_results< chain::vesting_delegation_index, chain::by_delegation >(
            boost::make_tuple( key.first, key.second ),
            result.delegations,
            args.limit,
            &database_api_impl::on_push_default< api_vesting_delegation_object >,
            &database_api_impl::filter_default< vesting_delegation_object > );
         break;
      }
      default:
         FC_ASSERT( false, "Unknown or unsupported sort order" );
   }

   return result;
}

DEFINE_API_IMPL( database_api_impl, find_vesting_delegations )
{
   find_vesting_delegations_return result;
   const auto& delegation_idx = _db.get_index< chain::vesting_delegation_index, chain::by_delegation >();
   auto itr = delegation_idx.lower_bound( args.account );

   while( itr != delegation_idx.end() && itr->delegator == args.account && result.delegations.size() <= DATABASE_API_SINGLE_QUERY_LIMIT )
   {
      result.delegations.push_back( itr->copy_chain_object() ); //FIXME: exposes internal chain object as API result
      ++itr;
   }

   return result;
}


/* Vesting Delegation Expirations */

DEFINE_API_IMPL( database_api_impl, list_vesting_delegation_expirations )
{
   FC_ASSERT( args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT );

   list_vesting_delegation_expirations_return result;
   result.delegations.reserve( args.limit );

   switch( args.order )
   {
      case( by_expiration ):
      {
         auto key = args.start.as< std::pair< time_point_sec, vesting_delegation_expiration_id_type > >();
         iterate_results< chain::vesting_delegation_expiration_index, chain::by_expiration >(
            boost::make_tuple( key.first, key.second ),
            result.delegations,
            args.limit,
            &database_api_impl::on_push_default< api_vesting_delegation_expiration_object >,
            &database_api_impl::filter_default< vesting_delegation_expiration_object > );
         break;
      }
      case( by_account_expiration ):
      {
         auto key = args.start.as< std::vector< fc::variant > >();
         FC_ASSERT( key.size() == 3, "by_account_expiration start requires 3 values. (account_name_type, time_point_sec, vesting_delegation_expiration_id_type" );
         iterate_results< chain::vesting_delegation_expiration_index, chain::by_account_expiration >(
            boost::make_tuple( key[0].as< account_name_type >(), key[1].as< time_point_sec >(), key[2].as< vesting_delegation_expiration_id_type >() ),
            result.delegations,
            args.limit,
            &database_api_impl::on_push_default< api_vesting_delegation_expiration_object >,
            &database_api_impl::filter_default< vesting_delegation_expiration_object > );
         break;
      }
      default:
         FC_ASSERT( false, "Unknown or unsupported sort order" );
   }

   return result;
}

DEFINE_API_IMPL( database_api_impl, find_vesting_delegation_expirations )
{
   find_vesting_delegation_expirations_return result;
   const auto& del_exp_idx = _db.get_index< chain::vesting_delegation_expiration_index, chain::by_account_expiration >();
   auto itr = del_exp_idx.lower_bound( args.account );

   while( itr != del_exp_idx.end() && itr->delegator == args.account && result.delegations.size() <= DATABASE_API_SINGLE_QUERY_LIMIT )
   {
      result.delegations.push_back( itr->copy_chain_object() ); //FIXME: exposes internal chain object as API result
      ++itr;
   }

   return result;
}


/* HBD Conversion Requests */

DEFINE_API_IMPL( database_api_impl, list_hbd_conversion_requests )
{
   FC_ASSERT( args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT );

   list_hbd_conversion_requests_return result;
   result.requests.reserve( args.limit );

   switch( args.order )
   {
      case( by_conversion_date ):
      {
         auto key = args.start.as< std::pair< time_point_sec, convert_request_id_type > >();
         iterate_results< chain::convert_request_index, chain::by_conversion_date >(
            boost::make_tuple( key.first, key.second ),
            result.requests,
            args.limit,
            &database_api_impl::on_push_default< api_convert_request_object >,
            &database_api_impl::filter_default< convert_request_object > );
         break;
      }
      case( by_account ):
      {
         auto key = args.start.as< std::pair< account_name_type, uint32_t > >();
         iterate_results< chain::convert_request_index, chain::by_owner >(
            boost::make_tuple( key.first, key.second ),
            result.requests,
            args.limit,
            &database_api_impl::on_push_default< api_convert_request_object >,
            &database_api_impl::filter_default< convert_request_object > );
         break;
      }
      default:
         FC_ASSERT( false, "Unknown or unsupported sort order" );
   }

   return result;
}

DEFINE_API_IMPL( database_api_impl, find_hbd_conversion_requests )
{
   find_hbd_conversion_requests_return result;
   const auto& convert_idx = _db.get_index< chain::convert_request_index, chain::by_owner >();
   auto itr = convert_idx.lower_bound( args.account );

   while( itr != convert_idx.end() && itr->owner == args.account && result.requests.size() <= DATABASE_API_SINGLE_QUERY_LIMIT )
   {
      result.requests.push_back( itr->copy_chain_object() ); //FIXME: exposes internal chain object as API result
      ++itr;
   }

   return result;
}


/* Decline Voting Rights Requests */

DEFINE_API_IMPL( database_api_impl, list_decline_voting_rights_requests )
{
   FC_ASSERT( args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT );

   list_decline_voting_rights_requests_return result;
   result.requests.reserve( args.limit );

   switch( args.order )
   {
      case( by_account ):
      {
         iterate_results< chain::decline_voting_rights_request_index, chain::by_account >(
            args.start.as< account_name_type >(),
            result.requests,
            args.limit,
            &database_api_impl::on_push_default< api_decline_voting_rights_request_object >,
            &database_api_impl::filter_default< decline_voting_rights_request_object > );
         break;
      }
      case( by_effective_date ):
      {
         auto key = args.start.as< std::pair< time_point_sec, account_name_type > >();
         iterate_results< chain::decline_voting_rights_request_index, chain::by_effective_date >(
            boost::make_tuple( key.first, key.second ),
            result.requests,
            args.limit,
            &database_api_impl::on_push_default< api_decline_voting_rights_request_object >,
            &database_api_impl::filter_default< decline_voting_rights_request_object > );
         break;
      }
      default:
         FC_ASSERT( false, "Unknown or unsupported sort order" );
   }

   return result;
}

DEFINE_API_IMPL( database_api_impl, find_decline_voting_rights_requests )
{
   FC_ASSERT( args.accounts.size() <= DATABASE_API_SINGLE_QUERY_LIMIT );
   find_decline_voting_rights_requests_return result;

   for( auto& a : args.accounts )
   {
      auto request = _db.find< chain::decline_voting_rights_request_object, chain::by_account >( a );

      if( request != nullptr )
         result.requests.push_back( request->copy_chain_object() ); //FIXME: exposes internal chain object as API result
   }

   return result;
}


//////////////////////////////////////////////////////////////////////
//                                                                  //
// Comments                                                         //
//                                                                  //
//////////////////////////////////////////////////////////////////////

/* Comments */
DEFINE_API_IMPL( database_api_impl, find_comments )
{
   FC_ASSERT( args.comments.size() <= DATABASE_API_SINGLE_QUERY_LIMIT );
   find_comments_return result;
   result.comments.reserve( args.comments.size() );

   for( auto& key: args.comments )
   {
      auto comment = _db.find_comment( key.first, key.second );

      if( comment != nullptr )
         result.comments.push_back( api_comment_object( *comment, _db ) );
   }

   return result;
}

//====================================================last_votes_misc====================================================

namespace last_votes_misc
{

   //====================================================votes_impl====================================================
   template< sort_order_type SORTORDERTYPE >
   void votes_impl( database_api_impl& _impl, vector< api_comment_vote_object >& c, size_t nr_args, uint32_t limit, vector< fc::variant >& key, fc::time_point_sec& timestamp, uint64_t weight )
   {
      if( SORTORDERTYPE == by_comment_voter )
         FC_ASSERT( key.size() == nr_args, "by_comment_voter start requires ${nr_args} values. (account_name_type, string, account_name_type)", ("nr_args", nr_args ) );
      else
         FC_ASSERT( key.size() == nr_args, "by_comment_voter start requires ${nr_args} values. (account_name_type, ${desc}account_name_type, string)", ("nr_args", nr_args )("desc",( nr_args == 4 )?"time_point_sec, ":"" ) );

      account_name_type voter;
      account_name_type author;
      string permlink;

      account_id_type voter_id;
      comment_id_type comment_id;

      if( SORTORDERTYPE == by_comment_voter )
      {
         author = key[0].as< account_name_type >();
         permlink = key[1].as< string >();
         voter = key[ 2 ].as< account_name_type >();
      }
      else
      {
         author = key[ nr_args - 2 ].as< account_name_type >();
         permlink = key[ nr_args - 1 ].as< string >();
         voter = key[0].as< account_name_type >();
      }

      if( voter != account_name_type() )
      {
         auto account = _impl._db.find< chain::account_object, chain::by_name >( voter );
         FC_ASSERT( account != nullptr, "Could not find voter ${v}.", ("v", voter ) );
         voter_id = account->get_id();
      }

      if( author != account_name_type() || permlink.size() )
      {
         auto comment = _impl._db.find_comment( author, permlink );
         FC_ASSERT( comment != nullptr, "Could not find comment ${a}/${p}.", ("a", author)("p", permlink) );
         comment_id = comment->get_id();
      }

      if( SORTORDERTYPE == by_comment_voter )
      {
         _impl.iterate_results< chain::comment_vote_index, chain::by_comment_voter >(
         boost::make_tuple( comment_id, voter_id ),
         c,
         limit,
         [&]( const comment_vote_object& cv ){ return api_comment_vote_object( cv, _impl._db ); },
         &database_api_impl::filter_default< comment_vote_object > );
      }
      else if( SORTORDERTYPE == by_voter_comment )
      {
         _impl.iterate_results< chain::comment_vote_index, chain::by_voter_comment >(
         boost::make_tuple( voter_id, comment_id ),
         c,
         limit,
         [&]( const comment_vote_object& cv ){ return api_comment_vote_object( cv, _impl._db ); },
         &database_api_impl::filter_default< comment_vote_object > );
      }
   }

}//namespace last_votes_misc

//====================================================last_votes_misc====================================================

/* Votes */

DEFINE_API_IMPL( database_api_impl, list_votes )
{
   FC_ASSERT( args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT );

   auto key = args.start.as< vector< fc::variant > >();

   list_votes_return result;
   result.votes.reserve( args.limit );

   switch( args.order )
   {
      case( by_comment_voter ):
      {
         static fc::time_point_sec t( -1 );
         last_votes_misc::votes_impl< by_comment_voter >( *this, result.votes, 3/*nr_args*/, args.limit, key, t, 0 );
         break;
      }
      case( by_voter_comment ):
      {
         static fc::time_point_sec t( -1 );
         last_votes_misc::votes_impl< by_voter_comment >( *this, result.votes, 3/*nr_args*/, args.limit, key, t, 0 );
         break;
      }
      default:
         FC_ASSERT( false, "Unknown or unsupported sort order" );
   }

   return result;
}

DEFINE_API_IMPL( database_api_impl, find_votes )
{
   find_votes_return result;

   auto comment = _db.find_comment( args.author, args.permlink );
   FC_ASSERT( comment != nullptr, "Could not find comment ${a}/${p}", ("a", args.author)("p", args.permlink ) );

   const auto& vote_idx = _db.get_index< chain::comment_vote_index, chain::by_comment_voter >();
   auto itr = vote_idx.lower_bound( comment->get_id() );

   while( itr != vote_idx.end() && itr->comment == comment->get_id() && result.votes.size() <= DATABASE_API_SINGLE_QUERY_LIMIT )
   {
      result.votes.push_back( api_comment_vote_object( *itr, _db ) );
      ++itr;
   }

   return result;
}


//////////////////////////////////////////////////////////////////////
//                                                                  //
// Market                                                           //
//                                                                  //
//////////////////////////////////////////////////////////////////////

/* Limit Orders */

DEFINE_API_IMPL( database_api_impl, list_limit_orders )
{
   FC_ASSERT( args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT );

   list_limit_orders_return result;
   result.orders.reserve( args.limit );

   switch( args.order )
   {
      case( by_price ):
      {
         auto key = args.start.as< std::pair< price, limit_order_id_type > >();
         iterate_results< chain::limit_order_index, chain::by_price >(
            boost::make_tuple( key.first, key.second ),
            result.orders,
            args.limit,
            &database_api_impl::on_push_default< api_limit_order_object >,
            &database_api_impl::filter_default< limit_order_object > );
         break;
      }
      case( by_account ):
      {
         auto key = args.start.as< std::pair< account_name_type, uint32_t > >();
         iterate_results< chain::limit_order_index, chain::by_account >(
            boost::make_tuple( key.first, key.second ),
            result.orders,
            args.limit,
            &database_api_impl::on_push_default< api_limit_order_object >,
            &database_api_impl::filter_default< limit_order_object > );
         break;
      }
      default:
         FC_ASSERT( false, "Unknown or unsupported sort order" );
   }

   return result;
}

DEFINE_API_IMPL( database_api_impl, find_limit_orders )
{
   find_limit_orders_return result;
   const auto& order_idx = _db.get_index< chain::limit_order_index, chain::by_account >();
   auto itr = order_idx.lower_bound( args.account );

   while( itr != order_idx.end() && itr->seller == args.account && result.orders.size() <= DATABASE_API_SINGLE_QUERY_LIMIT )
   {
      result.orders.push_back( itr->copy_chain_object() ); //FIXME: exposes internal chain object as API result
      ++itr;
   }

   return result;
}


/* Order Book */

DEFINE_API_IMPL( database_api_impl, get_order_book )
{
   FC_ASSERT( args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT );
   get_order_book_return result;

   auto max_sell = price::max( HBD_SYMBOL, HIVE_SYMBOL );
   auto max_buy = price::max( HIVE_SYMBOL, HBD_SYMBOL );

   const auto& limit_price_idx = _db.get_index< chain::limit_order_index >().indices().get< chain::by_price >();
   auto sell_itr = limit_price_idx.lower_bound( max_sell );
   auto buy_itr  = limit_price_idx.lower_bound( max_buy );
   auto end = limit_price_idx.end();

   while( sell_itr != end && sell_itr->sell_price.base.symbol == HBD_SYMBOL && result.bids.size() < args.limit )
   {
      auto itr = sell_itr;
      order cur;
      cur.order_price = itr->sell_price;
      cur.real_price  = 0.0;
      // cur.real_price  = (cur.order_price).to_real();
      cur.hbd = itr->for_sale;
      cur.hive = ( asset( itr->for_sale, HBD_SYMBOL ) * cur.order_price ).amount;
      cur.created = itr->created;
      result.bids.push_back( cur );
      ++sell_itr;
   }
   while( buy_itr != end && buy_itr->sell_price.base.symbol == HIVE_SYMBOL && result.asks.size() < args.limit )
   {
      auto itr = buy_itr;
      order cur;
      cur.order_price = itr->sell_price;
      cur.real_price = 0.0;
      // cur.real_price  = (~cur.order_price).to_real();
      cur.hive    = itr->for_sale;
      cur.hbd     = ( asset( itr->for_sale, HIVE_SYMBOL ) * cur.order_price ).amount;
      cur.created = itr->created;
      result.asks.push_back( cur );
      ++buy_itr;
   }

   return result;
}

//////////////////////////////////////////////////////////////////////
//                                                                  //
// SPS                                                              //
//                                                                  //
//////////////////////////////////////////////////////////////////////

/* Proposals */

proposal_status get_proposal_status( const proposal_object& po, const time_point_sec current_time )
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

DEFINE_API_IMPL( database_api_impl, list_proposals )
{
   using variants = std::vector< fc::variant >;
   FC_ASSERT( args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT );

   list_proposals_return result;
   result.proposals.reserve( args.limit );

   const auto current_time = _db.head_block_time();

   constexpr auto LOWEST_PROPOSAL_ID = 0;
   constexpr auto GREATEST_PROPOSAL_ID = std::numeric_limits<api_id_type>::max();
   switch( args.order )
   {
      case by_creator:
      {
          // Workaround: at the moment there is assumption, that no more than one start parameter is passed, more are ignored
         auto start_parameters = args.start.as< variants >();
         auto start_creator = start_parameters.empty()
                 ? account_name_type()
                 : start_parameters.front().as< account_name_type >()
         ;
         iterate_results< hive::chain::proposal_index, hive::chain::by_creator >(
            boost::make_tuple( start_creator, args.order_direction == ascending ? LOWEST_PROPOSAL_ID : GREATEST_PROPOSAL_ID ),
            result.proposals,
            args.limit,
            [&]( const proposal_object& po ){ return api_proposal_object( po, current_time ); },
            [&]( const proposal_object& po ){ return filter_proposal_status( po, args.status, current_time ); },
            args.order_direction,
            args.last_id
         );
         break;
      }
      case by_start_date:
      {
         // Workaround: at the moment there is assumption, that no more than one start parameter is passed, more are ignored
         auto start_parameters = args.start.as< variants >();
         auto start_date_string = start_parameters.empty()
               ? std::string()
               : start_parameters.front().as< std::string >()
         ;
         // check if empty string was passed as the time
         auto time =  start_date_string.empty() || start_parameters.empty()
             ? time_point_sec( args.order_direction == ascending ? fc::time_point::min() : fc::time_point::maximum() )
             : start_parameters.front().as< time_point_sec >()
         ;

         iterate_results< hive::chain::proposal_index, hive::chain::by_start_date >(
            boost::make_tuple( time, args.order_direction == ascending ? LOWEST_PROPOSAL_ID : GREATEST_PROPOSAL_ID ),
            result.proposals,
            args.limit,
            [&]( const proposal_object& po ){ return api_proposal_object( po, current_time ); },
            [&]( const proposal_object& po ){ return filter_proposal_status( po, args.status, current_time ); },
            args.order_direction,
            args.last_id
         );
         break;
      }
      case by_end_date:
      {
         // Workaround: at the moment there is assumption, that no more than one start parameter is passed, more are ignored
         auto start_parameters = args.start.as< variants >();
         auto end_date_string = start_parameters.empty()
               ? std::string()
               : start_parameters.front().as< std::string >()
         ;
         // check if empty string was passed as the time
         auto time = end_date_string.empty() || start_parameters.empty()
               ? time_point_sec( args.order_direction == ascending ? fc::time_point::min() : fc::time_point::maximum() )
               : start_parameters.front().as< time_point_sec >()
         ;

         iterate_results< hive::chain::proposal_index, hive::chain::by_end_date >(
            boost::make_tuple( time, args.order_direction == ascending ? LOWEST_PROPOSAL_ID : GREATEST_PROPOSAL_ID ),
            result.proposals,
            args.limit,
            [&]( const proposal_object& po ){ return api_proposal_object( po, current_time ); },
            [&]( const proposal_object& po ){ return filter_proposal_status( po, args.status, current_time ); },
            args.order_direction,
            args.last_id
         );
         break;
      }
      case by_total_votes:
      {
         // Workaround: at the moment there is assumption, that no more than one start parameter is passed, more are ignored
         auto start_parameters = args.start.as< variants >();
         auto votes = start_parameters.empty()
               ? uint64_t(0)
               : start_parameters.front().as< uint64_t >()
         ;
         iterate_results< hive::chain::proposal_index, hive::chain::by_total_votes >(
            boost::make_tuple( votes, args.order_direction == ascending ? LOWEST_PROPOSAL_ID : GREATEST_PROPOSAL_ID ),
            result.proposals,
            args.limit,
            [&]( const proposal_object& po ){ return api_proposal_object( po, current_time ); },
            [&]( const proposal_object& po ){ return filter_proposal_status( po, args.status, current_time ); },
            args.order_direction,
            args.last_id
         );
         break;
      }
      default:
         FC_ASSERT( false, "Unknown or unsupported sort order" );
   }

   return result;
}

DEFINE_API_IMPL( database_api_impl, find_proposals )
{
   FC_ASSERT( args.proposal_ids.size() <= DATABASE_API_SINGLE_QUERY_LIMIT );

   find_proposals_return result;
   result.proposals.reserve( args.proposal_ids.size() );

   auto currentTime = _db.head_block_time();

   std::for_each( args.proposal_ids.begin(), args.proposal_ids.end(), [&](auto& id)
   {
      auto po = _db.find< hive::chain::proposal_object, hive::chain::by_proposal_id >( id );
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
   FC_ASSERT( args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT );

   list_proposal_votes_return result;
   result.proposal_votes.reserve( args.limit );

   //auto current_time = _db.head_block_time();

   switch( args.order )
   {
      case by_voter_proposal:
      {
         auto key = args.start.as< std::pair< account_name_type, api_id_type > >();
         iterate_results< hive::chain::proposal_vote_index, hive::chain::by_voter_proposal >(
            boost::make_tuple( key.first, key.second ),
            result.proposal_votes,
            args.limit,
            [&]( const proposal_vote_object& po ){ return api_proposal_vote_object( po, _db ); },
            [&]( const proposal_vote_object& po )
            {
               auto itr = _db.find< hive::chain::proposal_object, hive::chain::by_proposal_id >( po.proposal_id );
               return itr != nullptr && !itr->removed;
            },
            args.order_direction
         );
         break;
      }
      case by_proposal_voter:
      {
         auto key = args.start.as< std::pair< api_id_type, account_name_type > >();
         iterate_results< hive::chain::proposal_vote_index, hive::chain::by_proposal_voter >(
            boost::make_tuple( key.first, key.second ),
            result.proposal_votes,
            args.limit,
            [&]( const proposal_vote_object& po ){ return api_proposal_vote_object( po, _db ); },
            [&]( const proposal_vote_object& po )
            {
               auto itr = _db.find< hive::chain::proposal_object, hive::chain::by_proposal_id >( po.proposal_id );
               return itr != nullptr && !itr->removed;
            },
            args.order_direction
         );
         break;
      }
      default:
         FC_ASSERT( false, "Unknown or unsupported sort order" );
   }

   return result;
}


//////////////////////////////////////////////////////////////////////
//                                                                  //
// Authority / Validation                                           //
//                                                                  //
//////////////////////////////////////////////////////////////////////

DEFINE_API_IMPL( database_api_impl, get_transaction_hex )
{
   return get_transaction_hex_return( { fc::to_hex( fc::raw::pack_to_vector( args.trx ) ) } );
}

DEFINE_API_IMPL( database_api_impl, get_required_signatures )
{
   get_required_signatures_return result;
   result.keys = args.trx.get_required_signatures( _db.get_chain_id(),
                                                   args.available_keys,
                                                   [&]( string account_name ){ return authority( _db.get< chain::account_authority_object, chain::by_account >( account_name ).active  ); },
                                                   [&]( string account_name ){ return authority( _db.get< chain::account_authority_object, chain::by_account >( account_name ).owner   ); },
                                                   [&]( string account_name ){ return authority( _db.get< chain::account_authority_object, chain::by_account >( account_name ).posting ); },
                                                   HIVE_MAX_SIG_CHECK_DEPTH,
                                                   _db.has_hardfork( HIVE_HARDFORK_0_20__1944 ) ? fc::ecc::canonical_signature_type::bip_0062 : fc::ecc::canonical_signature_type::fc_canonical );

   return result;
}

DEFINE_API_IMPL( database_api_impl, get_potential_signatures )
{
   get_potential_signatures_return result;
   args.trx.get_required_signatures(
      _db.get_chain_id(),
      flat_set< public_key_type >(),
      [&]( account_name_type account_name )
      {
         const auto& auth = _db.get< chain::account_authority_object, chain::by_account >( account_name ).active;
         for( const auto& k : auth.get_keys() )
            result.keys.insert( k );
         return authority( auth );
      },
      [&]( account_name_type account_name )
      {
         const auto& auth = _db.get< chain::account_authority_object, chain::by_account >( account_name ).owner;
         for( const auto& k : auth.get_keys() )
            result.keys.insert( k );
         return authority( auth );
      },
      [&]( account_name_type account_name )
      {
         const auto& auth = _db.get< chain::account_authority_object, chain::by_account >( account_name ).posting;
         for( const auto& k : auth.get_keys() )
            result.keys.insert( k );
         return authority( auth );
      },
      HIVE_MAX_SIG_CHECK_DEPTH,
      _db.has_hardfork( HIVE_HARDFORK_0_20__1944 ) ? fc::ecc::canonical_signature_type::bip_0062 : fc::ecc::canonical_signature_type::fc_canonical
   );

   return result;
}

DEFINE_API_IMPL( database_api_impl, verify_authority )
{
   args.trx.verify_authority(_db.get_chain_id(),
                           [&]( string account_name ){ return authority( _db.get< chain::account_authority_object, chain::by_account >( account_name ).active  ); },
                           [&]( string account_name ){ return authority( _db.get< chain::account_authority_object, chain::by_account >( account_name ).owner   ); },
                           [&]( string account_name ){ return authority( _db.get< chain::account_authority_object, chain::by_account >( account_name ).posting ); },
                           HIVE_MAX_SIG_CHECK_DEPTH,
                           HIVE_MAX_AUTHORITY_MEMBERSHIP,
                           HIVE_MAX_SIG_CHECK_ACCOUNTS,
                           _db.has_hardfork( HIVE_HARDFORK_0_20__1944 ) ? fc::ecc::canonical_signature_type::bip_0062 : fc::ecc::canonical_signature_type::fc_canonical );
   return verify_authority_return( { true } );
}

// TODO: This is broken. By the look of is, it has been since BitShares. verify_authority always
// returns false because the TX is not signed.
DEFINE_API_IMPL( database_api_impl, verify_account_authority )
{
   auto account = _db.find< chain::account_object, chain::by_name >( args.account );
   FC_ASSERT( account != nullptr, "no such account" );

   /// reuse trx.verify_authority by creating a dummy transfer
   verify_authority_args vap;
   transfer_operation op;
   op.from = account->name;
   vap.trx.operations.emplace_back( op );

   return verify_authority( vap );
}

DEFINE_API_IMPL( database_api_impl, verify_signatures )
{
   // get_signature_keys can throw for dup sigs. Allow this to throw.
   flat_set< public_key_type > sig_keys;
   for( const auto&  sig : args.signatures )
   {
      HIVE_ASSERT(
         sig_keys.insert( fc::ecc::public_key( sig, args.hash ) ).second,
         protocol::tx_duplicate_sig,
         "Duplicate Signature detected" );
   }

   verify_signatures_return result;
   result.valid = true;

   // verify authority throws on failure, catch and return false
   try
   {
      hive::protocol::verify_authority< verify_signatures_args >(
         { args },
         sig_keys,
         [this]( const string& name ) { return authority( _db.get< chain::account_authority_object, chain::by_account >( name ).owner ); },
         [this]( const string& name ) { return authority( _db.get< chain::account_authority_object, chain::by_account >( name ).active ); },
         [this]( const string& name ) { return authority( _db.get< chain::account_authority_object, chain::by_account >( name ).posting ); },
         HIVE_MAX_SIG_CHECK_DEPTH );
   }
   catch( fc::exception& ) { result.valid = false; }

   return result;
}

#ifdef HIVE_ENABLE_SMT
//////////////////////////////////////////////////////////////////////
//                                                                  //
// SMT                                                              //
//                                                                  //
//////////////////////////////////////////////////////////////////////

DEFINE_API_IMPL( database_api_impl, get_nai_pool )
{
   get_nai_pool_return result;
   result.nai_pool = _db.get< nai_pool_object >().pool();
   return result;
}

DEFINE_API_IMPL( database_api_impl, list_smt_contributions )
{
   FC_ASSERT( args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT );

   list_smt_contributions_return result;
   result.contributions.reserve( args.limit );

   switch( args.order )
   {
      case( by_symbol_contributor ):
      {
         auto key = args.start.get_array();
         FC_ASSERT( key.size() == 0 || key.size() == 3, "The parameter 'start' must be an empty array or consist of asset_symbol_type, contributor and contribution_id" );

         boost::tuple< asset_symbol_type, account_name_type, uint32_t > start;
         if ( key.size() == 0 )
            start = boost::make_tuple( asset_symbol_type(), account_name_type(), 0 );
         else
            start = boost::make_tuple( key[ 0 ].as< asset_symbol_type >(), key[ 1 ].as< account_name_type >(), key[ 2 ].as< uint32_t >() );

         iterate_results< chain::smt_contribution_index, chain::by_symbol_contributor >(
            start,
            result.contributions,
            args.limit,
            &database_api_impl::on_push_default< chain::smt_contribution_object >,
            &database_api_impl::filter_default< chain::smt_contribution_object > );
         break;
      }
      case( by_symbol_id ):
      {
         auto key = args.start.get_array();
         FC_ASSERT( key.size() == 0 || key.size() == 2, "The parameter 'start' must be an empty array or consist of asset_symbol_type and id" );

         boost::tuple< asset_symbol_type, smt_contribution_id_type > start;
         if ( key.size() == 0 )
            start = boost::make_tuple( asset_symbol_type(), smt_contribution_id_type() );
         else
            start = boost::make_tuple( key[ 0 ].as< asset_symbol_type >(), key[ 1 ].as< smt_contribution_id_type >() );

         iterate_results< chain::smt_contribution_index, chain::by_symbol_id >(
            start,
            result.contributions,
            args.limit,
            &database_api_impl::on_push_default< chain::smt_contribution_object >,
            &database_api_impl::filter_default< chain::smt_contribution_object > );
         break;
      }
#ifndef IS_LOW_MEM
      case ( by_contributor ):
      {
         auto key = args.start.get_array();
         FC_ASSERT( key.size() == 0 || key.size() == 3, "The parameter 'start' must be an empty array or consist of contributor, asset_symbol_type and contribution_id" );

         boost::tuple< account_name_type, asset_symbol_type, uint32_t > start;
         if ( key.size() == 0 )
            start = boost::make_tuple( account_name_type(), asset_symbol_type(), 0 );
         else
            start = boost::make_tuple( key[ 0 ].as< account_name_type >(), key[ 1 ].as< asset_symbol_type >(), key[ 2 ].as< uint32_t >() );

         iterate_results< chain::smt_contribution_index, chain::by_contributor >(
            start,
            result.contributions,
            args.limit,
            &database_api_impl::on_push_default< chain::smt_contribution_object >,
            &database_api_impl::filter_default< chain::smt_contribution_object > );
         break;
      }
#endif
      default:
         FC_ASSERT( false, "Unknown or unsupported sort order" );
   }

   return result;
}

DEFINE_API_IMPL( database_api_impl, find_smt_contributions )
{
   find_smt_contributions_return result;

   const auto& idx = _db.get_index< chain::smt_contribution_index, chain::by_symbol_contributor >();

   for( auto& symbol_contributor : args.symbol_contributors )
   {
      auto itr = idx.lower_bound( boost::make_tuple( symbol_contributor.first, symbol_contributor.second, 0 ) );
      while( itr != idx.end() && itr->symbol == symbol_contributor.first && itr->contributor == symbol_contributor.second && result.contributions.size() <= DATABASE_API_SINGLE_QUERY_LIMIT )
      {
         result.contributions.push_back( itr->copy_chain_object() ); //FIXME: exposes internal chain object as API result
         ++itr;
      }
   }

   return result;
}

DEFINE_API_IMPL( database_api_impl, list_smt_tokens )
{
   FC_ASSERT( args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT );

   list_smt_tokens_return result;
   result.tokens.reserve( args.limit );

   switch( args.order )
   {
      case( by_symbol ):
      {
         asset_symbol_type start;

         if( args.start.get_object().size() > 0 )
         {
            start = args.start.as< asset_symbol_type >();
         }

         iterate_results< chain::smt_token_index, chain::by_symbol >(
            start,
            result.tokens,
            args.limit,
            [&]( const smt_token_object& t ) { return api_smt_token_object( t, _db ); },
            &database_api_impl::filter_default< chain::smt_token_object > );

         break;
      }
      case( by_control_account ):
      {
         boost::tuple< account_name_type, asset_symbol_type > start;

         if( args.start.is_string() )
         {
            start = boost::make_tuple( args.start.as< account_name_type >(), asset_symbol_type() );
         }
         else
         {
            auto key = args.start.get_array();
            FC_ASSERT( key.size() == 2, "The parameter 'start' must be an account name or an array containing an account name and an asset symbol" );

            start = boost::make_tuple( key[0].as< account_name_type >(), key[1].as< asset_symbol_type >() );
         }

         iterate_results< chain::smt_token_index, chain::by_control_account >(
            start,
            result.tokens,
            args.limit,
            [&]( const smt_token_object& t ) { return api_smt_token_object( t, _db ); },
            &database_api_impl::filter_default< chain::smt_token_object > );

         break;
      }
      default:
         FC_ASSERT( false, "Unknown or unsupported sort order" );
   }

   return result;
}

DEFINE_API_IMPL( database_api_impl, find_smt_tokens )
{
   FC_ASSERT( args.symbols.size() <= DATABASE_API_SINGLE_QUERY_LIMIT );

   find_smt_tokens_return result;
   result.tokens.reserve( args.symbols.size() );

   for( auto& symbol : args.symbols )
   {
      const auto token = chain::util::smt::find_token( _db, symbol, args.ignore_precision );
      if( token != nullptr )
      {
         result.tokens.push_back( api_smt_token_object( *token, _db ) );
      }
   }

   return result;
}

DEFINE_API_IMPL( database_api_impl, list_smt_token_emissions )
{
   FC_ASSERT( args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT );

   list_smt_token_emissions_return result;
   result.token_emissions.reserve( args.limit );

   switch( args.order )
   {
      case( by_symbol_time ):
      {
         auto key = args.start.get_array();
         FC_ASSERT( key.size() == 0 || key.size() == 2, "The parameter 'start' must be an empty array or consist of asset_symbol_type and time_point_sec" );

         boost::tuple< asset_symbol_type, time_point_sec > start;
         if ( key.size() == 0 )
            start = boost::make_tuple( asset_symbol_type(), time_point_sec() );
         else
            start = boost::make_tuple( key[ 0 ].as< asset_symbol_type >(), key[ 1 ].as< time_point_sec >() );

         iterate_results< chain::smt_token_emissions_index, chain::by_symbol_time >(
            start,
            result.token_emissions,
            args.limit,
            &database_api_impl::on_push_default< chain::smt_token_emissions_object >,
            &database_api_impl::filter_default< chain::smt_token_emissions_object > );
         break;
      }
      default:
         FC_ASSERT( false, "Unknown or unsupported sort order" );
   }

   return result;
}

DEFINE_API_IMPL( database_api_impl, find_smt_token_emissions )
{
   find_smt_token_emissions_return result;

   const auto& idx = _db.get_index< chain::smt_token_emissions_index, chain::by_symbol_time >();
   auto itr = idx.lower_bound( args.asset_symbol );

   while( itr != idx.end() && itr->symbol == args.asset_symbol && result.token_emissions.size() <= DATABASE_API_SINGLE_QUERY_LIMIT )
   {
      result.token_emissions.push_back( itr->copy_chain_object() ); //FIXME: exposes internal chain object as API result
      ++itr;
   }

   return result;
}

#endif

DEFINE_LOCKLESS_APIS( database_api, (get_config)(get_version) )

DEFINE_READ_APIS( database_api,
   (get_dynamic_global_properties)
   (get_witness_schedule)
   (get_hardfork_properties)
   (get_reward_funds)
   (get_current_price_feed)
   (get_feed_history)
   (list_witnesses)
   (find_witnesses)
   (list_witness_votes)
   (get_active_witnesses)
   (list_accounts)
   (find_accounts)
   (list_owner_histories)
   (find_owner_histories)
   (list_account_recovery_requests)
   (find_account_recovery_requests)
   (list_change_recovery_account_requests)
   (find_change_recovery_account_requests)
   (list_escrows)
   (find_escrows)
   (list_withdraw_vesting_routes)
   (find_withdraw_vesting_routes)
   (list_savings_withdrawals)
   (find_savings_withdrawals)
   (list_vesting_delegations)
   (find_vesting_delegations)
   (list_vesting_delegation_expirations)
   (find_vesting_delegation_expirations)
   (list_hbd_conversion_requests)
   (find_hbd_conversion_requests)
   (list_decline_voting_rights_requests)
   (find_decline_voting_rights_requests)
   (find_comments)
   (list_votes)
   (find_votes)
   (list_limit_orders)
   (find_limit_orders)
   (list_proposals)
   (find_proposals)
   (list_proposal_votes)
   (get_order_book)
   (get_transaction_hex)
   (get_required_signatures)
   (get_potential_signatures)
   (verify_authority)
   (verify_account_authority)
   (verify_signatures)
#ifdef HIVE_ENABLE_SMT
   (get_nai_pool)
   (list_smt_contributions)
   (find_smt_contributions)
   (list_smt_tokens)
   (find_smt_tokens)
   (list_smt_token_emissions)
   (find_smt_token_emissions)
#endif
)

} } } // hive::plugins::database_api
