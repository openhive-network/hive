#pragma once

#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <string>

#include <hive/protocol/operations.hpp>

#include <hive/wallet/wallet.hpp>

#include <hive/utilities/key_conversion.hpp>

namespace hive {

  using namespace protocol;
  using namespace utilities;
  using namespace wallet::detail;

  namespace converter {

    class derived_keys_map
    {
    private:
      // Key is the public key from original the original block log and T is private key derived from initminer's private key
      std::unordered_map< public_key_type, fc::ecc::private_key > keys;

      int sequence_number = 0;
      std::string private_key_wif;

    public:

      derived_keys_map( const std::string& private_key_wif )
        : private_key_wif( private_key_wif ) {}

      derived_keys_map( const fc::ecc::private_key& private_key )
        : private_key_wif( key_to_wif(private_key) ) {}

      /// Generates public key from the private key mapped to the public key from the original block_log
      public_key_type get_public( const public_key_type& original )
      {
        return at( original ).get_public_key();
      }

      /// Inserts key to the container if public key was not found in the unordered_map.
      /// Returns const reference to the private key mapped to the public key from the original block_log.
      const fc::ecc::private_key& operator[]( const public_key_type& original )
      {
        if( keys.find( original ) != keys.end() )
          return (*keys.emplace( original, derive_private_key( private_key_wif, sequence_number++ ) ).first).second;
        return keys.at( original );
      }

      const fc::ecc::private_key& at( const public_key_type& original )
      {
        if( keys.find( original ) != keys.end() )
          return (*keys.emplace( original, derive_private_key( private_key_wif, sequence_number++ ) ).first).second;
        return keys.at( original );
      }
    };

    class convert_operations_visitor
    {
    private:
      std::shared_ptr< derived_keys_map > derived_keys;

    public:
      typedef operation result_type;

      convert_operations_visitor( const std::shared_ptr< derived_keys_map >& derived_keys )
        : derived_keys( derived_keys ) {}

      const account_create_operation& operator()( const account_create_operation& op )const
      {
        for( auto& key : op.owner.key_auths )
        {
          key.first = derived_keys->get_public(key.first);
        }

        return op;
      }

      const account_create_with_delegation_operation& operator()( const account_create_with_delegation_operation& op )const
      { return op; }

      const account_update_operation& operator()( const account_update_operation& op )const
      { return op; }

      const account_update2_operation& operator()( const account_update2_operation& op )const
      { return op; }

      const create_claimed_account_operation& operator()( const create_claimed_account_operation& op )const
      { return op; }

      const witness_update_operation& operator()( const witness_update_operation& op )const
      { return op; }

      const witness_set_properties_operation& operator()( const witness_set_properties_operation& op )const
      { return op; }

      const custom_binary_operation& operator()( const custom_binary_operation& op )const
      { return op; }

      const pow2_operation& operator()( const pow2_operation& op )const
      { return op; }

      const report_over_production_operation& operator()( const report_over_production_operation& op )const
      { return op; }

      const request_account_recovery_operation& operator()( const request_account_recovery_operation& op )const
      { return op; }

      const recover_account_operation& operator()( const recover_account_operation& op )const
      { return op; }

      // No signatures modification ops
      template< typename T >
      const T& operator()( const T& op )const
      {
        FC_ASSERT( !op.is_virtual(), "block log should not contain virtual operations" );
        return op;
      }
    };

  }
}