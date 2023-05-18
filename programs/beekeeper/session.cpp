#include <beekeeper/session.hpp>

namespace beekeeper {

  session::session( const std::string& notification_server, types::method_type&& lock_method )
          : notification_server( notification_server ), time( std::move( lock_method ) )
  {

  }

  void session::set_timeout( const std::chrono::seconds& t )
  {
    time.set_timeout( t );
  }

  void session::check_timeout()
  {
    time.check_timeout();
  }

  info session::get_info()
  {
    return time.get_info();
  }

} //beekeeper
