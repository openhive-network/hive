#include <hive/plugins/metadata_api/metadata_api_plugin.hpp>
#include <hive/plugins/metadata_api/metadata_api.hpp>
#include <hive/plugins/metadata/metadata_objects.hpp>

#include <hive/chain/account_object.hpp>

namespace hive { namespace plugins { namespace metadata {

namespace detail {

using namespace hive::plugins::metadata;

class metadata_api_impl
{
  public:
    metadata_api_impl( appbase::application& app ) : _db( app.get_plugin< hive::plugins::chain::chain_plugin >().db() ){}

    DECLARE_API_IMPL(
      (get_metadata)
      (find_account_metadata)
    )

    chain::database& _db;
};

DEFINE_API_IMPL( metadata_api_impl, get_metadata )
{
  get_metadata_return result;

  const auto* account = _db.find_account( args.account );
  FC_ASSERT( account != nullptr, "Account not found: ${a}", ("a", args.account) );

  const auto* meta = _db.find< account_metadata_object, by_account >( account->get_id() );
  if( !meta )
    return result;

  result.json_metadata = to_string( meta->json_metadata );
  result.posting_json_metadata = to_string( meta->posting_json_metadata );

  return result;
}

DEFINE_API_IMPL( metadata_api_impl, find_account_metadata )
{
  find_account_metadata_return result;
  result.metadata.reserve( args.accounts.size() );

  for( const auto& account_name : args.accounts )
  {
    const auto* account = _db.find_account( account_name );
    if( !account )
      continue;

    const auto* meta = _db.find< account_metadata_object, by_account >( account->get_id() );
    if( meta )
    {
      result.metadata.push_back({
        account_name,
        to_string( meta->json_metadata ),
        to_string( meta->posting_json_metadata )
      });
    }
    else
    {
      // Account exists but no metadata (plugin wasn't loaded during account creation)
      result.metadata.push_back({ account_name, "", "" });
    }
  }

  return result;
}

} // detail

metadata_api::metadata_api( appbase::application& app ): my( new detail::metadata_api_impl( app ) )
{
  JSON_RPC_REGISTER_API( HIVE_METADATA_API_PLUGIN_NAME );
}

metadata_api::~metadata_api() {}

DEFINE_READ_APIS( metadata_api,
  (get_metadata)
  (find_account_metadata)
)

} } } // hive::plugins::metadata
