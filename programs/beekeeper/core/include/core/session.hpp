#pragma once

#include <core/time_manager.hpp>
#include <core/utilities.hpp>
#include <core/wallet_manager_impl.hpp>

#include <string>

namespace beekeeper {

class session
{
  private:


    std::chrono::seconds timeout    = std::chrono::seconds::max(); ///< how long to wait before calling lock_all()
    types::timepoint_t timeout_time = types::timepoint_t::max(); ///< when to call lock_all()

    const std::string token;

    std::shared_ptr<wallet_manager_impl> wallet_mgr;

    void check_timeout_impl( bool allow_update_timeout_time );

    std::shared_ptr<time_manager> time;

    hive::utilities::notifications::detail::notification_handler notification_handler;

  public:

    session( const std::string& notifications_endpoint, const std::string& token, std::shared_ptr<time_manager> time );

    void set_timeout( const std::chrono::seconds& t );
    void check_timeout();
    info get_info();

    std::shared_ptr<wallet_manager_impl> get_wallet_manager();

    hive::utilities::notifications::detail::notification_handler& get_notification_handler();
};

} //beekeeper
