#ifdef IS_TEST_NET

#include <hive/protocol/hive_operations.hpp>
#include <hive/protocol/transaction.hpp>

#include <boost/test/unit_test.hpp>

#include <fc/crypto/elliptic.hpp>
#include <fc/filesystem.hpp>
#include <fc/io/json.hpp>

#include <core/beekeeper_wallet.hpp>
#include <core/beekeeper_wallet_manager.hpp>
#include <beekeeper/session_manager.hpp>
#include <beekeeper/beekeeper_instance.hpp>

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
  return beekeeper_wallet_manager( std::make_shared<session_manager>(), std::make_shared<beekeeper_instance>( cmd_wallet_dir ),
                                    cmd_wallet_dir, cmd_unlock_timeout, cmd_session_limit );
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

  auto pw = wm.create(_token, "test");
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

  wm.remove_key(_token, "test", pw, public_key_type::to_base58( pub_pri_pair(key2).first, false/*is_sha256*/ ) );
  BOOST_REQUIRE_EQUAL(1u, wm.get_public_keys(_token).size());
  keys = wm.list_keys(_token, "test", pw);
  BOOST_REQUIRE( cmp_keys( key2, keys ) == keys.end() );
  wm.import_key(_token, "test", key2_str);
  BOOST_REQUIRE_EQUAL(2u, wm.get_public_keys(_token).size());
  keys = wm.list_keys(_token, "test", pw);
  BOOST_REQUIRE( cmp_keys( key2, keys ) != keys.end() );
  BOOST_REQUIRE_THROW(wm.remove_key(_token, "test", pw, public_key_type::to_base58( pub_pri_pair(key3).first, false/*is_sha256*/ ) ), fc::exception);
  BOOST_REQUIRE_EQUAL(2u, wm.get_public_keys(_token).size());
  BOOST_REQUIRE_THROW(wm.remove_key(_token, "test", "PWnogood", public_key_type::to_base58( pub_pri_pair(key2).first, false/*is_sha256*/ ) ), fc::exception);
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

  auto pw2 = wm.create(_token, "test2");
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

  wm.create(_token, "testgen");
  wm.lock(_token, "testgen");
  fc::remove("testgen.wallet");

  pw = wm.create(_token, "testgen");

  wm.lock(_token, "testgen");
  BOOST_REQUIRE(fc::exists("testgen.wallet"));
  fc::remove("testgen.wallet");

  BOOST_REQUIRE(fc::exists("test.wallet"));
  BOOST_REQUIRE(fc::exists("test2.wallet"));
  fc::remove("test.wallet");
  fc::remove("test2.wallet");

} FC_LOG_AND_RETHROW() }

/// Test wallet manager
BOOST_AUTO_TEST_CASE(wallet_manager_create_test) {
  try {
    if (fc::exists("test.wallet")) fc::remove("test.wallet");

    beekeeper_wallet_manager wm = create_wallet( ".", 900, 3 );

    BOOST_REQUIRE( wm.start() );
    std::string _token = wm.create_session( "this is salt", "127.0.0.1:666" );

    wm.create(_token, "test");
    constexpr auto key1 = "5JktVNHnRX48BUdtewU7N1CyL4Z886c42x7wYW7XhNWkDQRhdcS";
    wm.import_key(_token, "test", key1);
    BOOST_REQUIRE_THROW(wm.create(_token, "test"),        fc::exception);

    BOOST_REQUIRE_THROW(wm.create(_token, "./test"),      fc::exception);
    BOOST_REQUIRE_THROW(wm.create(_token, "../../test"),  fc::exception);
    BOOST_REQUIRE_THROW(wm.create(_token, "/tmp/test"),   fc::exception);
    BOOST_REQUIRE_THROW(wm.create(_token, "/tmp/"),       fc::exception);
    BOOST_REQUIRE_THROW(wm.create(_token, "/"),           fc::exception);
    BOOST_REQUIRE_THROW(wm.create(_token, ",/"),          fc::exception);
    BOOST_REQUIRE_THROW(wm.create(_token, ","),           fc::exception);
    BOOST_REQUIRE_THROW(wm.create(_token, "<<"),          fc::exception);
    BOOST_REQUIRE_THROW(wm.create(_token, "<"),           fc::exception);
    BOOST_REQUIRE_THROW(wm.create(_token, ",<"),          fc::exception);
    BOOST_REQUIRE_THROW(wm.create(_token, ",<<"),         fc::exception);
    BOOST_REQUIRE_THROW(wm.create(_token, ""),            fc::exception);

    fc::remove("test.wallet");

    wm.create(_token, ".test");
    BOOST_REQUIRE(fc::exists(".test.wallet"));
    fc::remove(".test.wallet");
    wm.create(_token, "..test");
    BOOST_REQUIRE(fc::exists("..test.wallet"));
    fc::remove("..test.wallet");
    wm.create(_token, "...test");
    BOOST_REQUIRE(fc::exists("...test.wallet"));
    fc::remove("...test.wallet");
    wm.create(_token, ".");
    BOOST_REQUIRE(fc::exists("..wallet"));
    fc::remove("..wallet");
    wm.create(_token, "__test_test");
    BOOST_REQUIRE(fc::exists("__test_test.wallet"));
    fc::remove("__test_test.wallet");
    wm.create(_token, "t-t");
    BOOST_REQUIRE(fc::exists("t-t.wallet"));
    fc::remove("t-t.wallet");

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(wallet_manager_sessions) {
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

      std::string _pass_00 = wm.create( _token_00, "avocado" );
      std::string _pass_01 = wm.create( _token_01, "banana" );
      std::string _pass_02 = wm.create( _token_01, "cherry" );

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

BOOST_AUTO_TEST_CASE(wallet_manager_info) {
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

BOOST_AUTO_TEST_CASE(wallet_manager_session_limit) {
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

BOOST_AUTO_TEST_CASE(wallet_manager_close) {
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
      wm.create( _token, wallet_name_0 );

      auto _wallets = wm.list_wallets( _token );
      BOOST_REQUIRE( _wallets.size() == 1 );
      BOOST_REQUIRE( _wallets[0].name == wallet_name_0 );

      BOOST_REQUIRE_THROW( wm.close( _token, wallet_name_1 ), fc::exception );
      wm.close( _token, wallet_name_0 );

      _wallets = wm.list_wallets( _token );
      BOOST_REQUIRE( _wallets.size() == 0 );

      wm.create( _token, wallet_name_1 );

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
      wm.create( _token, wallet_name_0 );

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
      wm.create( _token, wallet_name_0 );
      wm.create( _token, wallet_name_1 );

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

BOOST_AUTO_TEST_CASE(wallet_manager_sign_transaction) {
  try {

    {
      hive::protocol::serialization_mode_controller::pack_guard guard( hive::protocol::pack_type::hf26 );

      if (fc::exists("0.wallet")) fc::remove("0.wallet");

      auto _private_key_str = "5JEADfzTuGSkZidDV5DJYjayK5YXd4CxGtvTi84g7CzfLMGzhk7";
      auto _public_key_str  = "86iMbKqHgJMwWLfAnm2NbDe44HsjajJnCM38EAor9gFvLgFyaW";

      const auto _private_key = private_key_type::wif_to_key( _private_key_str ).value();
      const auto _public_key = public_key_type::from_base58( _public_key_str, false/*is_sha256*/ );

      hive::protocol::digest_type _sig_digest;
      std::string _trx_serialized;

      hive::protocol::transfer_operation _op;
      _op.to     = "alice";
      _op.from   = "bob";
      _op.amount = hive::protocol::asset( 6, HIVE_SYMBOL );

      hive::protocol::transaction _trx;
      _trx.operations.push_back( _op );

      _sig_digest = _trx.sig_digest( HIVE_CHAIN_ID, hive::protocol::pack_type::hf26 );
      _trx_serialized = fc::to_hex( fc::raw::pack_to_vector( _trx ) );

      auto _trx_str = fc::json::to_string( _trx );

      auto _signature_00 = _private_key.sign_compact( _sig_digest );

      const std::string _port = "127.0.0.1:666";
      const std::string _dir = ".";
      const uint64_t _timeout = 90;
      const uint32_t _session_limit = 64;

      beekeeper_wallet_manager wm = create_wallet(  _dir, _timeout, _session_limit, [](){} );
      BOOST_REQUIRE( wm.start() );

      auto _token = wm.create_session( "salt", _port );
      auto _password = wm.create( _token, "0" );
      auto _imported_public_key = wm.import_key( _token, "0", _private_key_str );
      BOOST_REQUIRE( _imported_public_key == _public_key_str );

      auto _signature_01 = wm.sign_transaction( _token, _trx_serialized, HIVE_CHAIN_ID, _public_key, _sig_digest );

      auto _signature_00_str = fc::json::to_string( _signature_00 );
      auto _signature_01_str = fc::json::to_string( _signature_01 );

      BOOST_TEST_MESSAGE( _signature_00_str );
      BOOST_TEST_MESSAGE( _signature_01_str );
      BOOST_REQUIRE( _signature_00 == _signature_01 );

    }

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()

#endif
