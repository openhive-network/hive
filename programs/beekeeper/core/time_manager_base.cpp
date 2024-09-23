#include <core/time_manager_base.hpp>

namespace beekeeper {

void time_manager_base::modify( const std::string& token, const types::timepoint_t& new_time, bool allow_lock )
{
  auto& _idx_token = items.get<by_token>();
  auto _result = exception::exception_handler([&]()
                                              {
                                                auto _found = _idx_token.find( token );
                                                FC_ASSERT( _found != _idx_token.end() );

                                                _idx_token.modify( _found, [new_time]( session_data &sd ){ sd.time = new_time; });

                                                if( allow_lock )
                                                  _found->lock_method( _found->token );

                                                return "";
                                              }
                                            );
}

void time_manager_base::run( const std::string& token, const types::timepoint_t& new_time, bool move_time_forward, bool allow_lock )
{
  auto& _idx = items.get<by_token>();
  const auto& _found = _idx.find( token );

  FC_ASSERT( _found != _idx.end() );

  const auto& _now = std::chrono::system_clock::now();

  types::timepoint_t _new_time = new_time;

  if( _now >= _found->time )
  {
    auto _microseconds = std::chrono::duration_cast<std::chrono::microseconds>( _now - _found->time );
    if( _microseconds.count() / 1000 > 0 )
      dlog("lock activated: actual time is longer ${differ}[ms] than timeout time", ("differ", _microseconds.count() / 1000));
    else
      dlog("lock activated: actual time is equals to timeout time");

    if( !move_time_forward )
      _new_time = types::timepoint_t::max();
  }

  modify( _found->token, _new_time, allow_lock && ( _now >= _found->time ) );
}

void time_manager_base::add( const std::string& token, types::lock_method_type&& lock_method )
{
  auto& _idx = items.get<by_token>();
  _idx.emplace( session_data{ token, lock_method } );
}

void time_manager_base::close( const std::string& token )
{
  auto& _idx = items.get<by_token>();
  _idx.erase( token );
}
} //beekeeper
