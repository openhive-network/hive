#include <beekeeper/session.hpp>

namespace beekeeper {

session::session( const std::string& notifications_endpoint, const std::string& token, std::shared_ptr<time_manager> time )
        : token(token), time( time )
{
  wallet = std::make_shared<wallet_manager_impl>();
  notification_handler.register_endpoints( { notifications_endpoint } );
}

void session::set_timeout( const std::chrono::seconds& t )
{
  timeout = t;
  auto now = std::chrono::system_clock::now();
  timeout_time = now + timeout;
  FC_ASSERT( timeout_time >= now && timeout_time.time_since_epoch().count() > 0, "Overflow on timeout_time, specified ${t}, now ${now}, timeout_time ${timeout_time}",
         ("t", t.count())("now", now.time_since_epoch().count())("timeout_time", timeout_time.time_since_epoch().count()));

  FC_ASSERT( time );
  time->change( token, timeout_time );
}

void session::check_timeout()
{
  FC_ASSERT( time );
  time->run();
}

info session::get_info()
{
  auto to_string = []( const std::chrono::system_clock::time_point& tp )
  {
    fc::time_point_sec _time( tp.time_since_epoch() / std::chrono::milliseconds(1000) );
    return _time.to_iso_string();
  };

  return { to_string( std::chrono::system_clock::now() ), to_string( timeout_time ) };
}

std::shared_ptr<wallet_manager_impl> session::get_wallet()
{
  return wallet;
}

hive::utilities::notifications::detail::notification_handler& session::get_notification_handler()
{
  return notification_handler;
}

} //beekeeper
