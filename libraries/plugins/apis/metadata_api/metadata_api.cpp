#include <hive/plugins/metadata_api/metadata_api_plugin.hpp>
#include <hive/plugins/metadata_api/metadata_api.hpp>

#include <hive/chain/hive_objects.hpp>

#define ASSET_TO_REAL( asset ) (double)( asset.amount.value )

namespace hive { namespace plugins { namespace metadata {

namespace detail {

using namespace hive::plugins::metadata;

class metadata_api_impl
{
  public:
    metadata_api_impl( appbase::application& app ) : _db( app.get_plugin< hive::plugins::chain::chain_plugin >().db() ), theApp( app ) {}

    DECLARE_API_IMPL(
      (get_metadata)
    )

    chain::database& _db;
    appbase::application& theApp;
};

DEFINE_API_IMPL( metadata_api_impl, get_metadata )
{
  get_metadata_return result;
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
