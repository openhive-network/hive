#pragma once

#include <memory>
#include <map>
#include <string>

#include <fc/crypto/elliptic.hpp>

#include <hive/protocol/operations.hpp>
#include <hive/protocol/block.hpp>

namespace hive {

  using namespace protocol;

  namespace converter {

    class blockchain_converter
    {
    private:
      private_key_type _private_key;
      chain_id_type    chain_id;
      signed_block*    current_signed_block;

      std::map< account_name_type, public_key_type >          pow_auths;
      std::map< authority::classification, private_key_type > second_authority;

      void post_convert_transaction( signed_transaction& _transaction );

    public:
      /// All converted blocks will be signed using keys derived from the given private key
      blockchain_converter( const private_key_type& _private_key, const chain_id_type& chain_id = HIVE_CHAIN_ID );

      /// Sets signed_block previous member to the given value and re-signs content of the block using derived keys. Returns current block id.
      block_id_type convert_signed_block( signed_block& _signed_block, const block_id_type& previous_block_id );

      void convert_signed_header( signed_block_header& _signed_header );

      void convert_authority( authority& _auth, authority::classification type );

      const private_key_type& get_second_authority_key( authority::classification type )const;
      void set_second_authority_key( const private_key_type& key, authority::classification type );

      const priate_key_type& get_witness_key()const;

      void add_pow_authority( const account_name_type& name, const public_key_type& key );

      const signed_block& get_current_signed_block()const;
    };

    class convert_operations_visitor
    {
    private:
      blockchain_converter& converter;

    public:
      typedef operation result_type;

      convert_operations_visitor( blockchain_converter& converter );

      const account_create_operation& operator()( account_create_operation& op )const;

      const account_create_with_delegation_operation& operator()( account_create_with_delegation_operation& op )const;

      const account_update_operation& operator()( account_update_operation& op )const;

      const account_update2_operation& operator()( account_update2_operation& op )const;

      const create_claimed_account_operation& operator()( create_claimed_account_operation& op )const;

      const witness_update_operation& operator()( witness_update_operation& op )const;

      const witness_set_properties_operation& operator()( witness_set_properties_operation& op )const;

      const custom_binary_operation& operator()( custom_binary_operation& op )const;

      const pow_operation& operator()( pow_operation& op )const;

      const pow2_operation& operator()( pow2_operation& op )const;

      const report_over_production_operation& operator()( report_over_production_operation& op )const;

      const request_account_recovery_operation& operator()( request_account_recovery_operation& op )const;

      const recover_account_operation& operator()( recover_account_operation& op )const;

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
