#include <beekeeper/session_manager.hpp>
#include <beekeeper/session.hpp>
#include <beekeeper/time_manager.hpp>

namespace beekeeper {

session_manager::session_manager( std::shared_ptr<mutex_handler> mtx_handler )
              : mtx_handler( mtx_handler )
{
  time = std::make_shared<time_manager>();
}

std::shared_ptr<session_base> session_manager::create_session( const std::string& token, std::shared_ptr<time_manager_base> not_used_time, const boost::filesystem::path& wallet_directory )
{
  return std::make_shared<session>( content_deliverer, token, time, wallet_directory );
}

void session_manager::lock( const std::string& token )
{
  FC_ASSERT( mtx_handler );

  std::unique_lock guard( mtx_handler->get_mutex() );
  session_manager_base::lock( token );
}

} //beekeeper
