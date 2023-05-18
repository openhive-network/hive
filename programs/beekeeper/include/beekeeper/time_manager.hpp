#pragma once

#include <beekeeper/utilities.hpp>

#include <chrono>
#include <thread>
#include <string>
#include <functional>
#include <mutex>

namespace beekeeper {

class time_manager
{
   private:

      using timepoint_t = std::chrono::time_point<std::chrono::system_clock>;

      std::chrono::seconds timeout  = std::chrono::seconds::max(); ///< how long to wait before calling lock_all()
      timepoint_t timeout_time      = timepoint_t::max(); ///< when to call lock_all()

      bool stop_requested = false;

      types::method_type lock_method;
      types::method_type notification_method;

      std::mutex methods_mutex;
      std::unique_ptr<std::thread> notification_thread;

      void check_timeout_impl( bool allow_update_timeout_time );

   public:

      time_manager( types::method_type&& lock_method );
      ~time_manager();

      void set_timeout( const std::chrono::seconds& t );

      /// Verify timeout has not occurred and reset timeout if not.
      /// Calls lock_all() if timeout has passed.
      void check_timeout();
      info get_info();
};

} //beekeeper

