#pragma once

#include <beekeeper/session.hpp>

#include <string>

namespace beekeeper {

class session_manager
{
  private:

    using items = std::map<std::string/*token*/, std::shared_ptr<session>>;

    const unsigned int token_length = 32;

    std::shared_ptr<time_manager> time;

    items sessions;

    std::shared_ptr<session> get_session( const std::string& token );

  public:

    session_manager();

    std::string create_session( const std::string& salt, const std::string& notifications_endpoint );
    void close_session( const std::string& token );
    bool empty() const;

    void set_timeout( const std::string& token, const std::chrono::seconds& t );
    void check_timeout( const std::string& token );
    info get_info( const std::string& token );

    std::shared_ptr<wallet_manager_impl> get_wallet( const std::string& token );
};

} //beekeeper
