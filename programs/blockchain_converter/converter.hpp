#pragma once

#include <memory>
#include <map>
#include <array>
#include <string>

#include <fc/crypto/elliptic.hpp>

#include <hive/protocol/operations.hpp>
#include <hive/protocol/block.hpp>


#define HIVE_HARDFORK_0_17_BLOCK_NUM 10629455 // for pow operations

namespace hive { namespace converter {

  namespace hp = hive::protocol;

  using hp::authority;

  class blockchain_converter
  {
  private:
    hp::private_key_type _private_key;
    hp::chain_id_type    chain_id;
    hp::block_id_type    previous_block_id;

    std::map< hp::account_name_type, std::array< fc::optional< authority >, 3 > >  pow_auths; // the array contains the following keys: owner, active and posting
    std::map< authority::classification, hp::private_key_type >                    second_authority;

    void post_convert_transaction( hp::signed_transaction& _transaction );

  public:
    /// All converted blocks will be signed using given private key
    blockchain_converter( const hp::private_key_type& _private_key, const hp::chain_id_type& chain_id = HIVE_CHAIN_ID );

    /// Sets previous id of the block to the given value and re-signs content of the block. Converts transactions. Returns current block id
    hp::block_id_type convert_signed_block( hp::signed_block& _signed_block, const hp::block_id_type& previous_block_id );

    void convert_signed_header( hp::signed_block_header& _signed_header );

    void convert_authority( const hp::account_name_type& name, authority& _auth, authority::classification type );

    const hp::private_key_type& get_second_authority_key( authority::classification type )const;
    void set_second_authority_key( const hp::private_key_type& key, authority::classification type );

    const hp::private_key_type& get_witness_key()const;

    void add_pow_authority( const hp::account_name_type& name, authority auth, authority::classification type );
    bool has_pow_authority( const hp::account_name_type& name )const;

    const hp::block_id_type& get_previous_block_id()const;
  };

  class convert_operations_visitor
  {
  private:
    blockchain_converter& converter;

  public:
    typedef hp::operation result_type;

    convert_operations_visitor( blockchain_converter& converter );

    const hp::account_create_operation& operator()( hp::account_create_operation& op )const;

    const hp::account_create_with_delegation_operation& operator()( hp::account_create_with_delegation_operation& op )const;

    const hp::account_update_operation& operator()( hp::account_update_operation& op )const;

    const hp::account_update2_operation& operator()( hp::account_update2_operation& op )const;

    const hp::create_claimed_account_operation& operator()( hp::create_claimed_account_operation& op )const;

    const hp::custom_binary_operation& operator()( hp::custom_binary_operation& op )const;

    const hp::pow_operation& operator()( hp::pow_operation& op )const;

    const hp::pow2_operation& operator()( hp::pow2_operation& op )const;

    const hp::report_over_production_operation& operator()( hp::report_over_production_operation& op )const;

    const hp::request_account_recovery_operation& operator()( hp::request_account_recovery_operation& op )const;

    const hp::recover_account_operation& operator()( hp::recover_account_operation& op )const;

    // No signatures modification ops
    template< typename T >
    const T& operator()( const T& op )const
    {
      FC_ASSERT( !op.is_virtual(), "block log should not contain virtual operations" );
      return op;
    }
  };

} } // hive::converter
