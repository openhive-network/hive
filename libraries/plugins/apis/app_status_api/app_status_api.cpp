#include <hive/plugins/app_status_api/app_status_api.hpp>
#include <hive/plugins/app_status_api/app_status_api_plugin.hpp>

#include <hive/utilities/signal.hpp>

#include <appbase/application.hpp>

namespace hive { namespace plugins { namespace app_status_api {

namespace detail
{
  class app_status_api_impl
  {
    private:
  
      boost::signals2::connection      _on_notify_status_conn;

      hive::utilities::notifications::collector_t app_status;

      void on_notify_status( const hive::utilities::notifications::collector_t& current_app_status )
      {
        app_status = current_app_status;
      }

    public:

      app_status_api_impl( appbase::application& app )
      {
        _on_notify_status_conn = app.add_notify_status_handler(
          [&]( const hive::utilities::notifications::collector_t& current_app_status )
          {
            on_notify_status( current_app_status );
          }
        );
      }

      ~app_status_api_impl()
      {
        hive::utilities::disconnect_signal( _on_notify_status_conn );
      }

      DECLARE_API_IMPL((get_app_status))
  };

  DEFINE_API_IMPL(app_status_api_impl, get_app_status)
  {
    return app_status;
  }
} // detail

app_status_api::app_status_api( appbase::application& app ) : my(new detail::app_status_api_impl( app ))
{
  JSON_RPC_REGISTER_EARLY_API(HIVE_API_STATUS_API_PLUGIN_NAME);
}

app_status_api::~app_status_api() {}

DEFINE_LOCKLESS_APIS(app_status_api, (get_app_status))

} } } // hive::plugins::app_status_api
