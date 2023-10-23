#pragma once

#include <core/session_base.hpp>

#include <string>

namespace beekeeper {

class session_manager_base
{
  private:

    using items = std::map<std::string/*token*/, std::shared_ptr<session_base>>;

    const unsigned int token_length = 32;

    items sessions;

    std::shared_ptr<session_base> get_session( const std::string& token );

  protected:

    std::shared_ptr<time_manager_base> time;

    virtual std::shared_ptr<session_base> create_session( const std::string& notifications_endpoint, const std::string& token, std::shared_ptr<time_manager_base> time );

  public:

    session_manager_base();
    virtual ~session_manager_base(){}

    std::string create_session( const std::string& salt, const std::string& notifications_endpoint, const boost::filesystem::path& directory, const std::string& extension );
    void close_session( const std::string& token );
    bool empty() const;

    void set_timeout( const std::string& token, const std::chrono::seconds& t );
    void check_timeout( const std::string& token );
    info get_info( const std::string& token );

    std::shared_ptr<wallet_manager_impl> get_wallet_manager( const std::string& token );
};

} //beekeeper
