#pragma once

#include <hive/utilities/key_conversion.hpp>

#include <iostream>

#include "converter.hpp"

namespace hive {

using namespace utilities;

namespace converter { namespace plugins {

  class conversion_plugin_impl {
  public:
    uint32_t log_per_block = 0, log_specific = 0;
    blockchain_converter converter;

    conversion_plugin_impl( const private_key_type& _private_key, const chain_id_type& chain_id = HIVE_CHAIN_ID )
      : converter( _private_key, chain_id ) {}

    virtual void convert( uint32_t start_block_num, uint32_t stop_block_num ) = 0;

    /// If use_private is set to true then private keys of the second authority will be same as the given private key
    void set_wifs( bool use_private = false, const std::string& _owner = "", const std::string& _active = "", const std::string& _posting = "" );
    void set_logging( uint32_t log_per_block = 0, uint32_t log_specific = 0 );
    void print_wifs()const;
  };

} } } // hive::converter::plugins
