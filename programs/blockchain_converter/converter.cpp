#include "converter.hpp"

#include <array>
#include <string>
#include <thread>
#include <mutex>
#include <vector>
#include <functional>

#include <boost/preprocessor/repetition.hpp>
#include <boost/preprocessor/arithmetic/add.hpp>
#include <boost/preprocessor/control/if.hpp>
#include <boost/preprocessor/comparison/equal.hpp>
#include <boost/preprocessor/logical/and.hpp>

#include <hive/protocol/types.hpp>
#include <hive/protocol/authority.hpp>
#include <hive/protocol/operations.hpp>
#include <hive/protocol/block.hpp>
#include <hive/protocol/hardfork_block.hpp>

#include <fc/log/logger.hpp>

#include <hive/utilities/key_conversion.hpp>

namespace hive { namespace converter {

  namespace hp = hive::protocol;

  using hp::authority;

  convert_operations_visitor::convert_operations_visitor( blockchain_converter& converter, const fc::time_point_sec& trx_now_time )
    : converter( converter ), trx_now_time( trx_now_time ) {}

  const hp::account_create_operation& convert_operations_visitor::operator()( hp::account_create_operation& op )const
  {
    converter.add_second_authority( op.owner, authority::owner );
    converter.add_second_authority( op.active, authority::active );
    converter.add_second_authority( op.posting, authority::posting );

    converter.add_account( op.new_account_name );

    return op;
  }

  const hp::account_create_with_delegation_operation& convert_operations_visitor::operator()( hp::account_create_with_delegation_operation& op )const
  {
    converter.add_second_authority( op.owner, authority::owner );
    converter.add_second_authority( op.active, authority::active );
    converter.add_second_authority( op.posting, authority::posting );

    converter.add_account( op.new_account_name );

    return op;
  }

  const hp::account_update_operation& convert_operations_visitor::operator()( hp::account_update_operation& op )const
  {
    if( op.owner.valid() )
      converter.add_second_authority( *op.owner, authority::owner );
    if( op.active.valid() )
      converter.add_second_authority( *op.active, authority::active );
    if( op.posting.valid() )
      converter.add_second_authority( *op.posting, authority::posting );

    return op;
  }

  const hp::account_update2_operation& convert_operations_visitor::operator()( hp::account_update2_operation& op )const
  {
    if( op.owner.valid() )
      converter.add_second_authority( *op.owner, authority::owner );
    if( op.active.valid() )
      converter.add_second_authority( *op.active, authority::active );
    if( op.posting.valid() )
      converter.add_second_authority( *op.posting, authority::posting );

    return op;
  }

  const hp::create_claimed_account_operation& convert_operations_visitor::operator()( hp::create_claimed_account_operation& op )const
  {
    converter.add_second_authority( op.owner, authority::owner );
    converter.add_second_authority( op.active, authority::active );
    converter.add_second_authority( op.posting, authority::posting );

    converter.add_account( op.new_account_name );

    return op;
  }

  const hp::custom_binary_operation& convert_operations_visitor::operator()( hp::custom_binary_operation& op )const
  {
    op.required_auths.clear();
    wlog( "Clearing custom_binary_operation required_auths in block: ${block_num}", ("block_num", converter.get_current_block().block_num()) );

    return op;
  }

  const hp::escrow_transfer_operation& convert_operations_visitor::operator()( hp::escrow_transfer_operation& op )const
  {
    if( trx_now_time != blockchain_converter::auto_trx_time )
    { // For the live conversion
      op.escrow_expiration = trx_now_time + (op.escrow_expiration-converter.get_current_block().timestamp);
      op.ratification_deadline = trx_now_time + (op.ratification_deadline-converter.get_current_block().timestamp);
    }

    return op;
  }

  const hp::limit_order_create_operation& convert_operations_visitor::operator()( hp::limit_order_create_operation& op )const
  {
    if( !converter.has_hardfork( HIVE_HARDFORK_0_20__1449 ) )
    {
      uint32_t rand_offset = converter.get_mainnet_head_block_id()._hash[ 4 ] % 86400;
      op.expiration = std::min( op.expiration, fc::time_point_sec( HIVE_HARDFORK_0_20_TIME + HIVE_MAX_LIMIT_ORDER_EXPIRATION + rand_offset ) );
    }
    else if( trx_now_time != blockchain_converter::auto_trx_time )
    { // For the live conversion
      op.expiration = trx_now_time + (op.expiration-converter.get_current_block().timestamp);
    }

    return op;
  }

  const hp::limit_order_create2_operation& convert_operations_visitor::operator()( hp::limit_order_create2_operation& op )const
  {
    if( trx_now_time != blockchain_converter::auto_trx_time )
    { // For the live conversion
      op.expiration = trx_now_time + (op.expiration-converter.get_current_block().timestamp);
    }

    return op;
  }

  const hp::pow_operation& convert_operations_visitor::operator()( hp::pow_operation& op )const
  {
    op.block_id = converter.get_current_block().previous;

    converter.add_pow_key( op.worker_account, op.work.worker );

    if( op.work.worker != hp::public_key_type{} )
      op.work.worker = converter.get_witness_key().get_public_key();

    return op;
  }

  const hp::pow2_operation& convert_operations_visitor::operator()( hp::pow2_operation& op )const
  {
    const auto& input = op.work.which() ? op.work.get< hp::equihash_pow >().input : op.work.get< hp::pow2 >().input;

    if( op.new_owner_key.valid() )
    {
      converter.add_pow_key( input.worker_account, *op.new_owner_key );

      if( *op.new_owner_key != hp::public_key_type{} )
        *op.new_owner_key = converter.get_witness_key().get_public_key();
    }

    auto& prev_block = op.work.which() ? op.work.get< hp::equihash_pow >().prev_block : op.work.get< hp::pow2 >().input.prev_block;

    prev_block = converter.get_current_block().previous;

    return op;
  }

  const hp::report_over_production_operation& convert_operations_visitor::operator()( hp::report_over_production_operation& op )const
  {
    converter.sign_header( op.first_block );
    converter.sign_header( op.second_block );

    return op;
  }

  const hp::request_account_recovery_operation& convert_operations_visitor::operator()( hp::request_account_recovery_operation& op )const
  {
    converter.add_second_authority( op.new_owner_authority, authority::owner );

    return op;
  }

  const hp::recover_account_operation& convert_operations_visitor::operator()( hp::recover_account_operation& op )const
  {
    converter.add_second_authority( op.new_owner_authority, authority::owner );
    converter.add_second_authority( op.recent_owner_authority, authority::owner );

    return op;
  }

  const hp::witness_update_operation& convert_operations_visitor::operator()( hp::witness_update_operation& op )const
  {
    if( converter.block_size_increase_enabled() )
      op.props.maximum_block_size = HIVE_SOFT_MAX_BLOCK_SIZE;

    if( op.block_signing_key != hp::public_key_type{} )
      op.block_signing_key = converter.get_witness_key().get_public_key();

    return op;
  }

  const hp::witness_set_properties_operation& convert_operations_visitor::operator()( hp::witness_set_properties_operation& op )const
  {
    if( converter.block_size_increase_enabled() )
      op.props[ "maximum_block_size" ] = fc::raw::pack_to_vector( HIVE_SOFT_MAX_BLOCK_SIZE );

    const auto apply_witness_key_on_prop = [&]( const std::string& key )
      {
        hp::public_key_type signing_key;
        auto key_itr = op.props.find( key );
        if( key_itr != op.props.end() )
        {
          fc::raw::unpack_from_vector( key_itr->second, signing_key );
          if( signing_key != hp::public_key_type{} )
            key_itr->second = fc::raw::pack_to_vector( converter.get_witness_key().get_public_key() );
        }
      };

    apply_witness_key_on_prop( "key" );
    apply_witness_key_on_prop( "new_signing_key" );

    return op;
  }


  blockchain_converter::blockchain_converter( const hp::private_key_type& _private_key, const hp::chain_id_type& chain_id, size_t signers_size, bool increase_block_size )
    : _private_key( _private_key ), chain_id( chain_id ), shared_signatures_stack_in(10000), shared_signatures_stack_out(10000), increase_block_size( increase_block_size ), signers_exit( false ) 
  {
    FC_ASSERT( signers_size > 0, "There must be at least 1 signer thread!" );
    for( size_t i = 0; i < signers_size; ++i )
      signers.emplace( signers.end(), std::bind( [&]( size_t worker_index ) {
        sig_stack_in_type local_trx;
        while( !signers_exit.load() )
          if( shared_signatures_stack_in.pop( local_trx ) )
          {
            while( !shared_signatures_stack_out.push(
              std::make_pair(local_trx.first, generate_signature( *local_trx.second ))
            ) ) continue;
          }
      }, i ) );
  }

  blockchain_converter::~blockchain_converter()
  {
    signers_exit = true;
    for( size_t i = 0; i < signers.size(); ++i )
      signers.at( i ).join(); // Wait for all the signer threads to end their work
  }

  void blockchain_converter::post_convert_transaction( hp::signed_transaction& trx )
  {
    while( pow_keys.size() )
    {
      auto& pow_auth = pow_keys.front();
      auto acc = pow_auth.account_auths.begin()->first;
      pow_auth.account_auths.clear();

      // Add 2nd auth to the pow auths
      hp::account_update_operation acc_update_op;
      acc_update_op.account = acc;
      acc_update_op.active  = pow_auth;
      acc_update_op.owner   = pow_auth;
      acc_update_op.posting = pow_auth;

      trx.operations.push_back( acc_update_op );
      pow_keys.pop();
    }
  }

  const fc::time_point_sec blockchain_converter::auto_trx_time = fc::time_point::min();

// When we have definition like `HIVE_HARDFORK_1_24_BLOCK`, `1` would stand for the **major** version and `24` for the **minor** version
//
// Number of the existing blockchain forks (major versions)
#define HIVE_BC_HF_CHAINS_NUMBER  2

// Range of the pre-hive blockchain hardforks
#define HIVE_BC_HF_CHAIN_0_START  1
#define HIVE_BC_HF_CHAIN_0_STOP  23

// Range of the hive blockchain hardforks
#define HIVE_BC_HF_CHAIN_1_START BOOST_PP_ADD(1, HIVE_BC_HF_CHAIN_0_STOP)
#define HIVE_BC_HF_CHAIN_1_STOP  HIVE_NUM_HARDFORKS

// Hard fork applier implementation - lambda that returns true if block with given number already has requested hardfork
#define HIVE_BC_HF_FORK_APPLIER_GENERATOR_IMPL(_z, minor, major) \
  HIVE_HARDFORK_## major ##_## minor ##_BLOCK, /* Note: comma as an array separator */

// Generates fork appliers for all of the chains
#define HIVE_BC_HF_FORK_APPLIER_GENERATOR(_z, major, ...) \
  BOOST_PP_REPEAT_FROM_TO(                                \
    HIVE_BC_HF_CHAIN_## major ##_START,                   \
    BOOST_PP_ADD(1, HIVE_BC_HF_CHAIN_## major ##_STOP ),  \
    HIVE_BC_HF_FORK_APPLIER_GENERATOR_IMPL,               \
    major                                                 \
  )

  const std::array< uint32_t, HIVE_NUM_HARDFORKS + 1 > blockchain_converter::hardfork_blocks = {
    BOOST_PP_REPEAT(HIVE_BC_HF_CHAINS_NUMBER, HIVE_BC_HF_FORK_APPLIER_GENERATOR, 0) /*, */
    static_cast< uint32_t >(-1) // Default value for future hardforks (maximum block number - should never evaluate to `true` in #has_hardfork)
  };

// Cleanup definitions as they were already used and thus no longer required
#undef HIVE_BC_HF_CHAINS_NUMBER
#undef HIVE_BC_HF_CHAIN_0_START
#undef HIVE_BC_HF_CHAIN_0_STOP
#undef HIVE_BC_HF_CHAIN_1_START
#undef HIVE_BC_HF_CHAIN_1_START
#undef HIVE_BC_HF_FORK_APPLIER_GENERATOR_IMPL
#undef HIVE_BC_HF_FORK_APPLIER_GENERATOR

  hp::block_id_type blockchain_converter::convert_signed_block( hp::signed_block& _signed_block, const hp::block_id_type& previous_block_id, const fc::time_point_sec& trx_now_time )
  {
    touch( _signed_block ); // Update the mainnet head block id
    // Now when we have our mainnet head block id saved for the expiration time before HF20 generation in the
    // `limit_order_create_operation` operation generation, we can override the previous block id in our block:
    _signed_block.previous = previous_block_id;

    current_block_ptr = &_signed_block;

    auto trx_time = trx_now_time;

    if( trx_now_time == auto_trx_time )
      trx_time = _signed_block.timestamp; // Deduce time from the signed block
    else
      trx_time += HIVE_BLOCK_INTERVAL; // Apply min expiration time and then increase this value to avoid trx id duplication

    std::set<size_t> already_signed_transaction_pos;

    for( auto transaction_itr = _signed_block.transactions.begin(); transaction_itr != _signed_block.transactions.end(); ++transaction_itr )
    {
      //by default, use transaction's original expiration time
      if (trx_now_time == auto_trx_time)
        trx_time = transaction_itr->expiration + fc::seconds(1);
      wlog("###### trx_time=${trx_time}",(trx_time));

      transaction_itr->operations = transaction_itr->visit( convert_operations_visitor( *this, trx_now_time ) );

      transaction_itr->set_reference_block( previous_block_id );

      trx_time += 1;

      transaction_itr->expiration = trx_time;

      hive::protocol::signed_transaction helper_tx;
      helper_tx.set_reference_block(previous_block_id);

      post_convert_transaction(helper_tx);

      if(helper_tx.operations.empty() == false)
      {
        trx_time += 1;
        helper_tx.expiration = trx_time;
        sign_transaction(helper_tx, true);

        auto insert_pos = transaction_itr;
        ++insert_pos;
        auto new_tx_pos = _signed_block.transactions.insert(insert_pos, helper_tx);

        auto tx_pos = std::distance(_signed_block.transactions.begin(), new_tx_pos);

        already_signed_transaction_pos.insert(tx_pos);

        transaction_itr = new_tx_pos;
      }
    }

    switch( _signed_block.transactions.size() )
    {
      case 0: break; // Skip signatures generation when block does not contain any trxs

      case 2:
        if (already_signed_transaction_pos.find(1)== already_signed_transaction_pos.end())
          sign_transaction(_signed_block.transactions.at(1));
      case 1:
        if(already_signed_transaction_pos.find(0) == already_signed_transaction_pos.end())
          sign_transaction(_signed_block.transactions.at(0));
        break; // Optimize signing when block contains only 2 trx

      default: // There are multiple trxs in block. Enable multithreading

      size_t trx_applied_count = 0;

      for( size_t i = 0; i < _signed_block.transactions.size(); ++i )
        if( _signed_block.transactions.at( i ).signatures.size()  && already_signed_transaction_pos.find(i) == already_signed_transaction_pos.end() )
          shared_signatures_stack_in.push( std::make_pair( i, &_signed_block.transactions.at( i ) ) );
        else
          ++trx_applied_count; // We do not have to replace any signature(s) so skip this transaction

      sig_stack_out_type current_sig;

      while( trx_applied_count < _signed_block.transactions.size() ) // Replace the signatures
      {
        if( shared_signatures_stack_out.pop( current_sig ) )
        {
          if( _signed_block.transactions.at( current_sig.first ).signatures.size() > 1 ) // Remove redundant signatures
          {
            auto& sigs = _signed_block.transactions.at( current_sig.first ).signatures;
            sigs.erase( sigs.begin() + 1, sigs.end() );
          }

          _signed_block.transactions.at( current_sig.first ).signatures.at( 0 ) = current_sig.second;

          ++trx_applied_count;
        }
      }
    }

    _signed_block.transaction_merkle_root = _signed_block.calculate_merkle_root();

    // Sign header (using given witness' private key)
    sign_header( _signed_block );

    current_block_ptr = nullptr; // Invalidate to make sure that other functions will not try to use deallocated data

    return _signed_block.id();
  }

  const hp::block_id_type& blockchain_converter::get_mainnet_head_block_id()const
  {
    return mainnet_head_block_id;
  }

  uint32_t blockchain_converter::get_converter_head_block_num()const
  {
    /*
     * If we used `current_block_ptr` here, it would be impossible for the users to access this function outside the conversion scope.
     * The solution is the usage of the `mainnet_head_block_id`, which we hold a copy of. We calculate the block number from the block id
     * which is in the standard implementation: `num_from_id(id) + 1` (block numbers should be the same in both mainnet and mirrornet)
     */
    return hp::block_header::num_from_id(mainnet_head_block_id) + 1;
  }

  hp::signature_type blockchain_converter::generate_signature( const hp::signed_transaction& trx, authority::classification type )const
  {
    return get_second_authority_key( type ).sign_compact(
            trx.sig_digest( chain_id ),
            has_hardfork( HIVE_HARDFORK_0_20__1944 ) ? fc::ecc::bip_0062 : fc::ecc::fc_canonical
          );
  }

  void blockchain_converter::sign_transaction( hp::signed_transaction& trx, bool force )const
  {
    if( trx.signatures.size() || force)
    {
      trx.signatures.clear();
      trx.signatures.push_back( generate_signature(trx) ); // XXX: All operations are being signed using the owner key of the 2nd authority
    }
  }

  void blockchain_converter::sign_header( hp::signed_block_header& _signed_header )
  {
    _signed_header.sign( _private_key, has_hardfork( HIVE_HARDFORK_0_20__1944 ) ? fc::ecc::bip_0062 : fc::ecc::fc_canonical );
  }

  void blockchain_converter::add_second_authority( authority& _auth, authority::classification type )
  {
    _auth.add_authority( second_authority.at( type ).get_public_key(), std::max(_auth.weight_threshold, 1u) );
  }

  const hp::private_key_type& blockchain_converter::get_second_authority_key( authority::classification type )const
  {
    std::lock_guard< std::mutex > _guard( second_auth_mutex );
    return second_authority.at( type );
  }

  void blockchain_converter::set_second_authority_key( const hp::private_key_type& key, authority::classification type )
  {
    std::lock_guard< std::mutex > _guard( second_auth_mutex );
    second_authority[ type ] = key;
  }

  void blockchain_converter::add_pow_key( const hp::account_name_type& acc, const hp::public_key_type& key )
  {
    if( !has_account( acc ) ) // Update pow auth only on acc creation
    {
      pow_keys.emplace( 1, acc, 1, key, 1, get_second_authority_key( authority::active ).get_public_key(), 1 ); // add pow acc
      add_account( acc );
    }
  }

  void blockchain_converter::add_account( const hp::account_name_type& acc )
  {
    if( !has_hardfork( HIVE_HARDFORK_0_17__770 ) )
      accounts.insert( acc );
    else if( accounts.size() )
      accounts.clear(); // Free some space
  }

  bool blockchain_converter::has_account( const hp::account_name_type& acc )const
  {
    return accounts.find( acc ) != accounts.end();
  }

  const hp::private_key_type& blockchain_converter::get_witness_key()const
  {
    return _private_key;
  }

  const hp::chain_id_type& blockchain_converter::get_chain_id()const
  {
    return chain_id;
  }

  void blockchain_converter::touch( const hp::signed_block_header& _signed_header )
  {
    touch(_signed_header.previous);
  }

  void blockchain_converter::touch( const hp::block_id_type& id )
  {
    mainnet_head_block_id = id;
  }

  bool blockchain_converter::has_hardfork( uint32_t hf )const
  {
    return has_hardfork( hf, get_converter_head_block_num() );
  }

  bool blockchain_converter::has_hardfork( uint32_t hf, const hp::signed_block& _signed_block )
  {
    return has_hardfork( hf, _signed_block.block_num() );
  }

  bool blockchain_converter::has_hardfork( uint32_t hf, uint32_t block_num )
  {
    if( hf > HIVE_NUM_HARDFORKS )
      hf = HIVE_NUM_HARDFORKS + 1; // See #hardfork_blocks - default value for future hard forks: 2^32 - 1

    return block_num > hardfork_blocks.at(hf - 1);
  }

  const hp::signed_block& blockchain_converter::get_current_block()const
  {
    FC_ASSERT( current_block_ptr != nullptr, "Cannot return reference to the signed block. You can access block that is currently being converted only during its conversion." );
    return *current_block_ptr;
  }

  bool blockchain_converter::block_size_increase_enabled()const
  {
    return increase_block_size;
  }

} } // hive::converter
