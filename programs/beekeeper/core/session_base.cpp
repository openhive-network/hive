#include <core/session_base.hpp>

namespace beekeeper {

session_base::session_base( const std::string& token, std::shared_ptr<time_manager_base> time )
        : token(token), time( time )
{
  wallet_mgr = std::make_shared<wallet_manager_impl>();
}

void session_base::set_timeout( const std::chrono::seconds& t )
{
  timeout = t;
  auto now = std::chrono::system_clock::now();
  timeout_time = now + timeout;
  FC_ASSERT( timeout_time >= now && timeout_time.time_since_epoch().count() > 0, "Overflow on timeout_time, specified ${t}, now ${now}, timeout_time ${timeout_time}",
         ("t", t.count())("now", now.time_since_epoch().count())("timeout_time", timeout_time.time_since_epoch().count()));

  FC_ASSERT( time );
  time->change( token, timeout_time );
}

void session_base::check_timeout()
{
  FC_ASSERT( time );
  time->run();
}

info session_base::get_info()
{
  auto to_string = []( const std::chrono::system_clock::time_point& tp )
  {
    fc::time_point_sec _time( tp.time_since_epoch() / std::chrono::milliseconds(1000) );
    return _time.to_iso_string();
  };

  return { to_string( std::chrono::system_clock::now() ), to_string( timeout_time ) };
}

std::shared_ptr<wallet_manager_impl> session_base::get_wallet_manager()
{
  return wallet_mgr;
}

} //beekeeper
