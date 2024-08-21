#pragma once

#include <core/wallet_content_handler.hpp>

namespace beekeeper {

class wallet_content_handler_session
{
  public:

    using ptr = std::shared_ptr<wallet_content_handler_session>;

  private:

    bool locked = true;
    wallet_content_handler::ptr content;

  public:

    wallet_content_handler_session( bool locked, wallet_content_handler::ptr& content )
    : locked( locked ), content( content )
    {
    }

    void set_locked( bool value ) { locked = value; }
    bool is_locked() const { return locked; }

    wallet_content_handler::ptr& get_content()
    {
      FC_ASSERT( content );
      return content;
    };
  };

class wallet_content_handlers_deliverer
{
   public:

      using wallet_data = std::map<std::string/*wallet_name*/, wallet_content_handler_session::ptr>;

   private:

      std::map<std::string, wallet_content_handler::ptr> items;

      using session_wallet_data = std::map<std::string/*token*/, wallet_data>;

      session_wallet_data session_items;

   private:

      void add( const std::string& token, const std::string& wallet_name, bool locked, wallet_content_handler::ptr& content );
   public:

      bool empty( const std::string& token );
      wallet_data get_wallets( const std::string& token );
      std::optional<wallet_content_handler_session::ptr> find( const std::string& token, const std::string& wallet_name );
      void erase( const std::string& token, const std::string& wallet_name );

      void create( const std::string& token, const std::string& wallet_name, const std::string& wallet_file_name, const std::string& password );
      void open( const std::string& token, const std::string& wallet_name, const std::string& wallet_file_name );
      void unlock( const std::string& wallet_name, const std::string& password, wallet_content_handler_session::ptr& wallet );
      void lock( wallet_content_handler_session::ptr& wallet );
};

} //wallet_content_handler
