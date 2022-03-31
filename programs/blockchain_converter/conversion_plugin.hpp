#pragma once

#include <hive/protocol/config.hpp>
#include <hive/protocol/types.hpp>

#include <string>

#include "converter.hpp"

namespace hive { namespace converter { namespace plugins {

  namespace hp = hive::protocol;
  using hp::chain_id_type;
  using hp::private_key_type;

  class conversion_plugin_impl {
  protected:
    blockchain_converter converter;

  public:
    uint32_t log_per_block = 0, log_specific = 0;

    conversion_plugin_impl( const private_key_type& _private_key, const chain_id_type& chain_id, size_t signers_size, bool increase_block_size = true )
      : converter( _private_key, chain_id, signers_size, increase_block_size ) {}

    virtual void convert( uint32_t start_block_num, uint32_t stop_block_num ) = 0;

    /// If use_private is set to true then private keys of the second authority will be same as the given private key
    void set_wifs( bool use_private = false, const std::string& _owner = "", const std::string& _active = "", const std::string& _posting = "" );
    void print_wifs()const;

    const blockchain_converter& get_converter()const;
  };

} } } // hive::converter::plugins
