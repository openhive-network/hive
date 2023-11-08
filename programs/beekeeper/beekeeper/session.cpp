#include <beekeeper/session.hpp>

#include <core/beekeeper_instance_base.hpp>

#include <appbase/application.hpp>

namespace beekeeper {

session::session( const std::string& notifications_endpoint, const std::string& token, std::shared_ptr<time_manager_base> time )
        : session_base( token, time )
{
  notification_handler.register_endpoint( notifications_endpoint );
}

void session::prepare_notifications(const beekeeper_instance_base& bk_instance )
{
  fc::variant _v;

  auto _wallets = get_wallet_manager()->list_wallets( [&](const std::string& name) { return bk_instance.create_wallet_filename(name); },
                                                      std::vector<std::string>() );
  fc::to_variant( _wallets, _v );

  appbase::application::dynamic_notify( notification_handler, "Attempt of closing all wallets", "token", get_token(), "wallets", _v );
}

} //beekeeper
