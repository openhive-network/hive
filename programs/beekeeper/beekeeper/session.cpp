#include <beekeeper/session.hpp>

namespace beekeeper {

session::session( const std::string& notifications_endpoint, const std::string& token, std::shared_ptr<time_manager_base> time )
        : session_base( token, time )
{
  notification_handler.register_endpoints( { notifications_endpoint } );
}

void session::prepare_notifications()
{
  hive::utilities::notifications::dynamic_notify( notification_handler, "Attempt of closing all wallets");
}

} //beekeeper
