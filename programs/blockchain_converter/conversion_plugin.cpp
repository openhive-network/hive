#include "conversion_plugin.hpp"

#include <fc/optional.hpp>

#include <hive/protocol/types.hpp>
#include <hive/protocol/authority.hpp>

#include <hive/utilities/key_conversion.hpp>

#include <string>
#include <iostream>

namespace hive { namespace converter { namespace plugins {

  using hive::protocol::private_key_type;
  using hive::protocol::authority;

  using hive::utilities::wif_to_key;
  using hive::utilities::key_to_wif;

  void conversion_plugin_impl::set_wifs( bool use_private, const std::string& _owner, const std::string& _active, const std::string& _posting )
  {
    fc::optional< private_key_type > _owner_key = wif_to_key( _owner );
    fc::optional< private_key_type > _active_key = wif_to_key( _active );
    fc::optional< private_key_type > _posting_key = wif_to_key( _posting );

    if( use_private )
      _owner_key = converter.get_witness_key();
    else if( !_owner_key.valid() )
      _owner_key = private_key_type::generate();

    if( use_private )
      _active_key = converter.get_witness_key();
    else if( !_active_key.valid() )
      _active_key = private_key_type::generate();

    if( use_private )
      _posting_key = converter.get_witness_key();
    else if( !_posting_key.valid() )
      _posting_key = private_key_type::generate();

    converter.set_second_authority_key( *_owner_key, authority::owner );
    converter.set_second_authority_key( *_active_key, authority::active );
    converter.set_second_authority_key( *_posting_key, authority::posting );
  }

  void conversion_plugin_impl::set_logging( uint32_t log_per_block, uint32_t log_specific )
  {
    this->log_per_block = log_per_block;
    this->log_specific = log_specific;
  }

  void conversion_plugin_impl::print_wifs()const
  {
    std::cout << "Second authority wif private keys:\n"
      << "Owner:   " << key_to_wif( converter.get_second_authority_key( authority::owner ) ) << '\n'
      << "Active:  " << key_to_wif( converter.get_second_authority_key( authority::active ) ) << '\n'
      << "Posting: " << key_to_wif( converter.get_second_authority_key( authority::posting ) ) << '\n';
  }

} } } // hive::converter::plugins
