#include <hive/plugins/rc_api/rc_api_plugin.hpp>
#include <hive/plugins/rc_api/rc_api.hpp>

#include <hive/plugins/rc/rc_objects.hpp>
#include <hive/plugins/rc/resource_sizes.hpp>

#include <hive/chain/account_object.hpp>

#include <fc/variant_object.hpp>
#include <fc/reflect/variant.hpp>

namespace hive { namespace plugins { namespace rc {

namespace detail {

template< typename ValueType >
static bool filter_default( const ValueType& r ) { return true; }

class rc_api_impl
{
  public:
    rc_api_impl() : _db( appbase::app().get_plugin< hive::plugins::chain::chain_plugin >().db() ) {}

    DECLARE_API_IMPL
    (
      (get_resource_params)
      (get_resource_pool)
      (find_rc_accounts)
      (list_rc_accounts)
      (list_rc_direct_delegations)
      (get_rc_stats)
      (get_rc_operation_stats)
    )

    chain::database& _db;
};

DEFINE_API_IMPL( rc_api_impl, get_resource_params )
{
  get_resource_params_return result;
  const rc_resource_param_object& params_obj = _db.get< rc_resource_param_object, by_id >( rc_resource_param_id_type() );
  fc::mutable_variant_object resource_params_mvo;

  for( size_t i=0; i<HIVE_RC_NUM_RESOURCE_TYPES; i++ )
  {
    std::string resource_name = fc::reflector< rc_resource_types >::to_string( i );
    result.resource_names.push_back( resource_name );
    resource_params_mvo( resource_name, params_obj.resource_param_array[i] );
  }

  state_object_size_info state_size_info;
  operation_exec_info exec_info;

  fc::mutable_variant_object size_info_mvo;
  size_info_mvo
    ( "resource_state_bytes", state_size_info )
    ( "resource_execution_time", exec_info );

  result.size_info = size_info_mvo;
  result.resource_params = resource_params_mvo;

  return result;
}

DEFINE_API_IMPL( rc_api_impl, get_resource_pool )
{
  get_resource_pool_return result;
  fc::mutable_variant_object mvo;
  const rc_pool_object& pool_obj = _db.get< rc_pool_object, by_id >( rc_pool_id_type() );
  const rc_resource_param_object& params_obj = _db.get< rc_resource_param_object, by_id >( rc_resource_param_id_type() );

  for( size_t i=0; i<HIVE_RC_NUM_RESOURCE_TYPES; i++ )
  {
    const auto& pool_params = params_obj.resource_param_array[i].resource_dynamics_params;
    resource_pool_api_object api_pool;
    api_pool.pool = pool_obj.get_pool(i);
    api_pool.fill_level = ( api_pool.pool * HIVE_100_PERCENT ) / pool_params.pool_eq;
    mvo( fc::reflector< rc_resource_types >::to_string( i ), api_pool );
  }

  result.resource_pool = mvo;
  return result;
}

DEFINE_API_IMPL( rc_api_impl, find_rc_accounts )
{
  FC_ASSERT( args.accounts.size() <= RC_API_SINGLE_QUERY_LIMIT );

  find_rc_accounts_return result;
  result.rc_accounts.reserve( args.accounts.size() );

  for( const account_name_type& a : args.accounts )
  {
    const account_object* rc_account = _db.find_account( a );

    if( rc_account != nullptr )
    {
      result.rc_accounts.emplace_back( *rc_account, _db );
    }
  }

  return result;
}

DEFINE_API_IMPL( rc_api_impl, list_rc_accounts )
{
  FC_ASSERT( 0 < args.limit && args.limit <= RC_API_SINGLE_QUERY_LIMIT, "limit not set or too big" );

  list_rc_accounts_return result;
  result.rc_accounts.reserve( args.limit );

  auto& idx = _db.get_index< hive::chain::account_index, hive::chain::by_name >();
  auto itr = idx.lower_bound( args.start.as< account_name_type >() );
  auto filter = &filter_default< hive::chain::account_object >;
  auto end = idx.end();

  while( result.rc_accounts.size() < args.limit && itr != end )
  {
    if( filter( *itr ) )
      result.rc_accounts.emplace_back( *itr, _db );

    ++itr;
  }

  return result;
}

DEFINE_API_IMPL( rc_api_impl, list_rc_direct_delegations )
{
  FC_ASSERT( 0 < args.limit && args.limit <= RC_API_SINGLE_QUERY_LIMIT, "limit not set or too big" );
  list_rc_direct_delegations_return result;
  result.rc_direct_delegations.reserve( args.limit );

  auto key = args.start.as< vector< fc::variant > >();
  FC_ASSERT( key.size() == 2, "by_from start requires 2 value. (from, to)" );

  const account_object* delegator = _db.find< account_object, hive::chain::by_name >( key[0].as< account_name_type >() );
  // If the delegatee is not provided, we default to the first id
  auto delegatee_id = account_id_type::start_id();
  if (key[1].as< account_name_type >() != "") {
    const account_object* delegatee = _db.find< account_object, hive::chain::by_name >( key[1].as< account_name_type >() );
    FC_ASSERT( delegatee, "Account ${a} does not exist", ("a", key[1].as< account_name_type >()) );
    delegatee_id = delegatee->get_id();
  }

  FC_ASSERT( delegator, "Account ${a} does not exist", ("a", key[0].as< account_name_type >()) );

  auto& idx  = _db.get_index< rc_direct_delegation_index, by_from_to >();
  auto itr = idx.lower_bound( boost::make_tuple( delegator->get_id(), delegatee_id));
  auto end = idx.end();

  while( result.rc_direct_delegations.size() < args.limit && itr != end && itr->from == delegator->get_id())
  {
    const rc_direct_delegation_object& rcdd = *itr;
    const account_object &to_account = _db.get<account_object, by_id>(rcdd.to);
    result.rc_direct_delegations.emplace_back( *itr, delegator->get_name(), to_account.get_name() );
    ++itr;
  }

  return result;
}

DEFINE_API_IMPL( rc_api_impl, get_rc_stats )
{
  get_rc_stats_return result;

  const auto& rc_plugin = appbase::app().get_plugin< rc::rc_plugin >();
  // stats won't be enabled until RC plugin starts working (plus they are not enabled in testnet)
  FC_ASSERT( rc_plugin.is_rc_stats_enabled(), "Gathering of RC stats not enabled." );
  // FULL report would not be useful in itself - it'd have to be supplemented like in get_rc_operation_stats
  result.rc_stats = rc_plugin.get_report( rc::rc_plugin::report_type::REGULAR );

  return result;
}

DEFINE_API_IMPL( rc_api_impl, get_rc_operation_stats )
{
  get_rc_operation_stats_return result;

  const auto& rc_plugin = appbase::app().get_plugin< rc::rc_plugin >();
  // stats won't be enabled until RC plugin starts working (plus they are not enabled in testnet)
  FC_ASSERT( rc_plugin.is_rc_stats_enabled(), "Gathering of RC stats not enabled." );

  //compare with from_string defined in FC_REFLECT_ENUM - we don't have enum here but situation is similar
  int op_id = 0;
  for( ; op_id < HIVE_RC_NUM_OPERATIONS; ++op_id )
  {
    hive::protocol::operation op;
    op.set_which( op_id );
    std::string op_name = op.get_stored_type_name( true );
    if( strcmp( op_name.c_str(), args.operation.c_str() ) == 0 )
      break;
  }
  if( op_id == HIVE_RC_NUM_OPERATIONS )
  {
    if( strcmp( "multiop", args.operation.c_str() ) != 0 )
    {
      try
      {
        op_id = boost::lexical_cast< int >( args.operation );
        if( op_id < 0 || HIVE_RC_NUM_OPERATIONS < op_id )
          fc::throw_bad_enum_cast( args.operation.c_str(), "hive::protocol::operation" );
      }
      catch( const boost::bad_lexical_cast& e )
      {
        fc::throw_bad_enum_cast( args.operation.c_str(), "hive::protocol::operation" );
      }
    }
  }

  const auto& op_stats = _db.get< rc_stats_object >( RC_ARCHIVE_STATS_ID ).get_op_stats( op_id );

  result.count = op_stats.count;
  result.avg_cost_rc = op_stats.average_cost();

  resource_cost_type cost;
  resource_share_type share;
  resource_count_type usage;
  if( result.avg_cost_rc > 0 )
  {
    for( int i = 0; i < HIVE_RC_NUM_RESOURCE_TYPES; ++i )
    {
      cost[i] = op_stats.cost[i] / op_stats.count;
      usage[i] = op_stats.usage[i] / op_stats.count;
      // there should still be a solid margin even for new account tokens for the calculation below
      share[i] = cost[i] * HIVE_100_PERCENT / result.avg_cost_rc;
    }
  }
  // we are not using arrays in result to increase readability, so we need to rewrite values one by one
  result.resource_cost.history_rc = cost[ resource_history_bytes ];
  result.resource_cost.tokens_rc = cost[ resource_new_accounts ];
  result.resource_cost.market_rc = cost[ resource_market_bytes ];
  result.resource_cost.state_rc = cost[ resource_state_bytes ];
  result.resource_cost.exec_rc = cost[ resource_execution_time ];
  result.resource_cost_share.history_bp = share[ resource_history_bytes ];
  result.resource_cost_share.tokens_bp = share[ resource_new_accounts ];
  result.resource_cost_share.market_bp = share[ resource_market_bytes ];
  result.resource_cost_share.state_bp = share[ resource_state_bytes ];
  result.resource_cost_share.exec_bp = share[ resource_execution_time ];
  //ABW: somehow I can't be bothered to code it in generic way; it only works when resource units are [1,10000,10,1,1] respectively
  result.resource_usage.history_bytes = usage[ resource_history_bytes ];
  std::stringstream tokens_str;
  tokens_str << std::fixed << std::setprecision(4) << double( usage[ resource_new_accounts ] ) / 10000.0;
  result.resource_usage.tokens = tokens_str.str();
  result.resource_usage.market_bytes = usage[ resource_market_bytes ] / 10;
  result.resource_usage.state_hbytes = usage[ resource_state_bytes ];
  result.resource_usage.exec_ns = usage[ resource_execution_time ];

  return result;
}

} // detail

rc_api::rc_api(): my( new detail::rc_api_impl() )
{
  JSON_RPC_REGISTER_API( HIVE_RC_API_PLUGIN_NAME );
}

rc_api::~rc_api() {}

DEFINE_READ_APIS( rc_api,
  (get_resource_params)
  (get_resource_pool)
  (find_rc_accounts)
  (list_rc_accounts)
  (list_rc_direct_delegations)
  (get_rc_stats)
  (get_rc_operation_stats)
  )

} } } // hive::plugins::rc
