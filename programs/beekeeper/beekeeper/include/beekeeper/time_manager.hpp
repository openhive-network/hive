#pragma once

#include <thread>

#include <core/time_manager_base.hpp>

namespace beekeeper {

class time_manager: public time_manager_base
{
  private:

    bool stop_requested = false;

    std::string error_notifications_endpoint;

    std::unique_ptr<std::thread> notification_thread;

  protected:

    void send_auto_lock_error_message( const std::string& message ) override;

  public:

    time_manager( const std::string& notifications_endpoint );
    ~time_manager() override;
};

} //beekeeper
