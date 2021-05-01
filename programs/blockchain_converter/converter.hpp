#pragma once

#include <memory>
#include <map>
#include <string>

#include <fc/crypto/elliptic.hpp>

#include <hive/protocol/operations.hpp>
#include <hive/protocol/block.hpp>

namespace hive {

  using namespace protocol; /// XXX: Using using ... instead of using namespace is propably a better idea

  namespace converter {

    class derived_keys_map
    {
    private:
      // Key is the public key from the original block log and T is private key derived from initminer's private key
      std::map< public_key_type, fc::ecc::private_key > keys;

      std::string private_key_wif;

    public:

      derived_keys_map( const std::string& private_key_wif );

      derived_keys_map( const fc::ecc::private_key& _private_key );

      /// Generates public key from the private key mapped to the public key from the original block_log
      public_key_type get_public( const public_key_type& original );

      /// Inserts key to the container if public key was not found in the unordered_map.
      /// Returns const reference to the private key mapped to the public key from the original block_log.
      const fc::ecc::private_key& get_private( const public_key_type& original );

      // TODO: Save to file or convert into wallet.json
    };

    class blockchain_converter
    {
    private:
      fc::ecc::private_key                _private_key;
      chain_id_type                       chain_id;
      std::shared_ptr< derived_keys_map > keys;

    public:
      blockchain_converter( const fc::ecc::private_key& _private_key, const chain_id_type& chain_id = HIVE_CHAIN_ID );

      void convert_signed_block( signed_block& _signed_block );

      void convert_signed_header( signed_block_header& _signed_header );

      const fc::ecc::private_key& convert_signature_from_header( const signature_type& _signature, const signed_block_header& _signed_header );

      fc::ecc::canonical_signature_type get_canon_type( const signature_type& _signature )const;

      derived_keys_map& get_keys();
    };

    class convert_operations_visitor
    {
    private:
      std::shared_ptr< blockchain_converter > converter;

    public:
      typedef operation result_type;

      convert_operations_visitor( const std::shared_ptr< blockchain_converter >& converter );

      const account_create_operation& operator()( account_create_operation& op );

      const account_create_with_delegation_operation& operator()( account_create_with_delegation_operation& op );

      const account_update_operation& operator()( account_update_operation& op );

      const account_update2_operation& operator()( account_update2_operation& op );

      const create_claimed_account_operation& operator()( create_claimed_account_operation& op );

      const witness_update_operation& operator()( witness_update_operation& op );

      const witness_set_properties_operation& operator()( witness_set_properties_operation& op );

      const custom_binary_operation& operator()( custom_binary_operation& op );

      const pow2_operation& operator()( pow2_operation& op );

      const report_over_production_operation& operator()( report_over_production_operation& op );

      const request_account_recovery_operation& operator()( request_account_recovery_operation& op );

      const recover_account_operation& operator()( recover_account_operation& op );

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