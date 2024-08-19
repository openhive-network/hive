#pragma once

#include <core/session_base.hpp>

#include <string>

namespace beekeeper {

class beekeeper_instance_base;

class session_manager_base
{
  private:

    using items = std::map<std::string/*token*/, std::shared_ptr<session_base>>;

    const unsigned int token_length = 32;

    items sessions;

    std::shared_ptr<session_base> get_session( const std::string& token );

  protected:

    wallet_content_handlers_deliverer content_deliverer;

    std::shared_ptr<time_manager_base> time;

    virtual std::shared_ptr<session_base> create_session( const std::optional<std::string>& notifications_endpoint, const std::string& token, std::shared_ptr<time_manager_base> time, const boost::filesystem::path& wallet_directory );

  public:

    session_manager_base();
    virtual ~session_manager_base(){}

    std::string create_session( const std::optional<std::string>& salt, const std::optional<std::string>& notifications_endpoint, const boost::filesystem::path& wallet_directory );
    void close_session( const std::string& token );
    bool empty() const;

    void set_timeout( const std::string& token, const std::chrono::seconds& t );
    void check_timeout( const std::string& token );
    void refresh_timeout( const std::string& token );
    info get_info( const std::string& token );

    std::shared_ptr<wallet_manager_impl> get_wallet_manager( const std::string& token );
};

} //beekeeper
