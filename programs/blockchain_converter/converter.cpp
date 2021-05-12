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
      converter.convert_authority( op.owner, authority::owner );
      converter.convert_authority( op.active, authority::active );
      converter.convert_authority( op.posting, authority::posting );

      return op;
    }

    const account_create_with_delegation_operation& convert_operations_visitor::operator()( account_create_with_delegation_operation& op )const
    {
      converter.convert_authority( op.owner, authority::owner );
      converter.convert_authority( op.active, authority::active );
      converter.convert_authority( op.posting, authority::posting );

      return op;
    }

    const account_update_operation& convert_operations_visitor::operator()( account_update_operation& op )const
    {
      if( op.owner.valid() )
        converter.convert_authority( *op.owner, authority::owner );
      if( op.active.valid() )
        converter.convert_authority( *op.active, authority::active );
      if( op.posting.valid() )
        converter.convert_authority( *op.posting, authority::posting );

      return op;
    }

    const account_update2_operation& convert_operations_visitor::operator()( account_update2_operation& op )const
    {
      if( op.owner.valid() )
        converter.convert_authority( *op.owner, authority::owner );
      if( op.active.valid() )
        converter.convert_authority( *op.active, authority::active );
      if( op.posting.valid() )
        converter.convert_authority( *op.posting, authority::posting );

      return op;
    }

    const create_claimed_account_operation& convert_operations_visitor::operator()( create_claimed_account_operation& op )const
    {
      converter.convert_authority( op.owner, authority::owner );
      converter.convert_authority( op.active, authority::active );
      converter.convert_authority( op.posting, authority::posting );

      return op;
    }

    const witness_update_operation& convert_operations_visitor::operator()( witness_update_operation& op )const
    {
      op.block_signing_key = converter.get_witness_key().get_public_key();

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
      for( auto& auth : op.required_auths )
        converter.convert_authority( auth, authority::active );

      return op;
    }

    const pow_operation& convert_operations_visitor::operator()( pow_operation& op )const
    {
      op.block_id = converter.get_current_signed_block().previous;

      converter.add_pow_authority( op.worker_account, op.work.worker );

      return op;
    }

    const pow2_operation& convert_operations_visitor::operator()( pow2_operation& op )const
    {
      if( op.new_owner_key.valid() )
        converter.add_pow_authority(
          op.work.which() ? op.work.get< equihash_pow >().input.worker_account : op.work.get< pow2 >().input.worker_account,
          *op.new_owner_key
        );

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
      converter.convert_authority( op.new_owner_authority, authority::owner );

      return op;
    }

    const recover_account_operation& convert_operations_visitor::operator()( recover_account_operation& op )const
    {
      converter.convert_authority( op.new_owner_authority, authority::owner );
      converter.convert_authority( op.recent_owner_authority, authority::owner );

      return op;
    }


    blockchain_converter::blockchain_converter( const private_key_type& _private_key, const chain_id_type& chain_id )
      : _private_key( _private_key ), chain_id( chain_id ) {}

    void blockchain_converter::post_convert_transaction( signed_transaction& _transaction )
    {
      while( pow_auths.size() )
      {
        auto it = pow_auths.begin();

        account_update_operation op;
        op.account = it->first;
        op.owner = authority( 1, it->first, 1, it->second, 1, second_authority.at(authority::owner).get_public_key(), 1 );
        op.active = authority( 1, it->first, 1, it->second, 1, second_authority.at(authority::active).get_public_key(), 1 );
        op.posting = authority( 1, it->first, 1, it->second, 1, second_authority.at(authority::posting).get_public_key(), 1 );

        _transaction.operations.push_back( op );
        pow_auths.erase( it );
      }
    }

    block_id_type blockchain_converter::convert_signed_block( signed_block& _signed_block, const block_id_type& previous_block_id )
    {
      current_signed_block = &_signed_block;

      _signed_block.previous = previous_block_id;

      for( auto _transaction = _signed_block.transactions.begin(); _transaction != _signed_block.transactions.end(); ++_transaction )
      {
        _transaction->operations = _transaction->visit( convert_operations_visitor( *this ) );
        for( auto _signature = _transaction->signatures.begin(); _signature != _transaction->signatures.end(); ++_signature )
          *_signature = _private_key.sign_compact( _transaction->sig_digest( chain_id ) );
        post_convert_transaction( *_transaction );
        _transaction->set_reference_block( previous_block_id );
      }

      _signed_block.transaction_merkle_root = _signed_block.calculate_merkle_root();

      convert_signed_header( _signed_block );

      return _signed_block.id();
    }

    void blockchain_converter::convert_signed_header( signed_block_header& _signed_header )
    {
      _signed_header.sign( _private_key );
    }

    void blockchain_converter::convert_authority( authority& _auth, authority::classification type )
    {
      _auth.key_auths[ second_authority.at( type ).get_public_key() ] = 1;

      validate_auth_size( _auth );
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

    void blockchain_converter::add_pow_authority( const account_name_type& name, const public_key_type& key )
    {
      pow_auths[ name ] = key;
    }

    const signed_block& blockchain_converter::get_current_signed_block()const
    {
      return *current_signed_block;
    }
  }
}
