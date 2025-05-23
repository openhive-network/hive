
#include <hive/protocol/transaction.hpp>
#include <hive/protocol/transaction_util.hpp>

#include <fc/io/raw.hpp>
#include <fc/bitutil.hpp>
#include <fc/smart_ref_impl.hpp>

#include <algorithm>

namespace hive { namespace protocol {

digest_type signed_transaction::merkle_digest(hive::protocol::pack_type pack)const
{
  hive::protocol::serialization_mode_controller::pack_guard guard( pack );
  digest_type::encoder enc;
  fc::raw::pack( enc, *this );
  return enc.result();
}

digest_type transaction::digest(hive::protocol::pack_type pack)const
{
  hive::protocol::serialization_mode_controller::pack_guard guard( pack );
  digest_type::encoder enc;
  fc::raw::pack( enc, *this );
  return enc.result();
}

digest_type transaction::sig_digest( const chain_id_type& chain_id, hive::protocol::pack_type pack )const
{
  digest_type::encoder enc;

  hive::protocol::serialization_mode_controller::pack_guard guard( pack );
  fc::raw::pack( enc, chain_id );
  fc::raw::pack( enc, *this );

  return enc.result();
}

void transaction::validate() const
{
  FC_ASSERT( operations.size() > 0, "A transaction must have at least one operation", ("trx",*this) );
  for( const auto& op : operations )
    operation_validate(op);
}

void transaction::validate( const std::function<void( const operation& op, bool post )>& notify ) const
{
  FC_ASSERT( operations.size() > 0, "A transaction must have at least one operation", ("trx",*this) );
  for( const auto& op : operations )
  {
    notify( op, false );
    operation_validate(op);
    notify( op, true );
  }
}

transaction_id_type transaction::id(hive::protocol::pack_type pack) const
{
  auto h = digest(pack);
  transaction_id_type result;
  memcpy(result._hash, h._hash, std::min(sizeof(result), sizeof(h)));
  return result;
}
/*
const signature_type& signed_transaction::sign( const private_key_type& key, const chain_id_type& chain_id )
{
  digest_type h = sig_digest( chain_id, hive::protocol::serialization_mode_controller::get_current_pack() );
  signatures.push_back( key.sign_compact( h ) );
  return signatures.back();
}

signature_type signed_transaction::sign( const private_key_type& key, const chain_id_type& chain_id )const
{
  digest_type h = sig_digest( chain_id, hive::protocol::serialization_mode_controller::get_current_pack() );
  return key.sign_compact( h );
}
*/
void transaction::set_expiration( fc::time_point_sec expiration_time )
{
    expiration = expiration_time;
}

void transaction::set_reference_block( const block_id_type& reference_block )
{
  ref_block_num = fc::endian_reverse_u32(reference_block._hash[0]);
  ref_block_prefix = reference_block._hash[1];
}

void transaction::get_required_authorities( flat_set< account_name_type >& active,
                              flat_set< account_name_type >& owner,
                              flat_set< account_name_type >& posting,
                              flat_set< account_name_type >& witness,
                              vector< authority >& other )const
{
  for( const auto& op : operations )
    operation_get_required_authorities( op, active, owner, posting, witness, other );
}

flat_set<public_key_type> signed_transaction::get_signature_keys( const chain_id_type& chain_id, hive::protocol::pack_type pack )const
{ try {
  auto d = sig_digest( chain_id, pack );
  flat_set<public_key_type> result;
  for( const auto&  sig : signatures )
  {
    HIVE_ASSERT(
      result.insert( fc::ecc::public_key( sig, d ) ).second,
      tx_duplicate_sig,
      "Duplicate signature detected" );
  }
  return result;
} FC_CAPTURE_AND_RETHROW() }



set<public_key_type> signed_transaction::get_required_signatures(
  bool allow_strict_and_mixed_authorities,
  bool allow_redundant_signatures,
  const chain_id_type& chain_id,
  const flat_set<public_key_type>& available_keys,
  const authority_getter& get_active,
  const authority_getter& get_owner,
  const authority_getter& get_posting,
  const witness_public_key_getter& get_witness_key,
  uint32_t max_recursion_depth,
  uint32_t max_membership,
  uint32_t max_account_auths )const
{
  flat_set< account_name_type > required_active;
  flat_set< account_name_type > required_owner;
  flat_set< account_name_type > required_posting;
  flat_set< account_name_type > required_witness;
  vector< authority > other;
  get_required_authorities( required_active, required_owner, required_posting, required_witness, other );

  set<public_key_type> result;

  auto _find_keys = [&result, &available_keys]( const flat_map<public_key_type,bool>& provided_signatures )
  {
    for( auto& provided_sig : provided_signatures )
      if( available_keys.find( provided_sig.first ) != available_keys.end() )
        result.insert( provided_sig.first );
  };

  sign_state s( get_signature_keys( chain_id, hive::protocol::serialization_mode_controller::get_current_pack() ), get_posting,
                 { allow_strict_and_mixed_authorities, max_recursion_depth, max_membership, max_account_auths } );

  s.extend_provided_signatures( available_keys );

  /** Up to HF28 posting authority cannot be mixed with active authority in same transaction */
  if( required_posting.size() ) {
    if( !allow_strict_and_mixed_authorities )
    {
      FC_ASSERT( !required_owner.size() );
      FC_ASSERT( !required_active.size() );
    }

    for( auto& posting : required_posting )
      s.check_authority( posting  );

    if( !allow_strict_and_mixed_authorities )
    {
      s.remove_unused_signatures();

      _find_keys( s.get_provided_signatures() );
      return result;
    }
  }

  s.change_current_authority( get_active );

  s.clear_approved();
  for( const auto& auth : other )
    s.check_authority( auth, "?", "?" );
  for( auto& owner : required_owner )
    s.check_authority( get_owner( owner ), owner, "owner" );
  for( auto& active : required_active )
    s.check_authority( active );

  s.remove_unused_signatures();

  _find_keys( s.get_provided_signatures() );

  return result;
}

set<public_key_type> signed_transaction::minimize_required_signatures(
  bool allow_strict_and_mixed_authorities,
  const chain_id_type& chain_id,
  const flat_set< public_key_type >& available_keys,
  const authority_getter& get_active,
  const authority_getter& get_owner,
  const authority_getter& get_posting,
  const witness_public_key_getter& get_witness_key,
  uint32_t max_recursion,
  uint32_t max_membership,
  uint32_t max_account_auths
  ) const
{
  //Don't allow redundant authorities. A transaction should be as small as possible.
  const bool _allow_redundant_authorities = false;
  set< public_key_type > s = get_required_signatures( allow_strict_and_mixed_authorities, _allow_redundant_authorities, chain_id, available_keys, get_active, get_owner, get_posting, get_witness_key, max_recursion, max_membership, max_account_auths );
  flat_set< public_key_type > result( s.begin(), s.end() );

  for( const public_key_type& k : s )
  {
    result.erase( k );
    try
    {
      hive::protocol::verify_authority(
        allow_strict_and_mixed_authorities,
        _allow_redundant_authorities,
        operations,
        result,
        get_active,
        get_owner,
        get_posting,
        get_witness_key,
        max_recursion,
        max_membership,
        max_account_auths,
        false,
        flat_set< account_name_type >(),
        flat_set< account_name_type >(),
        flat_set< account_name_type >() );
      continue;  // element stays erased if verify_authority is ok
    }
    catch( const tx_missing_owner_auth& e ) {}
    catch( const tx_missing_active_auth& e ) {}
    catch( const tx_missing_posting_auth& e ) {}
    catch( const tx_missing_other_auth& e ) {}
    result.insert( k );
  }
  return set<public_key_type>( result.begin(), result.end() );
}

void signed_transaction::verify_authority(bool allow_strict_and_mixed_authorities,
                                          bool allow_redundant_signatures,
                                          const chain_id_type& chain_id,
                                          const authority_getter& get_active,
                                          const authority_getter& get_owner,
                                          const authority_getter& get_posting,
                                          const witness_public_key_getter& get_witness_key,
                                          hive::protocol::pack_type pack,
                                          uint32_t max_recursion,
                                          uint32_t max_membership,
                                          uint32_t max_account_auths) const
{ try {
  hive::protocol::verify_authority(allow_strict_and_mixed_authorities,
                                   allow_redundant_signatures,
                                   operations,
                                   get_signature_keys(chain_id, pack),
                                   get_active,
                                   get_owner,
                                   get_posting,
                                   get_witness_key,
                                   max_recursion,
                                   max_membership,
                                   max_account_auths,
                                   false,
                                   flat_set<account_name_type>(),
                                   flat_set<account_name_type>(),
                                   flat_set<account_name_type>());
} FC_CAPTURE_AND_RETHROW((*this)) }

} } // hive::protocol
