#include <beekeeper/session.hpp>

#include <core/beekeeper_instance_base.hpp>

#include <appbase/application.hpp>

namespace beekeeper {

session::session( const std::optional<std::string>& notifications_endpoint, const std::string& token, std::shared_ptr<time_manager_base> time, const boost::filesystem::path& wallet_directory )
        : session_base( token, time, wallet_directory )
{
  notification_handler.register_endpoint( notifications_endpoint );
}

void session::prepare_notifications()
{
  fc::variant _v;

  auto _wallets = get_wallet_manager()->list_wallets();
  fc::to_variant( _wallets, _v );

  appbase::application::dynamic_notify( notification_handler, "Attempt of closing all wallets", "token", get_token(), "wallets", _v );
}

} //beekeeper
