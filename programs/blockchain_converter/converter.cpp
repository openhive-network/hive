#include "converter.hpp"

#include <stdexcept>
#include <map>
#include <string>

#include <hive/protocol/operations.hpp>
#include <hive/protocol/block.hpp>

#include <hive/utilities/key_conversion.hpp>

#include <fc/io/raw.hpp>

namespace hive {

  using namespace protocol;
  using namespace utilities;

  namespace converter {

    derived_keys_map::derived_keys_map( const std::string& private_key_wif )
      : private_key_wif( private_key_wif ) {}

    derived_keys_map::derived_keys_map( const fc::ecc::private_key& _private_key )
      : private_key_wif( key_to_wif(_private_key) ) {}

    public_key_type derived_keys_map::get_public( const public_key_type& original )
    {
      return get_private( original ).get_public_key();
    }

    const fc::ecc::private_key& derived_keys_map::get_private( const public_key_type& original )
    {
      if( keys.find( original ) != keys.end() )
        return keys.at( original );
      return (*keys.emplace( original,
          fc::ecc::private_key::regenerate(fc::sha256::hash(fc::sha512::hash( private_key_wif + ' ' + std::to_string( keys.size() ) )))
        ).first).second;
    }

    const fc::ecc::private_key& derived_keys_map::at( const public_key_type& original )const
    {
      return keys.at( original );
    }

    derived_keys_map::keys_map_type::iterator        derived_keys_map::begin()      { return keys.begin(); }
    derived_keys_map::keys_map_type::const_iterator  derived_keys_map::begin()const { return keys.begin(); }
    derived_keys_map::keys_map_type::const_iterator derived_keys_map::cbegin()const { return keys.begin(); }

    derived_keys_map::keys_map_type::iterator        derived_keys_map::end()      { return keys.end(); }
    derived_keys_map::keys_map_type::const_iterator  derived_keys_map::end()const { return keys.end(); }
    derived_keys_map::keys_map_type::const_iterator derived_keys_map::cend()const { return keys.end(); }


    convert_operations_visitor::convert_operations_visitor( blockchain_converter* converter )
      : converter( converter ) {}

    const account_create_operation& convert_operations_visitor::operator()( account_create_operation& op )const
    {
      op.owner.key_auths = converter->convert_authorities( op.owner.key_auths );
      op.active.key_auths = converter->convert_authorities( op.active.key_auths );
      op.posting.key_auths = converter->convert_authorities( op.posting.key_auths );

      return op;
    }

    const account_create_with_delegation_operation& convert_operations_visitor::operator()( account_create_with_delegation_operation& op )const
    {
      op.owner.key_auths = converter->convert_authorities( op.owner.key_auths );
      op.active.key_auths = converter->convert_authorities( op.active.key_auths );
      op.posting.key_auths = converter->convert_authorities( op.posting.key_auths );

      return op;
    }

    const account_update_operation& convert_operations_visitor::operator()( account_update_operation& op )const
    {
      if( op.owner.valid() )
        (*op.owner).key_auths = converter->convert_authorities( (*op.owner).key_auths );
      if( op.active.valid() )
        (*op.active).key_auths = converter->convert_authorities( (*op.active).key_auths );
      if( op.posting.valid() )
        (*op.posting).key_auths = converter->convert_authorities( (*op.posting).key_auths );

      return op;
    }

    const account_update2_operation& convert_operations_visitor::operator()( account_update2_operation& op )const
    {
      if( op.owner.valid() )
        (*op.owner).key_auths = converter->convert_authorities( (*op.owner).key_auths );
      if( op.active.valid() )
        (*op.active).key_auths = converter->convert_authorities( (*op.active).key_auths );
      if( op.posting.valid() )
        (*op.posting).key_auths = converter->convert_authorities( (*op.posting).key_auths );

      return op;
    }

    const create_claimed_account_operation& convert_operations_visitor::operator()( create_claimed_account_operation& op )const
    {
      op.owner.key_auths = converter->convert_authorities( op.owner.key_auths );
      op.active.key_auths = converter->convert_authorities( op.active.key_auths );
      op.posting.key_auths = converter->convert_authorities( op.posting.key_auths );

      return op;
    }

    const witness_update_operation& convert_operations_visitor::operator()( witness_update_operation& op )const
    {
      op.block_signing_key = converter->get_keys().get_public(op.block_signing_key);

      return op;
    }

    const witness_set_properties_operation& convert_operations_visitor::operator()( witness_set_properties_operation& op )const
    {
      auto key_itr = op.props.find( "key" );

      if( key_itr != op.props.end() )
      {
        public_key_type signing_key;
        fc::raw::unpack_from_vector( key_itr->second, signing_key );
        op.props.at( "key" ) = fc::raw::pack_to_vector( converter->get_keys().get_public(signing_key) );
      }

      return op;
    }

    const custom_binary_operation& convert_operations_visitor::operator()( custom_binary_operation& op )const
    {
      for( auto& auth : op.required_auths )
        auth.key_auths = converter->convert_authorities( auth.key_auths );

      return op;
    }

    const pow2_operation& convert_operations_visitor::operator()( pow2_operation& op )const
    {
      if( op.new_owner_key.valid() )
        *op.new_owner_key = converter->get_keys().get_public(*op.new_owner_key);

      return op;
    }

    const report_over_production_operation& convert_operations_visitor::operator()( report_over_production_operation& op )const
    {
      converter->convert_signed_header( op.first_block );
      converter->convert_signed_header( op.second_block );

      return op;
    }

    const request_account_recovery_operation& convert_operations_visitor::operator()( request_account_recovery_operation& op )const
    {
      op.new_owner_authority.key_auths = converter->convert_authorities( op.new_owner_authority.key_auths );

      return op;
    }

    const recover_account_operation& convert_operations_visitor::operator()( recover_account_operation& op )const
    {
      op.new_owner_authority.key_auths = converter->convert_authorities( op.new_owner_authority.key_auths );
      op.recent_owner_authority.key_auths = converter->convert_authorities( op.recent_owner_authority.key_auths );

      return op;
    }


    blockchain_converter::blockchain_converter( const fc::ecc::private_key& _private_key, const chain_id_type& chain_id )
      : _private_key( _private_key ), chain_id( chain_id ), keys( _private_key ) {}

    void blockchain_converter::convert_signed_block( signed_block& _signed_block )
    {
      convert_signed_header( _signed_block );

      for( auto _transaction = _signed_block.transactions.begin(); _transaction != _signed_block.transactions.end(); ++_transaction )
      {
        _transaction->visit( convert_operations_visitor( this ) );
        for( auto _signature = _transaction->signatures.begin(); _signature != _transaction->signatures.end(); ++_signature )
          *_signature = convert_signature_from_header( *_signature, _signed_block ).sign_compact( _transaction->sig_digest( chain_id ), get_canon_type( *_signature ) );
      }

      _signed_block.transaction_merkle_root = _signed_block.calculate_merkle_root();
    }

    void blockchain_converter::convert_signed_header( signed_block_header& _signed_header )
    {
      _signed_header.sign( convert_signature_from_header( _signed_header.witness_signature, _signed_header ), get_canon_type( _signed_header.witness_signature ) );
    }

    const fc::ecc::private_key& blockchain_converter::convert_signature_from_header( const signature_type& _signature, const signed_block_header& _signed_header )
    {
      switch( get_canon_type( _signature ) )
      {
        case fc::ecc::bip_0062:
          return keys.get_private( _signed_header.signee(fc::ecc::bip_0062) );
        case fc::ecc::fc_canonical:
          return keys.get_private( _signed_header.signee(fc::ecc::fc_canonical) );
        default:
          return keys.get_private( _signed_header.signee(fc::ecc::non_canonical) );
      }
    }

    fc::ecc::canonical_signature_type blockchain_converter::get_canon_type( const signature_type& _signature )const
    {
      if ( fc::ecc::public_key::is_canonical( _signature, fc::ecc::bip_0062 ) )
        return fc::ecc::bip_0062;
      else if ( fc::ecc::public_key::is_canonical( _signature, fc::ecc::fc_canonical ) )
        return fc::ecc::fc_canonical;

      return fc::ecc::non_canonical;
    }

    typename authority::key_authority_map blockchain_converter::convert_authorities( const typename authority::key_authority_map& auths )
    {
      typename authority::key_authority_map auth_keys;

      for( auto& key : auths )
        auth_keys[ keys.get_public(key.first) ] = key.second;

      return auth_keys;
    }

    derived_keys_map& blockchain_converter::get_keys()
    {
      return keys;
    }
  }
}