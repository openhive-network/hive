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
  std::vector<std::string> _modified_items;

  {
    const auto& _now = std::chrono::system_clock::now();

    auto& _idx_time = items.get<by_time>();
    auto _it = _idx_time.begin();

    while( _it != _idx_time.end() )
    {
      if( _now >= _it->time )
      {
        auto _microseconds = std::chrono::duration_cast<std::chrono::microseconds>( _now - _it->time );
        if( _microseconds.count() / 1000 > 0 )
          dlog("lock activated: actual time is longer ${differ}[ms] than timeout time", ("differ", _microseconds.count() / 1000));
        else
          dlog("lock activated: actual time is equals to timeout time");
        auto _result = exception::exception_handler([&]()
                                                    {
                                                      _modified_items.emplace_back( _it->token );
                                                      return "";
                                                    }
                                                  );
        if( !_result.second )
        {
          send_auto_lock_error_message( _result.first );
          return; //First error during locking finishes whole procedure
        }
      }
      ++_it;
    }
  }

  if( _modified_items.empty() )
    return;

  auto& _idx_token = items.get<by_token>();
  auto _result = exception::exception_handler([&]()
                                              {
                                                for( auto& token : _modified_items )
                                                {
                                                  auto _found = _idx_token.find( token );
                                                  FC_ASSERT( _found != _idx_token.end() );

                                                  _idx_token.modify( _found, []( session_data &sd ){ sd.time = types::timepoint_t::max(); });

                                                  _found->notification_method( _found->token );
                                                  _found->lock_method( _found->token );
                                                }
                                                return "";
                                              }
                                            );
  if( !_result.second )
    send_auto_lock_error_message( _result.first );
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
