#ifdef IS_TEST_NET


#include "../utils/beekeeper_mgr.hpp"

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
using beekeeper_api             = beekeeper::beekeeper_api;
using wallet_data               = beekeeper::wallet_data;

BOOST_AUTO_TEST_SUITE(beekeeper_tests)

/// Test creating the wallet
BOOST_AUTO_TEST_CASE(wallet_test)
{ try {
  test_utils::beekeeper_mgr b_mgr;
  b_mgr.remove_wallets();

  beekeeper_wallet wallet;
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

  beekeeper_wallet wallet2;

  BOOST_REQUIRE(wallet2.is_locked());
  wallet2.load_wallet_file( _wallet_file_name );
  BOOST_REQUIRE(wallet2.is_locked());

  wallet2.unlock("pass");
  BOOST_REQUIRE_EQUAL(1u, wallet2.list_keys().size());

  auto privCopy2 = wallet2.get_private_key(pub);
  BOOST_REQUIRE_EQUAL(wif, privCopy2.key_to_wif());

} FC_LOG_AND_RETHROW() }


BOOST_AUTO_TEST_CASE(wallet_name_test)
{ try {
  test_utils::beekeeper_mgr b_mgr;
  b_mgr.remove_wallets();

  appbase::application app;

  beekeeper_wallet_manager wm = b_mgr.create_wallet( app, 900, 3 );

  BOOST_REQUIRE( wm.start() );
  std::string _token = wm.create_session( "this is salt", "" );

  wm.create(_token, "wallet.wallet", std::optional<std::string>());
  wm.create(_token, "wallet_wallet", std::optional<std::string>());
  wm.create(_token, "wallet-wallet", std::optional<std::string>());
  wm.create(_token, "wallet@wallet", std::optional<std::string>());

  wm.create(_token, ".wallet", std::optional<std::string>());
  wm.create(_token, "_wallet", std::optional<std::string>());
  wm.create(_token, "-wallet", std::optional<std::string>());
  wm.create(_token, "@wallet", std::optional<std::string>());

  wm.create(_token, "wallet.", std::optional<std::string>());
  wm.create(_token, "wallet_", std::optional<std::string>());
  wm.create(_token, "wallet-", std::optional<std::string>());
  wm.create(_token, "wallet@", std::optional<std::string>());

  wm.create(_token, ".wallet.", std::optional<std::string>());
  wm.create(_token, "_wallet_", std::optional<std::string>());
  wm.create(_token, "-wallet-", std::optional<std::string>());
  wm.create(_token, "@wallet@", std::optional<std::string>());

} FC_LOG_AND_RETHROW() }

/// Test wallet manager
BOOST_AUTO_TEST_CASE(wallet_manager_test)
{ try {
  test_utils::beekeeper_mgr b_mgr;
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
  wm.import_key(_token, "test2", key3_str);
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
    test_utils::beekeeper_mgr b_mgr;
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
    test_utils::beekeeper_mgr b_mgr;
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
    test_utils::beekeeper_mgr b_mgr;
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
    test_utils::beekeeper_mgr b_mgr;
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
    test_utils::beekeeper_mgr b_mgr;
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
    test_utils::beekeeper_mgr b_mgr;
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

      const std::string _wallet_name = "0";

      appbase::application app;

      beekeeper_wallet_manager wm = b_mgr.create_wallet( app, _timeout, _session_limit );
      BOOST_REQUIRE( wm.start() );

      auto _token = wm.create_session( "salt", _host );
      auto _password = wm.create(_token, _wallet_name, std::optional<std::string>());
      auto _imported_public_key = wm.import_key( _token, _wallet_name, _private_key_str );
      BOOST_REQUIRE( _imported_public_key == _public_key_str );

      auto _calculate_signature = [&]( const std::string& json_trx, const std::string& signature_pattern )
      {
        hive::protocol::transaction _trx = fc::json::from_string( json_trx, fc::json::format_validation_mode::full ).as<hive::protocol::transaction>();
        hive::protocol::digest_type _sig_digest = _trx.sig_digest( HIVE_CHAIN_ID, hive::protocol::pack_type::hf26 );

        auto _signature_local   = _private_key.sign_compact( _sig_digest );
        auto __signature_local  = fc::json::to_string( _signature_local );

        auto _signature_wallet  = wm.sign_digest( _token, _sig_digest.str(), _imported_public_key, _wallet_name );
        auto __signature_wallet = fc::json::to_string( _signature_wallet );

        BOOST_TEST_MESSAGE( __signature_local );
        BOOST_REQUIRE( __signature_local.substr( 1, __signature_local.size() - 2 )    == signature_pattern );

        BOOST_TEST_MESSAGE( __signature_wallet );
        BOOST_REQUIRE( __signature_wallet.substr( 1, __signature_wallet.size() - 2 )  == signature_pattern );
      };

      std::string _signature_00_result = "1f17cc07f7c769073d39fac3385220b549e261fb33c5f619c5dced7f5b0fe9c0954f2684e703710840b7ea01ad7238b8db1d8a9309d03e93de212f86de38d66f21";
      _calculate_signature( "{}", _signature_00_result );

      std::string _signature_01_result = "1f69e091fc79b0e8d1812fc662f12076561f9e38ffc212b901ae90fe559f863ad266fe459a8e946cff9bbe7e56ce253bbfab0cccdde944edc1d05161c61ae86340";
      _calculate_signature( "{\"ref_block_num\":95,\"ref_block_prefix\":4189425605,\"expiration\":\"2023-07-18T08:38:29\",\"operations\":[{\"type\":\"transfer_operation\",\"value\":{\"from\":\"initminer\",\"to\":\"alice\",\"amount\":{\"amount\":\"666\",\"precision\":3,\"nai\":\"@@000000021\"},\"memo\":\"memmm\"}}],\"extensions\":[],\"signatures\":[],\"transaction_id\":\"cc9630cdbc39da1c9b6264df3588c7bedb5762fa\",\"block_num\":0,\"transaction_num\":0}",
                            _signature_01_result );

      BOOST_REQUIRE_THROW( wm.sign_digest( _token, "", _imported_public_key, _wallet_name ), fc::exception );
    }

  } FC_LOG_AND_RETHROW()
}

std::string extract_json( const std::string& str )
{
  BOOST_TEST_MESSAGE( "JSON: " + str );
  if( str.empty() )
    return str;
  auto _v_init = fc::json::from_string( str, fc::json::format_validation_mode::full );
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
    test_utils::beekeeper_mgr b_mgr;
    b_mgr.remove_wallets();

    beekeeper_api _obj( { "--wallet-dir", b_mgr.dir.string() } );

    BOOST_REQUIRE( fc::json::from_string( extract_json( _obj.init() ), fc::json::format_validation_mode::full ).as<beekeeper::init_data>().status );

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
      beekeeper::list_wallets_return _result = fc::json::from_string( _wallets, fc::json::format_validation_mode::full ).as<beekeeper::list_wallets_return>();
      BOOST_REQUIRE( _result.wallets.size() == 2 );
      BOOST_REQUIRE( _result.wallets[0].unlocked );
      BOOST_REQUIRE( _result.wallets[1].unlocked );
    }

    {
      auto _keys_all = fc::json::from_string( extract_json( _obj.get_public_keys( _token ) ), fc::json::format_validation_mode::full ).as<beekeeper::get_public_keys_return>().keys;
      auto _keys_wallet_0 = fc::json::from_string( extract_json( _obj.get_public_keys( _token, "wallet_0" ) ), fc::json::format_validation_mode::full ).as<beekeeper::get_public_keys_return>().keys;
      auto _keys_wallet_1 = fc::json::from_string( extract_json( _obj.get_public_keys( _token, "wallet_1" ) ), fc::json::format_validation_mode::full ).as<beekeeper::get_public_keys_return>().keys;

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
      beekeeper::list_wallets_return _result = fc::json::from_string( _wallets, fc::json::format_validation_mode::full ).as<beekeeper::list_wallets_return>();
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
      beekeeper::list_wallets_return _result = fc::json::from_string( _wallets, fc::json::format_validation_mode::full ).as<beekeeper::list_wallets_return>();
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
      beekeeper::list_wallets_return _result = fc::json::from_string( _wallets, fc::json::format_validation_mode::full ).as<beekeeper::list_wallets_return>();
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
      beekeeper::list_wallets_return _result = fc::json::from_string( _wallets, fc::json::format_validation_mode::full ).as<beekeeper::list_wallets_return>();
      BOOST_REQUIRE( _result.wallets.size() == 2 );
      BOOST_REQUIRE( _result.wallets[0].unlocked );
      BOOST_REQUIRE( _result.wallets[1].unlocked );
    }

    _obj.lock( _token, "wallet_0" );

    _wallets = extract_json( _obj.list_wallets( _token ) );
    BOOST_TEST_MESSAGE( _wallets );
    {
      beekeeper::list_wallets_return _result = fc::json::from_string( _wallets, fc::json::format_validation_mode::full ).as<beekeeper::list_wallets_return>();
      BOOST_REQUIRE( _result.wallets.size() == 2 );
      BOOST_REQUIRE( !_result.wallets[0].unlocked );
      BOOST_REQUIRE( _result.wallets[0].name == "wallet_0" );
      BOOST_REQUIRE( _result.wallets[1].unlocked );
    }

    _obj.unlock( _token, "wallet_0", _password_0 );

    _wallets = extract_json( _obj.list_wallets( _token ) );
    BOOST_TEST_MESSAGE( _wallets );
    {
      beekeeper::list_wallets_return _result = fc::json::from_string( _wallets, fc::json::format_validation_mode::full ).as<beekeeper::list_wallets_return>();
      BOOST_REQUIRE( _result.wallets.size() == 2 );
      BOOST_REQUIRE( _result.wallets[0].unlocked );
      BOOST_REQUIRE( _result.wallets[1].unlocked );
    }

    _obj.lock_all( _token );

    _wallets = extract_json( _obj.list_wallets( _token ) );
    BOOST_TEST_MESSAGE( _wallets );
    {
      beekeeper::list_wallets_return _result = fc::json::from_string( _wallets, fc::json::format_validation_mode::full ).as<beekeeper::list_wallets_return>();
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
        hive::protocol::transaction _trx = fc::json::from_string( json_trx, fc::json::format_validation_mode::full ).as<hive::protocol::transaction>();
        hive::protocol::digest_type _sig_digest = _trx.sig_digest( HIVE_CHAIN_ID, hive::protocol::pack_type::hf26 );

        auto _signature_local = _private_key.sign_compact( _sig_digest );

        auto _signature_beekeeper = fc::json::from_string( extract_json( _obj.sign_digest( _token, _sig_digest, _public_key_str ) ), fc::json::format_validation_mode::full ).as<beekeeper::signature_return>();
        auto _error_message = _obj.sign_digest( _token, _sig_digest, _public_key_str, "avocado" );
        BOOST_REQUIRE( _error_message.find( "not found in avocado wallet" ) != std::string::npos );
        auto _signature_beekeeper_2 = fc::json::from_string( extract_json( _obj.sign_digest( _token, _sig_digest, _public_key_str, "wallet_0" ) ), fc::json::format_validation_mode::full ).as<beekeeper::signature_return>();

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

      auto _error_message = _obj.sign_digest( _token, "", _public_key_str, "avocado" );
      BOOST_REQUIRE( _error_message.find( "`sig_digest` can't be empty" ) != std::string::npos );
    }

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(wasm_beekeeper_false)
{
  try {
    test_utils::beekeeper_mgr b_mgr;
    b_mgr.remove_wallets();

    {
      beekeeper_api _obj( { "--unknown-parameter", "value_without_sense" } );

      auto _init_error_msg = extract_json( _obj.init() );
      BOOST_REQUIRE( _init_error_msg.find( "unrecognised option" ) != std::string::npos );
      BOOST_REQUIRE( _init_error_msg.find( "--unknown-parameter" ) != std::string::npos );
      BOOST_REQUIRE( _init_error_msg.find( "value_without_sense" ) != std::string::npos );

      auto _create_session_error_msg = extract_json( _obj.create_session( "banana" ) );
      BOOST_REQUIRE( _create_session_error_msg.find( "Initialization failed. API call aborted." ) != std::string::npos );
    }

    {
      beekeeper_api _obj( { "--export-keys-wallet", "[\"2\", \"PW5JViFn5gd4rt6ohk7DQMgHzQN6Z9FuMRfKoE5Ysk25mkjy5AY1b\"]" } );

      auto _init_error_msg = extract_json( _obj.init() );
      BOOST_REQUIRE( _init_error_msg.find( "unrecognised option" ) != std::string::npos );
    }

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(wallet_manager_brute_force_protection_test)
{
  try {
    test_utils::beekeeper_mgr b_mgr;
    b_mgr.remove_wallets();

    uint64_t _interval = 10;
    beekeeper::extended_api _api( _interval );

    const uint32_t _nr_threads = 10;

    auto _unlock_in_threads = [&]()
    {
      std::atomic<bool> _ready{false};
      std::mutex _mtx_message;
      std::mutex _mtx;
      std::condition_variable _cv;

      std::vector<std::shared_ptr<std::thread>> threads;

      auto _start = std::chrono::high_resolution_clock::now();

      auto _calculate_interval = [&_start]( size_t number_thread )
      {
        std::string _message = "*****thread: " + std::to_string( number_thread ) + " *****";
        auto _duration = std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::high_resolution_clock::now() - _start );
        BOOST_TEST_MESSAGE( _message + std::to_string( _duration.count() ) + " [ms]" );

        return _duration;
      };

      auto _calculate_summary = [&_start]()
      {
        auto _duration = std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::high_resolution_clock::now() - _start );
        BOOST_TEST_MESSAGE( std::to_string( _duration.count() ) + " [ms]" );

        return _duration;
      };

      std::atomic<size_t> _cnt{0};

      for( size_t i = 0; i < _nr_threads; ++i )
        threads.emplace_back( std::make_shared<std::thread>( [&]( size_t number_thread )
        {
          {
            std::unique_lock<std::mutex> _lock( _mtx );
            _cv.wait( _lock, [&_ready](){ return true; } );
          }

          if( number_thread % 2 == 0 )
          {
            while( _cnt.load() < _nr_threads / 2 )
            {
              _api.was_error();
              std::this_thread::sleep_for( std::chrono::milliseconds(1) );
            }
          }
          else
          {
            while( _api.unlock_allowed() != beekeeper::extended_api::enabled_after_interval )
            {
              std::this_thread::sleep_for( std::chrono::milliseconds(1) );
            }
            {
              std::lock_guard<std::mutex> _guard( _mtx_message );
              _calculate_interval( number_thread / 2 );
              _cnt.store( _cnt.load() + 1 );
            }
          }
        }, i ) );

      std::this_thread::sleep_for( std::chrono::milliseconds(10) );
      _ready.store( true );
      _cv.notify_all();

      for( auto& thread : threads )
        thread->join();

      return _calculate_summary();
    };

    const uint32_t _nr_attempts = 20;
    bool _work_in_threads_was_correct = false;
    for( uint32_t i = 0; i < _nr_attempts; ++i )
    {
      auto _duration = _unlock_in_threads();
      if( _duration.count() >= (int64_t)( _interval * ( _nr_threads / 2 ) ) )
      {
        BOOST_TEST_MESSAGE("********unlocks work correctly in many threads. Finished: (" + std::to_string(i) + ")");
        _work_in_threads_was_correct = true;
        break;
      }
      else
      {
        BOOST_TEST_MESSAGE("********unlocks didn't work correctly in many threads. Repeating: (" + std::to_string(i) + ")");
      }
    }
    BOOST_REQUIRE( _work_in_threads_was_correct );

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

      test_utils::beekeeper_mgr b_mgr;
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
  return fc::json::from_string( _result, fc::json::format_validation_mode::full ). template as<beekeeper::list_wallets_return>().wallets;
}

template<>
std::vector<beekeeper::wallet_details> timeout_simulation<beekeeper_wallet_manager>::list_wallets( beekeeper_wallet_manager& beekeeper_obj, const std::string& token )
{
  return beekeeper_obj.list_wallets( token );
}

class wasm_simulation_executor
{
  test_utils::beekeeper_mgr b_mgr;
  timeout_simulation<beekeeper_api> sim;

  public:

    void run( const std::string& simulation_name, const uint32_t nr_sessions, const uint32_t nr_wallets, const std::vector<size_t>& timeouts, const std::vector<size_t>& stage_timeouts )
    {
      for( auto& stage_timeout : stage_timeouts )
      {
        beekeeper_api _beekeeper( { "--wallet-dir", b_mgr.dir.string() } );
        BOOST_REQUIRE( fc::json::from_string( extract_json( _beekeeper.init() ), fc::json::format_validation_mode::full ).as<beekeeper::init_data>().status );

        auto _details = sim.create( _beekeeper, simulation_name, nr_sessions, nr_wallets, timeouts );
        sim.test( _beekeeper, "Wait for: " + std::to_string( stage_timeout ) + "[s]", _details, stage_timeout );
      }
    }
};

class simulation_executor
{
  test_utils::beekeeper_mgr b_mgr;
  timeout_simulation<beekeeper_wallet_manager> sim;

  appbase::application app;

  uint64_t _unlock_timeout  = 900;
  int32_t _session_limit    = 64;

  public:

    void run( const std::string& simulation_name, const uint32_t nr_sessions, const uint32_t nr_wallets, const std::vector<size_t>& timeouts, const std::vector<size_t>& stage_timeouts )
    {
      for( auto& stage_timeout : stage_timeouts )
      {
        beekeeper_wallet_manager _beekeeper = b_mgr.create_wallet( app, _unlock_timeout, _session_limit );

        auto _details = sim.create( _beekeeper, simulation_name, nr_sessions, nr_wallets, timeouts );
        sim.test( _beekeeper, "Wait for: " + std::to_string( stage_timeout ) + "[s]", _details, stage_timeout );
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
    auto _list_wallets_action = []( beekeeper_api& beekeeper, const std::string& token )
    {
      auto _result = extract_json( beekeeper.list_wallets( token ) );
      auto _wallets = fc::json::from_string( _result, fc::json::format_validation_mode::full ).as<beekeeper::list_wallets_return>().wallets;
      BOOST_REQUIRE_EQUAL( _wallets.size(), 1 );
      BOOST_REQUIRE_EQUAL( _wallets[0].unlocked, true );
    };

    auto _set_timeout_action = []( beekeeper_api& beekeeper, const std::string& token )
    {
      beekeeper.set_timeout( token, 1 );
    };

    using action_type = std::function<void(beekeeper_api& beekeeper, const std::string& token)>;

    auto _refresh_timeout_simulation = []( action_type&& action, action_type&& aux_action = action_type() )
    {
      test_utils::beekeeper_mgr b_mgr;
      b_mgr.remove_wallets();

      beekeeper_api _beekeeper( { "--wallet-dir", b_mgr.dir.string() } );
      BOOST_REQUIRE( fc::json::from_string( extract_json( _beekeeper.init() ), fc::json::format_validation_mode::full ).as<beekeeper::init_data>().status );

      auto _token = get_wasm_data( extract_json( _beekeeper.create_session( "salt" ) ) );
      _beekeeper.create( _token, "w0" );

      _beekeeper.set_timeout( _token, 1 );

      for( uint32_t i = 0; i < 4; ++i )
      {
        std::this_thread::sleep_for( std::chrono::milliseconds(500) );
        action( _beekeeper, _token );
      }

      if( aux_action )
        aux_action( _beekeeper, _token );
    };

    _refresh_timeout_simulation( _list_wallets_action );
    _refresh_timeout_simulation( _set_timeout_action ,_list_wallets_action );

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(beekeeper_refresh_timeout)
{
  try {
    auto _list_wallets_action = []( beekeeper_wallet_manager& beekeeper, const std::string& token )
    {
      auto _wallets = beekeeper.list_wallets( token );
      BOOST_REQUIRE_EQUAL( _wallets.size(), 1 );
      BOOST_REQUIRE_EQUAL( _wallets[0].unlocked, true );
    };

    auto _set_timeout_action = []( beekeeper_wallet_manager& beekeeper, const std::string& token )
    {
      beekeeper.set_timeout( token, 1 );
    };

    using action_type = std::function<void(beekeeper_wallet_manager& beekeeper, const std::string& token)>;

    auto _refresh_timeout_simulation = []( action_type&& action, action_type&& aux_action = action_type() )
    {
      test_utils::beekeeper_mgr b_mgr;
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
        action( _beekeeper, _token );
      }

      if( aux_action )
        aux_action( _beekeeper, _token );
    };

    _refresh_timeout_simulation( _list_wallets_action );
    _refresh_timeout_simulation( _set_timeout_action ,_list_wallets_action );

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(has_matching_private_key_endpoint_test)
{
  try {
    test_utils::beekeeper_mgr b_mgr;
    b_mgr.remove_wallets();

    hive::protocol::serialization_mode_controller::pack_guard guard( hive::protocol::pack_type::hf26 );

    auto _private_key_str = "5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n";
    auto _public_key_str  = "6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4";

    auto _private_key_str_2 = "5J8C7BMfvMFXFkvPhHNk2NHGk4zy3jF4Mrpf5k5EzAecuuzqDnn";
    auto _public_key_str_2  = "6Pg5jd1w8rXgGoqvpZXy1tHPdz43itPW6L2AGJuw8kgSAbtsxm";

    const std::string _host = "127.0.0.1:666";
    const uint64_t _timeout = 90;
    const uint32_t _session_limit = 64;

    appbase::application app;

    beekeeper_wallet_manager wm = b_mgr.create_wallet( app, _timeout, _session_limit, [](){} );
    BOOST_REQUIRE( wm.start() );

    auto _token = wm.create_session( "salt", _host );
    auto _password = wm.create( _token, "0", std::optional<std::string>() );

    wm.import_key( _token, "0", _private_key_str );

    BOOST_REQUIRE_THROW( wm.has_matching_private_key( _token, "pear", _public_key_str ), fc::exception );
    BOOST_REQUIRE_THROW( wm.has_matching_private_key( "_token", "0", _public_key_str ), fc::exception );

    BOOST_REQUIRE_EQUAL( wm.has_matching_private_key( _token, "0", _public_key_str ), true );
    BOOST_REQUIRE_EQUAL( wm.has_matching_private_key( _token, "0", _public_key_str_2 ), false );

    wm.import_key( _token, "0", _private_key_str_2 );

    BOOST_REQUIRE_EQUAL( wm.has_matching_private_key( _token, "0", _public_key_str ), true );
    BOOST_REQUIRE_EQUAL( wm.has_matching_private_key( _token, "0", _public_key_str_2 ), true );

    wm.close( _token, "0" );

    BOOST_REQUIRE_THROW( wm.has_matching_private_key( _token, "0", _public_key_str ), fc::exception );
    BOOST_REQUIRE_THROW( wm.has_matching_private_key( _token, "0", _public_key_str_2 ), fc::exception );

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(beekeeper_timeout_list_wallets)
{
  try {
    test_utils::beekeeper_mgr b_mgr;
    b_mgr.remove_wallets();

    const std::string _host = "";
    const uint64_t _timeout = 90;
    const uint32_t _session_limit = 64;

    appbase::application app;

    beekeeper_wallet_manager _beekeeper = b_mgr.create_wallet( app, _timeout, _session_limit );
    BOOST_REQUIRE( _beekeeper.start() );

    auto _token = _beekeeper.create_session( "salt", _host );

    struct wallet
    {
      std::string name;
      std::string password;
    };
    std::vector<wallet> _wallets{ { "0" }, { "1" }, { "2" } };

    for( auto& wallet : _wallets )
    {
      wallet.password = _beekeeper.create( _token, wallet.name, std::optional<std::string>() );
      _beekeeper.close( _token, wallet.name );
    }
    {
      auto _wallets = _beekeeper.list_wallets( _token );
      BOOST_REQUIRE_EQUAL( _wallets.size(), 0 );
    }
    {
      const size_t _wallet_cnt = 1;
      _beekeeper.unlock( _token, _wallets[_wallet_cnt].name, _wallets[_wallet_cnt].password );
    }
    {
      _beekeeper.set_timeout( _token, 1 );
      std::this_thread::sleep_for( std::chrono::milliseconds(1200) );
    }
    {
      auto _wallets = _beekeeper.list_wallets( _token );
      BOOST_REQUIRE_EQUAL( _wallets.size(), 1 );
      const size_t _wallet_cnt = 0;
      BOOST_REQUIRE_EQUAL( _wallets[_wallet_cnt].unlocked, false );
    }
  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(data_reliability_when_file_with_wallet_is_removed)
{
  try
  {
    test_utils::beekeeper_mgr b_mgr;
    b_mgr.remove_wallets();

    const std::string _host = "";
    const uint64_t _timeout = 90;
    const uint32_t _session_limit = 64;

    appbase::application app;

    beekeeper_wallet_manager _beekeeper = b_mgr.create_wallet( app, _timeout, _session_limit );
    BOOST_REQUIRE( _beekeeper.start() );

    auto _token = _beekeeper.create_session( "salt", _host );

    struct keys
    {
      std::string private_key;
      std::string public_key;
    };
    std::vector<keys> _keys_a =
    {
      {"5J15npVK6qABGsbdsLnJdaF5esrEWxeejeE3KUx6r534ug4tyze", "6TqSJaS1aRj6p6yZEo5xicX7bvLhrfdVqi5ToNrKxHU3FRBEdW"},
      {"5K1gv5rEtHiACVTFq9ikhEijezMh4rkbbTPqu4CAGMnXcTLC1su", "8LbCRyqtXk5VKbdFwK1YBgiafqprAd7yysN49PnDwAsyoMqQME"},
      {"5KLytoW1AiGSoHHBA73x1AmgZnN16QDgU1SPpG9Vd2dpdiBgSYw", "8FDsHdPkHbY8fuUkVLyAmrnKMvj6DddLopi3YJ51dVqsG9vZa4"},
      {"5KXNQP5feaaXpp28yRrGaFeNYZT7Vrb1PqLEyo7E3pJiG1veLKG", "6a34GANY5LD8deYvvfySSWGd7sPahgVNYoFPapngMUD27pWb45"},
      {"5KKvoNaCPtN9vUEU1Zq9epSAVsEPEtocbJsp7pjZndt9Rn4dNRg", "8mmxXz5BfQc2NJfqhiPkbgcyJm4EvWEr2UAUdr56gEWSN9ZnA5"}
    };
    std::vector<keys> _keys_b =
    {
      {"5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT", "5RqVBAVNp5ufMCetQtvLGLJo7unX9nyCBMMrTXRWQ9i1Zzzizh"},
      {"5KGKYWMXReJewfj5M29APNMqGEu173DzvHv5TeJAg9SkjUeQV78", "6oR6ckA4TejTWTjatUdbcS98AKETc3rcnQ9dWxmeNiKDzfhBZa"},
      {"5KNbAE7pLwsLbPUkz6kboVpTR24CycqSNHDG95Y8nbQqSqd6tgS", "7j1orEPpWp4bU2SuH46eYXuXkFKEMeJkuXkZVJSaru2zFDGaEH"},
      {"5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n", "6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4"},
      {"5J8C7BMfvMFXFkvPhHNk2NHGk4zy3jF4Mrpf5k5EzAecuuzqDnn", "6Pg5jd1w8rXgGoqvpZXy1tHPdz43itPW6L2AGJuw8kgSAbtsxm"}
    };

    struct wallet
    {
      std::string name;
      std::string password;
    };
    std::vector<wallet> _wallets{ { "0" }, { "1" }, { "2" } };

    for( auto& wallet : _wallets )
    {
      wallet.password = _beekeeper.create( _token, wallet.name, std::optional<std::string>() );
      for( auto& item : _keys_a )
      {
        _beekeeper.import_key( _token, wallet.name, item.private_key );
      }
    }

  b_mgr.remove_wallet( _wallets[0].name );
  b_mgr.remove_wallet( _wallets[1].name );

  auto _cmp = []( const flat_set<public_key_type>& a, const flat_set<public_key_type>& b )
  {
    if( a.size() != b.size() )
      return false;

    for( auto& item : a )
    {
      if( b.find( item ) == b.end() )
      {
        return false;
      }
    }

    return true;
  };

  {
    for( auto& item : _keys_a )
      _beekeeper.import_key( _token, _wallets[0].name, item.private_key );

    auto _public_keys_0 = _beekeeper.get_public_keys( _token, _wallets[0].name );
    auto _public_keys_2 = _beekeeper.get_public_keys( _token, _wallets[2].name );
    BOOST_REQUIRE_EQUAL( _public_keys_0.size(), 5 );
    BOOST_REQUIRE( _cmp( _public_keys_0, _public_keys_2 ) );
  }
  {
    for( auto& item : _keys_b )
    {
      _beekeeper.import_key( _token, _wallets[1].name, item.private_key );
      _beekeeper.import_key( _token, _wallets[2].name, item.private_key );
    }

    auto _public_keys_1 = _beekeeper.get_public_keys( _token, _wallets[1].name );
    auto _public_keys_2 = _beekeeper.get_public_keys( _token, _wallets[2].name );
    BOOST_REQUIRE_EQUAL( _public_keys_1.size(), 10 );
    BOOST_REQUIRE( _cmp( _public_keys_1, _public_keys_2 ) );
  }

  BOOST_REQUIRE( !b_mgr.exists_wallet( _wallets[0].name ) );
  BOOST_REQUIRE( b_mgr.exists_wallet( _wallets[1].name ) );
  BOOST_REQUIRE( b_mgr.exists_wallet( _wallets[2].name ) );

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(encrypt_decrypt_data)
{
  try
  {
    test_utils::beekeeper_mgr b_mgr;
    b_mgr.remove_wallets();

    const std::string _host = "";
    const uint64_t _timeout = 90;
    const uint32_t _session_limit = 64;

    appbase::application app;

    beekeeper_wallet_manager _beekeeper = b_mgr.create_wallet( app, _timeout, _session_limit );
    BOOST_REQUIRE( _beekeeper.start() );

    auto _token = _beekeeper.create_session( "salt", _host );

    struct keys
    {
      std::string private_key;
      std::string public_key;
    };
    std::vector<keys> _keys =
    {
      {"5J15npVK6qABGsbdsLnJdaF5esrEWxeejeE3KUx6r534ug4tyze", "6TqSJaS1aRj6p6yZEo5xicX7bvLhrfdVqi5ToNrKxHU3FRBEdW"},
      {"5K1gv5rEtHiACVTFq9ikhEijezMh4rkbbTPqu4CAGMnXcTLC1su", "8LbCRyqtXk5VKbdFwK1YBgiafqprAd7yysN49PnDwAsyoMqQME"},
      {"5KLytoW1AiGSoHHBA73x1AmgZnN16QDgU1SPpG9Vd2dpdiBgSYw", "8FDsHdPkHbY8fuUkVLyAmrnKMvj6DddLopi3YJ51dVqsG9vZa4"},
      {"5KXNQP5feaaXpp28yRrGaFeNYZT7Vrb1PqLEyo7E3pJiG1veLKG", "6a34GANY5LD8deYvvfySSWGd7sPahgVNYoFPapngMUD27pWb45"}
    };

    const string _fruits_content = "avocado-banana-cherry-durian";
    const string _empty_content = "";
    const string _dummy_content = "xxxxxxxxxxxxxxxxxxxxxxxxxxx";

    auto _encrypt = [&_token, &_beekeeper, &_keys]( uint32_t nr_from_public_key, uint32_t nr_to_public_key, const std::string& wallet_name, const std::string& content )
    {
      auto __encrypt = [&]()
      {
        return _beekeeper.encrypt_data( _token, _keys[nr_from_public_key].public_key, _keys[nr_to_public_key].public_key, wallet_name, content );
      };

      std::string _encrypted_content = __encrypt();
      std::string _encrypted_content_2 = __encrypt();

      BOOST_REQUIRE( _encrypted_content != _encrypted_content_2 );

      return std::make_pair( _encrypted_content, _encrypted_content_2 );
    };

    auto _decrypt = [&_token, &_beekeeper, &_keys]( const std::string& pattern, uint32_t nr_from_public_key, uint32_t nr_to_public_key, const std::string& wallet_name, const std::string& content )
    {
      std::string _encrypted_content = _beekeeper.decrypt_data( _token, _keys[nr_from_public_key].public_key, _keys[nr_to_public_key].public_key, wallet_name, content );
      BOOST_REQUIRE_EQUAL( _encrypted_content, pattern );
    };

    struct wallet
    {
      std::string name;
      std::string password;
    };
    std::vector<wallet> _wallets{ { "0" }, { "1" }, { "2" }, { "3" } };

    //========================Preparation========================
    /*
      wallet "0" has _keys[0]
      wallet "1" has _keys[1]
      wallet "2" has _keys[0] and _keys[1]
      wallet "3" has _keys[2] and _keys[3]
    */
    auto _cnt = 0;
    for( auto& wallet : _wallets )
    {
      wallet.password = _beekeeper.create( _token, wallet.name, std::optional<std::string>() );
      switch( _cnt )
      {
        case 0:
          _beekeeper.import_key( _token, wallet.name, _keys[0].private_key );
        break;
        case 1:
          _beekeeper.import_key( _token, wallet.name, _keys[1].private_key );
        break;
        case 2:
          _beekeeper.import_key( _token, wallet.name, _keys[0].private_key );
          _beekeeper.import_key( _token, wallet.name, _keys[1].private_key );
        break;
        case 3:
          _beekeeper.import_key( _token, wallet.name, _keys[2].private_key );
          _beekeeper.import_key( _token, wallet.name, _keys[3].private_key );
        break;
      }
      ++_cnt;
    }
    _beekeeper.lock_all( _token );
    //========================End of preparation========================

    {
      //lack of unlocked wallets
      BOOST_REQUIRE_THROW( _encrypt( 0 /*nr_from_public_key*/, 1 /*nr_to_public_key*/, _wallets[0].name, _fruits_content ), fc::exception );
      BOOST_REQUIRE_THROW( _encrypt( 0 /*nr_from_public_key*/, 1 /*nr_to_public_key*/, _wallets[0].name, _empty_content ), fc::exception );
    }
    {
      //unlock wallet "0"
      _beekeeper.unlock( _token, _wallets[0].name, _wallets[0].password );

      auto _encrypted_content = _encrypt( 0 /*nr_from_public_key*/, 1 /*nr_to_public_key*/, _wallets[0].name, _fruits_content );
      _decrypt( _fruits_content, 0 /*nr_from_public_key*/, 1 /*nr_to_public_key*/, _wallets[0].name, _encrypted_content.first );
      _decrypt( _fruits_content, 0 /*nr_from_public_key*/, 1 /*nr_to_public_key*/, _wallets[0].name, _encrypted_content.second );

      auto _encrypted_content_2 = _encrypt( 0 /*nr_from_public_key*/, 1 /*nr_to_public_key*/, _wallets[0].name, _empty_content );
      _decrypt( _empty_content, 0 /*nr_from_public_key*/, 1 /*nr_to_public_key*/, _wallets[0].name, _encrypted_content_2.first );
      _decrypt( _empty_content, 0 /*nr_from_public_key*/, 1 /*nr_to_public_key*/, _wallets[0].name, _encrypted_content_2.second );

      _beekeeper.lock_all( _token );

      BOOST_REQUIRE_THROW( _decrypt( _dummy_content, 0 /*nr_from_public_key*/, 1 /*nr_to_public_key*/, _wallets[0].name, _encrypted_content.first ), fc::exception );
      BOOST_REQUIRE_THROW( _decrypt( _dummy_content, 0 /*nr_from_public_key*/, 1 /*nr_to_public_key*/, _wallets[0].name, _encrypted_content.second ), fc::exception );

      BOOST_REQUIRE_THROW( _decrypt( _dummy_content, 0 /*nr_from_public_key*/, 1 /*nr_to_public_key*/, _wallets[0].name, _encrypted_content_2.first ), fc::exception );
      BOOST_REQUIRE_THROW( _decrypt( _dummy_content, 0 /*nr_from_public_key*/, 1 /*nr_to_public_key*/, _wallets[0].name, _encrypted_content_2.second ), fc::exception );
    }
    {
      //unlock wallet "1"
      _beekeeper.unlock( _token, _wallets[1].name, _wallets[1].password );

      BOOST_REQUIRE_THROW( _encrypt( 0 /*nr_from_public_key*/, 1 /*nr_to_public_key*/, _wallets[1].name, _fruits_content ), fc::exception );
      BOOST_REQUIRE_THROW( _encrypt( 0 /*nr_from_public_key*/, 1 /*nr_to_public_key*/, _wallets[1].name, _empty_content ), fc::exception );

      _beekeeper.lock_all( _token );
    }
    {
      //unlock wallet "2"
      _beekeeper.unlock( _token, _wallets[2].name, _wallets[2].password );

      auto _encrypted_content = _encrypt( 0 /*nr_from_public_key*/, 1 /*nr_to_public_key*/, _wallets[2].name, _fruits_content );
      _decrypt( _fruits_content, 0 /*nr_from_public_key*/, 1 /*nr_to_public_key*/, _wallets[2].name, _encrypted_content.first );
      _decrypt( _fruits_content, 0 /*nr_from_public_key*/, 1 /*nr_to_public_key*/, _wallets[2].name, _encrypted_content.second );

      _encrypted_content = _encrypt( 0 /*nr_from_public_key*/, 1 /*nr_to_public_key*/, _wallets[2].name, _empty_content );
      _decrypt( _empty_content, 0 /*nr_from_public_key*/, 1 /*nr_to_public_key*/, _wallets[2].name, _encrypted_content.first );
      _decrypt( _empty_content, 0 /*nr_from_public_key*/, 1 /*nr_to_public_key*/, _wallets[2].name, _encrypted_content.second );

      _beekeeper.lock_all( _token );
    }
    {
      //unlock wallet "3"
      _beekeeper.unlock( _token, _wallets[3].name, _wallets[3].password );

      BOOST_REQUIRE_THROW( _encrypt( 0 /*nr_from_public_key*/, 1 /*nr_to_public_key*/, _wallets[3].name, _fruits_content ), fc::exception );
      BOOST_REQUIRE_THROW( _encrypt( 0 /*nr_from_public_key*/, 1 /*nr_to_public_key*/, _wallets[3].name, _empty_content ), fc::exception );

      _beekeeper.lock_all( _token );
    }
    {
      //unlock all wallets
      for( auto& wallet : _wallets )
        _beekeeper.unlock( _token, wallet.name, wallet.password );

      auto _encrypted_content = _encrypt( 0 /*nr_from_public_key*/, 1 /*nr_to_public_key*/, _wallets[0].name, _fruits_content );
      _decrypt( _fruits_content, 0 /*nr_from_public_key*/, 1 /*nr_to_public_key*/, _wallets[2].name, _encrypted_content.first );
      _decrypt( _fruits_content, 0 /*nr_from_public_key*/, 1 /*nr_to_public_key*/, _wallets[0].name, _encrypted_content.second );

      _encrypted_content = _encrypt( 0 /*nr_from_public_key*/, 1 /*nr_to_public_key*/, _wallets[2].name, _empty_content );
      _decrypt( _empty_content, 0 /*nr_from_public_key*/, 1 /*nr_to_public_key*/, _wallets[2].name, _encrypted_content.first );
      _decrypt( _empty_content, 0 /*nr_from_public_key*/, 1 /*nr_to_public_key*/, _wallets[0].name, _encrypted_content.second );

      //`from` key == `to` key
      _encrypted_content = _encrypt( 0 /*nr_from_public_key*/, 0 /*nr_to_public_key*/, _wallets[2].name, _empty_content );
      _decrypt( _empty_content, 0 /*nr_from_public_key*/, 0 /*nr_to_public_key*/, _wallets[0].name, _encrypted_content.first );
      _decrypt( _empty_content, 0 /*nr_from_public_key*/, 0 /*nr_to_public_key*/, _wallets[2].name, _encrypted_content.second );
    }

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()

#endif
