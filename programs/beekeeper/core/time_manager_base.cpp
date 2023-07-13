#include <core/time_manager_base.hpp>

namespace beekeeper {

void time_manager_base::run()
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

void time_manager_base::add( const std::string& token, types::lock_method_type&& lock_method, types::notification_method_type&& notification_method )
{
  std::lock_guard<std::mutex> guard( methods_mutex );

  auto& _idx = items.get<by_token>();
  _idx.emplace( session_data{ token, lock_method, notification_method } );
}

void time_manager_base::change( const std::string& token, const types::timepoint_t& time )
{
  std::lock_guard<std::mutex> guard( methods_mutex );

  auto& _idx = items.get<by_token>();
  const auto& _found = _idx.find( token );

  _idx.modify( _found, [&time]( session_data &sd ){ sd.time = time; });
}

void time_manager_base::close( const std::string& token )
{
  std::lock_guard<std::mutex> guard( methods_mutex );

  auto& _idx = items.get<by_token>();
  _idx.erase( token );
}

} //beekeeper
