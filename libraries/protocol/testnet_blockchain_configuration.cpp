#include <hive/protocol/testnet_blockchain_configuration.hpp>

#include <hive/protocol/config.hpp>

namespace hive { namespace protocol { namespace testnet_blockchain_configuration {

  configuration configuration_data;

  void configuration::set_init_supply( uint64_t init_supply )
  {
    this->init_supply = init_supply;
  }

  void configuration::set_hbd_init_supply( uint64_t hbd_init_supply )
  {
    this->hbd_init_supply = hbd_init_supply;
  }

  uint64_t configuration::get_init_supply( uint64_t default_value )const
  {
    if( init_supply.valid() )
      return *init_supply;

    return default_value;
  }

  uint64_t configuration::get_hbd_init_supply( uint64_t default_value )const
  {
    if( hbd_init_supply.valid() )
      return *hbd_init_supply;

    return default_value;
  }

  void configuration::set_init_witnesses( const std::vector<std::string>& init_witnesses )
  {
    this->init_witnesses = init_witnesses;
  }

  const std::vector<std::string>& configuration::get_init_witnesses()const
  {
    return init_witnesses;
  }

  void configuration::set_genesis_time( const fc::time_point_sec& genesis_time )
  {
    this->genesis_time = genesis_time;
  }

  void configuration::set_hardfork_schedule( const std::vector<hardfork_schedule_item_t>& hardfork_schedule )
  {
    ilog("Hardfork schedule applied: ${hf_schedule}", ("hf_schedule", hardfork_schedule));

    // Init genesis
    blocks[0] = 0;

    for( size_t i = 0; i < hardfork_schedule.size(); ++i )
      blocks.at( hardfork_schedule[ i ].hardfork ) = hardfork_schedule[ i ].block_num;
  }

  uint32_t configuration::get_hf_time(uint32_t hf_num, uint32_t default_time_sec)const
  {
    FC_ASSERT( hf_num < blocks.size(), "Trying to retrieve hardfork of a non-existing hardfork ${hf}", ("hf", hf_num) );

    return blocks[hf_num].valid() ?
        // Deduce (calculate) the time based on the genesis time when hardfork schedule is specified:
        genesis_time.sec_since_epoch() + (blocks[hf_num].value() * HIVE_BLOCK_INTERVAL)
      : default_time_sec // No hardfork schedule specified, use default time sec
    ;
  }

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
