#include <beekeeper/time_manager.hpp>

#include <hive/utilities/notifications.hpp>

#include <boost/filesystem.hpp>

namespace beekeeper {

namespace bfs = boost::filesystem;

time_manager::time_manager()
{
  notification_thread = std::make_unique<std::thread>( [this]()
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
  notification_thread->join();
}

void time_manager::run()
{
  std::lock_guard<std::mutex> guard( methods_mutex );

  const auto& now = std::chrono::system_clock::now();

  auto& _idx = items.get<by_time>();

  auto _it = _idx.begin();
  while( _it != _idx.end() )
  {
    if( now >= _it->time )
    {
      _it->lock_method();
      _it->notification_method();
      _idx.modify( _it, []( session_data &sd ){ sd.time = types::timepoint_t::max(); });
    }
    ++_it;
  }

}

void time_manager::add( const std::string& token, types::lock_method_type&& lock_method, types::notification_method_type&& notification_method )
{
  std::lock_guard<std::mutex> guard( methods_mutex );

  auto& _idx = items.get<by_token>();
  _idx.emplace( session_data{ token, lock_method, notification_method } );
}

void time_manager::change( const std::string& token, const types::timepoint_t& time )
{
  std::lock_guard<std::mutex> guard( methods_mutex );

  auto& _idx = items.get<by_token>();
  const auto& _found = _idx.find( token );

  _idx.modify( _found, [&time]( session_data &sd ){ sd.time = time; });
}

void time_manager::close( const std::string& token )
{
  std::lock_guard<std::mutex> guard( methods_mutex );

  auto& _idx = items.get<by_token>();
  _idx.erase( token );
}

} //beekeeper
