#pragma once

#include <beekeeper/time_manager.hpp>
#include <beekeeper/utilities.hpp>

#include <string>

namespace beekeeper {

class session
{
  private:

    std::string notification_server;

    time_manager time;

  public:

    session( const std::string& notification_server, types::method_type&& lock_method );

    void set_timeout( const std::chrono::seconds& t );
    void check_timeout();
    info get_info();
};

} //beekeeper
