#include <hive/plugins/app_status_api/app_status_api.hpp>
#include <hive/plugins/app_status_api/app_status_api_plugin.hpp>

#include <hive/chain/database.hpp>

#include <appbase/application.hpp>

namespace hive { namespace plugins { namespace app_status_api {

namespace detail
{
  class app_status_api_impl
  {
    public:
      app_status_api_impl( appbase::application& app ) : _db(app.get_plugin< hive::plugins::chain::chain_plugin>().db())
      {}

      DECLARE_API_IMPL((get_app_status))
      hive::chain::database& _db;
  };

  DEFINE_API_IMPL(app_status_api_impl, get_app_status)
  {
    return get_app_status_return();
  }
} // detail

app_status_api::app_status_api( appbase::application& app ) : my(new detail::app_status_api_impl( app ))
{
  JSON_RPC_REGISTER_EARLY_API(HIVE_API_STATUS_API_PLUGIN_NAME);
}

app_status_api::~app_status_api() {}

DEFINE_LOCKLESS_APIS(app_status_api, (get_app_status))

} } } // hive::plugins::app_status_api
