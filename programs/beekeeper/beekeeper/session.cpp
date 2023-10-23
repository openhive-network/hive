#include <beekeeper/session.hpp>

#include <appbase/application.hpp>

namespace beekeeper {

session::session( const std::string& notifications_endpoint, const std::string& token, std::shared_ptr<time_manager_base> time )
        : session_base( token, time )
{
  notification_handler.register_endpoints( { notifications_endpoint } );
}

void session::prepare_notifications( const boost::filesystem::path& directory, const std::string& extension )
{
  fc::variant _v;

  auto _wallets = get_wallet_manager()->list_wallets( list_all_wallets( directory, extension ) );
  fc::to_variant( _wallets, _v );

  appbase::application::dynamic_notify( notification_handler, "Attempt of closing all wallets", "token", get_token(), "wallets", _v );
}

} //beekeeper
