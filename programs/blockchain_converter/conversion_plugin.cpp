#include "conversion_plugin.hpp"

#include <fc/optional.hpp>
#include <fc/log/logger.hpp>

#include <hive/protocol/types.hpp>
#include <hive/protocol/authority.hpp>

#include <hive/utilities/key_conversion.hpp>

#include <string>

namespace hive { namespace converter { namespace plugins {

  using hive::protocol::private_key_type;
  using hive::protocol::authority;

  using hive::utilities::wif_to_key;

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

    converter.apply_second_authority_keys();
  }

  void conversion_plugin_impl::print_wifs()const
  {
    ilog( "Second authority wif private keys:" );
    dlog( "Owner: ${key}", ("key", converter.get_second_authority_key( authority::owner ).str() ) );
    dlog( "Active: ${key}", ("key", converter.get_second_authority_key( authority::active ).str() ) );
    dlog( "Posting: ${key}", ("key", converter.get_second_authority_key( authority::posting ).str() ) );
  }

  const blockchain_converter& conversion_plugin_impl::get_converter()const
  {
    return converter;
  }

} } } // hive::converter::plugins
