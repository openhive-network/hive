#include "converter.hpp"

#include <map>
#include <string>
#include <limits>

#include <hive/protocol/operations.hpp>
#include <hive/protocol/block.hpp>

#include <hive/utilities/key_conversion.hpp>

#include <hive/wallet/wallet.hpp>

#include <fc/io/raw.hpp>
#include <fc/io/fstream.hpp>

#include <fc/crypto/aes.hpp>

namespace hive {

  using namespace protocol;
  using namespace utilities;
  using namespace wallet;

  namespace converter {

    convert_operations_visitor::convert_operations_visitor( blockchain_converter& converter )
      : converter( converter ) {}

    const account_create_operation& convert_operations_visitor::operator()( account_create_operation& op )const
    {
      converter.convert_authority( op.new_account_name, op.owner, authority::owner );
      converter.convert_authority( op.new_account_name, op.active, authority::active );
      converter.convert_authority( op.new_account_name, op.posting, authority::posting );

      return op;
    }

    const account_create_with_delegation_operation& convert_operations_visitor::operator()( account_create_with_delegation_operation& op )const
    {
      converter.convert_authority( op.new_account_name, op.owner, authority::owner );
      converter.convert_authority( op.new_account_name, op.active, authority::active );
      converter.convert_authority( op.new_account_name, op.posting, authority::posting );

      return op;
    }

    const account_update_operation& convert_operations_visitor::operator()( account_update_operation& op )const
    {
      if( op.owner.valid() )
        converter.convert_authority( op.account, *op.owner, authority::owner );
      if( op.active.valid() )
        converter.convert_authority( op.account, *op.active, authority::active );
      if( op.posting.valid() )
        converter.convert_authority( op.account, *op.posting, authority::posting );

      return op;
    }

    const account_update2_operation& convert_operations_visitor::operator()( account_update2_operation& op )const
    {
      if( op.owner.valid() )
        converter.convert_authority( op.account, *op.owner, authority::owner );
      if( op.active.valid() )
        converter.convert_authority( op.account, *op.active, authority::active );
      if( op.posting.valid() )
        converter.convert_authority( op.account, *op.posting, authority::posting );

      return op;
    }

    const create_claimed_account_operation& convert_operations_visitor::operator()( create_claimed_account_operation& op )const
    {
      converter.convert_authority( op.new_account_name, op.owner, authority::owner );
      converter.convert_authority( op.new_account_name, op.active, authority::active );
      converter.convert_authority( op.new_account_name, op.posting, authority::posting );

      return op;
    }

    const witness_update_operation& convert_operations_visitor::operator()( witness_update_operation& op )const
    {
      // op.block_signing_key = converter.get_witness_key().get_public_key();

      return op;
    }

    const witness_set_properties_operation& convert_operations_visitor::operator()( witness_set_properties_operation& op )const
    {
      public_key_type signing_key;

      auto key_itr = op.props.find( "key" );

      if( key_itr != op.props.end() )
        op.props.at( "key" ) = fc::raw::pack_to_vector( converter.get_witness_key().get_public_key() );

      auto new_signing_key_itr = op.props.find( "new_signing_key" );

      if( new_signing_key_itr != op.props.end() )
        op.props.at( "new_signing_key" ) = fc::raw::pack_to_vector( converter.get_witness_key().get_public_key() );

      return op;
    }

    const custom_binary_operation& convert_operations_visitor::operator()( custom_binary_operation& op )const
    {
      op.required_auths.clear();
      std::cout << "Clearing custom_binary_operation required_auths in block: " << converter.get_current_signed_block().block_num() << '\n';

      return op;
    }

    const pow_operation& convert_operations_visitor::operator()( pow_operation& op )const
    {
      op.block_id = converter.get_current_signed_block().previous;

      authority working{ 1, op.work.worker, 1 };

      converter.convert_authority( op.worker_account, working, authority::owner );
      converter.convert_authority( op.worker_account, working, authority::active );
      converter.convert_authority( op.worker_account, working, authority::posting );

      return op;
    }

    const pow2_operation& convert_operations_visitor::operator()( pow2_operation& op )const
    {
      if( op.new_owner_key.valid() )
      {
        authority working{ 1, *op.new_owner_key, 1 };
        account_name_type worker = op.work.which() ? op.work.get< equihash_pow >().input.worker_account : op.work.get< pow2 >().input.worker_account;
        converter.convert_authority( worker, working, authority::owner );
        converter.convert_authority( worker, working, authority::active );
        converter.convert_authority( worker, working, authority::posting );
      }
      if( op.work.which() ) // equihash_pow
        op.work.get< equihash_pow >().prev_block = converter.get_current_signed_block().previous;
      else // pow2
        op.work.get< pow2 >().input.prev_block = converter.get_current_signed_block().previous;

      return op;
    }

    const report_over_production_operation& convert_operations_visitor::operator()( report_over_production_operation& op )const
    {
      converter.convert_signed_header( op.first_block );
      converter.convert_signed_header( op.second_block );

      return op;
    }

    const request_account_recovery_operation& convert_operations_visitor::operator()( request_account_recovery_operation& op )const
    {
      converter.convert_authority( op.account_to_recover, op.new_owner_authority, authority::owner );

      return op;
    }

    const recover_account_operation& convert_operations_visitor::operator()( recover_account_operation& op )const
    {
      converter.convert_authority( op.account_to_recover, op.new_owner_authority, authority::owner );

      return op;
    }


    blockchain_converter::blockchain_converter( const private_key_type& _private_key, const chain_id_type& chain_id )
      : _private_key( _private_key ), chain_id( chain_id ) {}

    void blockchain_converter::post_convert_transaction( signed_transaction& _transaction )
    {
      if( current_signed_block->block_num() > HIVE_HARDFORK_0_17_BLOCK_NUM && pow_auths.size() ) // Mining in HF 17 and above is disabled
      {
        auto pow_auths_itr = pow_auths.begin();

        account_update_operation op;
        op.account = pow_auths_itr->first;
        op.owner = pow_auths_itr->second.at( 0 );
        op.active = pow_auths_itr->second.at( 1 );
        op.posting = pow_auths_itr->second.at( 2 );

        _transaction.operations.push_back( op );
        pow_auths.erase( pow_auths_itr );
      }
    }

    block_id_type blockchain_converter::convert_signed_block( signed_block& _signed_block, const block_id_type& previous_block_id )
    {
      current_signed_block = &_signed_block;

      _signed_block.previous = previous_block_id;

      for( auto transaction_itr = _signed_block.transactions.begin(); transaction_itr != _signed_block.transactions.end(); ++transaction_itr )
      {
        transaction_itr->operations = transaction_itr->visit( convert_operations_visitor( *this ) );
        for( auto signature_itr = transaction_itr->signatures.begin(); signature_itr != transaction_itr->signatures.end(); ++signature_itr )
          *signature_itr = _private_key.sign_compact( transaction_itr->sig_digest( chain_id ) );
        post_convert_transaction( *transaction_itr );
        transaction_itr->set_reference_block( previous_block_id );
      }

      _signed_block.transaction_merkle_root = _signed_block.calculate_merkle_root();

      convert_signed_header( _signed_block );

      return _signed_block.id();
    }

    void blockchain_converter::convert_signed_header( signed_block_header& _signed_header )
    {
      _signed_header.sign( _private_key );
    }

    void blockchain_converter::convert_authority( const account_name_type& name, authority& _auth, authority::classification type )
    {
      if( current_signed_block->block_num() > HIVE_HARDFORK_0_17_BLOCK_NUM )
        _auth.add_authority( second_authority.at( type ).get_public_key(), 1 );
      else
        add_pow_authority( name, _auth, type );
    }

    const private_key_type& blockchain_converter::get_second_authority_key( authority::classification type )const
    {
      return second_authority.at( type );
    }

    void blockchain_converter::set_second_authority_key( const private_key_type& key, authority::classification type )
    {
      second_authority[ type ] = key;
    }

    const private_key_type& blockchain_converter::get_witness_key()const
    {
      return _private_key;
    }

    void blockchain_converter::add_pow_authority( const account_name_type& name, authority auth, authority::classification type )
    {
      if( name == HIVE_TEMP_ACCOUNT ) // Cannot update temp account.
        return;

      // validate account names
      for( auto acc_name_itr = auth.account_auths.begin(); acc_name_itr != auth.account_auths.end(); )
        if( !is_valid_account_name( acc_name_itr->first ) ) // Before HF15 users were allowed to include invalid account names into their account_auths (see e.g. block 3705111)
          auth.account_auths.erase( acc_name_itr );
        else
          ++acc_name_itr;

      auto pow_auths_itr = pow_auths.find( name );
      if( pow_auths_itr != pow_auths.end() )
      {
        int classification_type;
        switch( type )
        {
          case authority::owner:           classification_type = 0; break;
          default: case authority::active: classification_type = 1; break;
          case authority::posting:         classification_type = 2; break;
        }

        // Merge auths
        for( const auto& _key : pow_auths_itr->second.at( classification_type ).key_auths )
          auth.add_authority( _key.first, _key.second );
        for( const auto& _acc : pow_auths_itr->second.at( classification_type ).account_auths )
          auth.add_authority( _acc.first, _acc.second );

        _auth->second.at( classification_type ) = auth;
      }
      else
      {
        auth.add_authority( second_authority.at( type ).get_public_key(), 1 );
        std::array< authority, 3 > auths_array;
        switch( type )
        {
          case authority::owner:           auths_array[ 0 ] = auth; break;
          default: case authority::active: auths_array[ 1 ] = auth; break;
          case authority::posting:         auths_array[ 2 ] = auth; break;
        }
        pow_auths.emplace( name, auths_array );
      }
    }

    bool blockchain_converter::has_pow_authority( const account_name_type& name )const
    {
      return pow_auths.find( name ) != pow_auths.end();
    }

    const signed_block& blockchain_converter::get_current_signed_block()const
    {
      return *current_signed_block;
    }
  }
}
