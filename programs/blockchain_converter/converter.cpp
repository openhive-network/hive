#include "converter.hpp"

#include <stdexcept>
#include <map>
#include <string>

#include <hive/protocol/operations.hpp>

#include <hive/utilities/key_conversion.hpp>

namespace hive {

  using namespace protocol;
  using namespace utilities;

  namespace converter {

    derived_keys_map::derived_keys_map( const std::string& private_key_wif )
      : private_key_wif( private_key_wif ) {}

    derived_keys_map::derived_keys_map( const fc::ecc::private_key& private_key )
      : private_key_wif( key_to_wif(private_key) ) {}

    public_key_type derived_keys_map::get_public( const public_key_type& original )
    {
      return at( original ).get_public_key();
    }

    const fc::ecc::private_key& derived_keys_map::operator[]( const public_key_type& original )
    {
      if( keys.find( original ) != keys.end() )
        return keys.at( original );
      return (*keys.emplace( original,
          fc::ecc::private_key::regenerate(fc::sha256::hash(fc::sha512::hash( private_key_wif + ' ' + std::to_string( keys.size() ) )))
        ).first).second;
    }

    const fc::ecc::private_key& derived_keys_map::at( const public_key_type& original )
    {
      if( keys.find( original ) != keys.end() )
        return keys.at( original );
      return (*keys.emplace( original,
          fc::ecc::private_key::regenerate(fc::sha256::hash(fc::sha512::hash( private_key_wif + ' ' + std::to_string( keys.size() ) )))
        ).first).second;
    }


    convert_operations_visitor::convert_operations_visitor( const std::shared_ptr< derived_keys_map >& derived_keys )
      : derived_keys( derived_keys ) {}

    const account_create_operation& convert_operations_visitor::operator()( account_create_operation& op )const
    {
      typename authority::key_authority_map keys;

      for( auto& key : op.owner.key_auths )
        keys[ derived_keys->get_public(key.first) ] = key.second;
      op.owner.key_auths = keys;
      keys.clear();

      for( auto& key : op.active.key_auths )
        keys[ derived_keys->get_public(key.first) ] = key.second;
      op.active.key_auths = keys;
      keys.clear();

      for( auto& key : op.posting.key_auths )
        keys[ derived_keys->get_public(key.first) ] = key.second;
      op.posting.key_auths = keys;

      return op;
    }

    const account_create_with_delegation_operation& convert_operations_visitor::operator()( const account_create_with_delegation_operation& op )const
    { return op; }

    const account_update_operation& convert_operations_visitor::operator()( const account_update_operation& op )const
    { return op; }

    const account_update2_operation& convert_operations_visitor::operator()( const account_update2_operation& op )const
    { return op; }

    const create_claimed_account_operation& convert_operations_visitor::operator()( const create_claimed_account_operation& op )const
    { return op; }

    const witness_update_operation& convert_operations_visitor::operator()( const witness_update_operation& op )const
    { return op; }

    const witness_set_properties_operation& convert_operations_visitor::operator()( const witness_set_properties_operation& op )const
    { return op; }

    const custom_binary_operation& convert_operations_visitor::operator()( const custom_binary_operation& op )const
    { return op; }

    const pow2_operation& convert_operations_visitor::operator()( const pow2_operation& op )const
    { return op; }

    const report_over_production_operation& convert_operations_visitor::operator()( const report_over_production_operation& op )const
    { return op; }

    const request_account_recovery_operation& convert_operations_visitor::operator()( const request_account_recovery_operation& op )const
    { return op; }

    const recover_account_operation& convert_operations_visitor::operator()( const recover_account_operation& op )const
    { return op; }

  }
}