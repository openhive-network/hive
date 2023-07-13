#pragma once

#include <core/time_manager_base.hpp>

#include <thread>

namespace beekeeper {

class time_manager: public time_manager_base
{
  private:
  
    std::unique_ptr<std::thread> notification_thread;

  protected:

    bool stop_requested = false;

  public:

    time_manager();
    ~time_manager() override;
};

} //beekeeper

