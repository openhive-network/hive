#pragma once

#include <map>
#include <mutex>
#include <string>
#include <queue>
#include <set>
#include <thread>
#include <vector>
#include <utility>
#include <atomic>
#include <array>

#include <fc/exception/exception.hpp>
#include <fc/optional.hpp>
#include <fc/time.hpp>

#include <hive/chain/full_block.hpp>
#include <hive/chain/full_transaction.hpp>

#include <hive/protocol/config.hpp>
#include <hive/protocol/types.hpp>
#include <hive/protocol/authority.hpp>
#include <hive/protocol/operations.hpp>
#include <hive/protocol/block.hpp>

#include <boost/lockfree/stack.hpp>

#define HIVE_BC_TIME_BUFFER 10

namespace hive { namespace converter {

  namespace hp = hive::protocol;
  namespace hc = hive::chain;

  class blockchain_converter
  {
  private:
    hp::private_key_type _private_key;
    hp::chain_id_type    chain_id;
    hp::signed_block*    current_block_ptr = nullptr;

    std::map< hp::authority::classification, hp::private_key_type > second_authority;

    std::queue< hp::authority > pow_keys;
    std::set< hp::account_name_type > accounts;

    std::set< hp::transaction_id_type > tapos_scope_tx_ids;

    void post_convert_transaction( hp::signed_transaction& trx );

    std::vector< std::thread > signers; // Transactions signers (defualt number is 1)

    typedef std::pair< size_t, hc::full_transaction_ptr > sig_stack_in_type;
    typedef size_t sig_stack_out_type;

    boost::lockfree::stack< sig_stack_in_type >  shared_signatures_stack_in;  // pair< trx index in block, signed transaction ptr to convert >
    boost::lockfree::stack< sig_stack_out_type > shared_signatures_stack_out; // pair< trx index in block, converted signature >

    bool increase_block_size;

    std::atomic_bool        signers_exit;

    hp::block_id_type converter_head_block_id;
    hp::block_id_type mainnet_head_block_id;

    static const std::array< uint32_t, HIVE_NUM_HARDFORKS + 1 > hardfork_blocks;

    std::vector< hp::private_key_type > transaction_signing_keys;

  public:
    /// All converted blocks will be signed using given private key
    blockchain_converter( const hp::private_key_type& _private_key, const hp::chain_id_type& chain_id, size_t signers_size = 1, bool increase_block_size = true );
    ~blockchain_converter();

    /// Sets previous id of the block to the given value and re-signs content of the block. Converts transactions. Returns current block id
    std::shared_ptr< hc::full_block_type > convert_signed_block( hp::signed_block& _signed_block, const hp::block_id_type& previous_block_id, const fc::time_point_sec& now_time, bool alter_time_in_visitor = false );

    const hp::block_id_type& get_converter_head_block_id()const;
    const hp::block_id_type& get_mainnet_head_block_id()const;

    uint32_t get_converter_head_block_num()const;

    void add_second_authority( hp::authority& _auth, hp::authority::classification type );

    void sign_transaction( hc::full_transaction_type& trx, hp::authority::classification type = hp::authority::owner, bool force = false )const;

    const hp::private_key_type& get_second_authority_key( hp::authority::classification type )const;
    void set_second_authority_key( const hp::private_key_type& key, hp::authority::classification type );

    void add_pow_key( const hp::account_name_type& acc, const hp::public_key_type& key );
    void add_account( const hp::account_name_type& acc );
    bool has_account( const hp::account_name_type& acc )const;

    const hp::private_key_type& get_witness_key()const;
    const hp::chain_id_type& get_chain_id()const;

    /**
     * @brief Alters the current converter state by changing the saved mainnet head block id.
     *        This function can be used to "move in time", e.g. if you want to apply hardforks
     *        without running the actual block conversion. Useful in the initial checks like the chain id validation
     * @note Saved mainnet head block id is the original value of the `previous` struct member of the currently
     *       processed or the previously processed block
     */
    void touch( const hp::signed_block_header& _signed_header );
    void touch( const hp::block_id_type& id ); // Function override if you want to directly set your mainnet head block id

    // Should be called every time TaPoS value changes in your plugin
    void on_tapos_change();

    bool has_hardfork( uint32_t hf )const;
    static bool has_hardfork( uint32_t hf, uint32_t block_num );
    static bool has_hardfork( uint32_t hf, const hp::signed_block& _signed_block );

    /**
     * @throws if there is no block being currently converted
     */
    const hp::signed_block& get_current_block()const;

    bool block_size_increase_enabled()const;

    // Needs to be called before signing transactions
    void apply_second_authority_keys();
  };

  class convert_operations_visitor
  {
  private:
    blockchain_converter& converter;
    uint32_t block_offset;

  public:
    typedef hp::operation result_type;

    convert_operations_visitor( blockchain_converter& converter, const fc::time_point_sec& block_offset );

    const hp::account_create_operation& operator()( hp::account_create_operation& op )const;

    const hp::account_create_with_delegation_operation& operator()( hp::account_create_with_delegation_operation& op )const;

    const hp::account_update_operation& operator()( hp::account_update_operation& op )const;

    const hp::account_update2_operation& operator()( hp::account_update2_operation& op )const;

    const hp::create_claimed_account_operation& operator()( hp::create_claimed_account_operation& op )const;

    const hp::custom_binary_operation& operator()( hp::custom_binary_operation& op )const;

    const hp::escrow_transfer_operation& operator()( hp::escrow_transfer_operation& op )const;

    const hp::limit_order_create_operation& operator()( hp::limit_order_create_operation& op )const;

    const hp::limit_order_create2_operation& operator()( hp::limit_order_create2_operation& op )const;

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
