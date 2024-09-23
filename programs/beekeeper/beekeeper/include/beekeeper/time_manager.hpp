#pragma once

#include <core/time_manager_base.hpp>

namespace beekeeper {

class time_manager: public time_manager_base
{
  private:

    std::mutex mtx;

  public:

    ~time_manager() override {};

    void add( const std::string& token, types::lock_method_type&& lock_method ) override;
    void run( const std::string& token, const types::timepoint_t& new_time, bool move_time_forward, bool allow_lock ) override;
    void close( const std::string& token ) override;
};

} //beekeeper
