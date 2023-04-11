#ifdef IS_TEST_NET

#include <boost/test/unit_test.hpp>

#include <fc/crypto/elliptic.hpp>
#include <fc/filesystem.hpp>

#include <hive/plugins/clive/clive.hpp>
#include <hive/plugins/clive/wallet_manager.hpp>
#include <hive/plugins/clive/clive_exceptions.hpp>

using public_key_type   = hive::plugins::clive::public_key_type;
using private_key_type  = hive::plugins::clive::private_key_type;
using wallet_manager    = hive::plugins::clive::wallet_manager;
using wallet_data       = hive::plugins::clive::wallet_data;
using clive             = hive::plugins::clive::clive;

BOOST_AUTO_TEST_SUITE(clive_tests)

/// Test creating the wallet
BOOST_AUTO_TEST_CASE(wallet_test)
{ try {
   wallet_data d;
   clive wallet(d);
   BOOST_CHECK(wallet.is_locked());

   wallet.set_password("pass");
   BOOST_CHECK(wallet.is_locked());

   wallet.unlock("pass");
   BOOST_CHECK(!wallet.is_locked());

   wallet.set_wallet_filename("test");
   BOOST_CHECK_EQUAL("test", wallet.get_wallet_filename());

   BOOST_CHECK_EQUAL(0u, wallet.list_keys().size());

   auto priv = fc::ecc::private_key::generate();
   auto pub = priv.get_public_key();
   auto wif = priv.to_base58();
   wallet.import_key(wif);
   BOOST_CHECK_EQUAL(1u, wallet.list_keys().size());

   auto privCopy = wallet.get_private_key(pub);
   BOOST_CHECK_EQUAL(wif, privCopy.to_base58());

   wallet.lock();
   BOOST_CHECK(wallet.is_locked());
   wallet.unlock("pass");
   BOOST_CHECK_EQUAL(1u, wallet.list_keys().size());
   wallet.save_wallet_file("wallet_test.json");
   BOOST_CHECK(fc::exists("wallet_test.json"));

   wallet_data d2;
   clive wallet2(d2);

   BOOST_CHECK(wallet2.is_locked());
   wallet2.load_wallet_file("wallet_test.json");
   BOOST_CHECK(wallet2.is_locked());

   wallet2.unlock("pass");
   BOOST_CHECK_EQUAL(1u, wallet2.list_keys().size());

   auto privCopy2 = wallet2.get_private_key(pub);
   BOOST_CHECK_EQUAL(wif, privCopy2.to_base58());

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

   wallet_manager wm;
   BOOST_CHECK_EQUAL(0u, wm.list_wallets().size());
   BOOST_CHECK_THROW(wm.get_public_keys(), hive::plugins::clive::clive_not_available_exception);
   BOOST_CHECK_NO_THROW(wm.lock_all());

   BOOST_CHECK_THROW(wm.lock("test"), fc::exception);
   BOOST_CHECK_THROW(wm.unlock("test", "pw"), fc::exception);
   BOOST_CHECK_THROW(wm.import_key("test", "pw"), fc::exception);

   auto pw = wm.create("test");
   BOOST_CHECK(!pw.empty());
   BOOST_CHECK_EQUAL(0u, pw.find("PW")); // starts with PW
   BOOST_CHECK_EQUAL(1u, wm.list_wallets().size());
   // wallet has no keys when it is created
   BOOST_CHECK_EQUAL(0u, wm.get_public_keys().size());
   BOOST_CHECK_EQUAL(0u, wm.list_keys("test", pw).size());
   BOOST_CHECK(wm.list_wallets().at(0).find("*") != std::string::npos);
   wm.lock("test");
   BOOST_CHECK(wm.list_wallets().at(0).find("*") == std::string::npos);
   wm.unlock("test", pw);
   BOOST_CHECK_THROW(wm.unlock("test", pw), hive::plugins::clive::clive_unlocked_exception);
   BOOST_CHECK(wm.list_wallets().at(0).find("*") != std::string::npos);
   wm.import_key("test", key1);
   BOOST_CHECK_EQUAL(1u, wm.get_public_keys().size());
   auto keys = wm.list_keys("test", pw);

   auto pub_pri_pair = [](const char *key) -> auto {
       auto prikey = private_key_type::generate_from_base58( key );
       return std::pair<const public_key_type, private_key_type>(prikey->get_public_key(), *prikey);
   };

   auto pub_pri_pair_str = [&](const char *key) -> auto {
      auto _keys = pub_pri_pair( key );
      return std::make_pair( _keys.first.to_base58_with_prefix( HIVE_ADDRESS_PREFIX ), _keys.second.to_base58() );
   };

   auto get_key = [&]( const char* key, const std::map<std::string, std::string>& keys )
   {
      return std::find_if( keys.begin(), keys.end(), [&]( const std::pair<std::string, std::string>& item ){ return pub_pri_pair_str( key ) == item; } );
   };

   BOOST_CHECK( get_key( key1, keys ) != keys.end() );

   wm.import_key("test", key2);
   keys = wm.list_keys("test", pw);
   BOOST_CHECK( get_key( key1, keys ) != keys.end() );
   BOOST_CHECK( get_key( key2, keys ) != keys.end() );
   // key3 was not automatically imported
   BOOST_CHECK( get_key( key3, keys ) == keys.end() );

   wm.remove_key("test", pw, pub_pri_pair(key2).first.to_base58_with_prefix( HIVE_ADDRESS_PREFIX ));
   BOOST_CHECK_EQUAL(1u, wm.get_public_keys().size());
   keys = wm.list_keys("test", pw);
   BOOST_CHECK( get_key( key2, keys ) == keys.end() );
   wm.import_key("test", key2);
   BOOST_CHECK_EQUAL(2u, wm.get_public_keys().size());
   keys = wm.list_keys("test", pw);
   BOOST_CHECK( get_key( key2, keys ) != keys.end() );
   BOOST_CHECK_THROW(wm.remove_key("test", pw, pub_pri_pair(key3).first.to_base58_with_prefix( HIVE_ADDRESS_PREFIX )), fc::exception);
   BOOST_CHECK_EQUAL(2u, wm.get_public_keys().size());
   BOOST_CHECK_THROW(wm.remove_key("test", "PWnogood", pub_pri_pair(key2).first.to_base58_with_prefix( HIVE_ADDRESS_PREFIX )), hive::plugins::clive::clive_invalid_password_exception);
   BOOST_CHECK_EQUAL(2u, wm.get_public_keys().size());

   wm.lock("test");
   BOOST_CHECK_THROW(wm.list_keys("test", pw), hive::plugins::clive::clive_locked_exception);
   BOOST_CHECK_THROW(wm.get_public_keys(), hive::plugins::clive::clive_locked_exception);
   wm.unlock("test", pw);
   BOOST_CHECK_EQUAL(2u, wm.get_public_keys().size());
   BOOST_CHECK_EQUAL(2u, wm.list_keys("test", pw).size());
   wm.lock_all();
   BOOST_CHECK_THROW(wm.get_public_keys(), hive::plugins::clive::clive_locked_exception);
   BOOST_CHECK(wm.list_wallets().at(0).find("*") == std::string::npos);

   auto pw2 = wm.create("test2");
   BOOST_CHECK_EQUAL(2u, wm.list_wallets().size());
   // wallet has no keys when it is created
   BOOST_CHECK_EQUAL(0u, wm.get_public_keys().size());
   wm.import_key("test2", key3);
   BOOST_CHECK_EQUAL(1u, wm.get_public_keys().size());
   BOOST_CHECK_THROW(wm.import_key("test2", key3), fc::exception);
   keys = wm.list_keys("test2", pw2);
   BOOST_CHECK( get_key( key1, keys ) == keys.end() );
   BOOST_CHECK( get_key( key2, keys ) == keys.end() );
   BOOST_CHECK( get_key( key3, keys ) != keys.end() );
   wm.unlock("test", pw);
   keys = wm.list_keys("test", pw);
   auto keys2 = wm.list_keys("test2", pw2);
   keys.insert(keys2.begin(), keys2.end());
   BOOST_CHECK( get_key( key1, keys ) != keys.end() );
   BOOST_CHECK( get_key( key2, keys ) != keys.end() );
   BOOST_CHECK( get_key( key3, keys ) != keys.end() );
   BOOST_CHECK_EQUAL(3u, keys.size());

   BOOST_CHECK_THROW(wm.list_keys("test2", "PWnogood"), hive::plugins::clive::clive_invalid_password_exception);

   // private_key_type pkey1 = *( private_key_type::generate_from_base58( key1 ) );
   //private_key_type pkey2 = *( private_key_type::generate_from_base58( key2 ) );
   // chain::signed_transaction trx;
   // auto chain_id = genesis_state().compute_chain_id();
   // flat_set<public_key_type> pubkeys;
   // pubkeys.emplace(pkey1.get_public_key());
   // pubkeys.emplace(pkey2.get_public_key());
   // trx = wm.sign_transaction(trx, pubkeys, chain_id );
   // flat_set<public_key_type> pks;
   // trx.get_signature_keys(chain_id, fc::time_point::maximum(), pks);
   // BOOST_CHECK_EQUAL(2u, pks.size());
   // BOOST_CHECK(find(pks.cbegin(), pks.cend(), pkey1.get_public_key()) != pks.cend());
   // BOOST_CHECK(find(pks.cbegin(), pks.cend(), pkey2.get_public_key()) != pks.cend());

   BOOST_CHECK_EQUAL(3u, wm.get_public_keys().size());
   wm.set_timeout(chrono::seconds(0));
   BOOST_CHECK_THROW(wm.get_public_keys(), hive::plugins::clive::clive_locked_exception);
   BOOST_CHECK_THROW(wm.list_keys("test", pw), hive::plugins::clive::clive_locked_exception);

   wm.set_timeout(chrono::seconds(15));

   wm.create("testgen");
   BOOST_CHECK_THROW(wm.create_key("testgen"), hive::plugins::clive::clive_exception);
   wm.lock("testgen");
   fc::remove("testgen.wallet");

   pw = wm.create("testgen");

   //check that the public key returned looks legit through a string conversion
   // (would throw otherwise)
   public_key_type create_key_pub( fc::ecc::public_key::from_base58_with_prefix( wm.create_key("testgen"), HIVE_ADDRESS_PREFIX ) );

   //now pluck out the private key from the wallet and see if the public key of said
   // private key matches what was returned earlier from the create_key() call
   auto create_key_priv = private_key_type::generate_from_base58( wm.list_keys("testgen", pw).cbegin()->second );
   BOOST_CHECK_EQUAL(create_key_pub.to_base58_with_prefix( HIVE_ADDRESS_PREFIX ), create_key_priv->get_public_key().to_base58_with_prefix( HIVE_ADDRESS_PREFIX ));

   wm.lock("testgen");
   BOOST_CHECK(fc::exists("testgen.wallet"));
   fc::remove("testgen.wallet");

   BOOST_CHECK(fc::exists("test.wallet"));
   BOOST_CHECK(fc::exists("test2.wallet"));
   fc::remove("test.wallet");
   fc::remove("test2.wallet");

} FC_LOG_AND_RETHROW() }

/// Test wallet manager
BOOST_AUTO_TEST_CASE(wallet_manager_create_test) {
   try {
      if (fc::exists("test.wallet")) fc::remove("test.wallet");

      wallet_manager wm;
      wm.create("test");
      constexpr auto key1 = "5JktVNHnRX48BUdtewU7N1CyL4Z886c42x7wYW7XhNWkDQRhdcS";
      wm.import_key("test", key1);
      BOOST_CHECK_THROW(wm.create("test"), hive::plugins::clive::clive_exist_exception);

      BOOST_CHECK_THROW(wm.create("./test"), hive::plugins::clive::clive_exception);
      BOOST_CHECK_THROW(wm.create("../../test"), hive::plugins::clive::clive_exception);
      BOOST_CHECK_THROW(wm.create("/tmp/test"), hive::plugins::clive::clive_exception);
      BOOST_CHECK_THROW(wm.create("/tmp/"), hive::plugins::clive::clive_exception);
      BOOST_CHECK_THROW(wm.create("/"), hive::plugins::clive::clive_exception);
      BOOST_CHECK_THROW(wm.create(",/"), hive::plugins::clive::clive_exception);
      BOOST_CHECK_THROW(wm.create(","), hive::plugins::clive::clive_exception);
      BOOST_CHECK_THROW(wm.create("<<"), hive::plugins::clive::clive_exception);
      BOOST_CHECK_THROW(wm.create("<"), hive::plugins::clive::clive_exception);
      BOOST_CHECK_THROW(wm.create(",<"), hive::plugins::clive::clive_exception);
      BOOST_CHECK_THROW(wm.create(",<<"), hive::plugins::clive::clive_exception);
      BOOST_CHECK_THROW(wm.create(""), hive::plugins::clive::clive_exception);

      fc::remove("test.wallet");

      wm.create(".test");
      BOOST_CHECK(fc::exists(".test.wallet"));
      fc::remove(".test.wallet");
      wm.create("..test");
      BOOST_CHECK(fc::exists("..test.wallet"));
      fc::remove("..test.wallet");
      wm.create("...test");
      BOOST_CHECK(fc::exists("...test.wallet"));
      fc::remove("...test.wallet");
      wm.create(".");
      BOOST_CHECK(fc::exists("..wallet"));
      fc::remove("..wallet");
      wm.create("__test_test");
      BOOST_CHECK(fc::exists("__test_test.wallet"));
      fc::remove("__test_test.wallet");
      wm.create("t-t");
      BOOST_CHECK(fc::exists("t-t.wallet"));
      fc::remove("t-t.wallet");

   } FC_LOG_AND_RETHROW()
}


BOOST_AUTO_TEST_SUITE_END()

#endif
