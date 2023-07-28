#pragma once

#include <thread>

#include <core/time_manager_base.hpp>

namespace beekeeper {

class time_manager: public time_manager_base
{
  private:

    bool stop_requested = false;

    std::unique_ptr<std::thread> notification_thread;

  public:

    time_manager();
    ~time_manager() override;
};

} //beekeeper
