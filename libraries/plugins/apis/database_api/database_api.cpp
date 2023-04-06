#include <hive/chain/hive_fwd.hpp>

#include <appbase/application.hpp>

#include <hive/plugins/database_api/database_api.hpp>
#include <hive/plugins/database_api/database_api_plugin.hpp>

#include <hive/protocol/get_config.hpp>
#include <hive/protocol/exceptions.hpp>
#include <hive/protocol/transaction_util.hpp>
#include <hive/protocol/forward_impacted.hpp>

#include <hive/chain/util/smt_token.hpp>

#include <hive/utilities/git_revision.hpp>

namespace hive { namespace app {
std::shared_ptr<hive::chain::full_block_type> from_variant_to_full_block_ptr(const fc::variant& v, int block_num_debug );
}}

bool czy_printowac(int block_num);

namespace hive { namespace plugins { namespace database_api {

api_commment_cashout_info::api_commment_cashout_info(const comment_cashout_object& cc, const database&)
{
  total_vote_weight = cc.get_total_vote_weight();
  reward_weight = HIVE_100_PERCENT; // since HF17 reward is not limited if posts are too frequent
  total_payout_value = HBD_asset(); // since HF19 it was either default 0 or cc did not exist
  curator_payout_value = HBD_asset(); // since HF19 it was either default 0 or cc did not exist
  author_rewards = 0; // since HF19 author_rewards was either default 0 or cc did not exist
  net_votes = cc.get_net_votes();
  last_payout = time_point_sec::min(); // since HF17 there is only one payout and cc does not exist after HF19
  net_rshares = cc.get_net_rshares();
  abs_rshares = 0; // value was only used for comments created before HF6 (and to recognize that there are votes)
  vote_rshares = cc.get_vote_rshares();
  children_abs_rshares = 0; // value not accumulated after HF17
  cashout_time = cc.get_cashout_time();
  max_cashout_time = time_point_sec::maximum(); // since HF17 it is the only possible value
  max_accepted_payout = cc.get_max_accepted_payout();
  percent_hbd = cc.get_percent_hbd();
  allow_votes = cc.allows_votes();
  allow_curation_rewards = cc.allows_curation_rewards();
  was_voted_on = cc.has_votes();
}


class database_api_impl
{
  public:
    database_api_impl();
    database_api_impl(chain::database& a_db);
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
      (find_comments)
      (list_votes)
      (find_votes)
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


database_api_impl::database_api_impl(chain::database& a_db)
  : _db( a_db) {}



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
  fc::mutable_variant_object version_storage;
  hive::utilities::build_version_info(&version_storage);

  version_storage.set("chain_id", std::string(_db.get_chain_id()));

  return version_storage;
}

DEFINE_API_IMPL( database_api_impl, get_dynamic_global_properties )
{
  return api_dynamic_global_property_object( _db.get_dynamic_global_properties(), _db );
}

#define FILL_FIELD(field) if( active.field != future.field ) { filled = true; field = future.field; }

bool future_chain_properties::fill( const chain_properties& active, const chain_properties& future )
{
  bool filled = false;
  FILL_FIELD( account_creation_fee );
  FILL_FIELD( maximum_block_size );
  FILL_FIELD( hbd_interest_rate );
  FILL_FIELD( account_subsidy_budget );
  FILL_FIELD( account_subsidy_decay );
  return filled;
}

bool future_witness_schedule::fill( const witness_schedule_object& active, const witness_schedule_object& future )
{
  bool filled = false;
  FILL_FIELD( num_scheduled_witnesses );
  FILL_FIELD( elected_weight );
  FILL_FIELD( timeshare_weight );
  FILL_FIELD( miner_weight );
  FILL_FIELD( witness_pay_normalization_factor );
  median_props = future_chain_properties();
  if( !median_props->fill( active.median_props, future.median_props ) )
    median_props.reset();
  else
    filled = true;
  FILL_FIELD( majority_version );
  FILL_FIELD( max_voted_witnesses );
  FILL_FIELD( max_miner_witnesses );
  FILL_FIELD( max_runner_witnesses );
  FILL_FIELD( hardfork_required_witnesses );
  FILL_FIELD( account_subsidy_rd );
  FILL_FIELD( account_subsidy_witness_rd );
  FILL_FIELD( min_witness_account_subsidy_decay );
  return filled;
}

#undef FILL_FIELD

DEFINE_API_IMPL( database_api_impl, get_witness_schedule )
{
  FC_ASSERT( _db.has_hardfork( HIVE_HARDFORK_1_26 ) || !args.include_future, "Future witnesses only become available after HF26" );
  const auto& wso = _db.get_witness_schedule_object();
  const auto& future_wso = _db.has_hardfork( HIVE_HARDFORK_1_26 ) ? _db.get_future_witness_schedule_object() : wso;
  get_witness_schedule_return result( wso, future_wso, args.include_future, _db );
  return result;
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
    result.funds.emplace_back( *itr, _db );
    ++itr;
  }

  return result;
}

DEFINE_API_IMPL( database_api_impl, get_current_price_feed )
{
  return _db.get_feed_history().current_median_history;
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
  FC_ASSERT( 0 < args.limit && args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT, "limit not set or too big" );

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
        &database_api_impl::on_push_default< api_witness_object, witness_object >,
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
        &database_api_impl::on_push_default< api_witness_object, witness_object >,
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
      auto key = args.start.as< std::pair< account_name_type, account_name_type > >();
      iterate_results< chain::witness_vote_index, chain::by_account_witness >(
        boost::make_tuple( key.first, key.second ),
        result.votes,
        args.limit,
        &database_api_impl::on_push_default< api_witness_vote_object, witness_vote_object >,
        &database_api_impl::filter_default< witness_vote_object > );
      break;
    }
    case( by_witness_account ):
    {
      auto key = args.start.as< std::pair< account_name_type, account_name_type > >();
      iterate_results< chain::witness_vote_index, chain::by_witness_account >(
        boost::make_tuple( key.first, key.second ),
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
      iterate_results< chain::account_index, chain::by_name >(
        args.start.as< protocol::account_name_type >(),
        result.accounts,
        args.limit,
        [&]( const account_object& a, const database& db ){ return api_account_object( a, db, args.delayed_votes_active ); },
        &database_api_impl::filter_default< account_object > );
      break;
    }
    case( by_proxy ):
    {
      auto key = args.start.as< std::pair< account_name_type, account_name_type > >();
      account_id_type proxy_id;
      if( key.first != HIVE_PROXY_TO_SELF_ACCOUNT )
      {
        const auto* proxy = _db.find_account( key.first );
        FC_ASSERT( proxy != nullptr, "Given proxy account does not exist." );
        proxy_id = proxy->get_id();
      }
      iterate_results< chain::account_index, chain::by_proxy >(
        boost::make_tuple( proxy_id, key.second ),
        result.accounts,
        args.limit,
        [&]( const account_object& a, const database& db ){ return api_account_object( a, db, args.delayed_votes_active ); },
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
        [&]( const account_object& a, const database& db ){ return api_account_object( a, db, args.delayed_votes_active ); },
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
      result.accounts.emplace_back( *acct, _db, args.delayed_votes_active );
  }

  return result;
}


/* Owner Auth Histories */

DEFINE_API_IMPL( database_api_impl, list_owner_histories )
{
  FC_ASSERT( 0 < args.limit && args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT, "limit not set or too big" );

  list_owner_histories_return result;
  result.owner_auths.reserve( args.limit );

  auto key = args.start.as< std::pair< account_name_type, fc::time_point_sec > >();
  iterate_results< chain::owner_authority_history_index, chain::by_account >(
    boost::make_tuple( key.first, key.second ),
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
      iterate_results< chain::account_recovery_request_index, chain::by_account >(
        args.start.as< account_name_type >(),
        result.requests,
        args.limit,
        &database_api_impl::on_push_default< api_account_recovery_request_object, account_recovery_request_object >,
        &database_api_impl::filter_default< account_recovery_request_object > );
      break;
    }
    case( by_expiration ):
    {
      auto key = args.start.as< std::pair< fc::time_point_sec, account_name_type > >();
      iterate_results< chain::account_recovery_request_index, chain::by_expiration >(
        boost::make_tuple( key.first, key.second ),
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
      iterate_results< chain::change_recovery_account_request_index, chain::by_account >(
        args.start.as< account_name_type >(),
        result.requests,
        args.limit,
        &database_api_impl::on_push_default< api_change_recovery_account_request_object, change_recovery_account_request_object >,
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
      auto key = args.start.as< std::pair< account_name_type, uint32_t > >();
      iterate_results< chain::escrow_index, chain::by_from_id >(
        boost::make_tuple( key.first, key.second ),
        result.escrows,
        args.limit,
        &database_api_impl::on_push_default< api_escrow_object, escrow_object >,
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

  while( itr != escrow_idx.end() && itr->from == args.from && result.escrows.size() <= DATABASE_API_SINGLE_QUERY_LIMIT )
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
      auto key = args.start.as< std::pair< account_name_type, account_name_type > >();
      iterate_results< chain::withdraw_vesting_route_index, chain::by_withdraw_route >(
        boost::make_tuple( key.first, key.second ),
        result.routes,
        args.limit,
        &database_api_impl::on_push_default< api_withdraw_vesting_route_object, withdraw_vesting_route_object >,
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
      auto key = args.start.as< std::pair< account_name_type, uint32_t > >();
      iterate_results< chain::savings_withdraw_index, chain::by_from_rid >(
        boost::make_tuple( key.first, key.second ),
        result.withdrawals,
        args.limit,
        &database_api_impl::on_push_default< api_savings_withdraw_object, savings_withdraw_object >,
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
        &database_api_impl::on_push_default< api_savings_withdraw_object, savings_withdraw_object >,
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

  while( itr != withdraw_idx.end() && itr->from == args.account && result.withdrawals.size() <= DATABASE_API_SINGLE_QUERY_LIMIT )
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
      iterate_results< chain::vesting_delegation_index, chain::by_delegation >(
        boost::make_tuple( delegator->get_id(), delegatee_id ),
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
      auto key = args.start.as< std::pair< time_point_sec, vesting_delegation_expiration_id_type > >();
      iterate_results< chain::vesting_delegation_expiration_index, chain::by_expiration >(
        boost::make_tuple( key.first, key.second ),
        result.delegations,
        args.limit,
        &database_api_impl::on_push_default< api_vesting_delegation_expiration_object, vesting_delegation_expiration_object >,
        &database_api_impl::filter_default< vesting_delegation_expiration_object > );
      break;
    }
    case( by_account_expiration ):
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
      iterate_results< chain::vesting_delegation_expiration_index, chain::by_account_expiration >(
        boost::make_tuple( delegator_id, key[1].as< time_point_sec >(), key[2].as< vesting_delegation_expiration_id_type >() ),
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
      auto key = args.start.as< std::pair< time_point_sec, convert_request_id_type > >();
      iterate_results< chain::convert_request_index, chain::by_conversion_date >(
        boost::make_tuple( key.first, key.second ),
        result.requests,
        args.limit,
        &database_api_impl::on_push_default< api_convert_request_object, convert_request_object >,
        &database_api_impl::filter_default< convert_request_object > );
      break;
    }
    case( by_account ):
    {
      auto key = args.start.as< std::pair< account_name_type, uint32_t > >();
      account_id_type owner_id;
      if( key.first != "" )
      {
        const auto* owner = _db.find_account( key.first );
        FC_ASSERT( owner != nullptr, "Given account does not exist." );
        owner_id = owner->get_id();
      }
      iterate_results< chain::convert_request_index, chain::by_owner >(
        boost::make_tuple( owner_id, key.second ),
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
      auto key = args.start.as< std::pair< time_point_sec, collateralized_convert_request_id_type > >();
      iterate_results< chain::collateralized_convert_request_index, chain::by_conversion_date >(
        boost::make_tuple( key.first, key.second ),
        result.requests,
        args.limit,
        &database_api_impl::on_push_default< api_collateralized_convert_request_object, collateralized_convert_request_object >,
        &database_api_impl::filter_default< collateralized_convert_request_object > );
      break;
    }
    case( by_account ):
    {
      auto key = args.start.as< std::pair< account_name_type, uint32_t > >();
      account_id_type owner_id;
      if( key.first != "" )
      {
        const auto* owner = _db.find_account( key.first );
        FC_ASSERT( owner != nullptr, "Given account does not exist." );
        owner_id = owner->get_id();
      }
      iterate_results< chain::collateralized_convert_request_index, chain::by_owner >(
        boost::make_tuple( owner_id, key.second ),
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
      iterate_results< chain::decline_voting_rights_request_index, chain::by_account >(
        args.start.as< account_name_type >(),
        result.requests,
        args.limit,
        &database_api_impl::on_push_default< api_decline_voting_rights_request_object, decline_voting_rights_request_object >,
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
    const comment_object* comment = _db.find_comment(key.first, key.second);
    if(comment != nullptr)
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
  FC_ASSERT( 0 < args.comments.size() && args.comments.size() <= DATABASE_API_SINGLE_QUERY_LIMIT,
    "list of comments to find not filled or too big" );
  find_comments_return result;
  result.comments.reserve( args.comments.size() );

  for( auto& key: args.comments )
  {
    auto comment = _db.find_comment( key.first, key.second );

    if( comment != nullptr )
      result.comments.emplace_back( *comment, _db );
  }

  return result;
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
// Market                                                           //
//                                                                  //
//////////////////////////////////////////////////////////////////////

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
      auto key = args.start.as< std::pair< price, limit_order_id_type > >();
      iterate_results< chain::limit_order_index, chain::by_price >(
        boost::make_tuple( key.first, key.second ),
        result.orders,
        args.limit,
        &database_api_impl::on_push_default< api_limit_order_object, limit_order_object >,
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

  while( itr != order_idx.end() && itr->seller == args.account && result.orders.size() <= DATABASE_API_SINGLE_QUERY_LIMIT )
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
    result.bids.emplace_back( std::move( cur ) );
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
    result.asks.emplace_back( std::move( cur ) );
    ++buy_itr;
  }

  return result;
}

//////////////////////////////////////////////////////////////////////
//                                                                  //
// DHF                                                              //
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

/* Find recurrent transfers   */

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
  result.keys = args.trx.get_required_signatures(
    _db.get_chain_id(),
    args.available_keys,
    [&]( string account_name ){ return authority( _db.get< chain::account_authority_object, chain::by_account >( account_name ).active  ); },
    [&]( string account_name ){ return authority( _db.get< chain::account_authority_object, chain::by_account >( account_name ).owner   ); },
    [&]( string account_name ){ return authority( _db.get< chain::account_authority_object, chain::by_account >( account_name ).posting ); },
    [&]( string witness_name ){ return _db.get_witness(witness_name).signing_key; }, // note: reflect any changes here in database::apply_transaction
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
    [&]( account_name_type witness_name )
    {
      return _db.get_witness(witness_name).signing_key;
    },
    HIVE_MAX_SIG_CHECK_DEPTH,
    _db.has_hardfork( HIVE_HARDFORK_0_20__1944 ) ? fc::ecc::canonical_signature_type::bip_0062 : fc::ecc::canonical_signature_type::fc_canonical
  );

  return result;
}

DEFINE_API_IMPL( database_api_impl, verify_authority )
{
  args.trx.verify_authority(
    _db.get_chain_id(),
    [&]( string account_name ){ return authority( _db.get< chain::account_authority_object, chain::by_account >( account_name ).active  ); },
    [&]( string account_name ){ return authority( _db.get< chain::account_authority_object, chain::by_account >( account_name ).owner   ); },
    [&]( string account_name ){ return authority( _db.get< chain::account_authority_object, chain::by_account >( account_name ).posting ); },
    [&]( string witness_name ){ return _db.get_witness(witness_name).signing_key; }, // note: reflect any changes here in database::apply_transaction
    args.pack,
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
  op.from = account->get_name();
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
      [this]( string witness_name ){ return _db.get_witness(witness_name).signing_key; }, // note: reflect any changes here in database::apply_transaction
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
  FC_ASSERT( 0 < args.limit && args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT, "limit not set or too big" );

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
        &database_api_impl::on_push_default< api_smt_contribution_object, chain::smt_contribution_object >,
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
        &database_api_impl::on_push_default< api_smt_contribution_object, chain::smt_contribution_object >,
        &database_api_impl::filter_default< chain::smt_contribution_object > );
      break;
    }
// #ifndef IS_LOW_MEM // indexing by contributor might cause optimization problems in the future
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
        &database_api_impl::on_push_default< api_smt_contribution_object, chain::smt_contribution_object >,
        &database_api_impl::filter_default< chain::smt_contribution_object > );
      break;
    }
// #endif
    default:
      FC_ASSERT( false, "Unknown or unsupported sort order '${o}'", ( "o", args.order ) );
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
      result.contributions.emplace_back( *itr, _db );
      ++itr;
    }
  }

  return result;
}

DEFINE_API_IMPL( database_api_impl, list_smt_tokens )
{
  FC_ASSERT( 0 < args.limit && args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT, "limit not set or too big" );

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
        &database_api_impl::on_push_default< api_smt_token_object, chain::smt_token_object >,
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
        &database_api_impl::on_push_default< api_smt_token_object, chain::smt_token_object >,
        &database_api_impl::filter_default< chain::smt_token_object > );

      break;
    }
    default:
      FC_ASSERT( false, "Unknown or unsupported sort order '${o}'", ( "o", args.order ) );
  }

  return result;
}

DEFINE_API_IMPL( database_api_impl, find_smt_tokens )
{
  FC_ASSERT( 0 < args.symbols.size() && args.symbols.size() <= DATABASE_API_SINGLE_QUERY_LIMIT,
    "list of tokens to find not filled or too big" );

  find_smt_tokens_return result;
  result.tokens.reserve( args.symbols.size() );

  for( auto& symbol : args.symbols )
  {
    const auto token = chain::util::smt::find_token( _db, symbol, args.ignore_precision );
    if( token != nullptr )
    {
      result.tokens.emplace_back( *token, _db );
    }
  }

  return result;
}

DEFINE_API_IMPL( database_api_impl, list_smt_token_emissions )
{
  FC_ASSERT( 0 < args.limit && args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT, "limit not set or too big" );

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
        &database_api_impl::on_push_default< api_smt_token_emissions_object, chain::smt_token_emissions_object >,
        &database_api_impl::filter_default< chain::smt_token_emissions_object > );
      break;
    }
    default:
      FC_ASSERT( false, "Unknown or unsupported sort order '${o}'", ( "o", args.order ) );
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
    result.token_emissions.emplace_back( *itr, _db );
    ++itr;
  }

  return result;
}

#endif

DEFINE_API_IMPL( database_api_impl, is_known_transaction )
{
  is_known_transaction_return result;
  result.is_known = _db.is_known_transaction(args.id);
  return result;
}

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
  (list_collateralized_conversion_requests)
  (find_collateralized_conversion_requests)
  (list_decline_voting_rights_requests)
  (find_decline_voting_rights_requests)
  (get_comment_pending_payouts)
  (find_comments)
  (list_votes)
  (find_votes)
  (list_limit_orders)
  (find_limit_orders)
  (list_proposals)
  (find_proposals)
  (list_proposal_votes)
  (get_order_book)
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

} } } // hive::plugins::database_api





#include <iostream>
#include <string>
#include <pqxx/pqxx>


#include <fc/variant.hpp>
#include <fc/io/json.hpp>
#include <fc/io/sstream.hpp>
#include <hive/protocol/operations.hpp>

#include <../../../apis/block_api/include/hive/plugins/block_api/block_api_objects.hpp>



namespace hive { namespace app {

auto get_context_shared_data_bin_dir()
{

    fc::path data_dir;
    char* parent = getenv( "PGDATA" );

    system("env");

    if( parent != nullptr )
    {
      data_dir = std::string( parent );
      data_dir = data_dir.parent_path();
      data_dir = data_dir.parent_path();
    }
    return data_dir;
}

}
}


void init(hive::chain::database& db, const char* context)
{


  db.set_flush_interval( 10'000 );//10 000
  db.set_require_locking( false );// false 


  hive::chain::open_args db_open_args;
  db_open_args.data_dir = "/home/dev/mainnet-5m";
  db_open_args.data_dir = "/home/dev/.consensus_state_provider";

  ilog("mtlk db_open_args.data_dir=${dd}",("dd", db_open_args.data_dir));

  db_open_args.data_dir = hive::app::get_context_shared_data_bin_dir();

  db_open_args.shared_mem_dir =  db_open_args.data_dir /  "blockchain"; // "/home/dev/mainnet-5m/blockchain"
  db_open_args.initial_supply = HIVE_INIT_SUPPLY; // 0
  db_open_args.hbd_initial_supply = HIVE_HBD_INIT_SUPPLY;// 0

  db_open_args.shared_file_size = 25769803776;  //my->shared_memory_size = fc::parse_size( options.at( "shared-file-size" ).as< string >() );

  db_open_args.shared_file_full_threshold = 0;// 0
  db_open_args.shared_file_scale_rate = 0;// 0
  db_open_args.chainbase_flags = 0;// 0
  db_open_args.do_validate_invariants = false; // false
  db_open_args.stop_replay_at = 0;//0
  db_open_args.exit_after_replay = false;//false
  db_open_args.validate_during_replay = false;// false
  db_open_args.benchmark_is_enabled = false;//false
  db_open_args.replay_in_memory = false;// false
  db_open_args.enable_block_log_compression = true;// true
  db_open_args.block_log_compression_level = 15;// 15

  db_open_args.postgres_not_block_log = true;

  db_open_args.force_replay = false;// false

  db.open( db_open_args, context );

}


namespace{
 std::unordered_map <std::string,  hive::plugins::database_api::database_api_impl> haf_database_api_impls;
}


int initialize_context(const char* context)
{
  if(haf_database_api_impls.find(context) == haf_database_api_impls.end())
  {
    hive::chain::database* db = new hive::chain::database;
    init(*db, context);
    haf_database_api_impls.emplace(std::make_pair(std::string(context), hive::plugins::database_api::database_api_impl(*db)));
    return db->head_block_num() + 1;
  }
  else
  {
    hive::plugins::database_api::database_api_impl& db_api_impl = haf_database_api_impls[context];
    hive::chain::database& db = db_api_impl._db;
    return db.head_block_num() + 1;
  }
}

namespace hive { namespace app {

int get_expected_block_num_impl(const char* context)
{
  return initialize_context(context);
}



void cab_destroy_C_impl(const char* context)
{
  if(haf_database_api_impls.find(context) != haf_database_api_impls.end())
  {
      hive::plugins::database_api::database_api_impl& db_api_impl = haf_database_api_impls[context];
      hive::chain::database& db = db_api_impl._db;
      db.close();
      db. chainbase::database::wipe( get_context_shared_data_bin_dir()  /  "blockchain" , context);
      haf_database_api_impls.erase(context);
  }
}

int char2bin(char c)
{
    switch(c)
    {
        case '0': return 0; break;
        case '1': return 1; break;
        case '2': return 2; break;
        case '3': return 3; break;
        case '4': return 4; break;
        case '5': return 5; break;
        case '6': return 6; break;
        case '7': return 7; break;
        case '8': return 8; break;
        case '9': return 9; break;
        case 'a': return 10; break;
        case 'b': return 11; break;
        case 'c': return 12; break;
        case 'd': return 13; break;
        case 'e': return 14; break;
        case 'f': return 15; break;
        default:
            throw std::exception();
    }
}



///////
static auto volatile stop_in_consume_json_block_impl = false;



int consume_json_block_impl(const char *json_block, const char* context, int block_num)
{

  static auto first_time = true;
  if(first_time)
  {
    first_time = false;
    wlog("mtlk consume_json_block_impl pid= ${pid}", ("pid", getpid()));
  }



  // if(1094 == block_num)
  // {
  //   wlog("mtlk consume_json_block_impl pid= ${pid}", ("pid", getpid()));
  //   while(stop_in_consume_json_block_impl)
  //   {
  //       int a = 0;
  //       a=a;
  //   }

  // }
  // if(1093 == block_num)
  // {
  //   wlog("mtlk consume_json_block_impl pid= ${pid}", ("pid", getpid()));
  //   while(stop_in_consume_json_block_impl)
  //   {
  //       int a = 0;
  //       a=a;
  //   }

  // }


    while(stop_in_consume_json_block_impl)
    {
        int a = 0;
        a=a;
    }

  if(994240 == block_num)
  {
    wlog("mtlk consume_json_block_impl pid= ${pid}", ("pid", getpid()));
    while(stop_in_consume_json_block_impl)
    {
        int a = 0;
        a=a;
    }

  }




  int expected_block_num = initialize_context(context);

  if(block_num != expected_block_num)
     return expected_block_num;

  expected_block_num++;


  std::string s(context);
  hive::plugins::database_api::database_api_impl& db_api_impl = haf_database_api_impls[s];
  hive::chain::database& db = db_api_impl._db;
  
  std::string json = std::string{ json_block };

  if(czy_printowac(block_num))
   {
   
    wlog("In block=${block_num}", ("block_num", block_num));
    wlog("json=${json}", ("json",std::string{ json_block } ));
   }  

//// block 000f2bc0 = F2BC0₁₆ = 994240₁₀
// json=R"""({
//     "previous": "000f2bbfcbdad7bb80bc42c476567c750badd90b",
//     "timestamp": "2016-04-28T23:22:18",
//     "witness": "steemychicken1",
//     "transaction_merkle_root": "8195f855f6d54d6856f9a08a08b21bdd6b14472e",
//     "extensions": [],
//     "witness_signature": "1f2ce40298d1569ba0f118146f71482ff18afdb02d844321addf3ebd7e970d65a625d4cb65aae4a9c884f896f0c2c6939603287997377d1f2d440d6f09825aab97",
//     "transactions": [
//         {
//             "ref_block_num": 11199,
//             "ref_block_prefix": 3151485643,
//             "expiration": "2016-04-28T23:22:45",
//             "operations": [
//                 {
//                     "type": "witness_update_operation",
//                     "value": {
//                         "owner": "liondani",
//                         "url": "https://bitcointalk.org/index.php?topic=1410943.msg14643675#msg14643675",
//                         "block_signing_key": "STM66crmnX96YMyWUPnFzSkv7DsGESbWaoSGbmoNq4EWDYq2wnwPf",
//                         "props": {
//                             "account_creation_fee": {
//                                 "amount": "1",
//                                 "precision": 3,
//                                 "nai": "@@000000021"
//                             },
//                             "maximum_block_size": 131072,
//                             "hbd_interest_rate": 1000
//                         },
//                         "fee": {
//                             "amount": "0",
//                             "precision": 3,
//                             "nai": "@@000000021"
//                         }
//                     }
//                 }
//             ],
//             "extensions": [],
//             "signatures": [
//                 "1f1fd68fd2b2ec919357c8e534d1473a16f5505a7719bbb2a2100478f212f9353272c38b12d1468d1c3eb830f659e80a28a5a985661bb2c79e4acd2ef34bc919b4"
//             ]
//         }
//     ],
//     "block_id": "000f2bc023d45d15a543cb9bb19b56c25012d15c",
//     "signing_key": "STM6Wf68LVi22QC9eS8LBWykRiSrKKp5RTWXcNqjh3VPNhiT9xFxx",
//     "transaction_ids": [
//         "5cf59b8b633f8e894adef4c8ccd6e2353040ff70"
//     ]
// })""";
  


// //In block=3705111
// json=R"""({
// "witness": "witness.svk",
// "block_id": "00388917fe8c0184c499e115d9ee3bf0cf6825bb",
// "previous": "00388916318d804f9371fe551732d707617c69e4",
// "timestamp": "2016-08-01T14:56:00",
// "extensions": [],
// "signing_key": "STM6vxp7zj1SRdd6QqbQKabjvM1xT1tURT8rNZf5qFHswEkjpYK3x",
// "transactions": [
//     {
//         "expiration": "2016-08-01T14:56:24",
//         "extensions": [],
//         "operations": [
//             {
//                 "type": "account_update_operation",
//                 "value": {
//                     "account": "jesus2",
//                     "posting": {
//                         "key_auths": [
//                             [
//                                 "STM7xqXUoCzjB7BXvzE7hCjVcLpgpEVMy3ALy3ygCFX3opkmuhDNd",
//                                 1
//                             ]
//                         ],
//                         "account_auths": [
//                             [
//                                 "STM6GVCS7JeftT27",
//                                 1
//                             ]
//                         ],
//                         "weight_threshold": 1
//                     },
//                     "memo_key": "STM8Mf3UvSfnoGtBsxi3SjaHYWuUxYgJUJKHWeMsJcLycH6jYMrM5",
//                     "json_metadata": ""
//                 }
//             }
//         ],
//         "signatures": [
//             "201123be78b9ee9560e60b90b04712e87e89655c9b6419e32b9b0e715fa651ea8078b09182f2ea09bf0e00b80c3a4dea7a081ddf69972a288fbfc0d30c5ad68a14"
//         ],
//         "ref_block_num": 35093,
//         "ref_block_prefix": 3914984601
//     },
//     {
//         "expiration": "2016-08-01T14:56:12",
//         "extensions": [],
//         "operations": [
//             {
//                 "type": "vote_operation",
//                 "value": {
//                     "voter": "kalimor",
//                     "author": "domino",
//                     "weight": 10000,
//                     "permlink": "re-lehard-steemit-dlya-chainikov-5-prostykh-shagov-ot-novichka-do-steemian-80-lvl-50-poleznykh-ssylok-2-redakciya-20160721t084441610z"
//                 }
//             }
//         ],
//         "signatures": [
//             "2030862252eb295c804e11e7ba40a9c02b9cfe8178522bc9ef22b8134b999da0c139d447752aefb29ee6f7caf310f2a052071a19fe9db07c4d2dbff69dbf7e31d4"
//         ],
//         "ref_block_num": 35067,
//         "ref_block_prefix": 2222533022
//     }
// ],
// "transaction_ids": [
//     "45c95c0ee2225a98622fc2cf38cdda7de0958098",
//     "23287b6f9a60b7a59c78f98d7a16cfdef4473dab"
// ],
// "witness_signature": "205094b547050ea59b5419792efbc633bdaac77f88e98c5927edb645bc642e7ee55254d4a9f51b77587d74eeb894ec96b2e6405a2d19fac2ea02b61dcec25f0b3f",
// "transaction_merkle_root": "e6ad5676bf6cb1a393376fd4d15f43e9e76a535f"
// })""";

  fc::variant v = fc::json::from_string( json );

  std::shared_ptr<hive::chain::full_block_type> fb_ptr = from_variant_to_full_block_ptr(v, block_num);

  if(czy_printowac(block_num))
  {
    wlog("signed_block from full_block=${signed_block}", ("signed_block", fb_ptr->get_block()));
  }  



  uint64_t skip_flags = hive::plugins::chain::database::skip_block_log;
  // skip_flags |= hive::plugins::chain::database::skip_validate_invariants;
  
  //skip_flags |= hive::plugins::chain::database::skip_witness_signature ; //try not to skip it mtlk 
  // skip_flags |= hive::plugins::chain::database::skip_transaction_signatures;
  // skip_flags |= hive::plugins::chain::database::skip_transaction_dupe_check;
  //skip_flags |= hive::plugins::chain::database::skip_tapos_check; //try not to skip it mtlk 
  //skip_flags |= hive::plugins::chain::database::skip_merkle_check;//try not to skip it mtlk 
  // skip_flags |= hive::plugins::chain::database::skip_witness_schedule_check;
  //skip_flags |= hive::plugins::chain::database::skip_authority_check;//try not to skip it mtlk 
  // skip_flags |= hive::plugins::chain::database::skip_validate;

  db.set_tx_status( hive::plugins::chain::database::TX_STATUS_BLOCK );


  db.public_apply_block(fb_ptr, skip_flags);

  db.clear_tx_status();

  db.set_revision( db.head_block_num() );


  return expected_block_num;
}


collected_account_balances_collection_t collect_current_all_accounts_balances(const char* context)
{
  wlog("mtlk inside  pid=${pid}", ("pid", getpid()));


  hive::plugins::database_api::database_api_impl& db_api_impl = haf_database_api_impls[context];



  hive::plugins::database_api::list_accounts_args args;
  
  collected_account_balances_collection_t r;
 
  
  args.start = "";
  args.limit = 1000;
  args.order = hive::plugins::database_api::by_name;


  while(true)
  { 
    hive::plugins::database_api::list_accounts_return db_api_impl_result = db_api_impl.list_accounts(args);
    if(db_api_impl_result.accounts.empty())
      break;

    decltype(args.limit) cnt = 0;
    for(const auto& a : db_api_impl_result.accounts)
    {
      if((cnt == 0) && (args.start != ""))
      {
        cnt++;
        continue;
      }

      collected_account_balances_t e;
      e.account_name = a.name;

      e.balance = a.balance.amount.value;
      e.hbd_balance = a.hbd_balance.amount.value;
      e.vesting_shares = a.vesting_shares.amount.value;
      e.savings_hbd_balance = a.savings_hbd_balance.amount.value;
      e.reward_hbd_balance = a.reward_hbd_balance.amount.value;
      r.emplace_back(e);
      cnt++;
    }



    args.start = db_api_impl_result.accounts[db_api_impl_result.accounts.size() - 1].name;


    if(cnt < args.limit)
      break;

  }

  return r;
}

void sanity_check(const char *&postgres_url) 
{
    pqxx::connection c{postgres_url};
    pqxx::work txn{c};
    pqxx::result r{txn.exec("SELECT COUNT(*) FROM hive.blocks")};

    std::cout << "mtlk columns count=" << r.columns() << "\n";
    std::cout << "mtlk column name=" << r.column_name(0) << "\n";
    std::cout << "mtlk rows  count=" << r.size() << "\n";

    for (auto row : r)
      std::cout
          // Address column by name.  Use c_str() to get C-style string.
          << row["count"].c_str()
          << " makes "
          // Address column by zero-based index.  Use as<int>() to parse as int.
          << row[0].as<int>() << "." << std::endl;
    txn.commit();

  for(auto i = 0u; i < r.columns(); ++i)
  {
    std::cout << "Column " << i << " name=" << r.column_name(i) << " type= " << r.column_type(r.column_name(i)) << std::endl;
  }


//  pqxx::work op_work{c};
//  pqxx::result ops{op_work.exec("SELECT block_num, body FROM hive.operations WHERE block_num >= " 
//                             + std::to_string(from) 
//                             + " and block_num <= " 
//                             + std::to_string(to) 
//                             + " ORDER BY id ASC")};

//   for (const auto &row: ops)
//   {
//     for (const auto &field: row) std::cout << field.c_str() << '\t';
//     std::cout << std::endl;
//     const auto& o = row[1];
//     std::string json = std::string(o.c_str());
//     fc::variant v = fc::json::from_string( json );

//     hive::protocol::operation op;

//     fc::from_variant( v, op );

//     wlog("op=${op}", ("op", op));

//     //std::cout << op << std::endl;

//   }

//   op_work.commit();

}

volatile auto static stop_in_grab = false;


void try_grab_operations_C_impl(int from, int to, const char *context,
                                const char *postgres_url) 
{


  wlog("mtlk try_grab_operations_C pid= ${pid}", ("pid", getpid()));


  while(stop_in_grab)
  {
    int a = 0;
    a = a;
  }



  sanity_check(postgres_url);

  pqxx::connection c{postgres_url};

  pqxx::work blocks_work{c};
  pqxx::result blocks{blocks_work.exec("SELECT * FROM hive.blocks JOIN hive.accounts ON  id = producer_account_id WHERE num >= " 
                            + std::to_string(from) 
                            + " and num <= " 
                            + std::to_string(to) 
                            + " ORDER BY num ASC")};
  blocks_work.commit();

  pqxx::work transactions_work{c};
  pqxx::result transactions{transactions_work.exec("SELECT block_num, trx_in_block, ref_block_num, ref_block_prefix, expiration FROM hive.transactions WHERE block_num >= " 
                            + std::to_string(from) 
                            + " and block_num <= " 
                            + std::to_string(to) 
                            + " ORDER BY block_num, trx_in_block ASC")};
  transactions_work.commit();


  pqxx::work operations_work{c};
  pqxx::result operations{operations_work.exec("SELECT block_num, body, trx_in_block FROM hive.operations WHERE block_num >= " 
                            + std::to_string(from) 
                            + " and block_num <= " 
                            + std::to_string(to) 
                            + " ORDER BY id ASC")};
  operations_work.commit();


  for(auto i = 0u; i < blocks.columns(); ++i)
  {
    std::cout << "Column " << i << " name=" << blocks.column_name(i) << " type= " << blocks.column_type(blocks.column_name(i)) << std::endl;
  }


  auto transaction_expecting_block = -1;
  auto transactions_it = transactions.begin();
  if( transactions.size() > 0)
  {
      const auto& first_transaction = transactions[0];
      transaction_expecting_block = first_transaction["block_num"].as<int>();
  }

  auto operations_expecting_block = -1;
  auto operations_expecting_transaction = -1;
  auto operations_it = operations.begin();
  if(operations.size() > 0)
  {
    const auto& first_operation = operations[0];
    operations_expecting_block =  first_operation["block_num"].as<int>();
    operations_expecting_transaction = first_operation["trx_in_block"].as<int>();

  }



  for(const auto& block : blocks)
  {
    auto block_num = block["num"].as<int>();
    
    //fill in block header here
    fc::variant_object_builder block_v;
    block_v
    ("witness", block["name"].c_str()) ////"steemychicken1",
    ("block_id", block["id"].c_str())
    ("previous", block["prev"].c_str()) // "000f2bbfcbdad7bb80bc42c476567c750badd90b",
    ("timestamp",  block["created_at"].c_str()) //"2016-04-28T23:22:18"
    ("extensions", block["extensions"].c_str() )//[],
    ("signing_key", block["signing_key"].c_str())
    ;

    // "transaction_ids": [
    //     "5cf59b8b633f8e894adef4c8ccd6e2353040ff70"
    // ]


//    "transactions": [

    //contruct variant array

    std::vector<std::vector<fc::variant>> trancactions_vector;

    if(block_num == transaction_expecting_block)
    for(; transactions_it != transactions.end(); ++transactions_it)
    {
      const auto transaction = (*transactions_it);
      if(transaction["block_num"].as<int>() == block_num)
      {
        auto trx_in_block = transaction["trx_in_block"].as<int>();

        //fill in transaction here

        fc::variant_object_builder transaction_v;
        transaction_v
        ("ref_block_num", transaction["ref_block_num"].c_str())
        ("ref_block_prefix", transaction["ref_block_prefix"].c_str())
        ("expiration", transaction["expiration"].c_str())
        ;

            // "extensions": [],
            // "signatures": [
            //     "1f1fd68fd2b2ec919357c8e534d1473a16f5505a7719bbb2a2100478f212f9353272c38b12d1468d1c3eb830f659e80a28a5a985661bb2c79e4acd2ef34bc919b4"
            // ]

        variant tv;
        to_variant(transaction_v.get(),  tv);
        fc::stringstream toss;
        fc::json::to_stream(toss, tv);
    
        wlog("transaction=${j}",  ( "j", toss.str()));


        //rewind
        while(operations_expecting_block < block_num)
        {
          operations_it++;
          operations_expecting_block = operations_it["block_num"].as<int>();
          operations_expecting_transaction = operations_it["trx_in_block"].as<int>();
        }


        std::vector<fc::variant> operations_vector;
        if(block_num == operations_expecting_block && trx_in_block == operations_expecting_transaction)
        for(; operations_it != operations.end(); ++operations_it)
        {
          const auto operation = (*operations_it);
          if(operation["block_num"].as<int>() == block_num && operation["trx_in_block"].as<int>() == trx_in_block)
          {
            //fill in op here
            
                const auto& o = operation["body"];
                std::string json = std::string(o.c_str());
                fc::variant ov = fc::json::from_string( json );
           
                hive::protocol::operation op;
                fc::from_variant( ov, op );

                wlog("op=${op}",("op", op));
                
                operations_vector.push_back(ov);
                

          }
          else
          {
            operations_expecting_block = operations_it["block_num"].as<int>();
            operations_expecting_transaction = operations_it["trx_in_block"].as<int>();
            break;
          }

        }

        

        trancactions_vector.push_back(operations_vector);


      }
      else
      {
        transaction_expecting_block = transaction["block_num"].as<int>();
        break;
      }

      

    
    }

    //append do transaction array transakcję
    variant vt;
    to_variant(trancactions_vector, vt);
    block_v
    ("transactions", vt)
    ("witness_signature", block["witness_signature"].c_str()) //  "1f2ce40298d1569ba0f118146f71482ff18afdb02d844321addf3ebd7e970d65a625d4cb65aae4a9c884f896f0c2c6939603287997377d1f2d440d6f09825aab97",
    ("transaction_merkle_root", block["transaction_merkle_root"].c_str()) // "8195f855f6d54d6856f9a08a08b21bdd6b14472e",
    ;

    variant v;
    to_variant(block_v.get(),  v);
    fc::stringstream oss;
    fc::json::to_stream(oss, v);

    wlog("block_num=${block_num} header=${j}", ("block_num", block_num) ( "j", oss.str()));



  }
}

}}
