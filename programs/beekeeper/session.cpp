#include <beekeeper/session.hpp>

namespace beekeeper {

  session::session( const std::string& token, const std::string& notifications_endpoint, types::lock_method_type&& lock_method )
          : notifications_endpoint( notifications_endpoint ), time( token, std::move( lock_method ) )
  {
    wallet = std::make_shared<wallet_manager_impl>();
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

  std::shared_ptr<wallet_manager_impl>& session::get_wallet()
  {
    return wallet;
  }

} //beekeeper
