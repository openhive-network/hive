#ifdef IS_TEST_NET

#include <fc/filesystem.hpp>

#include <core/wallet_content_handler.hpp>
#include <core/beekeeper_wallet_manager.hpp>

#include <beekeeper/session_manager.hpp>
#include <beekeeper/beekeeper_instance.hpp>

using beekeeper_wallet_manager  = beekeeper::beekeeper_wallet_manager;
using wallet_content_handler    = beekeeper::wallet_content_handler;
using session_manager           = beekeeper::session_manager;
using beekeeper_instance        = beekeeper::beekeeper_instance;

namespace test_utils
{
  struct beekeeper_mgr
  {
    fc::path dir;

    beekeeper_mgr()
      : dir( fc::current_path() / "beekeeper-storage" )
    {
      fc::create_directories( dir );
    }

    void remove_wallets()
    {
      boost::filesystem::directory_iterator _end_itr;

      for( boost::filesystem::directory_iterator itr( dir ); itr != _end_itr; ++itr )
        boost::filesystem::remove_all( itr->path() );
    }

    void remove_wallet( const std::string& wallet_name )
    {
      try
      {
        auto _wallet_name = wallet_name + ".wallet";
        fc::remove( dir / _wallet_name );
      }
      catch(...)
      {
      }
    }

    bool exists_wallet( const std::string& wallet_name )
    {
      auto _wallet_name = wallet_name + ".wallet";
      return fc::exists( dir / _wallet_name );
    }

    beekeeper_wallet_manager create_wallet( appbase::application& app, uint64_t cmd_unlock_timeout, uint32_t cmd_session_limit, std::function<void()>&& method = [](){} )
    {
      return beekeeper_wallet_manager(  std::make_shared<session_manager>(),
                                        std::make_shared<beekeeper_instance>( app, dir, std::shared_ptr<status>() ),
                                        dir,
                                        cmd_unlock_timeout,
                                        cmd_session_limit,
                                        std::move( method )
                                      );
    }

    std::shared_ptr<beekeeper_wallet_manager> create_wallet_ptr( appbase::application& app, uint64_t cmd_unlock_timeout, uint32_t cmd_session_limit, std::function<void()>&& method = [](){} )
    {
      return std::shared_ptr<beekeeper_wallet_manager>( new beekeeper_wallet_manager( std::make_shared<session_manager>(),
                                                                                      std::make_shared<beekeeper_instance>( app, dir, std::shared_ptr<status>() ),
                                                                                      dir,
                                                                                      cmd_unlock_timeout,
                                                                                      cmd_session_limit,
                                                                                      std::move( method )
                                                                                    ) );
    }

  };
}

#endif
