#ifdef IS_TEST_NET
#include <boost/test/unit_test.hpp>

#include "../db_fixture/hived_fixture.hpp"

#include "../utils/beekeeper_mgr.hpp"

#include <core/beekeeper_wallet_manager.hpp>

#include <beekeeper/beekeeper_instance.hpp>
#include <beekeeper/beekeeper_wallet_api.hpp>

#include <boost/scope_exit.hpp>

#include <chrono>
#include <thread>

using namespace hive::chain;

using beekeeper_wallet_manager  = beekeeper::beekeeper_wallet_manager;
using session_manager           = beekeeper::session_manager_base;
using beekeeper_instance        = beekeeper::beekeeper_instance;
using beekeeper_wallet_api      = beekeeper::beekeeper_wallet_api;

BOOST_FIXTURE_TEST_SUITE( beekeeper_api_tests, json_rpc_database_fixture )

BOOST_AUTO_TEST_CASE(beekeeper_api_unlock_blocking)
{
  try
  {
    auto _sleep = []( uint32_t delay )
    {
      std::this_thread::sleep_for( std::chrono::milliseconds( delay ) );
    };

    struct wallet_data
    {
      std::string name;
      std::string password;
    };

    struct wallet_status
    {
      std::string name;
      bool unlocked = false;
    };

    struct cmp
    {
        bool operator()( const wallet_status& a, const wallet_status& b ) const
        {
          return a.name < b.name;
        }
    };
    using set_type = std::set<wallet_status, cmp>;

    std::vector<wallet_data> _wallets{ {"w0"}, {"w1"}, {"w2"} };

    test_utils::beekeeper_mgr b_mgr;
    b_mgr.remove_wallets();

    uint64_t _interval = 500;

    beekeeper::beekeeper_wallet_api _api( b_mgr.create_wallet_ptr( theApp, 900, 3 ), theApp, _interval );

    auto _list_created_wallets_checker = [&_api]( const std::string& token, const set_type& unlock_statuses )
    {
      std::vector<beekeeper::wallet_details> _wallets = _api.list_created_wallets( beekeeper::list_wallets_args{ token } ).wallets;
      BOOST_REQUIRE( _wallets.size() == unlock_statuses.size() );

      for( auto& item : _wallets )
      {
        auto _found = unlock_statuses.find( { item.name } );
        BOOST_REQUIRE( _found != unlock_statuses.end() );
        BOOST_REQUIRE_EQUAL( item.unlocked, _found->unlocked );
      }
    };

    //************preparation************
    std::string _token = _api.create_session( beekeeper::create_session_args{ "this is salt" } ).token;
    for( size_t i = 0; i < _wallets.size(); ++i )
    {
      auto _password = _api.create( beekeeper::create_args{ _token, _wallets[i].name } ).password;
      BOOST_REQUIRE( !_password.empty() );
      _wallets[i].password = _password;
    }
    _list_created_wallets_checker( _token, { {"w0", true}, {"w1", true}, {"w2", true} } );
    //************end of preparation************

    {
      BOOST_TEST_MESSAGE( "lock_all" );
      _api.lock_all( beekeeper::lock_all_args{ _token } );
      _list_created_wallets_checker( _token, { {"w0", false}, {"w1", false}, {"w2", false} } );
    }
    {
      {
        BOOST_TEST_MESSAGE( "sleep: _interval + 5" );
        _sleep(_interval + 5 );
        BOOST_TEST_MESSAGE( "unlock: _wallets[0]" );
        _api.unlock( beekeeper::unlock_args{ _token, _wallets[0].name, _wallets[0].password } );
        _list_created_wallets_checker( _token, { {"w0", true}, {"w1", false}, {"w2", false} } );
      }
      {
        BOOST_TEST_MESSAGE( "sleep: " + std::to_string( _interval / 2 ) );
        _sleep( _interval / 2 );
        BOOST_TEST_MESSAGE( "unlock: _wallets[1]" );
        _api.unlock( beekeeper::unlock_args{ _token, _wallets[1].name, _wallets[1].password } );
        _list_created_wallets_checker( _token, { {"w0", true}, {"w1", true}, {"w2", false} } );
      }
      {
        BOOST_TEST_MESSAGE( "sleep: 5" );
        _sleep( 5 );
        BOOST_TEST_MESSAGE( "unlock: _wallets[2]" );
        _api.unlock( beekeeper::unlock_args{ _token, _wallets[2].name, _wallets[2].password } );
        _list_created_wallets_checker( _token, { {"w0", true}, {"w1", true}, {"w2", true} } );
      }
    }
    {
      BOOST_TEST_MESSAGE( "lock_all" );
      _api.lock_all( beekeeper::lock_all_args{ _token } );
      _list_created_wallets_checker( _token, { {"w0", false}, {"w1", false}, {"w2", false} } );
    }
    {
      {
        BOOST_TEST_MESSAGE( "sleep: " + std::to_string( _interval + 5 ) );
        _sleep(_interval + 5 );
        BOOST_TEST_MESSAGE( "unlock: _wallets[0]" );
        bool _unlock_passed = true;
        try
        {
          _api.unlock( beekeeper::unlock_args{ _token, _wallets[0].name, _wallets[1].password } );
        }
        catch( const fc::exception& e )
        {
          _unlock_passed = false;
          BOOST_TEST_MESSAGE( e.to_string() );
          BOOST_REQUIRE( e.to_string().find( "Invalid password for wallet:" ) != std::string::npos );
        }
        BOOST_REQUIRE( _unlock_passed == false );
        _list_created_wallets_checker( _token, { {"w0", false}, {"w1", false}, {"w2", false} } );
      }
      {
        BOOST_TEST_MESSAGE( "sleep: " + std::to_string( _interval / 2 ) );
        _sleep( _interval / 2 );
        BOOST_TEST_MESSAGE( "unlock: _wallets[1]" );
        try
        {
          _api.unlock( beekeeper::unlock_args{ _token, _wallets[1].name, _wallets[1].password } );
        }
        catch( const fc::exception& e )
        {
          BOOST_TEST_MESSAGE( e.to_string() );
          BOOST_REQUIRE( e.to_string().find( "unlock is not accessible" ) != std::string::npos );
        }
        _list_created_wallets_checker( _token, { {"w0", false}, {"w1", false}, {"w2", false} } );
      }
      {
        BOOST_TEST_MESSAGE( "sleep: 5" );
        _sleep( 5 );
        BOOST_TEST_MESSAGE( "unlock: _wallets[2]" );
        try
        {
          _api.unlock( beekeeper::unlock_args{ _token, _wallets[2].name, _wallets[2].password } );
        }
        catch( const fc::exception& e )
        {
          BOOST_TEST_MESSAGE( e.to_string() );
          BOOST_REQUIRE( e.to_string().find( "unlock is not accessible" ) != std::string::npos );
        }
        _list_created_wallets_checker( _token, { {"w0", false}, {"w1", false}, {"w2", false} } );
      }
      {
        BOOST_TEST_MESSAGE( "sleep: " + std::to_string( _interval / 2 ) );
        _sleep( _interval / 2 );
        BOOST_TEST_MESSAGE( "unlock: _wallets[1]" );
        _api.unlock( beekeeper::unlock_args{ _token, _wallets[1].name, _wallets[1].password } );
        _list_created_wallets_checker( _token, { {"w0", false}, {"w1", true}, {"w2", false} } );
      }
    }
  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(beekeeper_api_endpoints)
{
  try
  {
    test_utils::beekeeper_mgr b_mgr;
    b_mgr.remove_wallets();

    uint64_t _interval = 500;
    beekeeper::beekeeper_wallet_api _api( b_mgr.create_wallet_ptr( theApp, 900, 3 ), theApp, _interval );

    std::string _wallet_name                = "w0";
    std::string _private_key                = "5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n";
    std::string _public_key                 = "STM6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4";
    hive::protocol::digest_type _sig_digest = hive::protocol::digest_type( "9b29ba0710af3918e81d7b935556d7ab205d8a8f5ca2e2427535980c2e8bdaff" );

    std::string _token = _api.create_session( beekeeper::create_session_args{ "this is salt" } ).token;
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
                              e.to_string().find( "Wallet is locked" )      != std::string::npos
                          );
            }
          }break;
          case 6:
          {
            try
            {
              _api.remove_key( beekeeper::remove_key_args{ _token, _wallet_name, _public_key } );
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
              BOOST_REQUIRE(  e.to_string().find( "not found in unlocked wallets" )  != std::string::npos );
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

struct password
{
  std::mutex mtx;
  std::vector<std::string> tokens;

  auto add( const std::string& token )
  {
    std::lock_guard<std::mutex> _guard( mtx );
    tokens.emplace_back( token );
  };

  auto get( bool remove = true )
  {
    std::string _token;
    std::lock_guard<std::mutex> _guard( mtx );
    if( !tokens.empty() )
    {
      auto _idx = rand() % tokens.size();
      _token = tokens[ _idx ];

      if( remove )
        tokens.erase( tokens.begin() + _idx );
    }
    return _token;
  };
};

BOOST_AUTO_TEST_CASE(beekeeper_api_sessions_create_close)
{
  try {
    test_utils::beekeeper_mgr b_mgr;
    b_mgr.remove_wallets();

    uint64_t _interval = 500;
    beekeeper::beekeeper_wallet_api _api( b_mgr.create_wallet_ptr( theApp, 900, 64, [](){} ), theApp, _interval );

    std::srand( time(0) );

    struct counters
    {
      size_t limit = 1000;

      std::atomic_size_t create_session_cnt       = { 0 };
      std::atomic_size_t create_session_error_cnt = { 0 };
      std::atomic_size_t close_session_cnt        = { 0 };
      std::atomic_size_t close_session_error_cnt  = { 0 };

      void inc( std::atomic_size_t& val )
      {
        val.store( val.load() + 1 );
      }
    };
    counters _cnts;
    password _password;

    const uint32_t _nr_threads = 10;

    std::vector<std::shared_ptr<std::thread>> threads;

    auto _create_session = [&]()
    {
      while( _cnts.create_session_cnt + _cnts.close_session_cnt < _cnts.limit )
      {
        _cnts.inc( _cnts.create_session_cnt );

        try
        {
          _password.add( _api.create_session( beekeeper::create_session_args{ "this is salt" } ).token );
        }
        catch( const fc::exception& e )
        {
          BOOST_TEST_MESSAGE( e.to_detail_string() );
          BOOST_REQUIRE(  e.to_string().find( "Number of concurrent sessions reached a limit" )  != std::string::npos );
          _cnts.inc( _cnts.create_session_error_cnt );
        }
      }
    };

    auto _close_session = [&]()
    {
      while( _cnts.create_session_cnt + _cnts.close_session_cnt < _cnts.limit )
      {
        _cnts.inc( _cnts.close_session_cnt );

        try
        {
          _api.close_session( beekeeper::close_session_args{ _password.get() } );
        }
        catch( const fc::exception& e )
        {
          BOOST_TEST_MESSAGE( e.to_detail_string() );
          BOOST_REQUIRE( e.to_string().find( "A session attached to" )  != std::string::npos );
          _cnts.inc( _cnts.close_session_error_cnt );
        }
      }
    };

    for( size_t i = 0; i < _nr_threads; ++i )
      threads.emplace_back( std::make_shared<std::thread>( [&]()
      {
        if( i % 2 == 0 )
          _create_session();
        else
          _close_session();
      } ) );

    BOOST_SCOPE_EXIT( &threads, &_cnts )
    {
      for( auto& thread : threads )
        thread->join();

      BOOST_TEST_MESSAGE("_create_session_cnt: " + std::to_string( _cnts.create_session_cnt.load() ) );
      BOOST_TEST_MESSAGE("_close_session_cnt: " + std::to_string( _cnts.close_session_cnt.load() ) );

      BOOST_TEST_MESSAGE("_create_session_error_cnt: " + std::to_string( _cnts.create_session_error_cnt.load() ) );
      BOOST_TEST_MESSAGE("_close_session_error_cnt: " + std::to_string( _cnts.close_session_error_cnt.load() ) );

      BOOST_REQUIRE_EQUAL( _cnts.create_session_cnt.load() + _cnts.close_session_cnt.load(), _cnts.limit );
    } BOOST_SCOPE_EXIT_END

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(beekeeper_api_sessions)
{
  try
  {
    test_utils::beekeeper_mgr b_mgr;
    b_mgr.remove_wallets();

    uint64_t _interval = 500;
    beekeeper::beekeeper_wallet_api _api( b_mgr.create_wallet_ptr( theApp, 900, 3, [](){} ), theApp, _interval );

    password _password;

    const uint32_t _nr_threads = 10;

    std::vector<std::shared_ptr<std::thread>> threads;

    std::string _public_key                 = "STM6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4";
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
              _password.add( _api.create_session( beekeeper::create_session_args{ "this is salt" } ).token );
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
              _api.set_timeout( beekeeper::set_timeout_args{ _password.get( false/*remove*/ ), 100 } );
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
              _api.get_info( beekeeper::get_info_args{ _password.get( false/*remove*/ ) } );
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
              _api.close_session( beekeeper::close_session_args{ _password.get() } );
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
              _api.sign_digest( beekeeper::sign_digest_args{ _password.get( false/*remove*/ ), _sig_digest, _public_key } );
            }
            catch( const fc::exception& e )
            {
              BOOST_REQUIRE(  e.to_string().find( "not found in unlocked wallets" )            != std::string::npos ||
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
    test_utils::beekeeper_mgr b_mgr;
    b_mgr.remove_wallets();

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

    uint64_t _interval = 500;
    beekeeper::beekeeper_wallet_api _api( b_mgr.create_wallet_ptr( theApp, 900, 3 ), theApp, _interval );

    std::string _token = _api.create_session( beekeeper::create_session_args{ "this is salt" } ).token;

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

BOOST_AUTO_TEST_CASE(beekeeper_api_performance_sign_transaction)
{
  try
  {
    test_utils::beekeeper_mgr b_mgr;
    b_mgr.remove_wallets();

    uint64_t _interval = 500;
    beekeeper::beekeeper_wallet_api _api( b_mgr.create_wallet_ptr( theApp, 900, 3 ), theApp, _interval );

    std::string _wallet_name                = "w0";
    std::string _private_key                = "5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n";
    std::string _public_key                 = "STM6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4";
    hive::protocol::digest_type _sig_digest = hive::protocol::digest_type( "9b29ba0710af3918e81d7b935556d7ab205d8a8f5ca2e2427535980c2e8bdaff" );

    std::string _token = _api.create_session( beekeeper::create_session_args{ "this is salt" } ).token;
    auto _password = _api.create( beekeeper::create_args{ _token, _wallet_name } ).password;
    BOOST_REQUIRE( !_password.empty() );

    _api.import_key( beekeeper::import_key_args{ _token, _wallet_name, _private_key } );

    auto _call = [&]( uint32_t nr_threads )
    {
      uint32_t _max = 50000 / nr_threads;
      for( uint32_t _cnt = 0; _cnt < _max; ++_cnt )
      {
        _api.sign_digest( beekeeper::sign_digest_args{ _token, _sig_digest, _public_key } );
      }
    };

    auto _execute_calls = [&_call]( uint32_t nr_threads )
    {
      std::vector<std::shared_ptr<std::thread>> threads;

      for( size_t i = 0; i < nr_threads; ++i )
        threads.emplace_back( std::make_shared<std::thread>( _call, nr_threads ) );

      for( auto& thread : threads )
        if( thread )
          thread->join();
    };

    int64_t _duration_1 = 0;
    int64_t _duration_10 = 0;
    {
      auto _start = std::chrono::high_resolution_clock::now();

      _execute_calls( 1 );

      _duration_1 = std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::high_resolution_clock::now() - _start ).count();
      BOOST_TEST_MESSAGE( std::to_string( _duration_1 ) + " [ms]" );
    }

    {
      auto _start = std::chrono::high_resolution_clock::now();

      _execute_calls( 10 );

      _duration_10 = std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::high_resolution_clock::now() - _start ).count();
      BOOST_TEST_MESSAGE( std::to_string( _duration_10 ) + " [ms]" );
    }

    /*
      AMD Ryzen 7 5800X 8-Core Processor
      _duration_1   = 1344 [ms]
      _duration_10  = 227 [ms]
    */
    BOOST_REQUIRE( _duration_10 * 3 < _duration_1 );

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(wallets_synchronization_threads)
{
  try
  {
    test_utils::beekeeper_mgr b_mgr;
    b_mgr.remove_wallets();

    const size_t _nr_threads = 10;

    std::string _wallet_name = "www";

    uint64_t _interval = 500;
    beekeeper::beekeeper_wallet_api _api( b_mgr.create_wallet_ptr( theApp, 900, 64 ), theApp, _interval );

    std::vector<std::string> _tokens;

    for( size_t i = 0; i < _nr_threads; ++i )
      _tokens.emplace_back( _api.create_session( beekeeper::create_session_args{ "this is salt", "127.0.0.1:666" } ).token );

    auto _password  = _api.create( beekeeper::create_args{ _tokens[0], _wallet_name } ).password;

    for( size_t i = 1; i < _nr_threads; ++i )
      _api.unlock( beekeeper::unlock_args{ _tokens[i], _wallet_name, _password } );

    std::vector<std::shared_ptr<std::thread>> threads;

    auto _call = [&]( int nr_thread )
    {
      size_t _max = 300;
      for( size_t _cnt = 0; _cnt < _max; ++_cnt )
      {
        if( nr_thread % 2 )
        {
          auto _priv = fc::ecc::private_key::generate();
          try
          {
            _api.import_key( beekeeper::import_key_args{ _tokens[nr_thread], _wallet_name, _priv.key_to_wif() } );
          }
          catch( fc::exception& e )
          {
            elog( "${e}", (e) );
            BOOST_REQUIRE( false );
          }
        }
        else
        {
          auto _keys = _api.get_public_keys( beekeeper::get_public_keys_args{ _tokens[nr_thread] } ).keys;
          if( !_keys.empty() )
          {
            beekeeper::public_key_details _key;

            if( _keys.size() == 1 )
              _key = *_keys.begin();
            else
              _key = *_keys.rbegin();

            try
            {
              _api.remove_key( beekeeper::remove_key_args{ _tokens[nr_thread], _wallet_name, _key.public_key } );
            }
            catch( fc::exception& e )
            {
              BOOST_REQUIRE( e.to_string().find( "Key not in wallet" ) != std::string::npos );
            }
          }
        }
      }
    };

    for( size_t i = 0; i < _nr_threads; ++i )
      threads.emplace_back( std::make_shared<std::thread>( _call, i ) );

    for( auto& thread : threads )
      if( thread )
        thread->join();

    auto _pattern_keys = _api.get_public_keys( beekeeper::get_public_keys_args{ _tokens[0] } ).keys;
    for( size_t i = 1; i < _nr_threads; ++i )
    {
      auto _keys = _api.get_public_keys( beekeeper::get_public_keys_args{ _tokens[i] } ).keys;

      BOOST_REQUIRE( _pattern_keys.size() == _keys.size() );

      auto _itr = _keys.begin();
      for( auto& item : _pattern_keys )
      {
        BOOST_REQUIRE( item.public_key == _itr->public_key );
        ++_itr;
      }
    }

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()
#endif
