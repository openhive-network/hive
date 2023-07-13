#include <beekeeper/time_manager.hpp>

#include <thread>
#include <mutex>

namespace beekeeper {

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

} //beekeeper