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

    session( const std::string& notifications_endpoint, const std::string& token, std::shared_ptr<time_manager_base> time );

    virtual void prepare_notifications(const beekeeper_instance_base& bk_instance, const boost::filesystem::path& directory, const std::string& extension ) override;
};

} //beekeeper
