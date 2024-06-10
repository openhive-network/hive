#include <beekeeper/time_manager.hpp>

#include <appbase/application.hpp>

namespace beekeeper {

time_manager::time_manager( const std::optional<std::string>& notifications_endpoint )
              : error_notifications_endpoint( notifications_endpoint )
{
  notification_thread = std::make_unique<std::thread>( [this]()
    {
      while( !stop_requested )
      {
        run();
        std::this_thread::sleep_for( std::chrono::milliseconds(200) );
      }
    } );
}

time_manager::~time_manager()
{
  stop_requested = true;
  notification_thread->join();
}

void time_manager::send_auto_lock_error_message( const std::string& message )
{
  auto _result = exception::exception_handler( [&, this]()
                                                {
                                                  hive::utilities::notifications::notification_handler_wrapper _notification_handler;
                                                  _notification_handler.register_endpoint( error_notifications_endpoint );

                                                  appbase::application::dynamic_notify( _notification_handler, "Automatic lock error", "message", message );
                                                  return "";
                                                }
                              );
  if( !_result.second )
  {
    elog( _result.first );
  }
}

void time_manager::add( const std::string& token, types::lock_method_type&& lock_method, types::notification_method_type&& notification_method )
{
  std::lock_guard<std::mutex> guard( mtx );
  time_manager_base::add( token, std::move( lock_method ), std::move( notification_method ) );
}

void time_manager::change( const std::string& token, const types::timepoint_t& time, bool refresh_only_active )
{
  std::lock_guard<std::mutex> guard( mtx );
  time_manager_base::change( token, time, refresh_only_active );
}

void time_manager::run()
{
  std::lock_guard<std::mutex> guard( mtx );
  time_manager_base::run();
}

void time_manager::run( const std::string& token )
{
  std::lock_guard<std::mutex> guard( mtx );
  time_manager_base::run( token );
}

void time_manager::close( const std::string& token )
{
  std::lock_guard<std::mutex> guard( mtx );
  time_manager_base::close( token );
}

} //beekeeper
