#pragma once

#include <beekeeper/session.hpp>

#include <string>

namespace beekeeper {

class session_manager
{
  private:

    using items = std::map<std::string/*token*/, std::unique_ptr<session>>;

    const unsigned int token_length = 32;

    items sessions;

    items::iterator get_session( const std::string& token );

  public:

    std::string create_session( const std::string& salt, const std::string& notifications_endpoint, types::lock_method_type&& lock_method );
    void close_session( const std::string& token );
    bool empty() const;

    void set_timeout( const std::string& token, const std::chrono::seconds& t );
    void check_timeout( const std::string& token );
    info get_info( const std::string& token );
};

} //beekeeper
