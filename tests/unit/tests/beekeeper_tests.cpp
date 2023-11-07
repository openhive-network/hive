#ifdef IS_TEST_NET

#include <hive/protocol/hive_operations.hpp>
#include <hive/protocol/transaction.hpp>

#include <boost/test/unit_test.hpp>
#include <boost/algorithm/string.hpp>

#include <fc/crypto/elliptic.hpp>
#include <fc/filesystem.hpp>
#include <fc/io/json.hpp>

#include <core/beekeeper_wallet.hpp>
#include <core/beekeeper_wallet_manager.hpp>
#include <core/utilities.hpp>

#include <beekeeper_wasm/beekeeper_wasm_api.hpp>
#include <beekeeper_wasm/beekeeper_wasm_app.hpp>

#include <beekeeper/session_manager.hpp>
#include <beekeeper/beekeeper_instance.hpp>
#include <beekeeper/extended_api.hpp>

#include<thread>

using public_key_type           = beekeeper::public_key_type;
using private_key_type          = beekeeper::private_key_type;
using beekeeper_wallet_manager  = beekeeper::beekeeper_wallet_manager;
using beekeeper_api             = beekeeper::beekeeper_api;
using wallet_data               = beekeeper::wallet_data;
using beekeeper_wallet          = beekeeper::beekeeper_wallet;
using session_manager           = beekeeper::session_manager;
using beekeeper_instance        = beekeeper::beekeeper_instance;

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

  beekeeper_wallet_manager create_wallet( appbase::application& app, uint64_t cmd_unlock_timeout, uint32_t cmd_session_limit, std::function<void()>&& method = [](){} )
  {
    return beekeeper_wallet_manager( std::make_shared<session_manager>( "" ), std::make_shared<beekeeper_instance>( app, dir, "" ),
                                      dir, cmd_unlock_timeout, cmd_session_limit, std::move( method ) );
  }
};

BOOST_AUTO_TEST_SUITE(beekeeper_tests)

/// Test creating the wallet
BOOST_AUTO_TEST_CASE(wallet_test)
{ try {
  beekeeper_mgr b_mgr;
  b_mgr.remove_wallets();

  wallet_data d;
  beekeeper_wallet wallet(d);
  BOOST_REQUIRE(wallet.is_locked());

  wallet.set_password("pass");
  BOOST_REQUIRE(wallet.is_locked());

  wallet.unlock("pass");
  BOOST_REQUIRE(!wallet.is_locked());

  auto _wallet_file_name = ( b_mgr.dir / "test" ).string();

  wallet.set_wallet_filename( _wallet_file_name );
  BOOST_REQUIRE_EQUAL( _wallet_file_name , wallet.get_wallet_filename() );

  BOOST_REQUIRE_EQUAL(0u, wallet.list_keys().size());

  auto priv = fc::ecc::private_key::generate();
  auto pub = priv.get_public_key();
  auto wif = priv.key_to_wif();
  wallet.import_key(wif);
  BOOST_REQUIRE_EQUAL(1u, wallet.list_keys().size());

  auto privCopy = wallet.get_private_key(pub);
  BOOST_REQUIRE_EQUAL(wif, privCopy.key_to_wif());

  wallet.lock();
  BOOST_REQUIRE(wallet.is_locked());
  wallet.unlock("pass");
  BOOST_REQUIRE_EQUAL(1u, wallet.list_keys().size());
  wallet.save_wallet_file( _wallet_file_name );
  BOOST_REQUIRE( fc::exists( _wallet_file_name ) );

  wallet_data d2;
  beekeeper_wallet wallet2(d2);

  BOOST_REQUIRE(wallet2.is_locked());
  wallet2.load_wallet_file( _wallet_file_name );
  BOOST_REQUIRE(wallet2.is_locked());

  wallet2.unlock("pass");
  BOOST_REQUIRE_EQUAL(1u, wallet2.list_keys().size());

  auto privCopy2 = wallet2.get_private_key(pub);
  BOOST_REQUIRE_EQUAL(wif, privCopy2.key_to_wif());

} FC_LOG_AND_RETHROW() }

/// Test wallet manager
BOOST_AUTO_TEST_CASE(wallet_manager_test)
{ try {
  beekeeper_mgr b_mgr;
  b_mgr.remove_wallets();

  appbase::application app;

  const auto key1_str = "5JktVNHnRX48BUdtewU7N1CyL4Z886c42x7wYW7XhNWkDQRhdcS";
  const auto key2_str = "5Ju5RTcVDo35ndtzHioPMgebvBM6LkJ6tvuU6LTNQv8yaz3ggZr";
  const auto key3_str = "5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3";

  const auto key1 = private_key_type::wif_to_key( key1_str ).value();
  const auto key2 = private_key_type::wif_to_key( key2_str ).value();
  const auto key3 = private_key_type::wif_to_key( key3_str ).value();

  beekeeper_wallet_manager wm = b_mgr.create_wallet( app, 900, 3 );

  BOOST_REQUIRE( wm.start() );
  std::string _token = wm.create_session( "this is salt", "" );

  BOOST_REQUIRE_EQUAL(0u, wm.list_wallets(_token).size());
  BOOST_REQUIRE_THROW(wm.get_public_keys(_token, std::optional<std::string>()), fc::exception);
  BOOST_REQUIRE_THROW(wm.get_public_keys(_token, std::optional<std::string>("avocado")), fc::exception);
  BOOST_REQUIRE_NO_THROW(wm.lock_all(_token));

  BOOST_REQUIRE_THROW(wm.lock(_token, "test"), fc::exception);
  BOOST_REQUIRE_THROW(wm.unlock(_token, "test", "pw"), fc::exception);
  BOOST_REQUIRE_THROW(wm.import_key(_token, "test", "pw"), fc::exception);

  auto pw = wm.create(_token, "test", std::optional<std::string>());
  BOOST_REQUIRE(!pw.empty());
  BOOST_REQUIRE_EQUAL(0u, pw.find("PW")); // starts with PW
  BOOST_REQUIRE_EQUAL(1u, wm.list_wallets(_token).size());
  // wallet has no keys when it is created
  BOOST_REQUIRE_EQUAL(0u, wm.get_public_keys(_token, std::optional<std::string>()).size());
  BOOST_REQUIRE_EQUAL(0u, wm.get_public_keys(_token, std::optional<std::string>("test")).size());
  BOOST_REQUIRE_EQUAL(0u, wm.list_keys(_token, "test", pw).size());
  BOOST_REQUIRE(wm.list_wallets(_token)[0].unlocked);
  wm.lock(_token, "test");
  BOOST_REQUIRE(!wm.list_wallets(_token)[0].unlocked);
  wm.unlock(_token, "test", pw);
  BOOST_REQUIRE_THROW(wm.unlock(_token, "test", pw), fc::exception);
  BOOST_REQUIRE(wm.list_wallets(_token)[0].unlocked);
  wm.import_key(_token, "test", key1_str);
  BOOST_REQUIRE_EQUAL(1u, wm.get_public_keys(_token, std::optional<std::string>()).size());
  BOOST_REQUIRE_EQUAL(1u, wm.get_public_keys(_token, std::optional<std::string>( "test" )).size());
  BOOST_REQUIRE_THROW(wm.get_public_keys(_token, std::optional<std::string>("avocado")), fc::exception);
  auto keys = wm.list_keys(_token, "test", pw);

  auto pub_pri_pair = []( const private_key_type& private_key ) -> auto
  {
      return std::pair<public_key_type, private_key_type>( private_key.get_public_key(), private_key );
  };

  auto cmp_keys = [&]( const private_key_type& private_key, const std::map<public_key_type, private_key_type>& keys )
  {
    return std::find_if( keys.begin(), keys.end(), [&]( const std::pair<public_key_type, private_key_type>& item )
    {
      return pub_pri_pair( private_key ) == item;
    });
  };

  BOOST_REQUIRE( cmp_keys( key1, keys ) != keys.end() );

  wm.import_key(_token, "test", key2_str);
  keys = wm.list_keys(_token, "test", pw);
  BOOST_REQUIRE( cmp_keys( key1, keys ) != keys.end() );
  BOOST_REQUIRE( cmp_keys( key2, keys ) != keys.end() );
  // key3 was not automatically imported
  BOOST_REQUIRE( cmp_keys( key3, keys ) == keys.end() );

  wm.remove_key(_token, "test", pw, beekeeper::utility::public_key::to_string( pub_pri_pair(key2).first ) );
  BOOST_REQUIRE_EQUAL(1u, wm.get_public_keys(_token, std::optional<std::string>()).size());
  keys = wm.list_keys(_token, "test", pw);
  BOOST_REQUIRE( cmp_keys( key2, keys ) == keys.end() );
  wm.import_key(_token, "test", key2_str);
  BOOST_REQUIRE_EQUAL(2u, wm.get_public_keys(_token, std::optional<std::string>()).size());
  keys = wm.list_keys(_token, "test", pw);
  BOOST_REQUIRE( cmp_keys( key2, keys ) != keys.end() );
  BOOST_REQUIRE_THROW(wm.remove_key(_token, "test", pw, beekeeper::utility::public_key::to_string( pub_pri_pair(key3).first ) ), fc::exception);
  BOOST_REQUIRE_EQUAL(2u, wm.get_public_keys(_token, std::optional<std::string>()).size());
  BOOST_REQUIRE_THROW(wm.remove_key(_token, "test", "PWnogood", beekeeper::utility::public_key::to_string( pub_pri_pair(key2).first ) ), fc::exception);
  BOOST_REQUIRE_EQUAL(2u, wm.get_public_keys(_token, std::optional<std::string>()).size());

  wm.lock(_token, "test");
  BOOST_REQUIRE_THROW(wm.list_keys(_token, "test", pw), fc::exception);
  BOOST_REQUIRE_THROW(wm.get_public_keys(_token, std::optional<std::string>()), fc::exception);
  wm.unlock(_token, "test", pw);
  BOOST_REQUIRE_EQUAL(2u, wm.get_public_keys(_token, std::optional<std::string>()).size());
  BOOST_REQUIRE_EQUAL(2u, wm.list_keys(_token, "test", pw).size());
  wm.lock_all(_token);
  BOOST_REQUIRE_THROW(wm.get_public_keys(_token, std::optional<std::string>()), fc::exception);
  BOOST_REQUIRE(!wm.list_wallets(_token)[0].unlocked);

  auto pw2 = wm.create(_token, "test2", std::optional<std::string>());
  BOOST_REQUIRE_EQUAL(2u, wm.list_wallets(_token).size());
  // wallet has no keys when it is created
  BOOST_REQUIRE_EQUAL(0u, wm.get_public_keys(_token, std::optional<std::string>()).size());
  wm.import_key(_token, "test2", key3_str);
  BOOST_REQUIRE_EQUAL(1u, wm.get_public_keys(_token, std::optional<std::string>()).size());
  BOOST_REQUIRE_THROW(wm.import_key(_token, "test2", key3_str), fc::exception);
  keys = wm.list_keys(_token, "test2", pw2);
  BOOST_REQUIRE( cmp_keys( key1, keys ) == keys.end() );
  BOOST_REQUIRE( cmp_keys( key2, keys ) == keys.end() );
  BOOST_REQUIRE( cmp_keys( key3, keys ) != keys.end() );
  wm.unlock(_token, "test", pw);
  keys = wm.list_keys(_token, "test", pw);
  auto keys2 = wm.list_keys(_token, "test2", pw2);
  keys.insert(keys2.begin(), keys2.end());
  BOOST_REQUIRE( cmp_keys( key1, keys ) != keys.end() );
  BOOST_REQUIRE( cmp_keys( key2, keys ) != keys.end() );
  BOOST_REQUIRE( cmp_keys( key3, keys ) != keys.end() );
  BOOST_REQUIRE_EQUAL(3u, keys.size());

  BOOST_REQUIRE_THROW(wm.list_keys(_token, "test2", "PWnogood"), fc::exception);

  BOOST_REQUIRE_EQUAL(3u, wm.get_public_keys(_token, std::optional<std::string>()).size());
  wm.set_timeout(_token, 0);
  BOOST_REQUIRE_THROW(wm.get_public_keys(_token, std::optional<std::string>()), fc::exception);
  BOOST_REQUIRE_THROW(wm.list_keys(_token, "test", pw), fc::exception);

  wm.set_timeout(_token, 15);

  wm.create(_token, "testgen", std::optional<std::string>());
  wm.lock(_token, "testgen");
  fc::remove( b_mgr.dir / "testgen.wallet" );

  pw = wm.create(_token, "testgen", std::optional<std::string>());

  wm.lock(_token, "testgen");
  BOOST_REQUIRE(fc::exists( b_mgr.dir / "testgen.wallet" ));

} FC_LOG_AND_RETHROW() }

/// Test wallet manager
BOOST_AUTO_TEST_CASE(wallet_manager_create_test)
{
  try {
    beekeeper_mgr b_mgr;
    b_mgr.remove_wallets();

    appbase::application app;

    beekeeper_wallet_manager wm = b_mgr.create_wallet( app, 900, 3 );

    BOOST_REQUIRE( wm.start() );
    std::string _token = wm.create_session( "this is salt", "" );

    wm.create(_token, "test", std::optional<std::string>());
    constexpr auto key1 = "5JktVNHnRX48BUdtewU7N1CyL4Z886c42x7wYW7XhNWkDQRhdcS";
    wm.import_key(_token, "test", key1);
    BOOST_REQUIRE_THROW(wm.create(_token, "test",       std::optional<std::string>()),  fc::exception);
    BOOST_REQUIRE_THROW(wm.create(_token, "./test",     std::optional<std::string>()),  fc::exception);
    BOOST_REQUIRE_THROW(wm.create(_token, "../../test", std::optional<std::string>()),  fc::exception);
    BOOST_REQUIRE_THROW(wm.create(_token, "/tmp/test",  std::optional<std::string>()),  fc::exception);
    BOOST_REQUIRE_THROW(wm.create(_token, "/tmp/",      std::optional<std::string>()),  fc::exception);
    BOOST_REQUIRE_THROW(wm.create(_token, "/",          std::optional<std::string>()),  fc::exception);
    BOOST_REQUIRE_THROW(wm.create(_token, ",/",         std::optional<std::string>()),  fc::exception);
    BOOST_REQUIRE_THROW(wm.create(_token, ",",          std::optional<std::string>()),  fc::exception);
    BOOST_REQUIRE_THROW(wm.create(_token, "<<",         std::optional<std::string>()),  fc::exception);
    BOOST_REQUIRE_THROW(wm.create(_token, "<",          std::optional<std::string>()),  fc::exception);
    BOOST_REQUIRE_THROW(wm.create(_token, ",<",         std::optional<std::string>()),  fc::exception);
    BOOST_REQUIRE_THROW(wm.create(_token, ",<<",        std::optional<std::string>()),  fc::exception);
    BOOST_REQUIRE_THROW(wm.create(_token, "",           std::optional<std::string>()),  fc::exception);

    wm.create(_token, ".test", std::optional<std::string>());
    BOOST_REQUIRE(fc::exists( b_mgr.dir / ".test.wallet" ));

    wm.create(_token, "..test", std::optional<std::string>());
    BOOST_REQUIRE(fc::exists( b_mgr.dir / "..test.wallet" ));

    wm.create(_token, "...test", std::optional<std::string>());
    BOOST_REQUIRE(fc::exists( b_mgr.dir / "...test.wallet" ));

    wm.create(_token, ".", std::optional<std::string>());
    BOOST_REQUIRE(fc::exists( b_mgr.dir / "..wallet" ));

    wm.create(_token, "__test_test", std::optional<std::string>());
    BOOST_REQUIRE(fc::exists( b_mgr.dir / "__test_test.wallet" ));

    wm.create(_token, "t-t", std::optional<std::string>());
    BOOST_REQUIRE(fc::exists( b_mgr.dir / "t-t.wallet" ));
  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(wallet_manager_sessions)
{
  try {
    beekeeper_mgr b_mgr;
    b_mgr.remove_wallets();

    const std::string _host = "";
    const uint64_t _timeout = 90;
    const uint32_t _limit = 3;

    appbase::application app;

    {
      bool _checker = false;
      beekeeper_wallet_manager wm = b_mgr.create_wallet( app, _timeout, _limit, [&_checker](){ _checker = true; } );

      auto _token = wm.create_session( "this is salt", _host );
      wm.close_session( _token );
      BOOST_REQUIRE( _checker );
    }
    {
      bool _checker = false;
      beekeeper_wallet_manager wm = b_mgr.create_wallet( app, _timeout, _limit, [&_checker](){ _checker = true; } );

      auto _token_00 = wm.create_session( "this is salt", _host );
      auto _token_01 = wm.create_session( "this is salt", _host );
      wm.close_session( _token_00 );
      BOOST_REQUIRE( !_checker );
      wm.close_session( _token_01 );
      BOOST_REQUIRE( _checker );
    }
    {
      bool _checker = false;
      beekeeper_wallet_manager wm = b_mgr.create_wallet( app, _timeout, _limit, [&_checker](){ _checker = true; } );
      BOOST_REQUIRE( wm.start() );

      auto _token_00 = wm.create_session( "aaaa", _host );
      auto _token_01 = wm.create_session( "bbbb", _host );

      std::string _pass_00 = wm.create(_token_00, "avocado", std::optional<std::string>());
      std::string _pass_01 = wm.create(_token_01, "banana", std::optional<std::string>());
      std::string _pass_02 = wm.create(_token_01, "cherry", std::optional<std::string>());

      BOOST_REQUIRE_THROW( wm.list_wallets( "not existed token" ), fc::exception );
      BOOST_REQUIRE_THROW( wm.list_created_wallets( "not existed token" ), fc::exception );
      BOOST_REQUIRE_EQUAL( wm.list_wallets( _token_00 ).size(), 1 );
      BOOST_REQUIRE_EQUAL( wm.list_wallets( _token_01 ).size(), 2 );
      BOOST_REQUIRE_EQUAL( wm.list_created_wallets( _token_00 ).size(), 3 );
      BOOST_REQUIRE_EQUAL( wm.list_created_wallets( _token_01 ).size(), 3 );

      wm.close_session( _token_00 );
      BOOST_REQUIRE( !_checker );

      BOOST_REQUIRE_THROW( wm.list_wallets( "not existed token" ), fc::exception );
      BOOST_REQUIRE_THROW( wm.list_wallets( _token_00 ), fc::exception );
      BOOST_REQUIRE_EQUAL( wm.list_wallets( _token_01 ).size(), 3 );
      BOOST_REQUIRE_THROW( wm.list_created_wallets( _token_00 ), fc::exception );
      BOOST_REQUIRE_EQUAL( wm.list_created_wallets( _token_01 ).size(), 3 );

      wm.close_session( _token_01 );
      BOOST_REQUIRE( _checker );

      BOOST_REQUIRE_THROW( wm.list_wallets( "not existed token" ), fc::exception );
      BOOST_REQUIRE_THROW( wm.list_wallets( _token_00 ), fc::exception );
      BOOST_REQUIRE_THROW( wm.list_wallets( _token_01 ), fc::exception );
      BOOST_REQUIRE_THROW( wm.list_created_wallets( _token_00 ), fc::exception );
      BOOST_REQUIRE_THROW( wm.list_created_wallets( _token_01 ), fc::exception );
    }

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(wallet_manager_info)
{
  try {
    beekeeper_mgr b_mgr;
    b_mgr.remove_wallets();

    const std::string _host = "";
    const uint64_t _timeout = 90;
    const uint32_t _limit = 3;

    appbase::application app;

    {
      bool _checker = false;
      beekeeper_wallet_manager wm = b_mgr.create_wallet( app, _timeout, _limit, [&_checker](){ _checker = true; } );
      BOOST_REQUIRE( wm.start() );

      auto _token_00 = wm.create_session( "aaaa", _host );

      std::this_thread::sleep_for( std::chrono::seconds(3) );

      auto _token_01 = wm.create_session( "bbbb", _host );

      auto _info_00 = wm.get_info( _token_00 );
      auto _info_01 = wm.get_info( _token_01 );

      BOOST_TEST_MESSAGE( _info_00.timeout_time );
      BOOST_TEST_MESSAGE( _info_01.timeout_time );

      auto _time_00 = fc::time_point::from_iso_string( _info_00.timeout_time );
      auto _time_01 = fc::time_point::from_iso_string( _info_01.timeout_time );

      BOOST_REQUIRE( _time_01 >_time_00 );

      wm.close_session( _token_01 );
      BOOST_REQUIRE( !_checker );

      auto _token_02 = wm.create_session( "cccc", _host );

      auto _info_02 = wm.get_info( _token_02 );

      BOOST_TEST_MESSAGE( _info_02.timeout_time );

      auto _time_02 = fc::time_point::from_iso_string( _info_02.timeout_time );

      BOOST_REQUIRE( _time_02 >_time_00 );

      wm.close_session( _token_02 );
      BOOST_REQUIRE( !_checker );

      wm.close_session( _token_00 );
      BOOST_REQUIRE( _checker );

      BOOST_REQUIRE_THROW( wm.close_session( _token_00 ), fc::exception );
    }

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(wallet_manager_session_limit)
{
  try {
    beekeeper_mgr b_mgr;
    b_mgr.remove_wallets();

    const std::string _host = "";
    const uint64_t _timeout = 90;
    const uint32_t _session_limit = 64;

    appbase::application app;

    bool _checker = false;
    beekeeper_wallet_manager wm = b_mgr.create_wallet( app, _timeout, _session_limit, [&_checker](){ _checker = true; } );
    BOOST_REQUIRE( wm.start() );

    std::vector<std::string> _tokens;
    for( uint32_t i = 0; i < _session_limit; ++i )
    {
      _tokens.emplace_back( wm.create_session( "salt", _host ) );
    }
    BOOST_REQUIRE_THROW( wm.create_session( "salt", _host ), fc::exception );

    BOOST_REQUIRE( _tokens.size() == _session_limit );

    wm.close_session( _tokens[0] );
    wm.close_session( _tokens[1] );
    _tokens.erase( _tokens.begin() );
    _tokens.erase( _tokens.begin() );

    _tokens.emplace_back( wm.create_session( "salt", _host ) );
    _tokens.emplace_back( wm.create_session( "salt", _host ) );

    BOOST_REQUIRE_THROW( wm.create_session( "salt", _host ), fc::exception );

    BOOST_REQUIRE( _tokens.size() == _session_limit );

    BOOST_REQUIRE( _checker == false );

    for( auto& token : _tokens )
    {
      wm.close_session( token );
    }

    BOOST_REQUIRE( _checker == true );

    _tokens.emplace_back( wm.create_session( "salt", _host ) );
    _tokens.emplace_back( wm.create_session( "salt", _host ) );

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(wallet_manager_close)
{
  try {
    beekeeper_mgr b_mgr;
    b_mgr.remove_wallets();

    const std::string _host = "";
    const uint64_t _timeout = 90;
    const uint32_t _session_limit = 64;

    appbase::application app;

    bool _checker = false;
    beekeeper_wallet_manager wm = b_mgr.create_wallet( app, _timeout, _session_limit, [&_checker](){ _checker = true; } );
    BOOST_REQUIRE( wm.start() );

    auto wallet_name_0 = "0";
    auto wallet_name_1 = "1";

    {
      auto _token = wm.create_session( "salt", _host );
      wm.create(_token, wallet_name_0, std::optional<std::string>());

      auto _wallets = wm.list_wallets( _token );
      BOOST_REQUIRE( _wallets.size() == 1 );
      BOOST_REQUIRE( _wallets[0].name == wallet_name_0 );

      BOOST_REQUIRE_THROW( wm.close( _token, wallet_name_1 ), fc::exception );
      wm.close( _token, wallet_name_0 );

      _wallets = wm.list_wallets( _token );
      BOOST_REQUIRE_EQUAL( _wallets.size(), 0 );

      wm.create(_token, wallet_name_1, std::optional<std::string>());

      _wallets = wm.list_wallets( _token );
      BOOST_REQUIRE_EQUAL( _wallets.size(), 1 );
      BOOST_REQUIRE_EQUAL( _wallets[0].name, wallet_name_1 );

      wm.close( _token, wallet_name_1 );

      _wallets = wm.list_wallets( _token );
      BOOST_REQUIRE_EQUAL( _wallets.size(), 0 );
    }

    {
      fc::remove( b_mgr.dir / "0.wallet" );
      fc::remove( b_mgr.dir / "1.wallet" );

      auto _token = wm.create_session( "salt", _host );
      wm.create(_token, wallet_name_0, std::optional<std::string>());

      wm.lock_all( _token );

      auto _wallets = wm.list_wallets( _token );
      BOOST_REQUIRE_EQUAL( _wallets.size(), 1 );
      BOOST_REQUIRE_EQUAL( _wallets[0].name, wallet_name_0 );

      wm.close( _token, wallet_name_0 );

      _wallets = wm.list_wallets( _token );
      BOOST_REQUIRE_EQUAL( _wallets.size(), 0 );
    }

    {
      fc::remove( b_mgr.dir / "0.wallet" );
      fc::remove( b_mgr.dir / "1.wallet" );

      auto _token = wm.create_session( "salt", _host );
      wm.create(_token, wallet_name_0, std::optional<std::string>());
      wm.create(_token, wallet_name_1, std::optional<std::string>());

      wm.lock( _token, wallet_name_1 );

      auto _wallets = wm.list_wallets( _token );
      BOOST_REQUIRE_EQUAL( _wallets.size(), 2 );
      BOOST_REQUIRE_EQUAL( _wallets[0].name, wallet_name_0 );
      BOOST_REQUIRE_EQUAL( _wallets[1].name, wallet_name_1 );

      wm.close( _token, wallet_name_1 );

      _wallets = wm.list_wallets( _token );
      BOOST_REQUIRE_EQUAL( _wallets.size(), 1 );
      BOOST_REQUIRE_EQUAL( _wallets[0].name, wallet_name_0 );

      wm.lock( _token, wallet_name_0 );

      _wallets = wm.list_wallets( _token );
      BOOST_REQUIRE_EQUAL( _wallets.size(), 1 );
      BOOST_REQUIRE_EQUAL( _wallets[0].name, wallet_name_0 );

      wm.close( _token, wallet_name_0 );

      _wallets = wm.list_wallets( _token );
      BOOST_REQUIRE_EQUAL( _wallets.size(), 0 );
    }

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(wallet_manager_sign_transaction)
{
  try {
    beekeeper_mgr b_mgr;
    b_mgr.remove_wallets();

    {
      hive::protocol::serialization_mode_controller::pack_guard guard( hive::protocol::pack_type::hf26 );

      auto _private_key_str = "5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n";
      auto _public_key_str  = "6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4";

      const auto _private_key = private_key_type::wif_to_key( _private_key_str ).value();
      const auto _public_key = public_key_type::from_base58( _public_key_str, false/*is_sha256*/ );

      const std::string _host = "";
      const uint64_t _timeout = 90;
      const uint32_t _session_limit = 64;

      appbase::application app;

      beekeeper_wallet_manager wm = b_mgr.create_wallet( app, _timeout, _session_limit );
      BOOST_REQUIRE( wm.start() );

      auto _token = wm.create_session( "salt", _host );
      auto _password = wm.create(_token, "0", std::optional<std::string>());
      auto _imported_public_key = wm.import_key( _token, "0", _private_key_str );
      BOOST_REQUIRE( _imported_public_key == _public_key_str );

      auto _calculate_signature = [&]( const std::string& json_trx, const std::string& signature_pattern )
      {
        hive::protocol::transaction _trx = fc::json::from_string( json_trx ).as<hive::protocol::transaction>();
        hive::protocol::digest_type _sig_digest = _trx.sig_digest( HIVE_CHAIN_ID, hive::protocol::pack_type::hf26 );

        auto _signature_local = _private_key.sign_compact( _sig_digest );

        auto _local = fc::json::to_string( _signature_local );
        BOOST_TEST_MESSAGE( _local );
        BOOST_REQUIRE( _local.substr( 1, _local.size() - 2 )          == signature_pattern );
      };

      std::string _signature_00_result = "1f17cc07f7c769073d39fac3385220b549e261fb33c5f619c5dced7f5b0fe9c0954f2684e703710840b7ea01ad7238b8db1d8a9309d03e93de212f86de38d66f21";
      _calculate_signature( "{}", _signature_00_result );

      std::string _signature_01_result = "1f69e091fc79b0e8d1812fc662f12076561f9e38ffc212b901ae90fe559f863ad266fe459a8e946cff9bbe7e56ce253bbfab0cccdde944edc1d05161c61ae86340";
      _calculate_signature( "{\"ref_block_num\":95,\"ref_block_prefix\":4189425605,\"expiration\":\"2023-07-18T08:38:29\",\"operations\":[{\"type\":\"transfer_operation\",\"value\":{\"from\":\"initminer\",\"to\":\"alice\",\"amount\":{\"amount\":\"666\",\"precision\":3,\"nai\":\"@@000000021\"},\"memo\":\"memmm\"}}],\"extensions\":[],\"signatures\":[],\"transaction_id\":\"cc9630cdbc39da1c9b6264df3588c7bedb5762fa\",\"block_num\":0,\"transaction_num\":0}",
                            _signature_01_result );
    }

  } FC_LOG_AND_RETHROW()
}

std::string extract_json( const std::string& str )
{
  BOOST_TEST_MESSAGE( "JSON: " + str );
  if( str.empty() )
    return str;
  auto _v_init = fc::json::from_string( str );
  BOOST_REQUIRE( _v_init.is_object() && ( _v_init.get_object().contains("result") || _v_init.get_object().contains("error") ) );

  std::string _result;
  if( _v_init.get_object().contains("result") )
    fc::from_variant( _v_init.get_object()["result"], _result );
  else
    fc::from_variant( _v_init.get_object()["error"], _result );

  return _result;
};

std::string get_wasm_data( const std::string& json )
{
  vector< string > _elements;
  boost::split( _elements, json, boost::is_any_of( "\"" ) );
  BOOST_REQUIRE( _elements.size() >= 2 );
  return _elements[ _elements.size() - 2 ];
};

BOOST_AUTO_TEST_CASE(wasm_beekeeper)
{
  try {
    beekeeper_mgr b_mgr;
    b_mgr.remove_wallets();

    beekeeper_api _obj( { "--wallet-dir", b_mgr.dir.string() } );

    BOOST_REQUIRE( fc::json::from_string( extract_json( _obj.init() ) ).as<beekeeper::init_data>().status );

    auto _token = extract_json( _obj.create_session( "banana" ) );
    BOOST_TEST_MESSAGE( _token );
    _token = get_wasm_data( _token );

    auto _password_0 = extract_json( _obj.create( _token, "wallet_0", "pear" ) );
    BOOST_TEST_MESSAGE( _password_0 );
    _password_0 = get_wasm_data( _password_0 );

    auto _password_1 = extract_json( _obj.create( _token, "wallet_1", "cherry" ) );
    BOOST_TEST_MESSAGE( _password_1 );
    BOOST_REQUIRE( _password_1.find( "cherry" ) != std::string::npos );
    _password_1 = get_wasm_data( _password_1 );

    auto _public_key_0 = extract_json( _obj.import_key( _token, "wallet_0", "5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n" ) );
    BOOST_TEST_MESSAGE( _public_key_0 );
    BOOST_REQUIRE( _public_key_0.find( "6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4" ) != std::string::npos );

    auto _public_key_1a = extract_json( _obj.import_key( _token, "wallet_1", "5KGKYWMXReJewfj5M29APNMqGEu173DzvHv5TeJAg9SkjUeQV78" ) );
    BOOST_TEST_MESSAGE( _public_key_1a );
    BOOST_REQUIRE( _public_key_1a.find( "6oR6ckA4TejTWTjatUdbcS98AKETc3rcnQ9dWxmeNiKDzfhBZa" ) != std::string::npos );

    auto _public_key_1b = extract_json( _obj.import_key( _token, "wallet_1", "5KNbAE7pLwsLbPUkz6kboVpTR24CycqSNHDG95Y8nbQqSqd6tgS" ) );
    BOOST_TEST_MESSAGE( _public_key_1b );
    BOOST_REQUIRE( _public_key_1b.find( "7j1orEPpWp4bU2SuH46eYXuXkFKEMeJkuXkZVJSaru2zFDGaEH" ) != std::string::npos );

    auto _wallets = extract_json( _obj.list_wallets( _token ) );
    BOOST_TEST_MESSAGE( _wallets );
    {
      beekeeper::list_wallets_return _result = fc::json::from_string( _wallets ).as<beekeeper::list_wallets_return>();
      BOOST_REQUIRE( _result.wallets.size() == 2 );
      BOOST_REQUIRE( _result.wallets[0].unlocked );
      BOOST_REQUIRE( _result.wallets[1].unlocked );
    }

    {
      auto _keys_all = fc::json::from_string( extract_json( _obj.get_public_keys( _token ) ) ).as<beekeeper::get_public_keys_return>().keys;
      auto _keys_wallet_0 = fc::json::from_string( extract_json( _obj.get_public_keys( _token, "wallet_0" ) ) ).as<beekeeper::get_public_keys_return>().keys;
      auto _keys_wallet_1 = fc::json::from_string( extract_json( _obj.get_public_keys( _token, "wallet_1" ) ) ).as<beekeeper::get_public_keys_return>().keys;

      BOOST_REQUIRE_EQUAL( _keys_all.size(), 3 );
      BOOST_REQUIRE_EQUAL( _keys_wallet_0.size(), 1 );
      BOOST_REQUIRE_EQUAL( _keys_wallet_1.size(), 2 );
    }

    _obj.remove_key( _token, "wallet_1", _password_1, "6oR6ckA4TejTWTjatUdbcS98AKETc3rcnQ9dWxmeNiKDzfhBZa" );

    auto _public_keys = extract_json( _obj.get_public_keys( _token ) );
    BOOST_TEST_MESSAGE( _public_keys );

    _obj.close_session( _token );

    _token = extract_json( _obj.create_session( "banana" ) );
    BOOST_TEST_MESSAGE( _token );
    _token = get_wasm_data( _token );

    _obj.open( _token, "wallet_1" );

    BOOST_REQUIRE( extract_json( _obj.get_public_keys( _token ) ).find( "You don't have any unlocked wallet" ) != std::string::npos );

    _obj.close( _token, "wallet_1" );

    _obj.unlock( _token, "wallet_0", _password_0 );

    _wallets = extract_json( _obj.list_wallets( _token ) );
    BOOST_TEST_MESSAGE( _wallets );
    {
      beekeeper::list_wallets_return _result = fc::json::from_string( _wallets ).as<beekeeper::list_wallets_return>();
      BOOST_REQUIRE( _result.wallets.size() == 2 ); /// Both wallets should be returned
      BOOST_REQUIRE( _result.wallets[0].name == "wallet_0" );
      BOOST_REQUIRE( _result.wallets[0].unlocked );
      BOOST_REQUIRE( _result.wallets[1].name == "wallet_1" );
      BOOST_REQUIRE( _result.wallets[1].unlocked == false );

    }

    _obj.unlock( _token, "wallet_1", _password_1 );

    _wallets = extract_json( _obj.list_wallets( _token ) );
    BOOST_TEST_MESSAGE( _wallets );
    {
      beekeeper::list_wallets_return _result = fc::json::from_string( _wallets ).as<beekeeper::list_wallets_return>();
      BOOST_REQUIRE( _result.wallets.size() == 2 );
      BOOST_REQUIRE( _result.wallets[0].unlocked );
      BOOST_REQUIRE( _result.wallets[1].unlocked );
    }

    auto _info = _obj.get_info( _token );
    BOOST_TEST_MESSAGE( _info );

    _obj.set_timeout( _token, 1 );

    std::this_thread::sleep_for( std::chrono::seconds(2) );

    _wallets = extract_json( _obj.list_wallets( _token ) );
    BOOST_TEST_MESSAGE( _wallets );
    {
      beekeeper::list_wallets_return _result = fc::json::from_string( _wallets ).as<beekeeper::list_wallets_return>();
      BOOST_REQUIRE( _result.wallets.size() == 2 );
      //although WASM beekeeper automatic locking is disabled, calling API endpoint triggers locks for every wallet
      BOOST_REQUIRE( !_result.wallets[0].unlocked );
      BOOST_REQUIRE( !_result.wallets[1].unlocked );
    }

    _info = _obj.get_info( _token );
    BOOST_TEST_MESSAGE( _info );

    _obj.unlock( _token, "wallet_0", _password_0 );
    _obj.unlock( _token, "wallet_1", _password_1 );

    _wallets = extract_json( _obj.list_wallets( _token ) );
    BOOST_TEST_MESSAGE( _wallets );
    {
      beekeeper::list_wallets_return _result = fc::json::from_string( _wallets ).as<beekeeper::list_wallets_return>();
      BOOST_REQUIRE( _result.wallets.size() == 2 );
      BOOST_REQUIRE( _result.wallets[0].unlocked );
      BOOST_REQUIRE( _result.wallets[1].unlocked );
    }

    _obj.lock( _token, "wallet_0" );

    _wallets = extract_json( _obj.list_wallets( _token ) );
    BOOST_TEST_MESSAGE( _wallets );
    {
      beekeeper::list_wallets_return _result = fc::json::from_string( _wallets ).as<beekeeper::list_wallets_return>();
      BOOST_REQUIRE( _result.wallets.size() == 2 );
      BOOST_REQUIRE( !_result.wallets[0].unlocked );
      BOOST_REQUIRE( _result.wallets[0].name == "wallet_0" );
      BOOST_REQUIRE( _result.wallets[1].unlocked );
    }

    _obj.unlock( _token, "wallet_0", _password_0 );

    _wallets = extract_json( _obj.list_wallets( _token ) );
    BOOST_TEST_MESSAGE( _wallets );
    {
      beekeeper::list_wallets_return _result = fc::json::from_string( _wallets ).as<beekeeper::list_wallets_return>();
      BOOST_REQUIRE( _result.wallets.size() == 2 );
      BOOST_REQUIRE( _result.wallets[0].unlocked );
      BOOST_REQUIRE( _result.wallets[1].unlocked );
    }

    _obj.lock_all( _token );

    _wallets = extract_json( _obj.list_wallets( _token ) );
    BOOST_TEST_MESSAGE( _wallets );
    {
      beekeeper::list_wallets_return _result = fc::json::from_string( _wallets ).as<beekeeper::list_wallets_return>();
      BOOST_REQUIRE( _result.wallets.size() == 2 );
      BOOST_REQUIRE( !_result.wallets[0].unlocked );
      BOOST_REQUIRE( !_result.wallets[1].unlocked );
    }

    {
      _obj.unlock( _token, "wallet_0", _password_0 );

      auto _private_key_str = "5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n";
      auto _public_key_str  = "6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4";

      const auto _private_key = private_key_type::wif_to_key( _private_key_str ).value();

      auto _calculate_signature = [&]( const std::string& json_trx, const std::string& signature_pattern )
      {
        hive::protocol::transaction _trx = fc::json::from_string( json_trx ).as<hive::protocol::transaction>();
        hive::protocol::digest_type _sig_digest = _trx.sig_digest( HIVE_CHAIN_ID, hive::protocol::pack_type::hf26 );

        auto _signature_local = _private_key.sign_compact( _sig_digest );

        auto _signature_beekeeper = fc::json::from_string( extract_json( _obj.sign_digest( _token, _sig_digest, _public_key_str ) ) ).as<beekeeper::signature_return>();
        auto _error_message = _obj.sign_digest( _token, _sig_digest, _public_key_str, "avocado" );
        BOOST_REQUIRE( _error_message.find( "not found in avocado wallet" ) != std::string::npos );
        auto _signature_beekeeper_2 = fc::json::from_string( extract_json( _obj.sign_digest( _token, _sig_digest, _public_key_str, "wallet_0" ) ) ).as<beekeeper::signature_return>();

        auto _local = fc::json::to_string( _signature_local );
        auto _beekeeper = fc::json::to_string( _signature_beekeeper.signature );
        auto _beekeeper_2 = fc::json::to_string( _signature_beekeeper_2.signature );
        BOOST_TEST_MESSAGE( _local );
        BOOST_TEST_MESSAGE( _beekeeper );
        BOOST_REQUIRE( _beekeeper == _beekeeper_2 );
        BOOST_REQUIRE( _local.substr( 1, _local.size() - 2 )              == signature_pattern );
        BOOST_REQUIRE( _beekeeper.substr( 1, _beekeeper.size() - 2 )      == signature_pattern );
      };

      std::string _signature_00_result = "1f17cc07f7c769073d39fac3385220b549e261fb33c5f619c5dced7f5b0fe9c0954f2684e703710840b7ea01ad7238b8db1d8a9309d03e93de212f86de38d66f21";
      _calculate_signature( "{}", _signature_00_result );

      //trx: "{\"ref_block_num\":95,\"ref_block_prefix\":4189425605,\"expiration\":\"2023-07-18T08:38:29\",\"operations\":[{\"type\":\"transfer_operation\",\"value\":{\"from\":\"initminer\",\"to\":\"alice\",\"amount\":{\"amount\":\"666\",\"precision\":3,\"nai\":\"@@000000021\"},\"memo\":\"memmm\"}}],\"extensions\":[],\"signatures\":[],\"transaction_id\":\"cc9630cdbc39da1c9b6264df3588c7bedb5762fa\",\"block_num\":0,\"transaction_num\":0}"
      std::string _signature_01_result = "1f69e091fc79b0e8d1812fc662f12076561f9e38ffc212b901ae90fe559f863ad266fe459a8e946cff9bbe7e56ce253bbfab0cccdde944edc1d05161c61ae86340";
      _calculate_signature( "{\"ref_block_num\":95,\"ref_block_prefix\":4189425605,\"expiration\":\"2023-07-18T08:38:29\",\"operations\":[{\"type\":\"transfer_operation\",\"value\":{\"from\":\"initminer\",\"to\":\"alice\",\"amount\":{\"amount\":\"666\",\"precision\":3,\"nai\":\"@@000000021\"},\"memo\":\"memmm\"}}],\"extensions\":[],\"signatures\":[],\"transaction_id\":\"cc9630cdbc39da1c9b6264df3588c7bedb5762fa\",\"block_num\":0,\"transaction_num\":0}",
                            _signature_01_result );
    }

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(wasm_beekeeper_false)
{
  try {
    beekeeper_mgr b_mgr;
    b_mgr.remove_wallets();

    beekeeper_api _obj( { "--unknown-parameter", "value_without_sense" } );

    auto _init_error_msg = extract_json( _obj.init() );
    BOOST_REQUIRE( _init_error_msg.find( "unrecognised option" ) != std::string::npos );
    BOOST_REQUIRE( _init_error_msg.find( "--unknown-parameter" ) != std::string::npos );
    BOOST_REQUIRE( _init_error_msg.find( "value_without_sense" ) != std::string::npos );

    auto _create_session_error_msg = extract_json( _obj.create_session( "banana" ) );
    BOOST_REQUIRE( _create_session_error_msg.find( "Initialization failed. API call aborted." ) != std::string::npos );

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(wallet_manager_brute_force_protection_test)
{
  try {
    beekeeper_mgr b_mgr;
    b_mgr.remove_wallets();

    beekeeper::extended_api _api;

    const uint32_t _nr_threads = 10;

    std::vector<std::shared_ptr<std::thread>> threads;

    auto _start = std::chrono::high_resolution_clock::now();

    for( size_t i = 0; i < _nr_threads; ++i )
      threads.emplace_back( std::make_shared<std::thread>( [&]()
      {
        while( !_api.enabled() )
        {
        }
      } ) );

    for( auto& thread : threads )
      thread->join();

    auto _stop = std::chrono::high_resolution_clock::now();

    auto _duration = std::chrono::duration_cast<std::chrono::milliseconds>( _stop - _start );

    BOOST_TEST_MESSAGE( std::to_string( _duration.count() ) + " [ms]" );
    BOOST_REQUIRE( _duration.count() >= 5000 );

  } FC_LOG_AND_RETHROW()
}

template<typename bekeeper_type>
class timeout_simulation
{
  struct wallet
  {
    std::string name;
    std::string password;

    bool operator<( const wallet& obj ) const
    {
      return name < obj.name;
    }
  };

  struct session
  {
    std::string name;
    size_t timeout = 0;
    std::string token;

    std::set<wallet> wallets;
  };

  struct simulation
  {
    std::vector<session> sessions;
  };

  public:

    std::string create_session( bekeeper_type& beekeeper_obj );
    std::string create( bekeeper_type& beekeeper_obj, const std::string& token, const std::string& name );
    std::vector<beekeeper::wallet_details> list_wallets( bekeeper_type& beekeeper_obj, const std::string& token );

    simulation create( bekeeper_type& beekeeper_obj, const std::string& name, size_t nr_sessions, size_t nr_wallets, const std::vector<size_t>& timeouts )
    {
      BOOST_TEST_MESSAGE("*********************" + name + "*********************");

      beekeeper_mgr b_mgr;
      b_mgr.remove_wallets();

      simulation _sim;
      BOOST_REQUIRE( nr_sessions == timeouts.size() );
      for( size_t session_cnt = 0; session_cnt < nr_sessions; ++session_cnt )
      {
        session _s{ name + "-" + std::to_string( session_cnt ), timeouts[ session_cnt ] };

        _s.token = create_session( beekeeper_obj );

        for( uint32_t wallet_cnt = 0; wallet_cnt < nr_wallets; ++wallet_cnt )
        {
          wallet _w{ _s.name + "-w-" + std::to_string( wallet_cnt ) };

          _w.password = create( beekeeper_obj, _s.token, _w.name );

          _s.wallets.emplace( _w );
        }

        beekeeper_obj.set_timeout( _s.token, _s.timeout );
        _sim.sessions.emplace_back( _s );
      }

      return _sim;
    };

    void test( bekeeper_type& beekeeper_obj, const std::string& name, const simulation& sim, size_t timeout )
    {
      BOOST_TEST_MESSAGE("=============" + name + "=============");

      std::this_thread::sleep_for( std::chrono::seconds( timeout ) );

      for( size_t session_cnt = 0; session_cnt < sim.sessions.size(); ++session_cnt )
      {
        auto& _s = sim.sessions[ session_cnt ];
        BOOST_TEST_MESSAGE( "+++++ session: name: " + _s.name + " token: " + _s.token + " timeout: " + std::to_string( _s.timeout ) + " +++++" );

        auto _wallets = list_wallets( beekeeper_obj, _s.token );

        for( auto& wallet_item : _wallets )
        {
          if( _s.wallets.find( wallet{ wallet_item.name } ) != _s.wallets.end() )
          {
            BOOST_TEST_MESSAGE( "+++++ " + wallet_item.name + " +++++" );
            BOOST_REQUIRE( timeout >= _s.timeout ? !wallet_item.unlocked : wallet_item.unlocked );
          }
        }
        BOOST_TEST_MESSAGE( "" );
        
      }
    };

};

template<>
std::string timeout_simulation<beekeeper_api>::create_session( beekeeper_api& beekeeper_obj )
{
  return get_wasm_data( extract_json( beekeeper_obj.create_session( "salt" ) ) );
}

template<>
std::string timeout_simulation<beekeeper_wallet_manager>::create_session( beekeeper_wallet_manager& beekeeper_obj )
{
  return beekeeper_obj.create_session( "this is salt", "127.0.0.1:666" );
}

template<>
std::string timeout_simulation<beekeeper_api>::create( beekeeper_api& beekeeper_obj, const std::string& token, const std::string& name )
{
  return get_wasm_data( extract_json( beekeeper_obj.create( token, name ) ) );
}

template<>
std::string timeout_simulation<beekeeper_wallet_manager>::create( beekeeper_wallet_manager& beekeeper_obj, const std::string& token, const std::string& name )
{
  return beekeeper_obj.create( token, name, std::optional<std::string>() );
}

template<>
std::vector<beekeeper::wallet_details> timeout_simulation<beekeeper_api>::list_wallets( beekeeper_api& beekeeper_obj, const std::string& token )
{
  auto _result = extract_json( beekeeper_obj.list_wallets( token ) );
  return fc::json::from_string( _result ). template as<beekeeper::list_wallets_return>().wallets;
}

template<>
std::vector<beekeeper::wallet_details> timeout_simulation<beekeeper_wallet_manager>::list_wallets( beekeeper_wallet_manager& beekeeper_obj, const std::string& token )
{
  return beekeeper_obj.list_wallets( token );
}

class wasm_simulation_executor
{
  beekeeper_mgr b_mgr;
  timeout_simulation<beekeeper_api> _sim;

  public:

    void run( const std::string& simulation_name, const uint32_t nr_sessions, const uint32_t nr_wallets, const std::vector<size_t>& timeouts, const std::vector<size_t>& stage_timeouts )
    {
      for( auto& stage_timeout : stage_timeouts )
      {
        beekeeper_api _beekeeper( { "--wallet-dir", b_mgr.dir.string() } );
        BOOST_REQUIRE( fc::json::from_string( extract_json( _beekeeper.init() ) ).as<beekeeper::init_data>().status );

        auto _details = _sim.create( _beekeeper, simulation_name, nr_sessions, nr_wallets, timeouts );
        _sim.test( _beekeeper, "Stage: " + std::to_string( stage_timeout ), _details, stage_timeout );
      }
    }
};

class simulation_executor
{
  beekeeper_mgr b_mgr;
  timeout_simulation<beekeeper_wallet_manager> _sim;

  appbase::application app;

  uint64_t _unlock_timeout  = 900;
  int32_t _session_limit    = 64;

  public:

    void run( const std::string& simulation_name, const uint32_t nr_sessions, const uint32_t nr_wallets, const std::vector<size_t>& timeouts, const std::vector<size_t>& stage_timeouts )
    {
      for( auto& stage_timeout : stage_timeouts )
      {
        beekeeper_wallet_manager _beekeeper = b_mgr.create_wallet( app, _unlock_timeout, _session_limit );

        auto _details = _sim.create( _beekeeper, simulation_name, nr_sessions, nr_wallets, timeouts );
        _sim.test( _beekeeper, "Stage: " + std::to_string( stage_timeout ), _details, stage_timeout );
      }
    }
};

BOOST_AUTO_TEST_CASE(wasm_beekeeper_timeout)
{
  try {
    {
      wasm_simulation_executor _executer;
      _executer.run( "a-sim", 2/*nr_sessions*/, 3/*nr_wallets*/, {1, 1}/*timeouts*/, {0}/*stage_timeouts*/ );
    }

    {
      wasm_simulation_executor _executer;
      _executer.run( "b-sim", 2/*nr_sessions*/, 2/*nr_wallets*/, {1, 3}/*timeouts*/, {1}/*stage_timeouts*/ );
    }

    {
      wasm_simulation_executor _executer;
      _executer.run( "c-sim", 3/*nr_sessions*/, 1/*nr_wallets*/, {3, 1, 2}/*timeouts*/, {0, 1, 3}/*stage_timeouts*/ );
    }

    {
      wasm_simulation_executor _executer;
      _executer.run( "d-sim", 5/*nr_sessions*/, 1/*nr_wallets*/, {4, 3, 2, 1, 0}/*timeouts*/, {1, 2, 1, 1, 1}/*stage_timeouts*/ );
    }

    {
      wasm_simulation_executor _executer;
      _executer.run( "e-sim", 4/*nr_sessions*/, 3/*nr_wallets*/, {3, 3, 3, 1}/*timeouts*/, {1, 1, 1}/*stage_timeouts*/ );
    }

    {
      wasm_simulation_executor _executer;
      _executer.run( "f-sim", 4/*nr_sessions*/, 2/*nr_wallets*/, {1, 1, 1, 1}/*timeouts*/, {2, 1}/*stage_timeouts*/ );
    }

    {
      wasm_simulation_executor _executer;
      _executer.run( "g-sim", 4/*nr_sessions*/, 1/*nr_wallets*/, {3, 1, 3, 1}/*timeouts*/, {2}/*stage_timeouts*/ );
    }

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(beekeeper_timeout)
{
  try {
    {
      simulation_executor _executer;
      _executer.run( "a-sim", 2/*nr_sessions*/, 3/*nr_wallets*/, {1, 1}/*timeouts*/, {0}/*stage_timeouts*/ );
    }

    {
      simulation_executor _executer;
      _executer.run( "b-sim", 2/*nr_sessions*/, 2/*nr_wallets*/, {1, 3}/*timeouts*/, {1}/*stage_timeouts*/ );
    }

    {
      simulation_executor _executer;
      _executer.run( "c-sim", 3/*nr_sessions*/, 1/*nr_wallets*/, {3, 1, 2}/*timeouts*/, {0, 1, 3}/*stage_timeouts*/ );
    }

    {
      simulation_executor _executer;
      _executer.run( "d-sim", 5/*nr_sessions*/, 1/*nr_wallets*/, {4, 3, 2, 1, 0}/*timeouts*/, {1, 2, 1, 1, 1}/*stage_timeouts*/ );
    }

    {
      simulation_executor _executer;
      _executer.run( "e-sim", 4/*nr_sessions*/, 3/*nr_wallets*/, {3, 3, 3, 1}/*timeouts*/, {1, 1, 1}/*stage_timeouts*/ );
    }

    {
      simulation_executor _executer;
      _executer.run( "f-sim", 4/*nr_sessions*/, 2/*nr_wallets*/, {1, 1, 1, 1}/*timeouts*/, {2, 1}/*stage_timeouts*/ );
    }

    {
      simulation_executor _executer;
      _executer.run( "g-sim", 4/*nr_sessions*/, 1/*nr_wallets*/, {3, 1, 3, 1}/*timeouts*/, {2}/*stage_timeouts*/ );
    }

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(wasm_beekeeper_refresh_timeout)
{
  try {
    beekeeper_mgr b_mgr;
    b_mgr.remove_wallets();

    beekeeper_api _beekeeper( { "--wallet-dir", b_mgr.dir.string() } );
    BOOST_REQUIRE( fc::json::from_string( extract_json( _beekeeper.init() ) ).as<beekeeper::init_data>().status );

    auto _token = get_wasm_data( extract_json( _beekeeper.create_session( "salt" ) ) );
    _beekeeper.create( _token, "w0" );

    _beekeeper.set_timeout( _token, 1 );

    for( uint32_t i = 0; i < 4; ++i )
    {
      std::this_thread::sleep_for( std::chrono::milliseconds(500) );

      auto _result = extract_json( _beekeeper.list_wallets( _token ) );
      auto _wallets = fc::json::from_string( _result ).as<beekeeper::list_wallets_return>().wallets;
      BOOST_REQUIRE_EQUAL( _wallets.size(), 1 );
      BOOST_REQUIRE_EQUAL( _wallets[0].unlocked, true );
    }

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(beekeeper_refresh_timeout)
{
  try {
    beekeeper_mgr b_mgr;
    b_mgr.remove_wallets();

    const std::string _host = "";
    const uint64_t _timeout = 90;
    const uint32_t _session_limit = 64;

    appbase::application app;

    beekeeper_wallet_manager _beekeeper = b_mgr.create_wallet( app, _timeout, _session_limit );
    BOOST_REQUIRE( _beekeeper.start() );

    auto _token = _beekeeper.create_session( "salt", _host );
    auto _password = _beekeeper.create( _token, "0", std::optional<std::string>() );
    _beekeeper.set_timeout( _token, 1 );

    for( uint32_t i = 0; i < 12; ++i )
    {
      std::this_thread::sleep_for( std::chrono::milliseconds(250) );

      auto _wallets = _beekeeper.list_wallets( _token );
      BOOST_REQUIRE_EQUAL( _wallets.size(), 1 );
      BOOST_REQUIRE_EQUAL( _wallets[0].unlocked, true );
    }

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()

#endif
