#ifdef IS_TEST_NET
#include <boost/test/unit_test.hpp>

#include "../db_fixture/hived_fixture.hpp"

#include <core/beekeeper_wallet_manager.hpp>

#include <beekeeper/session_manager.hpp>
#include <beekeeper/beekeeper_instance.hpp>
#include <beekeeper/beekeeper_wallet_api.hpp>

#include <boost/scope_exit.hpp>

using namespace hive::chain;

using beekeeper_wallet_manager  = beekeeper::beekeeper_wallet_manager;
using session_manager           = beekeeper::session_manager;
using beekeeper_instance        = beekeeper::beekeeper_instance;
using beekeeper_wallet_api      = beekeeper::beekeeper_wallet_api;

BOOST_FIXTURE_TEST_SUITE( beekeeper_api_tests, json_rpc_database_fixture )

std::shared_ptr<beekeeper_wallet_manager> create_wallet_ptr( appbase::application& app, const boost::filesystem::path& cmd_wallet_dir, uint64_t cmd_unlock_timeout, uint32_t cmd_session_limit, std::function<void()>&& method = [](){} )
{
  return std::shared_ptr<beekeeper_wallet_manager>( new beekeeper_wallet_manager( std::make_shared<session_manager>( "127.0.0.1:666" ), std::make_shared<beekeeper_instance>( app, cmd_wallet_dir, "127.0.0.1:666" ),
                                    cmd_wallet_dir, cmd_unlock_timeout, cmd_session_limit, std::move( method ) ) );
}

BOOST_AUTO_TEST_CASE(beekeeper_api_endpoints)
{
  try
  {
    if( fc::exists("w0.wallet") )
      fc::remove("w0.wallet");

    beekeeper::beekeeper_wallet_api _api( create_wallet_ptr( theApp, ".", 900, 3 ), theApp );

    std::string _wallet_name                = "w0";
    std::string _private_key                = "5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n";
    std::string _public_key                 = "6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4";
    hive::protocol::digest_type _sig_digest = hive::protocol::digest_type( "9b29ba0710af3918e81d7b935556d7ab205d8a8f5ca2e2427535980c2e8bdaff" );

    std::string _token = _api.create_session( beekeeper::create_session_args{ "this is salt", "127.0.0.1:666" } ).token;
    auto _password = _api.create( beekeeper::create_args{ _token, _wallet_name } ).password;
    BOOST_REQUIRE( !_password.empty() );

    _api.open( beekeeper::wallet_args{ _token, _wallet_name } );

    std::vector<beekeeper::wallet_details> _wallets = _api.list_wallets( beekeeper::list_wallets_args{ _token } ).wallets;
    BOOST_REQUIRE( _wallets.size() == 1 );
    BOOST_REQUIRE( _wallets[0].unlocked == false );

    const uint32_t _nr_threads = 10;

    std::vector<std::shared_ptr<std::thread>> threads;

    BOOST_SCOPE_EXIT(&threads)
    {
      for( auto& thread : threads )
        if( thread )
          thread->join();
    } BOOST_SCOPE_EXIT_END

    auto _call = [&]( int nr_thread )
    {
      uint32_t _max = 10000;
      for( uint32_t _cnt = 0; _cnt < _max; ++_cnt )
      {
        switch( nr_thread )
        {
          case 0:
          {
            try
            {
              _api.lock( beekeeper::lock_args{ _token, _wallet_name } );
            }
            catch( const fc::exception& e )
            {
              BOOST_REQUIRE(  e.to_string().find( "Unable to lock a locked wallet" )  != std::string::npos ||
                              e.to_string().find( "Wallet not found" )                != std::string::npos
                          );
            }
          }break;
          case 1:
          {
            try
            {
              _api.unlock( beekeeper::unlock_args{ _token, _wallet_name, _password } );
            }
            catch( const fc::exception& e )
            {
              BOOST_REQUIRE(  e.to_string().find( "Wallet is already unlocked" )  != std::string::npos ||
                              e.to_string().find( "unlock is not accessible" )    != std::string::npos
                          );
            }
          }break;
          case 2:
          {
            _api.lock_all( beekeeper::lock_all_args{ _token } );
          }break;
          case 3:
          {
            _api.list_wallets( beekeeper::list_wallets_args{ _token } );
          }break;
          case 4:
          {
            try
            {
              _api.get_public_keys( beekeeper::get_public_keys_args{ _token } );
            }
            catch( const fc::exception& e )
            {
              BOOST_REQUIRE(  e.to_string().find( "You don't have any wallet" )           != std::string::npos ||
                              e.to_string().find( "You don't have any unlocked wallet" )  != std::string::npos
                          );
            }
          }break;
          case 5:
          {
            try
            {
              _api.import_key( beekeeper::import_key_args{ _token, _wallet_name, _private_key } );
            }
            catch( const fc::exception& e )
            {
              BOOST_REQUIRE(  e.to_string().find( "Wallet not found" )      != std::string::npos ||
                              e.to_string().find( "Wallet is locked" )      != std::string::npos ||
                              e.to_string().find( "Key already in wallet" ) != std::string::npos
                          );
            }
          }break;
          case 6:
          {
            try
            {
              _api.remove_key( beekeeper::remove_key_args{ _token, _wallet_name, _password, _public_key } );
            }
            catch( const fc::exception& e )
            {
              BOOST_REQUIRE(  e.to_string().find( "Wallet not found" )  != std::string::npos ||
                              e.to_string().find( "Wallet is locked" )  != std::string::npos ||
                              e.to_string().find( "Key not in wallet" ) != std::string::npos
                          );
            }
          }break;
          case 7:
          {
            try
            {
              _api.sign_digest( beekeeper::sign_digest_args{ _token, _sig_digest, _public_key } );
            }
            catch( const fc::exception& e )
            {
              BOOST_REQUIRE(  e.to_string().find( "Public key not found in unlocked wallets" )  != std::string::npos );
            }
          }break;
          case 8:
          {
            _api.open( beekeeper::open_args{ _token, _wallet_name } );
          }break;
          case 9:
          {
            try
            {
              _api.close( beekeeper::close_args{ _token, _wallet_name } );
            }
            catch( const fc::exception& e )
            {
              BOOST_REQUIRE( e.to_string().find( "Wallet not found" )  != std::string::npos );
            }
          }break;
        }
      }
    };

    for( size_t i = 0; i < _nr_threads; ++i )
      threads.emplace_back( std::make_shared<std::thread>( _call, i ) );

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(beekeeper_api_sessions)
{
  try
  {
    if( fc::exists("w0.wallet") )
      fc::remove("w0.wallet");

    beekeeper::beekeeper_wallet_api _api( create_wallet_ptr( theApp, ".", 900, 3, [](){} ), theApp );

    std::mutex _mtx;

    std::vector<std::string> _v;

    auto _add_password = [&]( std::string&& password )
    {
      std::lock_guard<std::mutex> guard( _mtx );
      _v.emplace_back( password );
    };

    auto _get_password = [&]( bool remove = true )
    {
      std::lock_guard<std::mutex> guard( _mtx );

      if( _v.empty() )
        return std::string( "lackofpassword" );

      auto _idx = std::rand() % _v.size();
      std::string _result = _v[ _idx ];

      if( remove )
        _v.erase( _v.begin() + _idx );

      return _result;
      
    };

    const uint32_t _nr_threads = 10;

    std::vector<std::shared_ptr<std::thread>> threads;

    std::string _public_key                 = "6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4";
    hive::protocol::digest_type _sig_digest = hive::protocol::digest_type( "9b29ba0710af3918e81d7b935556d7ab205d8a8f5ca2e2427535980c2e8bdaff" );

    BOOST_SCOPE_EXIT(&threads)
    {
      for( auto& thread : threads )
        if( thread )
          thread->join();
    } BOOST_SCOPE_EXIT_END

    auto _call = [&]( int nr_thread )
    {
      uint32_t _max = 10000;
      for( uint32_t _cnt = 0; _cnt < _max; ++_cnt )
      {
        switch( nr_thread )
        {
          case 0: case 1: case 2: case 3:
          {
            try
            {
              _add_password( _api.create_session( beekeeper::create_session_args{ "this is salt", "127.0.0.1:666" } ).token );
            }
            catch( const fc::exception& e )
            {
              BOOST_REQUIRE( e.to_string().find( "Number of concurrent sessions reached a limit" )  != std::string::npos );
            }
          }break;

          case 4:
          {
            try
            {
              _api.set_timeout( beekeeper::set_timeout_args{ _get_password( false/*remove*/ ), 100 } );
            }
            catch( const fc::exception& e )
            {
              BOOST_REQUIRE( e.to_string().find( "A session attached to" )  != std::string::npos );
            }
          }break;

          case 5:
          {
            try
            {
              _api.get_info( beekeeper::get_info_args{ _get_password( false/*remove*/ ) } );
            }
            catch( const fc::exception& e )
            {
              BOOST_REQUIRE( e.to_string().find( "A session attached to" )  != std::string::npos );
            }
          }break;

          case 6: case 7: case 8:
          {
            try
            {
              _api.close_session( beekeeper::close_session_args{ _get_password() } );
            }
            catch( const fc::exception& e )
            {
              BOOST_REQUIRE( e.to_string().find( "A session attached to" )  != std::string::npos );
            }
          }break;

          case 9:
          {
            try
            {
              _api.sign_digest( beekeeper::sign_digest_args{ _get_password( false/*remove*/ ), _sig_digest, _public_key } );
            }
            catch( const fc::exception& e )
            {
              BOOST_REQUIRE(  e.to_string().find( "Public key not found in unlocked wallets" )            != std::string::npos ||
                              e.to_string().find( "A session attached to " )  != std::string::npos
                          );
            }
          }break;
        }
      }
    };

    for( size_t i = 0; i < _nr_threads; ++i )
      threads.emplace_back( std::make_shared<std::thread>( _call, i ) );

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(wallet_manager_threads_wallets)
{
  try
  {
    const uint32_t _nr_threads = 10;

    std::vector<std::string> _wallet_names;
    for( uint32_t i = 0; i < _nr_threads; ++i )
      _wallet_names.emplace_back( "w" + std::to_string(i) );

    auto _delete_wallet_file = []( const std::string& wallet_name )
    {
      try
      {
        fc::remove( wallet_name + ".wallet" );
      }
      catch(...)
      {
      }
    };

    for( auto& wallet_name : _wallet_names )
      _delete_wallet_file( wallet_name );

    beekeeper::beekeeper_wallet_api _api( create_wallet_ptr( theApp, ".", 900, 3 ), theApp );

    std::string _token = _api.create_session( beekeeper::create_session_args{ "this is salt", "127.0.0.1:666" } ).token;

    std::vector<std::shared_ptr<std::thread>> threads;

    BOOST_SCOPE_EXIT(&threads)
    {
      for( auto& thread : threads )
        if( thread )
          thread->join();
    } BOOST_SCOPE_EXIT_END

    auto _call = [&]( int nr_thread )
    {
      uint32_t _max = 1000;
      for( uint32_t _cnt = 0; _cnt < _max; ++_cnt )
      {
        switch( nr_thread )
        {
          case 0: case 1: case 2: case 3: case 4: case 5: case 6:
          {
            auto _idx = std::rand() % _wallet_names.size();
            try
            {
              auto _password = _api.create( beekeeper::create_args{ _token, _wallet_names[_idx] } ).password;
              BOOST_REQUIRE( !_password.empty() );
            }
            catch( const fc::exception& e )
            {
              BOOST_REQUIRE( e.to_string().find( "already exists at" ) != std::string::npos );
              _delete_wallet_file( _wallet_names[_idx] );
            }
          }break;
          case 7: case 8: case 9:
          {
            auto _idx = std::rand() % _wallet_names.size();
            _delete_wallet_file( _wallet_names[_idx] );
          }break;
        }
      }
    };

    for( size_t i = 0; i < _nr_threads; ++i )
      threads.emplace_back( std::make_shared<std::thread>( _call, i ) );

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()
#endif