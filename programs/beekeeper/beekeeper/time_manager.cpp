#include <beekeeper/time_manager.hpp>

#include <appbase/application.hpp>

namespace beekeeper {

time_manager::time_manager()
{
  lock_thread = std::make_unique<std::thread>( [this]()
    {
      while( !stop_requested )
      {
        run();
        std::this_thread::sleep_for( std::chrono::milliseconds(200) );
      }
    } );
}

time_manager::~time_manager()
{
  stop_requested = true;
  lock_thread->join();
}

void time_manager::add( const std::string& token, types::lock_method_type&& lock_method )
{
  std::lock_guard<std::mutex> guard( mtx );
  time_manager_base::add( token, std::move( lock_method ) );
}

void time_manager::change( const std::string& token, const types::timepoint_t& time, bool refresh_only_active )
{
  std::lock_guard<std::mutex> guard( mtx );
  time_manager_base::change( token, time, refresh_only_active );
}

void time_manager::run()
{
  std::lock_guard<std::mutex> guard( mtx );
  time_manager_base::run();
}

void time_manager::run( const std::string& token )
{
  std::lock_guard<std::mutex> guard( mtx );
  time_manager_base::run( token );
}

void time_manager::close( const std::string& token )
{
  std::lock_guard<std::mutex> guard( mtx );
  time_manager_base::close( token );
}

} //beekeeper
