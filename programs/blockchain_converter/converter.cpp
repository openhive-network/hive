#include "converter.hpp"

#include <iostream>
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

#include <hive/utilities/key_conversion.hpp>

namespace hive { namespace converter {

  namespace hp = hive::protocol;

  using hp::authority;

  convert_operations_visitor::convert_operations_visitor( blockchain_converter& converter )
    : converter( converter ) {}

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
    std::cout << "Clearing custom_binary_operation required_auths in block: " << converter.get_current_block().block_num() << '\n';

    return op;
  }

  const hp::limit_order_create_operation& convert_operations_visitor::operator()( hp::limit_order_create_operation& op )const
  {
    if( !converter.has_hardfork( HIVE_HARDFORK_0_20__1449 ) )
    {
      uint32_t rand_offset = converter.get_mainnet_head_block_id()._hash[ 4 ] % 86400;
      op.expiration = std::min( op.expiration, fc::time_point_sec( HIVE_HARDFORK_0_20_TIME + HIVE_MAX_LIMIT_ORDER_EXPIRATION + rand_offset ) );
    }

    return op;
  }

  const hp::pow_operation& convert_operations_visitor::operator()( hp::pow_operation& op )const
  {
    op.block_id = converter.get_current_block().previous;

    converter.add_pow_key( op.worker_account, op.work.worker );

    return op;
  }

  const hp::pow2_operation& convert_operations_visitor::operator()( hp::pow2_operation& op )const
  {
    const auto& input = op.work.which() ? op.work.get< hp::equihash_pow >().input : op.work.get< hp::pow2 >().input;

    if( op.new_owner_key.valid() )
      converter.add_pow_key( input.worker_account, *op.new_owner_key );

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
    op.props.maximum_block_size = HIVE_SOFT_MAX_BLOCK_SIZE;

    return op;
  }

  const hp::witness_set_properties_operation& convert_operations_visitor::operator()( hp::witness_set_properties_operation& op )const
  {
    op.props[ "maximum_block_size" ] = fc::raw::pack_to_vector( HIVE_SOFT_MAX_BLOCK_SIZE );

    return op;
  }


  blockchain_converter::blockchain_converter( const hp::private_key_type& _private_key, const hp::chain_id_type& chain_id, size_t signers_size )
    : _private_key( _private_key ), chain_id( chain_id ), signers_exit( false ), current_hardfork( 0 )
  {
    FC_ASSERT( signers_size > 0, "There must be at least 1 signer thread!" );
    for( size_t i = 0; i < signers_size; ++i )
      signers.emplace( signers.end(), std::bind( [&]( size_t worker_index ) {
        sig_stack_in_type local_trx;
        while( !signers_exit.load() )
          if( shared_signatures_stack_in.pop( local_trx ) )
          {
            // std::cout << "Worker: " << worker_index << " just got a new job. Trx index: " << local_trx.first << ". Signing... ";
            while( ! shared_signatures_stack_out.push( std::make_pair( local_trx.first,
              get_second_authority_key( authority::owner ).sign_compact( local_trx.second->sig_digest( get_chain_id() ), has_hardfork( HIVE_HARDFORK_0_20__1944 ) ? fc::ecc::bip_0062 : fc::ecc::fc_canonical )
              ) ) ) continue;
            // std::cout << "Done.\n";
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

#ifndef HIVE_BC_HARDFORKS_MAJOR_SIZE
#  define HIVE_BC_HARDFORKS_MAJOR_SIZE 2
#endif

#ifndef HIVE_BC_HARDFORKS_0_START
#  define HIVE_BC_HARDFORKS_0_START 1
#endif
#ifndef HIVE_BC_HARDFORKS_0_STOP
#  define HIVE_BC_HARDFORKS_0_STOP 23
#endif

#ifndef HIVE_BC_HARDFORKS_1_START
#  define HIVE_BC_HARDFORKS_1_START BOOST_PP_ADD( 1, HIVE_BC_HARDFORKS_0_STOP )
#endif
#ifndef HIVE_BC_HARDFORKS_1_STOP
#  define HIVE_BC_HARDFORKS_1_STOP HIVE_NUM_HARDFORKS
#endif

#ifndef HIVE_BC_HF_N_CASE_MACRO
#  define HIVE_BC_HF_N_CASE_MACRO(z, n, data) else if ( _signed_block.timestamp.sec_since_epoch() >= HIVE_HARDFORK_ ##data ## _ ##n ## _TIME \
                                                        && current_hardfork < n ) { ++current_hardfork; \
std::cout << "HF applied: " << current_hardfork << " in block " << _signed_block.block_num() << '\n'; }
#endif

#ifndef HIVE_BC_HF_ALL_CASE_MACRO_LOOP
#  define HIVE_BC_HF_ALL_CASE_MACRO_LOOP(z, n, data) \
  BOOST_PP_REPEAT_FROM_TO( HIVE_BC_HARDFORKS_ ##n ## _START, \
                           BOOST_PP_ADD( 1, HIVE_BC_HARDFORKS_ ##n ## _STOP ), \
                           HIVE_BC_HF_N_CASE_MACRO, \
                           n \
                           )
#endif

#ifndef HIVE_BC_HF_ALL_CASE_MACRO
#  define HIVE_BC_HF_ALL_CASE_MACRO() \
  if(false){} /* For else ifs */ \
  BOOST_PP_REPEAT( HIVE_BC_HARDFORKS_MAJOR_SIZE, HIVE_BC_HF_ALL_CASE_MACRO_LOOP, )
#endif

  void blockchain_converter::check_for_hardfork( const hp::signed_block& _signed_block )
  {
    // Expands to the if/elses that increment current_hardfork every time _signed_block.timestamp equals to the original timestamp of the hardfork
    HIVE_BC_HF_ALL_CASE_MACRO();
  }

// Cleanup defines
#undef HIVE_BC_HARDFORKS_MAJOR_SIZE
#undef HIVE_BC_HARDFORKS_0_START
#undef HIVE_BC_HARDFORKS_0_STOP
#undef HIVE_BC_HARDFORKS_1_START
#undef HIVE_BC_HARDFORKS_1_STOP
#undef HIVE_BC_HF_N_CASE_MACRO
#undef HIVE_BC_HF_ALL_CASE_MACRO_LOOP
#undef HIVE_BC_HF_ALL_CASE_MACRO

#undef HIVE_HARDFORK_1_25_TIME
#define HIVE_HARDFORK_1_25_TIME next_hf_time()

  hp::block_id_type blockchain_converter::convert_signed_block( hp::signed_block& _signed_block, const hp::block_id_type& previous_block_id, const fc::time_point_sec& trx_now_time )
  {
   this->mainnet_head_block_id = _signed_block.previous;

    current_block_ptr = &_signed_block;

    check_for_hardfork( _signed_block );

    auto trx_time = trx_now_time;

    if( trx_now_time == auto_trx_time )
      trx_time = _signed_block.timestamp; // Deduce time from the signed block
    else
      trx_time += HIVE_BLOCK_INTERVAL; // Apply min expiration time and then increase this value to avoid trx id duplication

    _signed_block.previous = previous_block_id;

    for( auto transaction_itr = _signed_block.transactions.begin(); transaction_itr != _signed_block.transactions.end(); ++transaction_itr )
    {
      transaction_itr->operations = transaction_itr->visit( convert_operations_visitor( *this ) );

      transaction_itr->set_reference_block( previous_block_id );

      post_convert_transaction( *transaction_itr );

      transaction_itr->expiration = trx_time;
      trx_time += 1;
    }

    switch( _signed_block.transactions.size() )
    {
      case 0: break; // Skip signatures generation when block does not contain any trxs
      case 1: sign_transaction( _signed_block.transactions.at( 0 ) ); break; // Optimize signing when block contains only 1 trx
      default: // There are multiple trxs in block. Enable multithreading

      size_t trx_applied_count = 0;

      for( size_t i = 0; i < _signed_block.transactions.size(); ++i )
        if( _signed_block.transactions.at( i ).signatures.size() )
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

  void blockchain_converter::sign_transaction( hp::signed_transaction& trx )const
  {
    if( trx.signatures.size() )
    {
      if( trx.signatures.size() > 1 )
        trx.signatures.clear();
      trx.signatures[ 0 ] = get_second_authority_key( authority::owner ).sign_compact( trx.sig_digest( chain_id ), has_hardfork( HIVE_HARDFORK_0_20__1944 ) ? fc::ecc::bip_0062 : fc::ecc::fc_canonical ); // XXX: All operations are being signed using the owner key of the 2nd authority
    }
  }

  void blockchain_converter::sign_header( hp::signed_block_header& _signed_header )
  {
    _signed_header.sign( _private_key, has_hardfork( HIVE_HARDFORK_0_20__1944 ) ? fc::ecc::bip_0062 : fc::ecc::fc_canonical );
  }

  void blockchain_converter::add_second_authority( authority& _auth, authority::classification type )
  {
    _auth.add_authority( second_authority.at( type ).get_public_key(), 1 );
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

  uint32_t blockchain_converter::get_current_hardfork()const
  {
    return current_hardfork;
  }

  bool blockchain_converter::has_hardfork( uint32_t hf )const
  {
    return current_hardfork >= hf;
  }

  const hp::signed_block& blockchain_converter::get_current_block()const
  {
    FC_ASSERT( current_block_ptr != nullptr, "Cannot return reference to the signed block. You can access block that is currently being converted only during its conversion." );
    return *current_block_ptr;
  }

} } // hive::converter
