#include <beekeeper/time_manager.hpp>

#include <appbase/application.hpp>

namespace beekeeper {

void time_manager::add( const std::string& token, types::lock_method_type&& lock_method )
{
  std::lock_guard<std::mutex> guard( mtx );
  time_manager_base::add( token, std::move( lock_method ) );
}

void time_manager::run( const std::string& token, const types::timepoint_t& new_time, bool move_time_forward, bool allow_lock )
{
  std::lock_guard<std::mutex> guard( mtx );
  time_manager_base::run( token, new_time, move_time_forward, allow_lock );
}

void time_manager::close( const std::string& token )
{
  std::lock_guard<std::mutex> guard( mtx );
  time_manager_base::close( token );
}

} //beekeeper
