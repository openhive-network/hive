#ifdef USE_ALTERNATE_CHAIN_ID

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

  void configuration::set_hardfork_schedule( const fc::time_point_sec& genesis_time,
    const std::vector<hardfork_schedule_item_t>& hardfork_schedule )
  {
    ilog("Hardfork schedule applied: ${hardfork_schedule} with genesis time ${genesis_time}", (hardfork_schedule)(genesis_time));
    
    this->genesis_time = genesis_time;

    // Clear container contents.
    hf_times.fill( 0 );
    // Set genesis time
    hf_times[ 0 ] = genesis_time.sec_since_epoch();

    size_t last_hf_index = 0;
    for( size_t i = 0; i < hardfork_schedule.size(); ++i )
    {
      size_t current_hf_index = hardfork_schedule[ i ].hardfork;
      uint32_t current_block_num = hardfork_schedule[ i ].block_num;

      FC_ASSERT( current_hf_index > 0, "You cannot specify the hardfork 0 block. Use 'genesis-time' option instead" );
      FC_ASSERT( current_hf_index <= HIVE_NUM_HARDFORKS, "You are not allowed to specify future hardfork times" );
      FC_ASSERT( current_hf_index > last_hf_index, "The hardfork schedule items must be in ascending order, no repetitions" );

      // Set provided hardfork time filling the gaps if needed.
      while( last_hf_index < current_hf_index )
      {
        ++last_hf_index;
        hf_times[ last_hf_index ] = genesis_time.sec_since_epoch() + (current_block_num * HIVE_BLOCK_INTERVAL);
      }
    }

    // Note that remainding hardforks are left initialized to zero, which is handled in get_hf_time
  }

  void configuration::reset_hardfork_schedule()
  {
    genesis_time = fc::time_point_sec();
    // Clear container contents.
    hf_times.fill( 0 );
    // Set genesis time
    hf_times[ 0 ] = genesis_time.sec_since_epoch();
  }

  uint32_t configuration::get_hf_time(uint32_t hf_num, uint32_t default_time_sec)const
  {
    FC_ASSERT( hf_num < hf_times.size(), "Trying to retrieve hardfork of a non-existing hardfork ${hf}", ("hf", hf_num) );

    return hf_times[hf_num] != 0 ? hf_times[hf_num] : default_time_sec; // No hardfork schedule specified, use default time sec
  }

  void configuration::set_min_root_comment_interval( const fc::microseconds& time )
  {
    min_root_comment_interval = time;
  }

  const fc::microseconds& configuration::get_min_root_comment_interval( const fc::microseconds& default_value )const
  {
    if( min_root_comment_interval.valid() )
      return *min_root_comment_interval;

    return default_value;
  }

  void configuration::set_min_reply_interval( const fc::microseconds& time )
  {
    min_reply_interval = time;
  }


  const fc::microseconds& configuration::get_min_reply_interval( const fc::microseconds& default_value )const
  {
    if( min_reply_interval.valid() )
      return *min_reply_interval;

    return default_value;
  }


  void configuration::set_min_comment_edit_interval( const fc::microseconds& time )
  {
    min_comment_edit_interval = time;
  }


  const fc::microseconds& configuration::get_min_comment_edit_interval( const fc::microseconds& default_value )const
  {
    if( min_comment_edit_interval.valid() )
      return *min_comment_edit_interval;

    return default_value;
  }

  void configuration::set_witness_custom_op_block_limit( uint64_t value )
  {
    witness_custom_op_block_limit = value;
  }

  uint64_t configuration::get_witness_custom_op_block_limit( uint64_t default_value )const
  {
    if( witness_custom_op_block_limit.valid() )
      return *witness_custom_op_block_limit;

    return default_value;
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

#endif // USE_ALTERNATE_CHAIN_ID