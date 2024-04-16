#pragma once

#include <core/session_base.hpp>

#include <hive/utilities/notifications.hpp>

namespace beekeeper {

class beekeeper_instance_base;

class session: public session_base
{
  private:

    hive::utilities::notifications::notification_handler_wrapper notification_handler;

  public:

    session( const std::optional<std::string>& notifications_endpoint, const std::string& token, std::shared_ptr<time_manager_base> time, const boost::filesystem::path& wallet_directory );

    virtual void prepare_notifications() override;
};

} //beekeeper
