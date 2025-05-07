#include <hive/plugins/app_status_api/app_status_api.hpp>
#include <hive/plugins/app_status_api/app_status_api_plugin.hpp>

#include <hive/utilities/signal.hpp>

#include <appbase/application.hpp>
#include <mutex>
#include <shared_mutex>

namespace hive { namespace plugins { namespace app_status_api {

namespace detail
{
  class app_status_api_impl
  {
    private:
      using connection_t = hive::utilities::statuses_signal_manager::connection_t;

      connection_t _on_new_status_connection;

      hive::utilities::threadsafe_statuses app_status;

    public:

      app_status_api_impl( appbase::application& app )
      {
        auto update_app_status = [&](auto item) { app_status.update(item); };
        _on_new_status_connection = app.status.add_new_status_handler(update_app_status);
      }

      ~app_status_api_impl()
      {
        hive::utilities::disconnect_signal( _on_new_status_connection );
      }

      DECLARE_API_IMPL((get_app_status))
  };

  DEFINE_API_IMPL(app_status_api_impl, get_app_status)
  {
    std::unique_lock<std::shared_mutex> guard{app_status.read_mtx};
    return app_status.data;
  }
} // detail

app_status_api::app_status_api( appbase::application& app ) : my(new detail::app_status_api_impl( app ))
{
  JSON_RPC_REGISTER_EARLY_API(HIVE_API_STATUS_API_PLUGIN_NAME);
}

app_status_api::~app_status_api() {}

DEFINE_LOCKLESS_APIS(app_status_api, (get_app_status))

} } } // hive::plugins::app_status_api
