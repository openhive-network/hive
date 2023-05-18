#include <beekeeper/session_manager.hpp>

#include <beekeeper/token_generator.hpp>

#include <fc/exception/exception.hpp>

namespace beekeeper {

  session_manager::items::iterator session_manager::get_session( const std::string& token )
  {
    auto _found = sessions.find( token );
    FC_ASSERT( _found != sessions.end(), "A session attached to ${token} doesn't exist", (token) );

    return _found;
  }

  std::string session_manager::create_session( const std::string& salt, const std::string& notification_server, types::method_type&& lock_method )
  {
    auto _token = token_generator::generate_token( salt, token_length );

    sessions.emplace( _token, std::make_unique<session>( notification_server, std::move( lock_method ) ) );

    return _token;
  }

  void session_manager::close_session( const std::string& token )
  {
    auto _found = get_session( token );
    sessions.erase( _found );
  }

  void session_manager::set_timeout( const std::chrono::seconds& t, const std::string& token )
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

} //beekeeper
