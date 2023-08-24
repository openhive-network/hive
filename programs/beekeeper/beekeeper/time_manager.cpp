#include <beekeeper/time_manager.hpp>

#include <appbase/application.hpp>

namespace beekeeper {

time_manager::time_manager( const std::string& notifications_endpoint )
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
  std::string _error_message = exception::exception_handler( "Send auto lock error_message",
                                [&, this]()
                                {
                                  hive::utilities::notifications::notification_handler_wrapper _notification_handler;
                                  _notification_handler.register_endpoints( { error_notifications_endpoint } );

                                  appbase::application::dynamic_notify( _notification_handler, "Automatic lock error", "message", message );
                                  return "";
                                }
                              );
  if( !_error_message.empty() )
  {
    elog( _error_message );
  }
}

} //beekeeper
