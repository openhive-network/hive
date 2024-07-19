#pragma once

#include <core/session_manager_base.hpp>

#include <string>

namespace beekeeper {

class session_manager: public session_manager_base
{
  protected:

    std::shared_ptr<session_base> create_session( const std::string& token, std::shared_ptr<time_manager_base> not_used_time, const boost::filesystem::path& wallet_directory ) override;

  public:

    session_manager();
    ~session_manager() override {}
};

} //beekeeper
