#include <core/time_manager_base.hpp>

namespace beekeeper {

bool time_manager_base::run( const types::timepoint_t& now, const session_data& s_data, std::vector<std::string>& modified_items )
{
  if( now >= s_data.time )
  {
    auto _microseconds = std::chrono::duration_cast<std::chrono::microseconds>( now - s_data.time );
    if( _microseconds.count() / 1000 > 0 )
      dlog("lock activated: actual time is longer ${differ}[ms] than timeout time", ("differ", _microseconds.count() / 1000));
    else
      dlog("lock activated: actual time is equals to timeout time");
    auto _result = exception::exception_handler([&]()
                                                {
                                                  modified_items.emplace_back( s_data.token );
                                                  return "";
                                                }
                                              );
  }

  return true;
}

void time_manager_base::modify_times( const std::vector<std::string>& modified_items )
{
  if( modified_items.empty() )
    return;

  auto& _idx_token = items.get<by_token>();
  auto _result = exception::exception_handler([&]()
                                              {
                                                for( auto& token : modified_items )
                                                {
                                                  auto _found = _idx_token.find( token );
                                                  FC_ASSERT( _found != _idx_token.end() );

                                                  _idx_token.modify( _found, []( session_data &sd ){ sd.time = types::timepoint_t::max(); });

                                                  _found->lock_method( _found->token );
                                                }
                                                return "";
                                              }
                                            );
}

void time_manager_base::run()
{
  std::vector<std::string> _modified_items;

  auto& _idx_time = items.get<by_time>();
  auto _it = _idx_time.begin();

  const auto& _now = std::chrono::system_clock::now();

  while( _it != _idx_time.end() )
  {
    if( !run( _now, *_it, _modified_items ) )
      return;
    ++_it;
  }

  modify_times( _modified_items );
}

void time_manager_base::run( const std::string& token )
{
  std::vector<std::string> _modified_items;

  auto& _idx = items.get<by_token>();
  const auto& _found = _idx.find( token );

  FC_ASSERT( _found != _idx.end() );

  const auto& _now = std::chrono::system_clock::now();

  if( !run( _now, *_found, _modified_items ) )
    return;

  modify_times( _modified_items );
}

void time_manager_base::add( const std::string& token, types::lock_method_type&& lock_method )
{
  auto& _idx = items.get<by_token>();
  _idx.emplace( session_data{ token, lock_method } );
}

void time_manager_base::change( const std::string& token, const types::timepoint_t& time, bool refresh_only_active )
{
  auto& _idx = items.get<by_token>();
  const auto& _found = _idx.find( token );

  FC_ASSERT( _found != _idx.end() );

  _idx.modify( _found, [&time, &refresh_only_active]( session_data &sd )
    {
      if( !refresh_only_active || ( refresh_only_active && sd.time != types::timepoint_t::max() ) )
        sd.time = time;
    });
}

void time_manager_base::close( const std::string& token )
{
  auto& _idx = items.get<by_token>();
  _idx.erase( token );
}
} //beekeeper
