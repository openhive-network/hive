#pragma once

#include <fc/reflect/reflect.hpp>

#include <chrono>
#include <thread>
#include <string>
#include <functional>
#include <mutex>

namespace beekeeper {

struct info
{
  std::string now;
  std::string timeout_time;
};

class time_manager
{
   private:

      using timepoint_t = std::chrono::time_point<std::chrono::system_clock>;
      using method_type = std::function<void()>;

      std::chrono::seconds timeout  = std::chrono::seconds::max(); ///< how long to wait before calling lock_all()
      timepoint_t timeout_time      = timepoint_t::max(); ///< when to call lock_all()

      bool stop_requested = false;

      method_type lock_method;
      method_type notification_method;

      std::mutex methods_mutex;
      std::unique_ptr<std::thread> notification_thread;

      void check_timeout_impl( bool allow_update_timeout_time );

   public:

      time_manager( method_type&& lock_method );
      ~time_manager();

      void set_timeout( const std::chrono::seconds& t );

      /// Verify timeout has not occurred and reset timeout if not.
      /// Calls lock_all() if timeout has passed.
      void check_timeout();
      info get_info();
};

} //beekeeper

FC_REFLECT( beekeeper::info, (now)(timeout_time) )
