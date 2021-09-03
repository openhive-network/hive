#include <chainbase/forward_declarations.hpp>

#include <hive/plugins/account_by_key_api/account_by_key_api_plugin.hpp>
#include <hive/plugins/account_by_key_api/account_by_key_api.hpp>

#include <hive/plugins/account_by_key/account_by_key_objects.hpp>

namespace hive { namespace plugins { namespace account_by_key {

namespace detail {

class account_by_key_api_impl
{
  public:
    account_by_key_api_impl() : _db( appbase::app().get_plugin< hive::plugins::chain::chain_plugin >().db() ) {}

    get_key_references_return get_key_references( const get_key_references_args& args )const;

    chain::database& _db;
};

get_key_references_return account_by_key_api_impl::get_key_references( const get_key_references_args& args )const
{
  get_key_references_return final_result;
  final_result.accounts.reserve( args.keys.size() );

  const auto& key_idx = _db.get_index< account_by_key::key_lookup_index >().indices().get< account_by_key::by_key >();

  for( auto& key : args.keys )
  {
    std::vector< hive::protocol::account_name_type > result;
    auto lookup_itr = key_idx.lower_bound( key );

    while( lookup_itr != key_idx.end() && lookup_itr->key == key )
    {
      result.push_back( lookup_itr->account );
      ++lookup_itr;
    }

    final_result.accounts.emplace_back( std::move( result ) );
  }

  return final_result;
}

} // detail

account_by_key_api::account_by_key_api(): my( new detail::account_by_key_api_impl() )
{
  JSON_RPC_REGISTER_API( HIVE_ACCOUNT_BY_KEY_API_PLUGIN_NAME );
}

account_by_key_api::~account_by_key_api() {}

DEFINE_READ_APIS( account_by_key_api, (get_key_references) )

} } } // hive::plugins::account_by_key
