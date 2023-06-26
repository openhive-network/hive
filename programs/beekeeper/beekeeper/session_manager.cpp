#include <beekeeper/session_manager.hpp>
#include <beekeeper/session.hpp>

namespace beekeeper {

std::shared_ptr<session_base> session_manager::create_session( const std::string& notifications_endpoint, const std::string& token, std::shared_ptr<time_manager> time )
{
  return std::make_shared<session>( notifications_endpoint, token, time );
}

} //beekeeper
