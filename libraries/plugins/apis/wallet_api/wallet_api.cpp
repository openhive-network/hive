#include <hive/plugins/wallet_api/wallet_api_plugin.hpp>
#include <hive/plugins/wallet_api/wallet_api.hpp>

#include <fc/variant_object.hpp>
#include <fc/reflect/variant.hpp>

namespace hive { namespace plugins { namespace wallet_api {

namespace detail {


using hive::plugins::wallet::wallet_manager;

class wallet_api_impl
{
  public:
    wallet_api_impl(): _wallet_mgr( appbase::app().get_plugin< hive::plugins::wallet::wallet_plugin >().get_wallet_manager() ) {}

    DECLARE_API_IMPL
    (
      (create)
    )

    wallet_manager& _wallet_mgr;
};

DEFINE_API_IMPL( wallet_api_impl, create )
{
  return { _wallet_mgr.create( args.wallet_name ) };
}

} // detail

wallet_api::wallet_api(): my( new detail::wallet_api_impl() )
{
  JSON_RPC_REGISTER_API( HIVE_WALLET_API_PLUGIN_NAME );
}

wallet_api::~wallet_api() {}

DEFINE_LOCKLESS_APIS( wallet_api,
  (create)
  )

} } } // hive::plugins::wallet_api
