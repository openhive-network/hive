#include <boost/test/unit_test.hpp>
#include <fc/crypto/bip39.hpp>
#include <fc/crypto/bip85.hpp>

#include <fc/crypto/hex.hpp>
#include <fc/crypto/elliptic.hpp>

#include <fc/exception/exception.hpp>

#include <iostream>


BOOST_AUTO_TEST_SUITE(fc_crypto)
BOOST_AUTO_TEST_SUITE(bip85_tests)

namespace {
  std::vector<unsigned char> from_hex(const std::string& hex) {
    std::vector<unsigned char> result(hex.size() / 2);
    fc::from_hex(hex, (char*)result.data(), result.size());
    return result;
  }
}

// test cases from: https://github.com/bitcoin/bips/blob/master/bip-0085.mediawiki
BOOST_AUTO_TEST_CASE(test_case_1)
{
  fc::ecc::extended_private_key master = fc::ecc::extended_private_key::from_base58("xprv9s21ZrQH143K2LBWUUQRFXhucrQqBpKdRRxNVq2zBqsx8HVqFk2uYo8kmbaLLHRdqtQpUm98uKfu3vca1LqdGhUtyoFnCNkfmXRyPXLjbKb");
  fc::ecc::extended_private_key m_83696968_0_0 = master.derive_child("m/83696968'/0'/0'");

  // check that the derived key is correct
  BOOST_CHECK_EQUAL(m_83696968_0_0.get_secret().str(), "cca20ccb0e9a90feb0912870c3323b24874b0ca3d8018c4b96d0b97c0e82ded0");

  const fc::sha512 entropy = fc::compute_bip85_entropy_from_key(m_83696968_0_0);

  // check that the derived entropy is correct
  BOOST_CHECK_EQUAL(entropy.str(), "efecfbccffea313214232d29e71563d941229afb4338c21f9517c41aaa0d16f00b83d2a09ef747e7a64e8e2bd5a14869e693da66ce94ac2da570ab7ee48618f7");
}

BOOST_AUTO_TEST_CASE(test_case_2)
{
  fc::ecc::extended_private_key master = fc::ecc::extended_private_key::from_base58("xprv9s21ZrQH143K2LBWUUQRFXhucrQqBpKdRRxNVq2zBqsx8HVqFk2uYo8kmbaLLHRdqtQpUm98uKfu3vca1LqdGhUtyoFnCNkfmXRyPXLjbKb");
  fc::ecc::extended_private_key m_83696968_0_1 = master.derive_child("m/83696968'/0'/1'");

  // check that the derived key is correct
  BOOST_CHECK_EQUAL(m_83696968_0_1.get_secret().str(), "503776919131758bb7de7beb6c0ae24894f4ec042c26032890c29359216e21ba");

  const fc::sha512 entropy = fc::compute_bip85_entropy_from_key(m_83696968_0_1);

  // check that the derived entropy is correct
  BOOST_CHECK_EQUAL(entropy.str(), "70c6e3e8ebee8dc4c0dbba66076819bb8c09672527c4277ca8729532ad711872218f826919f6b67218adde99018a6df9095ab2b58d803b5b93ec9802085a690e");
}

BOOST_AUTO_TEST_SUITE(bip39_tests)

BOOST_AUTO_TEST_CASE(twelve_words)
{
  fc::ecc::extended_private_key master = fc::ecc::extended_private_key::from_base58("xprv9s21ZrQH143K2LBWUUQRFXhucrQqBpKdRRxNVq2zBqsx8HVqFk2uYo8kmbaLLHRdqtQpUm98uKfu3vca1LqdGhUtyoFnCNkfmXRyPXLjbKb");
  fc::ecc::extended_private_key m_83696968_39_0_12_0 = master.derive_child("m/83696968'/39'/0'/12'/0'");

  const fc::sha512 full_entropy = fc::compute_bip85_entropy_from_key(m_83696968_39_0_12_0);

  // we're only using 128 bits of entropy here
  const std::vector<unsigned char> entropy = fc::compute_bip85_entropy_from_key(m_83696968_39_0_12_0, 16);

  // check that the derived entropy is correct
  BOOST_CHECK_EQUAL(fc::to_hex((const char*)entropy.data(), entropy.size()), "6250b68daf746d12a24d58b4787a714b");
  const std::string mnemonic_string = fc::generate_bip39_mnemonic(entropy);
  BOOST_CHECK_EQUAL(mnemonic_string, "girl mad pet galaxy egg matter matrix prison refuse sense ordinary nose");
}

BOOST_AUTO_TEST_CASE(eighteen_words)
{
  fc::ecc::extended_private_key master = fc::ecc::extended_private_key::from_base58("xprv9s21ZrQH143K2LBWUUQRFXhucrQqBpKdRRxNVq2zBqsx8HVqFk2uYo8kmbaLLHRdqtQpUm98uKfu3vca1LqdGhUtyoFnCNkfmXRyPXLjbKb");
  fc::ecc::extended_private_key m_83696968_39_0_18_0 = master.derive_child("m/83696968'/39'/0'/18'/0'");

  // we're only using 192 bits of entropy here
  const std::vector<unsigned char> entropy = fc::compute_bip85_entropy_from_key(m_83696968_39_0_18_0, 24);

  // check that the derived entropy is correct
  BOOST_CHECK_EQUAL(fc::to_hex((const char*)entropy.data(), entropy.size()), "938033ed8b12698449d4bbca3c853c66b293ea1b1ce9d9dc");
  const std::string mnemonic_string = fc::generate_bip39_mnemonic(entropy);
  BOOST_CHECK_EQUAL(mnemonic_string, "near account window bike charge season chef number sketch tomorrow excuse sniff circle vital hockey outdoor supply token");
}

BOOST_AUTO_TEST_CASE(twentyfour_words)
{
  fc::ecc::extended_private_key master = fc::ecc::extended_private_key::from_base58("xprv9s21ZrQH143K2LBWUUQRFXhucrQqBpKdRRxNVq2zBqsx8HVqFk2uYo8kmbaLLHRdqtQpUm98uKfu3vca1LqdGhUtyoFnCNkfmXRyPXLjbKb");
  fc::ecc::extended_private_key m_83696968_39_0_24_0 = master.derive_child("m/83696968'/39'/0'/24'/0'");

  // we're only using 256 bits of entropy here
  const std::vector<unsigned char> entropy = fc::compute_bip85_entropy_from_key(m_83696968_39_0_24_0, 32);

  // check that the derived entropy is correct
  BOOST_CHECK_EQUAL(fc::to_hex((const char*)entropy.data(), entropy.size()), "ae131e2312cdc61331542efe0d1077bac5ea803adf24b313a4f0e48e9c51f37f");
  const std::string mnemonic_string = fc::generate_bip39_mnemonic(entropy);
  BOOST_CHECK_EQUAL(mnemonic_string, "puppy ocean match cereal symbol another shed magic wrap hammer bulb intact gadget divorce twin tonight reason outdoor destroy simple truth cigar social volcano");
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()
