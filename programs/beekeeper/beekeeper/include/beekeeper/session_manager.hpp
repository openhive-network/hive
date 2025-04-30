#pragma once

#include <core/session_manager_base.hpp>

#include <beekeeper/mutex_handler.hpp>

#include <string>

namespace beekeeper {

class session_manager: public session_manager_base
{
  private:

    std::shared_ptr<mutex_handler> mtx_handler;

  protected:

    std::shared_ptr<session_base> create_session( const std::optional<std::string>& notifications_endpoint, const std::string& token, std::shared_ptr<time_manager_base> not_used_time, const boost::filesystem::path& wallet_directory ) override;

    void lock( const std::string& token ) override;

  public:

    session_manager( const std::optional<std::string>& notifications_endpoint, std::shared_ptr<mutex_handler> mtx_handler );
    ~session_manager() override {}
};

} //beekeeper
