#include <hive/plugins/metadata_api/metadata_api_plugin.hpp>
#include <hive/plugins/metadata_api/metadata_api.hpp>
#include <hive/plugins/metadata/metadata_objects.hpp>

#include <hive/chain/hive_objects.hpp>

namespace hive { namespace plugins { namespace metadata {

namespace detail {

using namespace hive::plugins::metadata;

class metadata_api_impl
{
  public:
    metadata_api_impl( appbase::application& app ) : _db( app.get_plugin< hive::plugins::chain::chain_plugin >().db() ){}

    DECLARE_API_IMPL(
      (get_metadata)
    )

    chain::database& _db;
};

DEFINE_API_IMPL( metadata_api_impl, get_metadata )
{
  get_metadata_return result;

  const auto* _account = _db.find_account( args.account );
  FC_ASSERT( _account != nullptr, "Account not found: ${a}", ("a", args.account) );

  const auto* _metadata = _db.find< account_metadata_object, by_account >( _account->get_id() );
  if( !_metadata )
    return result;

  result.json_metadata = to_string( _metadata->json_metadata );
  result.posting_json_metadata = to_string( _metadata->posting_json_metadata );

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
)

} } } // hive::plugins::metadata
