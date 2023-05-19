#include <beekeeper/time_manager.hpp>

#include <hive/utilities/notifications.hpp>

#include <boost/filesystem.hpp>

namespace beekeeper {

namespace bfs = boost::filesystem;

time_manager::time_manager( const std::string& token, types::lock_method_type&& lock_method )
               :  token( token ),
                  lock_method( lock_method ),
                  notification_method( [](){ hive::notify_hived_status("Attempt of closing all wallets"); } )
{
   notification_thread = std::make_unique<std::thread>( [this]()
      {
         while( !stop_requested )
         {
            check_timeout_impl( false/*allow_update_timeout_time*/ );
            std::this_thread::sleep_for( std::chrono::milliseconds(200) );
         }
      } );
}

time_manager::~time_manager()
{
   stop_requested = true;
   notification_thread->join();
}

void time_manager::set_timeout( const std::chrono::seconds& t )
{
   timeout = t;
   auto now = std::chrono::system_clock::now();
   timeout_time = now + timeout;
   FC_ASSERT( timeout_time >= now && timeout_time.time_since_epoch().count() > 0, "Overflow on timeout_time, specified ${t}, now ${now}, timeout_time ${timeout_time}",
             ("t", t.count())("now", now.time_since_epoch().count())("timeout_time", timeout_time.time_since_epoch().count()));
}

void time_manager::check_timeout_impl( bool allow_update_timeout_time )
{
   if( timeout_time != timepoint_t::max() )
   {
      const auto& now = std::chrono::system_clock::now();
      if( now >= timeout_time )
      {
         {
            std::lock_guard<std::mutex> guard( methods_mutex );
            lock_method( token );
            notification_method();
            allow_update_timeout_time = true;
         }
      }
      if( allow_update_timeout_time )
         timeout_time = now + timeout;
   }
}

void time_manager::check_timeout()
{
   check_timeout_impl( true/*allow_update_timeout_time*/ );
}

info time_manager::get_info()
{
  auto to_string = []( const std::chrono::system_clock::time_point& tp )
  {
    fc::time_point_sec _time( tp.time_since_epoch() / std::chrono::milliseconds(1000) );
    return _time.to_iso_string();
  };

  return { to_string( std::chrono::system_clock::now() ), to_string( timeout_time ) };
}

} //beekeeper
