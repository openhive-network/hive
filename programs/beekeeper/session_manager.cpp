#include <beekeeper/session_manager.hpp>

#include <beekeeper/token_generator.hpp>

#include <fc/exception/exception.hpp>

namespace beekeeper {

session_manager::session_manager()
{
  time = std::make_shared<time_manager>();
}

session_manager::items::iterator session_manager::get_session( const std::string& token )
{
  auto _found = sessions.find( token );
  FC_ASSERT( _found != sessions.end(), "A session attached to ${token} doesn't exist", (token) );

  return _found;
}

std::string session_manager::create_session( const std::string& salt, const std::string& notifications_endpoint )
{
  auto _token = token_generator::generate_token( salt, token_length );

  std::shared_ptr<session> _session = std::make_shared<session>( notifications_endpoint, _token, time );
  sessions.emplace( _token, _session );

  time->add(  _token,
              [this, _token](){ get_wallet( _token )->lock_all(); },
              [notifications_endpoint, _session](){ hive::dynamic_notify( _session->get_notification_handler(), "Attempt of closing all wallets"); }
              );

  return _token;
}

void session_manager::close_session( const std::string& token )
{
  sessions.erase( get_session( token ) );
  time->close( token );
}

bool session_manager::empty() const
{
  return sessions.empty();
}

void session_manager::set_timeout( const std::string& token, const std::chrono::seconds& t )
{
  get_session( token )->second->set_timeout( t );
}

void session_manager::check_timeout( const std::string& token )
{
  get_session( token )->second->check_timeout();
}

info session_manager::get_info( const std::string& token )
{
  return get_session( token )->second->get_info();
}

std::shared_ptr<wallet_manager_impl>& session_manager::get_wallet( const std::string& token )
{
  return get_session( token )->second->get_wallet();
}

} //beekeeper
