#pragma once

#include <core/time_manager_base.hpp>
#include <core/utilities.hpp>
#include <core/wallet_manager_impl.hpp>

#include <string>

namespace beekeeper {

class beekeeper_instance_base;

class session_base
{
  private:

    std::chrono::seconds timeout    = std::chrono::seconds::max(); ///< how long to wait before calling lock_all()
    types::timepoint_t timeout_time = types::timepoint_t::max(); ///< when to call lock_all()

    const std::string token;

    std::shared_ptr<wallet_manager_impl> wallet_mgr;

    std::shared_ptr<time_manager_base> time;

    void check_timeout_impl( bool allow_update_timeout_time );

    void refresh_timeout( bool refresh_only_active );

  protected:

    const std::string& get_token() const { return token; };

  public:

    session_base( const std::string& token, std::shared_ptr<time_manager_base> time, const boost::filesystem::path& wallet_directory );
    virtual ~session_base(){}

    void set_timeout( const std::chrono::seconds& t );
    void check_timeout();
    void refresh_timeout();

    info get_info();

    std::shared_ptr<wallet_manager_impl> get_wallet_manager();

    virtual void prepare_notifications(){};
};

} //beekeeper
