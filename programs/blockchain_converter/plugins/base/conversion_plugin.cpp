#include "conversion_plugin.hpp"

#include <fc/optional.hpp>
#include <fc/log/logger.hpp>

#include <hive/protocol/types.hpp>
#include <hive/protocol/authority.hpp>

#include <string>

namespace hive { namespace converter { namespace plugins {

  using hive::protocol::private_key_type;
  using hive::protocol::authority;

  using hive::utilities::wif_to_key;
  using hive::utilities::key_to_wif;

  void conversion_plugin_impl::print_pre_conversion_data( const hp::signed_block& block_to_log )const
  {
    uint32_t block_num = block_to_log.block_num();

    if ( ( log_per_block > 0 && block_num % log_per_block == 0 ) || log_specific == block_num )
      dlog("Processing block: ${block_num}. Data before conversion: ${block}", ("block_num", block_num)("block", block_to_log));
  }

  void conversion_plugin_impl::print_progress( uint32_t current_block, uint32_t stop_block )const
  {
    if( current_block % 1000 == 0 )
    { // Progress
      if( stop_block )
        ilog("[ ${progress}% ]: ${processed}/${stop_point} blocks rewritten",
          ("progress", int( float(current_block) / stop_block * 100 ))("processed", current_block)("stop_point", stop_block));
      else
        ilog("${block_num} blocks rewritten", ("block_num", current_block));
    }
  }

  void conversion_plugin_impl::print_post_conversion_data( const hp::signed_block& block_to_log )const
  {
    uint32_t block_num = block_to_log.block_num();

    if ( ( log_per_block > 0 && block_num % log_per_block == 0 ) || log_specific == block_num )
      dlog("Processing block: ${block_num}. Data before conversion: ${block}", ("block_num", block_num)("block", block_to_log));
  }

  void conversion_plugin_impl::set_wifs( bool use_private, const std::string& _owner, const std::string& _active, const std::string& _posting )
  {
    fc::optional< private_key_type > _owner_key = fc::ecc::private_key::wif_to_key( _owner );
    fc::optional< private_key_type > _active_key = fc::ecc::private_key::wif_to_key( _active );
    fc::optional< private_key_type > _posting_key = fc::ecc::private_key::wif_to_key( _posting );

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
    dlog( "Owner: ${key}", ("key", converter.get_second_authority_key( authority::owner ).key_to_wif() ) );
    dlog( "Active: ${key}", ("key", converter.get_second_authority_key( authority::active ).key_to_wif() ) );
    dlog( "Posting: ${key}", ("key", converter.get_second_authority_key( authority::posting ).key_to_wif() ) );
  }

  const blockchain_converter& conversion_plugin_impl::get_converter()const
  {
    return converter;
  }

} } } // hive::converter::plugins
