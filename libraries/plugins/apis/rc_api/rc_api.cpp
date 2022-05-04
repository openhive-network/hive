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
  fc::variant v_state_size_info;
  fc::to_variant( state_size_info, v_state_size_info );
  fc::mutable_variant_object state_bytes_mvo = v_state_size_info.get_object();
  state_bytes_mvo("STATE_BYTES_SCALE", STATE_BYTES_SCALE);
  // No need to expose other STATE_ constants, since they only affect state_size_info fields which are already returned

  operation_exec_info exec_info;

  fc::mutable_variant_object size_info_mvo;
  size_info_mvo
    ( "resource_state_bytes", state_bytes_mvo )
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
    const rc_account_object* rc_account = _db.find< rc_account_object, hive::chain::by_name >( a );

    if( rc_account != nullptr )
    {
      result.rc_accounts.emplace_back( *rc_account, _db );
    }
  }

  return result;
}

DEFINE_API_IMPL( rc_api_impl, list_rc_accounts )
{
  FC_ASSERT( args.limit <= RC_API_SINGLE_QUERY_LIMIT );

  list_rc_accounts_return result;
  result.rc_accounts.reserve( args.limit );

  auto& idx = _db.get_index< hive::plugins::rc::rc_account_index, hive::chain::by_name >();
  auto itr = idx.lower_bound(  args.start.as< account_name_type >() );
  auto filter = &filter_default< rc_account_object >;
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
  FC_ASSERT( args.limit <= RC_API_SINGLE_QUERY_LIMIT );
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

  auto& idx  = _db.get_index< rc_direct_delegation_object_index, by_from_to >();
  auto itr = idx.lower_bound( boost::make_tuple( delegator->get_id(), delegatee_id));
  auto end = idx.end();

  while( result.rc_direct_delegations.size() < args.limit && itr != end )
  {
    const rc_direct_delegation_object& rcdd = *itr;
    const account_object& to_account = _db.get< account_object, by_id >( rcdd.to );
    result.rc_direct_delegations.emplace_back(*itr, delegator->name, to_account.name);
    ++itr;
  }

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
  )

} } } // hive::plugins::rc
