#include <core/session_manager_base.hpp>

#include <core/token_generator.hpp>

namespace beekeeper {

session_manager_base::session_manager_base()
{
  time = std::make_shared<time_manager>();
}

std::shared_ptr<session_base> session_manager_base::get_session( const std::string& token )
{
  auto _found = sessions.find( token );
  FC_ASSERT( _found != sessions.end(), "A session attached to ${token} doesn't exist", (token) );

  return _found->second;
}

std::shared_ptr<session_base> session_manager_base::create_session( const std::string& notifications_endpoint/*not used here*/, const std::string& token, std::shared_ptr<time_manager> time )
{
  return std::make_shared<session_base>( token, time );
}

std::string session_manager_base::create_session( const std::string& salt, const std::string& notifications_endpoint )
{
  auto _token = token_generator::generate_token( salt, token_length );

  std::shared_ptr<session_base> _session = create_session( notifications_endpoint, _token, time );
  sessions.emplace( _token, _session );

  time->add(  _token,
              [_session]()
              {
                FC_ASSERT( _session, "lock: session is empty." );

                auto _wallet_mgr = _session->get_wallet_manager();
                FC_ASSERT( _wallet_mgr, "wallet manager is empty." );

                _wallet_mgr->lock_all();
              },
              [notifications_endpoint, _session]()
              {
                FC_ASSERT( _session, "notification: session is empty." );

                _session->prepare_notifications();
              }
              );

  return _token;
}

void session_manager_base::close_session( const std::string& token )
{
  time->close( token );
  FC_ASSERT( sessions.erase( token ), "A session attached to ${token} doesn't exist", (token) );
}

bool session_manager_base::empty() const
{
  return sessions.empty();
}

void session_manager_base::set_timeout( const std::string& token, const std::chrono::seconds& t )
{
  get_session( token )->set_timeout( t );
}

void session_manager_base::check_timeout( const std::string& token )
{
  get_session( token )->check_timeout();
}

info session_manager_base::get_info( const std::string& token )
{
  return get_session( token )->get_info();
}

std::shared_ptr<wallet_manager_impl> session_manager_base::get_wallet_manager( const std::string& token )
{
  return get_session( token )->get_wallet_manager();
}

} //beekeeper
