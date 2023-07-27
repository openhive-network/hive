#include<beekeeper/extended_api.hpp>

#include<chrono>
#include<mutex>

namespace beekeeper {

  extended_api::extended_api(): start( get_milliseconds() )
  {
  }

  bool extended_api::enabled()
  {
    std::atomic<uint64_t> _end( get_milliseconds() );

    if( _end.load() - start.load() > interval )
    {
      std::unique_lock<std::mutex> _lock( mtx );

      if( _end.load() - start.load() > interval )
      {
        start.store( get_milliseconds() );
        return true;
      }

      return false;
    }
    return false;
  }

}