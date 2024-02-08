#include<beekeeper/extended_api.hpp>

#include <fc/log/logger.hpp>

#include<chrono>
#include<mutex>

namespace beekeeper {

  extended_api::extended_api(): error_status( false ), start( get_milliseconds() )
  {
  }

  extended_api::status extended_api::unlock_allowed()
  {
    std::unique_lock<std::mutex> _lock( mtx );

    if( !error_status.load() )
    {
      return enabled_without_error;
    }

    std::atomic<uint64_t> _end( get_milliseconds() );

    if( enabled_impl( _end ) )
    {
      error_status.store( false );
      return enabled_after_interval;
    }

    return disabled;
  }

  void extended_api::was_error()
  {
    if( error_status.load() == false )
    {
      error_status.store( true );
      start.store( get_milliseconds() );
    }
  }

}
