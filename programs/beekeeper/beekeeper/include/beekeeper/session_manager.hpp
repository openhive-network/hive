#pragma once

#include <core/session_manager_base.hpp>

#include <string>

namespace beekeeper {

class session_manager: public session_manager_base
{
  protected:

    std::shared_ptr<session_base> create_session( const std::string& notifications_endpoint, const std::string& token, std::shared_ptr<time_manager_base> time ) override;

  public:

    session_manager();
    ~session_manager() override {}
};

} //beekeeper
