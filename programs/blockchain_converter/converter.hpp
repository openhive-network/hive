#pragma once

#include <memory>
#include <map>
#include <string>

#include <fc/crypto/elliptic.hpp>

#include <hive/protocol/operations.hpp>

namespace hive {

  using namespace protocol;

  namespace converter {

    class derived_keys_map
    {
    private:
      // Key is the public key from original the original block log and T is private key derived from initminer's private key
      std::map< public_key_type, fc::ecc::private_key > keys;

      std::string private_key_wif;

    public:

      derived_keys_map( const std::string& private_key_wif );

      derived_keys_map( const fc::ecc::private_key& private_key );

      /// Generates public key from the private key mapped to the public key from the original block_log
      public_key_type get_public( const public_key_type& original );

      /// Inserts key to the container if public key was not found in the unordered_map.
      /// Returns const reference to the private key mapped to the public key from the original block_log.
      const fc::ecc::private_key& operator[]( const public_key_type& original );

      const fc::ecc::private_key& at( const public_key_type& original );

      // TODO: Save to file or convert into wallet.json
    };

    class convert_operations_visitor
    {
    private:
      std::shared_ptr< derived_keys_map > derived_keys;

    public:
      typedef operation result_type;

      convert_operations_visitor( const std::shared_ptr< derived_keys_map >& derived_keys );

      const account_create_operation& operator()( account_create_operation& op )const;

      const account_create_with_delegation_operation& operator()( account_create_with_delegation_operation& op )const;

      const account_update_operation& operator()( account_update_operation& op )const;

      const account_update2_operation& operator()( account_update2_operation& op )const;

      const create_claimed_account_operation& operator()( create_claimed_account_operation& op )const;

      const witness_update_operation& operator()( witness_update_operation& op )const;

      const witness_set_properties_operation& operator()( witness_set_properties_operation& op )const;

      const custom_binary_operation& operator()( custom_binary_operation& op )const;

      const pow2_operation& operator()( pow2_operation& op )const;

      const report_over_production_operation& operator()( report_over_production_operation& op )const;

      const request_account_recovery_operation& operator()( request_account_recovery_operation& op )const;

      const recover_account_operation& operator()( recover_account_operation& op )const;

      // No signatures modification ops
      template< typename T >
      const T& operator()( T& op )const
      {
        FC_ASSERT( !op.is_virtual(), "block log should not contain virtual operations" );
        return op;
      }
    };

  }
}