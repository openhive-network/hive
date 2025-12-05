#pragma once

#include <hive/chain/hive_fwd.hpp>

#include <appbase/application.hpp>

#include <hive/plugins/database_api/database_api.hpp>
#include <hive/plugins/database_api/database_api_plugin.hpp>

#include <hive/protocol/get_config.hpp>
#include <hive/protocol/exceptions.hpp>
#include <hive/protocol/transaction_util.hpp>

#include <hive/plugins/metadata_api/metadata_api_plugin.hpp>

#include <hive/chain/database.hpp>
#include <hive/chain/dhf_objects.hpp>

namespace hive { namespace plugins { namespace database_api {

using namespace hive::chain;
using namespace hive::protocol;

class database_api_impl
{
  public:
    database_api_impl( appbase::application& app );
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
      (list_collateralized_conversion_requests)
      (find_collateralized_conversion_requests)
      (list_decline_voting_rights_requests)
      (find_decline_voting_rights_requests)
      (get_comment_pending_payouts)
      (list_limit_orders)
      (find_limit_orders)
      (get_order_book)
      (list_proposals)
      (find_proposals)
      (list_proposal_votes)
      (find_recurrent_transfers)
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
      (is_known_transaction)
    )

    template< typename ApiResultType, typename ResultType >
    static ApiResultType on_push_default( const ResultType& r, const database& db ) { return ApiResultType( r, db ); }

    template< typename ValueType >
    static bool filter_default( const ValueType& r ) { return true; }

    template<typename ResultType, typename OnPushType, typename FilterType, typename IteratorType >
    void iteration_loop(
      IteratorType iter,
      IteratorType end_iter,
      std::vector<ResultType>& result,
      uint32_t limit,
      OnPushType&& on_push,
      FilterType&& filter )
    {
      while ( result.size() < limit && iter != end_iter )
      {
        if( filter( *iter ) )
          result.emplace_back( on_push( *iter, _db ) );
        ++iter;
      }
    }

    template<typename IndexType, typename OrderType, typename ResultType, typename OnPushType, typename FilterType>
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

        iteration_loop< ResultType, OnPushType, FilterType >( itr, end, result, limit, std::forward<OnPushType>(on_push), std::forward<FilterType>(filter) );
      }
      else if( direction == descending )
      {
        auto index_it = idx.iterator_to(*(_db.get_index<IndexType, hive::chain::by_id>().upper_bound( id )));
        auto iter  = boost::make_reverse_iterator( index_it );
        auto iter_end = boost::make_reverse_iterator( idx.begin() );

        iteration_loop< ResultType, OnPushType, FilterType >( iter, iter_end, result, limit, std::forward<OnPushType>(on_push), std::forward<FilterType>(filter) );
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
        iterate_results_from_index< IndexType, OrderType >( *last_index, result, limit, std::forward<OnPushType>(on_push), std::forward<FilterType>(filter), direction );
        return;
      }

      const auto& idx = _db.get_index< IndexType, OrderType >();
      if( direction == ascending )
      {
        auto itr = idx.lower_bound( start );
        auto end = idx.end();

        iteration_loop< ResultType, OnPushType, FilterType >( itr, end, result, limit, std::forward<OnPushType>(on_push), std::forward<FilterType>(filter) );
      }
      else if( direction == descending )
      {
        auto iter = boost::make_reverse_iterator( idx.upper_bound(start) );
        auto end_iter = boost::make_reverse_iterator( idx.begin() );

        iteration_loop< ResultType, OnPushType, FilterType >( iter, end_iter, result, limit, std::forward<OnPushType>(on_push), std::forward<FilterType>(filter) );
      }
    }

    template<typename IndexType, typename OrderType, typename ResultType, typename OnPushType, typename FilterType>
    void iterate_results_no_start(
      std::vector<ResultType>& result,
      uint32_t limit,
      OnPushType&& on_push,
      FilterType&& filter,
      order_direction_type direction = ascending,
      fc::optional<uint64_t> last_index = fc::optional<uint64_t>()
    )
    {
      if( last_index.valid() )
      {
        iterate_results_from_index< IndexType, OrderType >( *last_index, result, limit, std::forward<OnPushType>(on_push), std::forward<FilterType>(filter), direction );
        return;
      }

      const auto& idx = _db.get_index< IndexType, OrderType >();
      if( direction == ascending )
      {
        auto itr = idx.begin();
        auto end = idx.end();

        iteration_loop< ResultType, OnPushType, FilterType >( itr, end, result, limit, std::forward<OnPushType>(on_push), std::forward<FilterType>(filter) );
      }
      else if( direction == descending )
      {
        auto iter = boost::make_reverse_iterator( idx.end() );
        auto end_iter = boost::make_reverse_iterator( idx.begin() );

        iteration_loop< ResultType, OnPushType, FilterType >( iter, end_iter, result, limit, std::forward<OnPushType>(on_push), std::forward<FilterType>(filter) );
      }
    }

    chain::database& _db;

    std::shared_ptr< metadata::metadata_api > _metadata_api;
};

// Helper function for proposal status - used by api_proposal_object constructor and content filtering
proposal_status get_proposal_status( const chain::proposal_object& po, const fc::time_point_sec current_time );

} } } // hive::plugins::database_api
