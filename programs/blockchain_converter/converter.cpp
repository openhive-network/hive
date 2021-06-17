#include "converter.hpp"

#include <iostream>
#include <array>
#include <string>

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
    converter.convert_authority( op.new_account_name, op.owner, authority::owner );
    converter.convert_authority( op.new_account_name, op.active, authority::active );
    converter.convert_authority( op.new_account_name, op.posting, authority::posting );

    return op;
  }

  const hp::account_create_with_delegation_operation& convert_operations_visitor::operator()( hp::account_create_with_delegation_operation& op )const
  {
    converter.convert_authority( op.new_account_name, op.owner, authority::owner );
    converter.convert_authority( op.new_account_name, op.active, authority::active );
    converter.convert_authority( op.new_account_name, op.posting, authority::posting );

    return op;
  }

  const hp::account_update_operation& convert_operations_visitor::operator()( hp::account_update_operation& op )const
  {
    if( op.owner.valid() )
      converter.convert_authority( op.account, *op.owner, authority::owner );
    if( op.active.valid() )
      converter.convert_authority( op.account, *op.active, authority::active );
    if( op.posting.valid() )
      converter.convert_authority( op.account, *op.posting, authority::posting );

    return op;
  }

  const hp::account_update2_operation& convert_operations_visitor::operator()( hp::account_update2_operation& op )const
  {
    if( op.owner.valid() )
      converter.convert_authority( op.account, *op.owner, authority::owner );
    if( op.active.valid() )
      converter.convert_authority( op.account, *op.active, authority::active );
    if( op.posting.valid() )
      converter.convert_authority( op.account, *op.posting, authority::posting );

    return op;
  }

  const hp::create_claimed_account_operation& convert_operations_visitor::operator()( hp::create_claimed_account_operation& op )const
  {
    converter.convert_authority( op.new_account_name, op.owner, authority::owner );
    converter.convert_authority( op.new_account_name, op.active, authority::active );
    converter.convert_authority( op.new_account_name, op.posting, authority::posting );

    return op;
  }

  const hp::custom_binary_operation& convert_operations_visitor::operator()( hp::custom_binary_operation& op )const
  {
    op.required_auths.clear();
    std::cout << "Clearing custom_binary_operation required_auths in block: " << hp::block_header::num_from_id( converter.get_previous_block_id() ) + 1 << '\n';

    return op;
  }

  const hp::pow_operation& convert_operations_visitor::operator()( hp::pow_operation& op )const
  {
    op.block_id = converter.get_previous_block_id();

    authority working{ 1, op.work.worker, 1 };

    converter.convert_authority( op.worker_account, working, authority::owner );
    converter.convert_authority( op.worker_account, working, authority::active );
    converter.convert_authority( op.worker_account, working, authority::posting );

    return op;
  }

  const hp::pow2_operation& convert_operations_visitor::operator()( hp::pow2_operation& op )const
  {
    if( op.new_owner_key.valid() )
    {
      authority working{ 1, *op.new_owner_key, 1 };
      hp::account_name_type worker = op.work.which() ? op.work.get< hp::equihash_pow >().input.worker_account : op.work.get< hp::pow2 >().input.worker_account;
      converter.convert_authority( worker, working, authority::owner );
      converter.convert_authority( worker, working, authority::active );
      converter.convert_authority( worker, working, authority::posting );
    }
    if( op.work.which() ) // equihash_pow
      op.work.get< hp::equihash_pow >().prev_block = converter.get_previous_block_id();
    else // pow2
      op.work.get< hp::pow2 >().input.prev_block = converter.get_previous_block_id();

    return op;
  }

  const hp::report_over_production_operation& convert_operations_visitor::operator()( hp::report_over_production_operation& op )const
  {
    converter.convert_signed_header( op.first_block );
    converter.convert_signed_header( op.second_block );

    return op;
  }

  const hp::request_account_recovery_operation& convert_operations_visitor::operator()( hp::request_account_recovery_operation& op )const
  {
    converter.convert_authority( op.account_to_recover, op.new_owner_authority, authority::owner );

    return op;
  }

  const hp::recover_account_operation& convert_operations_visitor::operator()( hp::recover_account_operation& op )const
  {
    converter.convert_authority( op.account_to_recover, op.new_owner_authority, authority::owner );
    converter.convert_authority( op.account_to_recover, op.recent_owner_authority, authority::owner ); // XXX: Check if fixes `Recent authority not found in authority history` FC_ASSERT

    return op;
  }


  blockchain_converter::blockchain_converter( const hp::private_key_type& _private_key, const hp::chain_id_type& chain_id )
    : _private_key( _private_key ), chain_id( chain_id ) {}

  void blockchain_converter::post_convert_transaction( hp::signed_transaction& _transaction )
  {
    if( hp::block_header::num_from_id( get_previous_block_id() ) + 1 > HIVE_HARDFORK_0_17_BLOCK_NUM && pow_auths.size() ) // Mining in HF 17 and above is disabled
    {
      auto pow_auths_itr = pow_auths.begin();

      // Add 2nd auth to the pow auths
      hp::account_update_operation op;
      op.account = pow_auths_itr->first;
      op.owner = pow_auths_itr->second.at( 0 );
      op.active = pow_auths_itr->second.at( 1 );
      op.posting = pow_auths_itr->second.at( 2 );

      _transaction.operations.push_back( op );
      pow_auths.erase( pow_auths_itr );
    }
  }

  hp::block_id_type blockchain_converter::convert_signed_block( hp::signed_block& _signed_block, const hp::block_id_type& previous_block_id )
  {
    this->previous_block_id = previous_block_id;

    _signed_block.previous = previous_block_id;

    std::unordered_set< hp::transaction_id_type > txid_checker; // Keeps the track of trx ids to change them if duplicated

    for( auto transaction_itr = _signed_block.transactions.begin(); transaction_itr != _signed_block.transactions.end(); ++transaction_itr )
    {
      transaction_itr->operations = transaction_itr->visit( convert_operations_visitor( *this ) );

      // re-sign ops
      for( auto signature_itr = transaction_itr->signatures.begin(); signature_itr != transaction_itr->signatures.end(); ++signature_itr )
        *signature_itr = get_second_authority_key( authority::owner ).sign_compact( transaction_itr->sig_digest( chain_id ) ); // XXX: All operations are being signed using the owner key of the 2nd authority

      // check for HF17 to add 2nd auth to the pow auths
      post_convert_transaction( *transaction_itr );

      transaction_itr->set_reference_block( previous_block_id );

      // Check for duplicated transaction ids
      while( !txid_checker.emplace( transaction_itr->id() ).second )
      {
        uint32_t tx_position = transaction_itr - _signed_block.transactions.begin();
        std::cout << "Duplicate transaction [" << tx_position << "] in block with num: "  << _signed_block.block_num() << " detected."
                  << "\nOld txid: " << transaction_itr->id().operator std::string();
        transaction_itr->expiration += tx_position;
        std::cout << "\nNew txid: " << transaction_itr->id().operator std::string() << '\n';
      }
    }

    _signed_block.transaction_merkle_root = _signed_block.calculate_merkle_root();

    // Sign header (using given witness' private key)
    convert_signed_header( _signed_block );

    return _signed_block.id();
  }

  void blockchain_converter::convert_signed_header( hp::signed_block_header& _signed_header )
  {
    _signed_header.sign( _private_key );
  }

  void blockchain_converter::convert_authority( const hp::account_name_type& name, authority& _auth, authority::classification type )
  {
    if( hp::block_header::num_from_id( get_previous_block_id() ) + 1 > HIVE_HARDFORK_0_17_BLOCK_NUM )
      _auth.add_authority( second_authority.at( type ).get_public_key(), 1 ); // Apply 2nd auth to every op after HF17
    else
      add_pow_authority( name, _auth, type );
  }

  const hp::private_key_type& blockchain_converter::get_second_authority_key( authority::classification type )const
  {
    return second_authority.at( type );
  }

  void blockchain_converter::set_second_authority_key( const hp::private_key_type& key, authority::classification type )
  {
    second_authority[ type ] = key;
  }

  const hp::private_key_type& blockchain_converter::get_witness_key()const
  {
    return _private_key;
  }

  void blockchain_converter::add_pow_authority( const hp::account_name_type& name, authority auth, authority::classification type )
  {
    if( name == HIVE_TEMP_ACCOUNT ) // Cannot update temp account.
      return;

    // validate account names
    for( auto acc_name_itr = auth.account_auths.begin(); acc_name_itr != auth.account_auths.end(); )
      if( !is_valid_account_name( acc_name_itr->first ) ) // Before HF15 users were allowed to include invalid account names into their account_auths (see e.g. block 3705111)
        auth.account_auths.erase( acc_name_itr );
      else
        ++acc_name_itr;

    // Get classification type (see pow_auths declaration)
    int classification_type;
    switch( type )
    {
      case authority::owner:           classification_type = 0; break;
      default: case authority::active: classification_type = 1; break;
      case authority::posting:         classification_type = 2; break;
    }

    auto pow_auths_itr = pow_auths.find( name );
    if( pow_auths_itr != pow_auths.end() )
    {
      if( pow_auths_itr->second.at( classification_type ).valid() ) // Given classification type already exists in pow_auths
      {
        // Merge auths
        for( const auto& _key : pow_auths_itr->second.at( classification_type )->key_auths )
          auth.add_authority( _key.first, _key.second );
        for( const auto& _acc : pow_auths_itr->second.at( classification_type )->account_auths )
          auth.add_authority( _acc.first, _acc.second );
      }
      else // Add second authority key to the auth before insertion
        auth.add_authority( second_authority.at( type ).get_public_key(), 1 );

      pow_auths_itr->second.at( classification_type ) = auth;
    }
    else
    {
      // Add new pow authority
      auth.add_authority( second_authority.at( type ).get_public_key(), 1 );
      std::array< fc::optional< authority >, 3 > auths_array;
      auths_array[ classification_type ] = auth;
      pow_auths.emplace( name, auths_array );
    }
  }

  bool blockchain_converter::has_pow_authority( const hp::account_name_type& name )const
  {
    return pow_auths.find( name ) != pow_auths.end();
  }

  const hp::block_id_type& blockchain_converter::get_previous_block_id()const
  {
    return previous_block_id;
  }

} } // hive::converter
