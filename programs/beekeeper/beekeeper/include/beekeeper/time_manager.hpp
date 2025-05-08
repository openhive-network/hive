#pragma once

#include <thread>

#include <core/time_manager_base.hpp>

namespace beekeeper {

class time_manager: public time_manager_base
{
  private:

    bool stop_requested = false;

    std::unique_ptr<std::thread> lock_thread;
    std::mutex mtx;

  public:

    time_manager();
    ~time_manager() override;

    void add( const std::string& token, types::lock_method_type&& lock_method ) override;
    void change( const std::string& token, const types::timepoint_t& time, bool refresh_only_active ) override;

    void run() override;
    void run( const std::string& token ) override;

    void close( const std::string& token ) override;
};

} //beekeeper
