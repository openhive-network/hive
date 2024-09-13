#include <hive/plugins/app_status_api/app_status_api.hpp>
#include <hive/plugins/app_status_api/app_status_api_plugin.hpp>

#include <hive/chain/database.hpp>

#include <appbase/application.hpp>

namespace hive { namespace plugins { namespace app_status_api {

namespace detail
{
  class app_status_api_impl
  {
    private:
  
      boost::signals2::connection      _on_notify_status_conn;

      get_app_status_return app_status;

      void on_notify_status( const std::string& current_status, const std::string& name )
      {
        app_status.value.current_status = current_status;
        app_status.name                 = name;
        app_status.time                 = fc::time_point::now();
      }

    public:

      app_status_api_impl( appbase::application& app ) : _db( app.get_plugin< hive::plugins::chain::chain_plugin>().db() )
      {
        _on_notify_status_conn = app.add_notify_status_handler(
          [&]( const std::string& current_status, const std::string& name )
          {
            on_notify_status( current_status, name );
          }
        );
      }

      ~app_status_api_impl()
      {
        chain::util::disconnect_signal( _on_notify_status_conn );
      }

      DECLARE_API_IMPL((get_app_status))

      hive::chain::database& _db;
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
