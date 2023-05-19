#pragma once

#include <beekeeper/time_manager.hpp>
#include <beekeeper/utilities.hpp>

#include <string>

namespace beekeeper {

class session
{
  private:

    const std::string notifications_endpoint;

    time_manager time;

  public:

    session( const std::string& token, const std::string& notifications_endpoint, types::lock_method_type&& lock_method );

    void set_timeout( const std::chrono::seconds& t );
    void check_timeout();
    info get_info();
};

} //beekeeper
