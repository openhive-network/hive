#include <core/session_base.hpp>

namespace beekeeper {

session_base::session_base( const std::string& token, std::shared_ptr<time_manager_base> time, const boost::filesystem::path& wallet_directory )
        : token(token), time( time )
{
  wallet_mgr = std::make_shared<wallet_manager_impl>( wallet_directory );
}

void session_base::set_timeout( const std::chrono::seconds& t )
{
  timeout = t;
  refresh_timeout( false/*refresh_only_active*/ );
}

void session_base::check_timeout()
{
  FC_ASSERT( time );
  time->run( token );

  refresh_timeout( true/*refresh_only_active*/ );
}

void session_base::refresh_timeout( bool refresh_only_active )
{
  auto _now = std::chrono::system_clock::now();
  timeout_time = _now + timeout;

  FC_ASSERT( timeout_time >= _now && timeout_time.time_since_epoch().count() > 0, "Overflow on timeout_time, specified ${t}, now ${now}, timeout_time ${timeout_time}",
         ("t", timeout.count())("now", _now.time_since_epoch().count())("timeout_time", timeout_time.time_since_epoch().count()));

  FC_ASSERT( time );
  time->change( token, timeout_time, refresh_only_active );
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
