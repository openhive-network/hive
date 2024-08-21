#pragma once

#include <core/wallet_content_handler.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/tag.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/composite_key.hpp>

namespace beekeeper {

  using boost::multi_index::multi_index_container;
  using boost::multi_index::indexed_by;
  using boost::multi_index::ordered_unique;
  using boost::multi_index::tag;
  using boost::multi_index::const_mem_fun;
  using boost::multi_index::composite_key;

class wallet_content_handler_session
{
  public:

    using ptr = std::shared_ptr<wallet_content_handler_session>;

  private:

    const std::string token;
    const std::string wallet_name;

    bool locked = true;
    wallet_content_handler::ptr content;

  public:

    wallet_content_handler_session( const std::string& token, const std::string& wallet_name, bool locked, const wallet_content_handler::ptr& content )
    : token( token ), wallet_name( wallet_name ), locked( locked ), content( content )
    {
    }

    void set_locked( bool value ) { locked = value; }
    bool is_locked() const { return locked; }

    const wallet_content_handler::ptr& get_content() const
    {
      FC_ASSERT( content );
      return content;
    };

    const std::string& get_token() const { return token; }
    const std::string& get_wallet_name() const { return wallet_name; }
  };

struct by_wallet_name;
struct by_token;
struct by_token_wallet_name;

class wallet_content_handlers_deliverer
{
  public:

    typedef multi_index_container<
      wallet_content_handler_session,
      indexed_by<
        ordered_unique< tag< by_token >,
          const_mem_fun< wallet_content_handler_session, const std::string&, &wallet_content_handler_session::get_token > >,
        ordered_unique< tag< by_wallet_name >,
          const_mem_fun< wallet_content_handler_session, const std::string&, &wallet_content_handler_session::get_wallet_name > >,
        ordered_unique< tag< by_token_wallet_name >,
          composite_key< wallet_content_handler_session,
            const_mem_fun< wallet_content_handler_session, const std::string&, &wallet_content_handler_session::get_token>,
            const_mem_fun< wallet_content_handler_session, const std::string&, &wallet_content_handler_session::get_wallet_name>
          >
        >
      >
    > wallet_content_handler_session_index;

  public:

      using wallet_data = std::map<std::string/*wallet_name*/, wallet_content_handler_session::ptr>;

   private:

      std::map<std::string, wallet_content_handler::ptr> items;

      using session_wallet_data = std::map<std::string/*token*/, wallet_data>;

      session_wallet_data session_items;

   private:

      void add( const std::string& token, const std::string& wallet_name, bool locked, wallet_content_handler::ptr& content );

   public:

      wallet_content_handler_session_index complete_items;

      bool empty( const std::string& token );
      std::optional<wallet_content_handler_session::ptr> find( const std::string& token, const std::string& wallet_name );
      void erase( const std::string& token, const std::string& wallet_name );

      void create( const std::string& token, const std::string& wallet_name, const std::string& wallet_file_name, const std::string& password );
      void open( const std::string& token, const std::string& wallet_name, const std::string& wallet_file_name );
      void unlock( const std::string& wallet_name, const std::string& password, wallet_content_handler_session::ptr& wallet );
      void lock( wallet_content_handler_session::ptr& wallet );
};

} //wallet_content_handler
