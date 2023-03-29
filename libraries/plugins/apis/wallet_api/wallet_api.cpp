#include <hive/plugins/wallet_api/wallet_api_plugin.hpp>
#include <hive/plugins/wallet_api/wallet_api.hpp>

#include <fc/variant_object.hpp>
#include <fc/reflect/variant.hpp>

namespace hive { namespace plugins { namespace wallet_api {

namespace detail {

class wallet_api_impl
{
  public:
    wallet_api_impl() : _db( appbase::app().get_plugin< hive::plugins::chain::chain_plugin >().db() ) {}

    DECLARE_API_IMPL
    (
      (pychol_mychol)
    )

    chain::database& _db;
};

DEFINE_API_IMPL( wallet_api_impl, pychol_mychol )
{
  return pychol_mychol_return();
}

} // detail

wallet_api::wallet_api(): my( new detail::wallet_api_impl() )
{
  JSON_RPC_REGISTER_API( HIVE_WALLET_API_PLUGIN_NAME );
}

wallet_api::~wallet_api() {}

DEFINE_READ_APIS( wallet_api,
  (pychol_mychol)
  )

} } } // hive::plugins::wallet_api
