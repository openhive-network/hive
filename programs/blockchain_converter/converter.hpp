#pragma once

#include <map>
#include <mutex>
#include <string>
#include <queue>
#include <set>
#include <thread>
#include <vector>
#include <utility>

#include <fc/exception/exception.hpp>
#include <fc/optional.hpp>

#include <hive/protocol/config.hpp>
#include <hive/protocol/types.hpp>
#include <hive/protocol/authority.hpp>
#include <hive/protocol/operations.hpp>
#include <hive/protocol/block.hpp>

#include <boost/lockfree/stack.hpp>

namespace hive { namespace converter {

  namespace hp = hive::protocol;

  using hp::authority;

  class blockchain_converter
  {
  private:
    hp::private_key_type _private_key;
    hp::chain_id_type    chain_id;
    hp::signed_block*    current_block_ptr = nullptr;

    std::map< authority::classification, hp::private_key_type > second_authority;

    std::queue< authority > pow_keys;
    std::set< hp::account_name_type > accounts;

    void post_convert_transaction( hp::signed_transaction& trx );

    std::vector< std::thread > signers; // Transactions signers (defualt number is 1)

    typedef std::pair< size_t, const hp::signed_transaction* > sig_stack_in_type;
    typedef std::pair< size_t, hp::signature_type >            sig_stack_out_type;

    boost::lockfree::stack< sig_stack_in_type >  shared_signatures_stack_in;  // pair< trx index in block, signed transaction ptr to convert >
    boost::lockfree::stack< sig_stack_out_type > shared_signatures_stack_out; // pair< trx index in block, converted signature >

    std::atomic_bool     signers_exit;
    std::atomic_uint32_t current_hardfork;

    mutable std::mutex second_auth_mutex;

    void check_for_hardfork( const hp::signed_block& _signed_block );

  public:
    /// Used in convert_signed_block to specify that expiration time of the transaction should not be altered (automatically deduct expiration time of the transaction using timestamp of the signed block)
    static const fc::time_point_sec auto_trx_time;

    /// All converted blocks will be signed using given private key
    blockchain_converter( const hp::private_key_type& _private_key, const hp::chain_id_type& chain_id = HIVE_CHAIN_ID, size_t signers_size = 1 );
    ~blockchain_converter();

    /// Sets previous id of the block to the given value and re-signs content of the block. Converts transactions. Returns current block id
    hp::block_id_type convert_signed_block( hp::signed_block& _signed_block, const hp::block_id_type& previous_block_id, const fc::time_point_sec& trx_now_time = auto_trx_time );

    void sign_header( hp::signed_block_header& _signed_header );

    void add_second_authority( authority& _auth, authority::classification type );

    void sign_transaction( hp::signed_transaction& trx )const;

    const hp::private_key_type& get_second_authority_key( authority::classification type )const;
    void set_second_authority_key( const hp::private_key_type& key, authority::classification type );

    void add_pow_key( const hp::account_name_type& acc, const hp::public_key_type& key );
    void add_account( const hp::account_name_type& acc );
    bool has_account( const hp::account_name_type& acc )const;

    const hp::private_key_type& get_witness_key()const;
    const hp::chain_id_type& get_chain_id()const;

    uint32_t get_current_hardfork()const;
    bool has_hardfork( uint32_t hf )const;

    const hp::signed_block& get_current_block()const;
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

    const hp::witness_update_operation& operator()( hp::witness_update_operation& op )const;

    const hp::witness_set_properties_operation& operator()( hp::witness_set_properties_operation& op )const;

    // No signatures modification ops
    template< typename T >
    const T& operator()( const T& op )const
    {
      FC_ASSERT( !op.is_virtual(), "block log should not contain virtual operations" );
      return op;
    }
  };

} } // hive::converter
