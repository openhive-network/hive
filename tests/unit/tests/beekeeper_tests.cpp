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
using wallet_data               = beekeeper::wallet_data;
using beekeeper_wallet          = beekeeper::beekeeper_wallet;
using session_manager           = beekeeper::session_manager;
using beekeeper_instance        = beekeeper::beekeeper_instance;

BOOST_AUTO_TEST_SUITE(beekeeper_tests)

/// Test creating the wallet
BOOST_AUTO_TEST_CASE(wallet_test)
{ try {
  wallet_data d;
  beekeeper_wallet wallet(d);
  BOOST_REQUIRE(wallet.is_locked());

  wallet.set_password("pass");
  BOOST_REQUIRE(wallet.is_locked());

  wallet.unlock("pass");
  BOOST_REQUIRE(!wallet.is_locked());

  wallet.set_wallet_filename("test");
  BOOST_REQUIRE_EQUAL("test", wallet.get_wallet_filename());

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
  wallet.save_wallet_file("wallet_test.json");
  BOOST_REQUIRE(fc::exists("wallet_test.json"));

  wallet_data d2;
  beekeeper_wallet wallet2(d2);

  BOOST_REQUIRE(wallet2.is_locked());
  wallet2.load_wallet_file("wallet_test.json");
  BOOST_REQUIRE(wallet2.is_locked());

  wallet2.unlock("pass");
  BOOST_REQUIRE_EQUAL(1u, wallet2.list_keys().size());

  auto privCopy2 = wallet2.get_private_key(pub);
  BOOST_REQUIRE_EQUAL(wif, privCopy2.key_to_wif());

  fc::remove("wallet_test.json");
} FC_LOG_AND_RETHROW() }


beekeeper_wallet_manager create_wallet( const boost::filesystem::path& cmd_wallet_dir, uint64_t cmd_unlock_timeout, uint32_t cmd_session_limit, std::function<void()>&& method = [](){} )
{
  return beekeeper_wallet_manager( std::make_shared<session_manager>( "127.0.0.1:666" ), std::make_shared<beekeeper_instance>( cmd_wallet_dir, "127.0.0.1:666" ),
                                    cmd_wallet_dir, cmd_unlock_timeout, cmd_session_limit, std::move( method ) );
}

std::string create( beekeeper_wallet_manager& mgr, const std::string& token, const std::string& name )
{
  return mgr.create( token, name, std::optional<std::string>() );
}

/// Test wallet manager
BOOST_AUTO_TEST_CASE(wallet_manager_test)
{ try {
  if (fc::exists("test.wallet")) fc::remove("test.wallet");
  if (fc::exists("test2.wallet")) fc::remove("test2.wallet");
  if (fc::exists("testgen.wallet")) fc::remove("testgen.wallet");

  const auto key1_str = "5JktVNHnRX48BUdtewU7N1CyL4Z886c42x7wYW7XhNWkDQRhdcS";
  const auto key2_str = "5Ju5RTcVDo35ndtzHioPMgebvBM6LkJ6tvuU6LTNQv8yaz3ggZr";
  const auto key3_str = "5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3";

  const auto key1 = private_key_type::wif_to_key( key1_str ).value();
  const auto key2 = private_key_type::wif_to_key( key2_str ).value();
  const auto key3 = private_key_type::wif_to_key( key3_str ).value();

  beekeeper_wallet_manager wm = create_wallet( ".", 900, 3 );

  BOOST_REQUIRE( wm.start() );
  std::string _token = wm.create_session( "this is salt", "127.0.0.1:666" );

  BOOST_REQUIRE_EQUAL(0u, wm.list_wallets(_token).size());
  BOOST_REQUIRE_THROW(wm.get_public_keys(_token), fc::exception);
  BOOST_REQUIRE_NO_THROW(wm.lock_all(_token));

  BOOST_REQUIRE_THROW(wm.lock(_token, "test"), fc::exception);
  BOOST_REQUIRE_THROW(wm.unlock(_token, "test", "pw"), fc::exception);
  BOOST_REQUIRE_THROW(wm.import_key(_token, "test", "pw"), fc::exception);

  auto pw = create(wm, _token, "test");
  BOOST_REQUIRE(!pw.empty());
  BOOST_REQUIRE_EQUAL(0u, pw.find("PW")); // starts with PW
  BOOST_REQUIRE_EQUAL(1u, wm.list_wallets(_token).size());
  // wallet has no keys when it is created
  BOOST_REQUIRE_EQUAL(0u, wm.get_public_keys(_token).size());
  BOOST_REQUIRE_EQUAL(0u, wm.list_keys(_token, "test", pw).size());
  BOOST_REQUIRE(wm.list_wallets(_token)[0].unlocked);
  wm.lock(_token, "test");
  BOOST_REQUIRE(!wm.list_wallets(_token)[0].unlocked);
  wm.unlock(_token, "test", pw);
  BOOST_REQUIRE_THROW(wm.unlock(_token, "test", pw), fc::exception);
  BOOST_REQUIRE(wm.list_wallets(_token)[0].unlocked);
  wm.import_key(_token, "test", key1_str);
  BOOST_REQUIRE_EQUAL(1u, wm.get_public_keys(_token).size());
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
  BOOST_REQUIRE_EQUAL(1u, wm.get_public_keys(_token).size());
  keys = wm.list_keys(_token, "test", pw);
  BOOST_REQUIRE( cmp_keys( key2, keys ) == keys.end() );
  wm.import_key(_token, "test", key2_str);
  BOOST_REQUIRE_EQUAL(2u, wm.get_public_keys(_token).size());
  keys = wm.list_keys(_token, "test", pw);
  BOOST_REQUIRE( cmp_keys( key2, keys ) != keys.end() );
  BOOST_REQUIRE_THROW(wm.remove_key(_token, "test", pw, beekeeper::utility::public_key::to_string( pub_pri_pair(key3).first ) ), fc::exception);
  BOOST_REQUIRE_EQUAL(2u, wm.get_public_keys(_token).size());
  BOOST_REQUIRE_THROW(wm.remove_key(_token, "test", "PWnogood", beekeeper::utility::public_key::to_string( pub_pri_pair(key2).first ) ), fc::exception);
  BOOST_REQUIRE_EQUAL(2u, wm.get_public_keys(_token).size());

  wm.lock(_token, "test");
  BOOST_REQUIRE_THROW(wm.list_keys(_token, "test", pw), fc::exception);
  BOOST_REQUIRE_THROW(wm.get_public_keys(_token), fc::exception);
  wm.unlock(_token, "test", pw);
  BOOST_REQUIRE_EQUAL(2u, wm.get_public_keys(_token).size());
  BOOST_REQUIRE_EQUAL(2u, wm.list_keys(_token, "test", pw).size());
  wm.lock_all(_token);
  BOOST_REQUIRE_THROW(wm.get_public_keys(_token), fc::exception);
  BOOST_REQUIRE(!wm.list_wallets(_token)[0].unlocked);

  auto pw2 = create(wm, _token, "test2");
  BOOST_REQUIRE_EQUAL(2u, wm.list_wallets(_token).size());
  // wallet has no keys when it is created
  BOOST_REQUIRE_EQUAL(0u, wm.get_public_keys(_token).size());
  wm.import_key(_token, "test2", key3_str);
  BOOST_REQUIRE_EQUAL(1u, wm.get_public_keys(_token).size());
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

  BOOST_REQUIRE_EQUAL(3u, wm.get_public_keys(_token).size());
  wm.set_timeout(_token, 0);
  BOOST_REQUIRE_THROW(wm.get_public_keys(_token), fc::exception);
  BOOST_REQUIRE_THROW(wm.list_keys(_token, "test", pw), fc::exception);

  wm.set_timeout(_token, 15);

  create(wm, _token, "testgen");
  wm.lock(_token, "testgen");
  fc::remove("testgen.wallet");

  pw = create(wm, _token, "testgen");

  wm.lock(_token, "testgen");
  BOOST_REQUIRE(fc::exists("testgen.wallet"));
  fc::remove("testgen.wallet");

  BOOST_REQUIRE(fc::exists("test.wallet"));
  BOOST_REQUIRE(fc::exists("test2.wallet"));
  fc::remove("test.wallet");
  fc::remove("test2.wallet");

} FC_LOG_AND_RETHROW() }

/// Test wallet manager
BOOST_AUTO_TEST_CASE(wallet_manager_create_test)
{
  try {
    if (fc::exists("test.wallet")) fc::remove("test.wallet");

    beekeeper_wallet_manager wm = create_wallet( ".", 900, 3 );

    BOOST_REQUIRE( wm.start() );
    std::string _token = wm.create_session( "this is salt", "127.0.0.1:666" );

    create(wm, _token, "test");
    constexpr auto key1 = "5JktVNHnRX48BUdtewU7N1CyL4Z886c42x7wYW7XhNWkDQRhdcS";
    wm.import_key(_token, "test", key1);
    BOOST_REQUIRE_THROW(create(wm, _token, "test"),        fc::exception);

    BOOST_REQUIRE_THROW(create(wm, _token, "./test"),      fc::exception);
    BOOST_REQUIRE_THROW(create(wm, _token, "../../test"),  fc::exception);
    BOOST_REQUIRE_THROW(create(wm, _token, "/tmp/test"),   fc::exception);
    BOOST_REQUIRE_THROW(create(wm, _token, "/tmp/"),       fc::exception);
    BOOST_REQUIRE_THROW(create(wm, _token, "/"),           fc::exception);
    BOOST_REQUIRE_THROW(create(wm, _token, ",/"),          fc::exception);
    BOOST_REQUIRE_THROW(create(wm, _token, ","),           fc::exception);
    BOOST_REQUIRE_THROW(create(wm, _token, "<<"),          fc::exception);
    BOOST_REQUIRE_THROW(create(wm, _token, "<"),           fc::exception);
    BOOST_REQUIRE_THROW(create(wm, _token, ",<"),          fc::exception);
    BOOST_REQUIRE_THROW(create(wm, _token, ",<<"),         fc::exception);
    BOOST_REQUIRE_THROW(create(wm, _token, ""),            fc::exception);

    fc::remove("test.wallet");

    create(wm, _token, ".test");
    BOOST_REQUIRE(fc::exists(".test.wallet"));
    fc::remove(".test.wallet");
    create(wm, _token, "..test");
    BOOST_REQUIRE(fc::exists("..test.wallet"));
    fc::remove("..test.wallet");
    create(wm, _token, "...test");
    BOOST_REQUIRE(fc::exists("...test.wallet"));
    fc::remove("...test.wallet");
    create(wm, _token, ".");
    BOOST_REQUIRE(fc::exists("..wallet"));
    fc::remove("..wallet");
    create(wm, _token, "__test_test");
    BOOST_REQUIRE(fc::exists("__test_test.wallet"));
    fc::remove("__test_test.wallet");
    create(wm, _token, "t-t");
    BOOST_REQUIRE(fc::exists("t-t.wallet"));
    fc::remove("t-t.wallet");

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(wallet_manager_sessions)
{
  try {
    const std::string _port = "127.0.0.1:666";
    const std::string _dir = ".";
    const uint64_t _timeout = 90;
    const uint32_t _limit = 3;

    {
      bool _checker = false;
      beekeeper_wallet_manager wm = create_wallet( _dir, _timeout, _limit, [&_checker](){ _checker = true; } );

      auto _token = wm.create_session( "this is salt", _port );
      wm.close_session( _token );
      BOOST_REQUIRE( _checker );
    }
    {
      bool _checker = false;
      beekeeper_wallet_manager wm = create_wallet(  _dir, _timeout, _limit, [&_checker](){ _checker = true; } );

      auto _token_00 = wm.create_session( "this is salt", _port );
      auto _token_01 = wm.create_session( "this is salt", _port );
      wm.close_session( _token_00 );
      BOOST_REQUIRE( !_checker );
      wm.close_session( _token_01 );
      BOOST_REQUIRE( _checker );
    }
    {
      bool _checker = false;
      beekeeper_wallet_manager wm = create_wallet(  _dir, _timeout, _limit, [&_checker](){ _checker = true; } );
      BOOST_REQUIRE( wm.start() );

      auto _token_00 = wm.create_session( "aaaa", _port );
      auto _token_01 = wm.create_session( "bbbb", _port );

      if (fc::exists("avocado.wallet")) fc::remove("avocado.wallet");
      if (fc::exists("banana.wallet")) fc::remove("banana.wallet");
      if (fc::exists("cherry.wallet")) fc::remove("cherry.wallet");

      std::string _pass_00 = create(wm,  _token_00, "avocado" );
      std::string _pass_01 = create(wm,  _token_01, "banana" );
      std::string _pass_02 = create(wm,  _token_01, "cherry" );

      BOOST_REQUIRE_THROW( wm.list_wallets( "not existed token" ), fc::exception );
      BOOST_REQUIRE_EQUAL( wm.list_wallets( _token_00 ).size(), 1 );
      BOOST_REQUIRE_EQUAL( wm.list_wallets( _token_01 ).size(), 2 );

      wm.close_session( _token_00 );
      BOOST_REQUIRE( !_checker );

      BOOST_REQUIRE_THROW( wm.list_wallets( "not existed token" ), fc::exception );
      BOOST_REQUIRE_THROW( wm.list_wallets( _token_00 ), fc::exception );
      BOOST_REQUIRE_EQUAL( wm.list_wallets( _token_01 ).size(), 2 );

      wm.close_session( _token_01 );
      BOOST_REQUIRE( _checker );

      BOOST_REQUIRE_THROW( wm.list_wallets( "not existed token" ), fc::exception );
      BOOST_REQUIRE_THROW( wm.list_wallets( _token_00 ), fc::exception );
      BOOST_REQUIRE_THROW( wm.list_wallets( _token_01 ), fc::exception );
    }

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(wallet_manager_info)
{
  try {

    const std::string _port = "127.0.0.1:666";
    const std::string _dir = ".";
    const uint64_t _timeout = 90;
    const uint32_t _limit = 3;

    {
      bool _checker = false;
      beekeeper_wallet_manager wm = create_wallet(  _dir, _timeout, _limit, [&_checker](){ _checker = true; } );
      BOOST_REQUIRE( wm.start() );

      auto _token_00 = wm.create_session( "aaaa", _port );

      std::this_thread::sleep_for( std::chrono::seconds(3) );

      auto _token_01 = wm.create_session( "bbbb", _port );

      auto _info_00 = wm.get_info( _token_00 );
      auto _info_01 = wm.get_info( _token_01 );

      BOOST_TEST_MESSAGE( _info_00.timeout_time );
      BOOST_TEST_MESSAGE( _info_01.timeout_time );

      auto _time_00 = fc::time_point::from_iso_string( _info_00.timeout_time );
      auto _time_01 = fc::time_point::from_iso_string( _info_01.timeout_time );

      BOOST_REQUIRE( _time_01 >_time_00 );

      wm.close_session( _token_01 );
      BOOST_REQUIRE( !_checker );

      auto _token_02 = wm.create_session( "cccc", _port );

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

    const std::string _port = "127.0.0.1:666";
    const std::string _dir = ".";
    const uint64_t _timeout = 90;
    const uint32_t _session_limit = 64;

    bool _checker = false;
    beekeeper_wallet_manager wm = create_wallet(  _dir, _timeout, _session_limit, [&_checker](){ _checker = true; } );
    BOOST_REQUIRE( wm.start() );

    std::vector<std::string> _tokens;
    for( uint32_t i = 0; i < _session_limit; ++i )
    {
      _tokens.emplace_back( wm.create_session( "salt", _port ) );
    }
    BOOST_REQUIRE_THROW( wm.create_session( "salt", _port ), fc::exception );

    BOOST_REQUIRE( _tokens.size() == _session_limit );

    wm.close_session( _tokens[0] );
    wm.close_session( _tokens[1] );
    _tokens.erase( _tokens.begin() );
    _tokens.erase( _tokens.begin() );

    _tokens.emplace_back( wm.create_session( "salt", _port ) );
    _tokens.emplace_back( wm.create_session( "salt", _port ) );

    BOOST_REQUIRE_THROW( wm.create_session( "salt", _port ), fc::exception );

    BOOST_REQUIRE( _tokens.size() == _session_limit );

    BOOST_REQUIRE( _checker == false );

    for( auto& token : _tokens )
    {
      wm.close_session( token );
    }

    BOOST_REQUIRE( _checker == true );

    _tokens.emplace_back( wm.create_session( "salt", _port ) );
    _tokens.emplace_back( wm.create_session( "salt", _port ) );

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(wallet_manager_close)
{
  try {

    const std::string _port = "127.0.0.1:666";
    const std::string _dir = ".";
    const uint64_t _timeout = 90;
    const uint32_t _session_limit = 64;

    bool _checker = false;
    beekeeper_wallet_manager wm = create_wallet(  _dir, _timeout, _session_limit, [&_checker](){ _checker = true; } );
    BOOST_REQUIRE( wm.start() );

    auto wallet_name_0 = "0";
    auto wallet_name_1 = "1";

    {
      if (fc::exists("0.wallet")) fc::remove("0.wallet");
      if (fc::exists("1.wallet")) fc::remove("1.wallet");

      auto _token = wm.create_session( "salt", _port );
      create(wm,  _token, wallet_name_0 );

      auto _wallets = wm.list_wallets( _token );
      BOOST_REQUIRE( _wallets.size() == 1 );
      BOOST_REQUIRE( _wallets[0].name == wallet_name_0 );

      BOOST_REQUIRE_THROW( wm.close( _token, wallet_name_1 ), fc::exception );
      wm.close( _token, wallet_name_0 );

      _wallets = wm.list_wallets( _token );
      BOOST_REQUIRE( _wallets.size() == 0 );

      create(wm,  _token, wallet_name_1 );

      _wallets = wm.list_wallets( _token );
      BOOST_REQUIRE( _wallets.size() == 1 );
      BOOST_REQUIRE( _wallets[0].name == wallet_name_1 );

      wm.close( _token, wallet_name_1 );

      _wallets = wm.list_wallets( _token );
      BOOST_REQUIRE( _wallets.size() == 0 );
    }

    {
      if (fc::exists("0.wallet")) fc::remove("0.wallet");
      if (fc::exists("1.wallet")) fc::remove("1.wallet");

      auto _token = wm.create_session( "salt", _port );
      create(wm,  _token, wallet_name_0 );

      wm.lock_all( _token );

      auto _wallets = wm.list_wallets( _token );
      BOOST_REQUIRE( _wallets.size() == 1 );
      BOOST_REQUIRE( _wallets[0].name == wallet_name_0 );

      wm.close( _token, wallet_name_0 );

      _wallets = wm.list_wallets( _token );
      BOOST_REQUIRE( _wallets.size() == 0 );
    }

    {
      if (fc::exists("0.wallet")) fc::remove("0.wallet");
      if (fc::exists("1.wallet")) fc::remove("1.wallet");

      auto _token = wm.create_session( "salt", _port );
      create(wm,  _token, wallet_name_0 );
      create(wm,  _token, wallet_name_1 );

      wm.lock( _token, wallet_name_1 );

      auto _wallets = wm.list_wallets( _token );
      BOOST_REQUIRE( _wallets.size() == 2 );
      BOOST_REQUIRE( _wallets[0].name == wallet_name_0 );
      BOOST_REQUIRE( _wallets[1].name == wallet_name_1 );

      wm.close( _token, wallet_name_1 );

      _wallets = wm.list_wallets( _token );
      BOOST_REQUIRE( _wallets.size() == 1 );
      BOOST_REQUIRE( _wallets[0].name == wallet_name_0 );

      wm.lock( _token, wallet_name_0 );

      _wallets = wm.list_wallets( _token );
      BOOST_REQUIRE( _wallets.size() == 1 );
      BOOST_REQUIRE( _wallets[0].name == wallet_name_0 );

      wm.close( _token, wallet_name_0 );

      _wallets = wm.list_wallets( _token );
      BOOST_REQUIRE( _wallets.size() == 0 );
    }

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(wallet_manager_sign_transaction)
{
  try {

    {
      hive::protocol::serialization_mode_controller::pack_guard guard( hive::protocol::pack_type::hf26 );

      if (fc::exists("0.wallet")) fc::remove("0.wallet");

      auto _private_key_str = "5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n";
      auto _public_key_str  = "6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4";

      const auto _private_key = private_key_type::wif_to_key( _private_key_str ).value();
      const auto _public_key = public_key_type::from_base58( _public_key_str, false/*is_sha256*/ );

      const std::string _port = "127.0.0.1:666";
      const std::string _dir = ".";
      const uint64_t _timeout = 90;
      const uint32_t _session_limit = 64;

      beekeeper_wallet_manager wm = create_wallet(  _dir, _timeout, _session_limit, [](){} );
      BOOST_REQUIRE( wm.start() );

      auto _token = wm.create_session( "salt", _port );
      auto _password = create(wm,  _token, "0" );
      auto _imported_public_key = wm.import_key( _token, "0", _private_key_str );
      BOOST_REQUIRE( _imported_public_key == _public_key_str );

      auto _calculate_signature = [&]( const std::string& json_trx, const std::string& binary_trx, const std::string& signature_pattern )
      {
        hive::protocol::transaction _trx = fc::json::from_string( json_trx ).as<hive::protocol::transaction>();
        hive::protocol::digest_type _sig_digest = _trx.sig_digest( HIVE_CHAIN_ID, hive::protocol::pack_type::hf26 );

        auto _signature_local = _private_key.sign_compact( _sig_digest );
        auto _signature_beekeeper = wm.sign_transaction( _token, json_trx, HIVE_CHAIN_ID, _public_key_str );
        auto _signature_beekeeper_2 = wm.sign_binary_transaction( _token, binary_trx, HIVE_CHAIN_ID, _public_key_str );

        auto _local = fc::json::to_string( _signature_local );
        auto _beekeeper = fc::json::to_string( _signature_beekeeper );
        auto _beekeeper_2 = fc::json::to_string( _signature_beekeeper_2 );
        BOOST_TEST_MESSAGE( _local );
        BOOST_TEST_MESSAGE( _beekeeper );
        BOOST_REQUIRE( _local.substr( 1, _local.size() - 2 )          == signature_pattern );
        BOOST_REQUIRE( _beekeeper.substr( 1, _beekeeper.size() - 2 )  == signature_pattern );
        BOOST_REQUIRE( _beekeeper_2.substr( 1, _beekeeper_2.size() - 2 )  == signature_pattern );
      };

      std::string _binary_trx_00 = "000000000000000000000000";
      std::string _signature_00_result = "1f17cc07f7c769073d39fac3385220b549e261fb33c5f619c5dced7f5b0fe9c0954f2684e703710840b7ea01ad7238b8db1d8a9309d03e93de212f86de38d66f21";
      _calculate_signature( "{}", _binary_trx_00, _signature_00_result );

      std::string _binary_trx_01 = "5f00c58fb5f9854fb664010209696e69746d696e657205616c6963659a020000000000002320bcbe056d656d6d6d00";
      std::string _signature_01_result = "1f69e091fc79b0e8d1812fc662f12076561f9e38ffc212b901ae90fe559f863ad266fe459a8e946cff9bbe7e56ce253bbfab0cccdde944edc1d05161c61ae86340";
      _calculate_signature( "{\"ref_block_num\":95,\"ref_block_prefix\":4189425605,\"expiration\":\"2023-07-18T08:38:29\",\"operations\":[{\"type\":\"transfer_operation\",\"value\":{\"from\":\"initminer\",\"to\":\"alice\",\"amount\":{\"amount\":\"666\",\"precision\":3,\"nai\":\"@@000000021\"},\"memo\":\"memmm\"}}],\"extensions\":[],\"signatures\":[],\"transaction_id\":\"cc9630cdbc39da1c9b6264df3588c7bedb5762fa\",\"block_num\":0,\"transaction_num\":0}",
                            _binary_trx_01,
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

BOOST_AUTO_TEST_CASE(wasm_beekeeper)
{
  try {
    auto _current_path = fc::current_path();
    fc::path _dir("./beekeeper-storage/"); 

    auto _wallet_0_path = _current_path / _dir / "wallet_0.wallet";
    auto _wallet_1_path = _current_path / _dir / "wallet_1.wallet";

    if( fc::exists( _wallet_0_path ) )
      fc::remove( _wallet_0_path );

    if( fc::exists( _wallet_1_path ) )
      fc::remove( _wallet_1_path );

    beekeeper::beekeeper_api _obj( { "--wallet-dir", _dir.string(), "--salt", "avocado" } );

    BOOST_REQUIRE( fc::json::from_string( extract_json( _obj.init() ) ).as<beekeeper::init_data>().status );

    auto get_data = []( const std::string& json )
    {
      vector< string > _elements;
      boost::split( _elements, json, boost::is_any_of( "\"" ) );
      BOOST_REQUIRE( _elements.size() >= 2 );
      return _elements[ _elements.size() - 2 ];
    };

    auto _token = extract_json( _obj.create_session( "banana" ) );
    BOOST_TEST_MESSAGE( _token );
    _token = get_data( _token );

    auto _password_0 = extract_json( _obj.create( _token, "wallet_0", "pear" ) );
    BOOST_TEST_MESSAGE( _password_0 );
    _password_0 = get_data( _password_0 );

    auto _password_1 = extract_json( _obj.create( _token, "wallet_1", "cherry" ) );
    BOOST_TEST_MESSAGE( _password_1 );
    BOOST_REQUIRE( _password_1.find( "cherry" ) != std::string::npos );
    _password_1 = get_data( _password_1 );

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

    auto _public_keys = extract_json( _obj.get_public_keys( _token ) );
    BOOST_TEST_MESSAGE( _public_keys );

    _obj.remove_key( _token, "wallet_1", _password_1, "6oR6ckA4TejTWTjatUdbcS98AKETc3rcnQ9dWxmeNiKDzfhBZa" );

    _public_keys = extract_json( _obj.get_public_keys( _token ) );
    BOOST_TEST_MESSAGE( _public_keys );

    _obj.close_session( _token );

    _token = extract_json( _obj.create_session( "banana" ) );
    BOOST_TEST_MESSAGE( _token );
    _token = get_data( _token );

    _obj.open( _token, "wallet_1" );

    BOOST_REQUIRE( extract_json( _obj.get_public_keys( _token ) ).find( "You don't have any unlocked wallet" ) != std::string::npos );

    _obj.close( _token, "wallet_1" );

    _obj.unlock( _token, "wallet_0", _password_0 );

    _wallets = extract_json( _obj.list_wallets( _token ) );
    BOOST_TEST_MESSAGE( _wallets );
    {
      beekeeper::list_wallets_return _result = fc::json::from_string( _wallets ).as<beekeeper::list_wallets_return>();
      BOOST_REQUIRE( _result.wallets.size() == 1 );
      BOOST_REQUIRE( _result.wallets[0].name == "wallet_0" );
      BOOST_REQUIRE( _result.wallets[0].unlocked );
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
        auto _signature_beekeeper_2 = fc::json::from_string( extract_json( _obj.sign_transaction( _token, json_trx, HIVE_CHAIN_ID, _public_key_str ) ) ).as<beekeeper::signature_return>();

        auto _local = fc::json::to_string( _signature_local );
        auto _beekeeper = fc::json::to_string( _signature_beekeeper.signature );
        auto _beekeeper_2 = fc::json::to_string( _signature_beekeeper_2.signature );
        BOOST_TEST_MESSAGE( _local );
        BOOST_TEST_MESSAGE( _beekeeper );
        BOOST_TEST_MESSAGE( _beekeeper_2 );
        BOOST_REQUIRE( _local.substr( 1, _local.size() - 2 )              == signature_pattern );
        BOOST_REQUIRE( _beekeeper.substr( 1, _beekeeper.size() - 2 )      == signature_pattern );
        BOOST_REQUIRE( _beekeeper_2.substr( 1, _beekeeper_2.size() - 2 )  == signature_pattern );
      };

      std::string _signature_00_result = "1f17cc07f7c769073d39fac3385220b549e261fb33c5f619c5dced7f5b0fe9c0954f2684e703710840b7ea01ad7238b8db1d8a9309d03e93de212f86de38d66f21";
      _calculate_signature( "{}", _signature_00_result );

      std::string _signature_01_result = "1f69e091fc79b0e8d1812fc662f12076561f9e38ffc212b901ae90fe559f863ad266fe459a8e946cff9bbe7e56ce253bbfab0cccdde944edc1d05161c61ae86340";
      _calculate_signature( "{\"ref_block_num\":95,\"ref_block_prefix\":4189425605,\"expiration\":\"2023-07-18T08:38:29\",\"operations\":[{\"type\":\"transfer_operation\",\"value\":{\"from\":\"initminer\",\"to\":\"alice\",\"amount\":{\"amount\":\"666\",\"precision\":3,\"nai\":\"@@000000021\"},\"memo\":\"memmm\"}}],\"extensions\":[],\"signatures\":[],\"transaction_id\":\"cc9630cdbc39da1c9b6264df3588c7bedb5762fa\",\"block_num\":0,\"transaction_num\":0}",
                            _signature_01_result );
    }

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(wallet_manager_brute_force_protection_test)
{
  try {
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
    //BOOST_REQUIRE( _duration.count() >= 5000 );

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(wasm_beekeeper_parameters)
{
  try {
    auto _current_path = fc::current_path();
    fc::path _dir("./beekeeper-storage/"); 

    {
      beekeeper::beekeeper_api _obj( { "--wallet-dir", _dir.string(), "--salt", "avocado", "--allow-implicit-session", "true" } );
      beekeeper::init_data _init_data = fc::json::from_string( extract_json( _obj.init() ) ).as<beekeeper::init_data>();
      BOOST_TEST_MESSAGE( _init_data.token );
      BOOST_TEST_MESSAGE( _init_data.version );
      BOOST_REQUIRE( !_init_data.token.empty() );
      BOOST_REQUIRE( !_init_data.version.empty() );
      BOOST_REQUIRE( _init_data.status );
    }

    {
      beekeeper::beekeeper_api _obj( { "--wallet-dir", _dir.string(), "--salt", "avocado", "--allow-implicit-session", "false" } );
      beekeeper::init_data _init_data = fc::json::from_string( extract_json( _obj.init() ) ).as<beekeeper::init_data>();
      BOOST_TEST_MESSAGE( _init_data.token );
      BOOST_REQUIRE( _init_data.token.empty() );
      BOOST_REQUIRE( _init_data.status );
    }

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()

#endif
