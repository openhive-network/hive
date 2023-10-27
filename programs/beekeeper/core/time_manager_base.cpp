#include <core/time_manager_base.hpp>

namespace beekeeper {

time_manager_base::time_manager_base()
{
}

time_manager_base::~time_manager_base()
{
}

void time_manager_base::run()
{
  const auto& now = std::chrono::system_clock::now();

  auto& _idx = items.get<by_time>();

  auto _it = _idx.begin();
  while( _it != _idx.end() )
  {
    if( now >= _it->time )
    {
      auto _result = exception::exception_handler([&]()
                                                  {
                                                    _idx.modify( _it, []( session_data &sd ){ sd.time = types::timepoint_t::max(); });
                                                    _it->notification_method( _it->token );
                                                    _it->lock_method( _it->token );
                                                    return "";
                                                  }
                                                );
      if( !_result.second )
      {
        send_auto_lock_error_message( _result.first );
        continue;
      }
    }
    ++_it;
  }
}

void time_manager_base::add( const std::string& token, types::lock_method_type&& lock_method, types::notification_method_type&& notification_method )
{
  auto& _idx = items.get<by_token>();
  _idx.emplace( session_data{ token, lock_method, notification_method } );
}

void time_manager_base::change( const std::string& token, const types::timepoint_t& time )
{
  auto& _idx = items.get<by_token>();
  const auto& _found = _idx.find( token );

  _idx.modify( _found, [&time]( session_data &sd ){ sd.time = time; });
}

void time_manager_base::close( const std::string& token )
{
  auto& _idx = items.get<by_token>();
  _idx.erase( token );
}
} //beekeeper
