#ifdef IS_TEST_NET

#include <boost/test/unit_test.hpp>

#include <fc/crypto/elliptic.hpp>
#include <fc/filesystem.hpp>

#include <beekeeper/beekeeper_wallet.hpp>
#include <beekeeper/beekeeper_wallet_manager.hpp>

using public_key_type           = beekeeper::public_key_type;
using private_key_type          = beekeeper::private_key_type;
using beekeeper_wallet_manager  = beekeeper::beekeeper_wallet_manager;
using wallet_data               = beekeeper::wallet_data;
using beekeeper_wallet          = beekeeper::beekeeper_wallet;

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

/// Test wallet manager
BOOST_AUTO_TEST_CASE(wallet_manager_test)
{ try {
  if (fc::exists("test.wallet")) fc::remove("test.wallet");
  if (fc::exists("test2.wallet")) fc::remove("test2.wallet");
  if (fc::exists("testgen.wallet")) fc::remove("testgen.wallet");

  constexpr auto key1 = "5JktVNHnRX48BUdtewU7N1CyL4Z886c42x7wYW7XhNWkDQRhdcS";
  constexpr auto key2 = "5Ju5RTcVDo35ndtzHioPMgebvBM6LkJ6tvuU6LTNQv8yaz3ggZr";
  constexpr auto key3 = "5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3";

  beekeeper_wallet_manager wm( ".", 900, 3 );

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
  wm.import_key(_token, "test", key1);
  BOOST_REQUIRE_EQUAL(1u, wm.get_public_keys(_token).size());
  auto keys = wm.list_keys(_token, "test", pw);

  auto pub_pri_pair = [](const char *key) -> auto {
      auto prikey = private_key_type::wif_to_key( key );
      return std::pair<const public_key_type, private_key_type>(prikey->get_public_key(), *prikey);
  };

  auto pub_pri_pair_str = [&](const char *key) -> auto {
    auto _keys = pub_pri_pair( key );
    return std::make_pair( _keys.first.to_base58_with_prefix( HIVE_ADDRESS_PREFIX ), _keys.second.key_to_wif() );
  };

  auto get_key = [&]( const char* key, const std::map<std::string, std::string>& keys )
  {
    return std::find_if( keys.begin(), keys.end(), [&]( const std::pair<std::string, std::string>& item ){ return pub_pri_pair_str( key ) == item; } );
  };

  BOOST_REQUIRE( get_key( key1, keys ) != keys.end() );

  wm.import_key(_token, "test", key2);
  keys = wm.list_keys(_token, "test", pw);
  BOOST_REQUIRE( get_key( key1, keys ) != keys.end() );
  BOOST_REQUIRE( get_key( key2, keys ) != keys.end() );
  // key3 was not automatically imported
  BOOST_REQUIRE( get_key( key3, keys ) == keys.end() );

  wm.remove_key(_token, "test", pw, pub_pri_pair(key2).first.to_base58_with_prefix( HIVE_ADDRESS_PREFIX ));
  BOOST_REQUIRE_EQUAL(1u, wm.get_public_keys(_token).size());
  keys = wm.list_keys(_token, "test", pw);
  BOOST_REQUIRE( get_key( key2, keys ) == keys.end() );
  wm.import_key(_token, "test", key2);
  BOOST_REQUIRE_EQUAL(2u, wm.get_public_keys(_token).size());
  keys = wm.list_keys(_token, "test", pw);
  BOOST_REQUIRE( get_key( key2, keys ) != keys.end() );
  BOOST_REQUIRE_THROW(wm.remove_key(_token, "test", pw, pub_pri_pair(key3).first.to_base58_with_prefix( HIVE_ADDRESS_PREFIX )), fc::exception);
  BOOST_REQUIRE_EQUAL(2u, wm.get_public_keys(_token).size());
  BOOST_REQUIRE_THROW(wm.remove_key(_token, "test", "PWnogood", pub_pri_pair(key2).first.to_base58_with_prefix( HIVE_ADDRESS_PREFIX )), fc::exception);
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
  wm.import_key(_token, "test2", key3);
  BOOST_REQUIRE_EQUAL(1u, wm.get_public_keys(_token).size());
  BOOST_REQUIRE_THROW(wm.import_key(_token, "test2", key3), fc::exception);
  keys = wm.list_keys(_token, "test2", pw2);
  BOOST_REQUIRE( get_key( key1, keys ) == keys.end() );
  BOOST_REQUIRE( get_key( key2, keys ) == keys.end() );
  BOOST_REQUIRE( get_key( key3, keys ) != keys.end() );
  wm.unlock(_token, "test", pw);
  keys = wm.list_keys(_token, "test", pw);
  auto keys2 = wm.list_keys(_token, "test2", pw2);
  keys.insert(keys2.begin(), keys2.end());
  BOOST_REQUIRE( get_key( key1, keys ) != keys.end() );
  BOOST_REQUIRE( get_key( key2, keys ) != keys.end() );
  BOOST_REQUIRE( get_key( key3, keys ) != keys.end() );
  BOOST_REQUIRE_EQUAL(3u, keys.size());

  BOOST_REQUIRE_THROW(wm.list_keys(_token, "test2", "PWnogood"), fc::exception);

  // private_key_type pkey1 = *( private_key_type::wif_to_key( key1 ) );
  //private_key_type pkey2 = *( private_key_type::wif_to_key( key2 ) );
  // chain::signed_transaction trx;
  // auto chain_id = genesis_state().compute_chain_id();
  // flat_set<public_key_type> pubkeys;
  // pubkeys.emplace(pkey1.get_public_key());
  // pubkeys.emplace(pkey2.get_public_key());
  // trx = wm.sign_transaction(trx, pubkeys, chain_id );
  // flat_set<public_key_type> pks;
  // trx.get_signature_keys(chain_id, fc::time_point::maximum(), pks);
  // BOOST_REQUIRE_EQUAL(2u, pks.size());
  // BOOST_REQUIRE(find(pks.cbegin(), pks.cend(), pkey1.get_public_key()) != pks.cend());
  // BOOST_REQUIRE(find(pks.cbegin(), pks.cend(), pkey2.get_public_key()) != pks.cend());

  BOOST_REQUIRE_EQUAL(3u, wm.get_public_keys(_token).size());
  wm.set_timeout(_token, 0);
  BOOST_REQUIRE_THROW(wm.get_public_keys(_token), fc::exception);
  BOOST_REQUIRE_THROW(wm.list_keys(_token, "test", pw), fc::exception);

  wm.set_timeout(_token, 15);

  wm.create(_token, "testgen");
  wm.create_key(_token, "testgen");
  wm.lock(_token, "testgen");
  fc::remove("testgen.wallet");

  pw = wm.create(_token, "testgen");

  //check that the public key returned looks legit through a string conversion
  // (would throw otherwise)
  public_key_type create_key_pub( fc::ecc::public_key::from_base58_with_prefix( wm.create_key(_token, "testgen"), HIVE_ADDRESS_PREFIX ) );

  //now pluck out the private key from the wallet and see if the public key of said
  // private key matches what was returned earlier from the create_key() call
  auto create_key_priv = private_key_type::wif_to_key( wm.list_keys(_token, "testgen", pw).cbegin()->second );
  BOOST_REQUIRE_EQUAL(create_key_pub.to_base58_with_prefix( HIVE_ADDRESS_PREFIX ), create_key_priv->get_public_key().to_base58_with_prefix( HIVE_ADDRESS_PREFIX ));

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

    beekeeper_wallet_manager wm( ".", 900, 3 );

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
      beekeeper_wallet_manager wm( _dir, _timeout, _limit, [&_checker](){ _checker = true; } );

      auto _token = wm.create_session( "this is salt", _port );
      wm.close_session( _token );
      BOOST_REQUIRE( _checker );
    }
    {
      bool _checker = false;
      beekeeper_wallet_manager wm(  _dir, _timeout, _limit, [&_checker](){ _checker = true; } );

      auto _token_00 = wm.create_session( "this is salt", _port );
      auto _token_01 = wm.create_session( "this is salt", _port );
      wm.close_session( _token_00 );
      BOOST_REQUIRE( !_checker );
      wm.close_session( _token_01 );
      BOOST_REQUIRE( _checker );
    }
    {
      bool _checker = false;
      beekeeper_wallet_manager wm(  _dir, _timeout, _limit, [&_checker](){ _checker = true; } );
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
      beekeeper_wallet_manager wm(  _dir, _timeout, _limit, [&_checker](){ _checker = true; } );
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
    beekeeper_wallet_manager wm(  _dir, _timeout, _session_limit, [&_checker](){ _checker = true; } );
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

BOOST_AUTO_TEST_SUITE_END()

#endif
