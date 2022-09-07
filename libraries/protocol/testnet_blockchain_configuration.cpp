#include <hive/protocol/testnet_blockchain_configuration.hpp>

#include <hive/protocol/config.hpp>

namespace hive { namespace protocol { namespace testnet_blockchain_configuration {

  configuration configuration_data;

  configuration::configuration()
  {
    hive_initminer_key = get_default_initminer_private_key();
    hive_hf9_compromised_key = HIVE_DEFAULT_HF_9_COMPROMISED_ACCOUNTS_PUBLIC_KEY_STR;
  }

  hive::protocol::private_key_type configuration::get_default_initminer_private_key() const
  {
    return fc::ecc::private_key::regenerate(fc::sha256::hash(
      std::string("init_key")));
  }

  hive::protocol::public_key_type configuration::get_initminer_public_key() const
  {
    return hive::protocol::public_key_type(hive_initminer_key.get_public_key());
  }

  void configuration::set_skeleton_key(const hive::protocol::private_key_type& private_key)
  {
    hive_initminer_key = private_key;
    hive_hf9_compromised_key = std::string(get_initminer_public_key());
  }

} } }// hive::protocol::testnet_blockchain_configuration
