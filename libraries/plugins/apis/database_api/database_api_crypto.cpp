#include <hive/plugins/database_api/database_api_impl.hpp>

#include <hive/protocol/transaction_util.hpp>

namespace hive { namespace plugins { namespace database_api {

//////////////////////////////////////////////////////////////////////
//                                                                  //
// Authority / Validation                                           //
//                                                                  //
//////////////////////////////////////////////////////////////////////

DEFINE_API_IMPL( database_api_impl, get_transaction_hex )
{
  return get_transaction_hex_return( { fc::to_hex( fc::raw::pack_to_vector( args.trx ) ) } );
}

DEFINE_API_IMPL( database_api_impl, get_required_signatures )
{
  get_required_signatures_return result;
  result.keys = args.trx.get_required_signatures(
    _db.has_hardfork( HIVE_HARDFORK_1_28_ALLOW_STRICT_AND_MIXED_AUTHORITIES ),
    _db.has_hardfork( HIVE_HARDFORK_1_28_ALLOW_REDUNDANT_SIGNATURES ),
    _db.get_chain_id(),
    args.available_keys,
    [&]( string account_name ){ return authority( _db.get_account_authority( account_name ).active  ); },
    [&]( string account_name ){ return authority( _db.get_account_authority( account_name ).owner   ); },
    [&]( string account_name ){ return authority( _db.get_account_authority( account_name ).posting ); },
    [&]( string witness_name ){ return _db.get_witness(witness_name).signing_key; }, // note: reflect any changes here in database::apply_transaction
    HIVE_MAX_SIG_CHECK_DEPTH );

  return result;
}

DEFINE_API_IMPL( database_api_impl, get_potential_signatures )
{
  get_potential_signatures_return result;
  args.trx.get_required_signatures(
    _db.has_hardfork( HIVE_HARDFORK_1_28_ALLOW_STRICT_AND_MIXED_AUTHORITIES ),
    _db.has_hardfork( HIVE_HARDFORK_1_28_ALLOW_REDUNDANT_SIGNATURES ),
    _db.get_chain_id(),
    flat_set< public_key_type >(),
    [&]( account_name_type account_name )
    {
      const auto& auth = _db.get_account_authority( account_name ).active;
      for( const auto& k : auth.get_keys() )
        result.keys.insert( k );
      return authority( auth );
    },
    [&]( account_name_type account_name )
    {
      const auto& auth = _db.get_account_authority( account_name ).owner;
      for( const auto& k : auth.get_keys() )
        result.keys.insert( k );
      return authority( auth );
    },
    [&]( account_name_type account_name )
    {
      const auto& auth = _db.get_account_authority( account_name ).posting;
      for( const auto& k : auth.get_keys() )
        result.keys.insert( k );
      return authority( auth );
    },
    [&]( account_name_type witness_name )
    {
      return _db.get_witness(witness_name).signing_key;
    },
    HIVE_MAX_SIG_CHECK_DEPTH
  );

  return result;
}

DEFINE_API_IMPL( database_api_impl, verify_authority )
{
  args.trx.verify_authority(
    _db.has_hardfork( HIVE_HARDFORK_1_28_ALLOW_STRICT_AND_MIXED_AUTHORITIES ),
    _db.has_hardfork( HIVE_HARDFORK_1_28_ALLOW_REDUNDANT_SIGNATURES ),
    _db.get_chain_id(),
    [&]( string account_name ){ return authority( _db.get_account_authority( account_name ).active  ); },
    [&]( string account_name ){ return authority( _db.get_account_authority( account_name ).owner   ); },
    [&]( string account_name ){ return authority( _db.get_account_authority( account_name ).posting ); },
    [&]( string witness_name ){ return _db.get_witness(witness_name).signing_key; }, // note: reflect any changes here in database::apply_transaction
    args.pack,
    HIVE_MAX_SIG_CHECK_DEPTH,
    HIVE_MAX_AUTHORITY_MEMBERSHIP,
    HIVE_MAX_SIG_CHECK_ACCOUNTS );
  return verify_authority_return( { true } );
}

DEFINE_API_IMPL( database_api_impl, verify_account_authority )
{
  auto account = _db.find_account( args.account );
  FC_ASSERT( account != nullptr, "no such account" );

  hive::protocol::required_authorities_type required_authorities;
  switch( args.level )
  {
  case authority_level::active:
    required_authorities.required_active.insert( args.account );
    break;
  case authority_level::owner:
    required_authorities.required_owner.insert( args.account );
    break;
  case authority_level::posting:
    required_authorities.required_posting.insert( args.account );
    break;
  }

  bool ok = hive::protocol::has_authorization(
    _db.has_hardfork( HIVE_HARDFORK_1_28_ALLOW_STRICT_AND_MIXED_AUTHORITIES ),
    _db.has_hardfork( HIVE_HARDFORK_1_28_ALLOW_REDUNDANT_SIGNATURES ),
    required_authorities,
    args.signers,
    [&]( string account_name ) { return authority( _db.get_account_authority( account_name ).active ); },
    [&]( string account_name ) { return authority( _db.get_account_authority( account_name ).owner ); },
    [&]( string account_name ) { return authority( _db.get_account_authority( account_name ).posting ); },
    [&]( string witness_name ) { return _db.get_witness( witness_name ).signing_key; } );

  return verify_account_authority_return( { ok } );
}

DEFINE_API_IMPL( database_api_impl, verify_signatures )
{
  // get_signature_keys can throw for dup sigs. Allow this to throw.
  flat_set< public_key_type > sig_keys;
  for( const auto&  sig : args.signatures )
  {
    HIVE_ASSERT(
      sig_keys.insert( fc::ecc::public_key( sig, args.hash ) ).second,
      protocol::tx_duplicate_sig,
      "Duplicate signature detected" );
  }

  verify_signatures_return result;
  result.valid = true;

  // verify authority throws on failure, catch and return false
  try
  {
    hive::protocol::verify_authority< verify_signatures_args >(
      _db.has_hardfork( HIVE_HARDFORK_1_28_ALLOW_STRICT_AND_MIXED_AUTHORITIES ),
      _db.has_hardfork( HIVE_HARDFORK_1_28_ALLOW_REDUNDANT_SIGNATURES ),
      { args },
      sig_keys,
      [this]( const string& name ) { return authority( _db.get_account_authority( name ).owner ); },
      [this]( const string& name ) { return authority( _db.get_account_authority( name ).active ); },
      [this]( const string& name ) { return authority( _db.get_account_authority( name ).posting ); },
      [this]( string witness_name ){ return _db.get_witness(witness_name).signing_key; }, // note: reflect any changes here in database::apply_transaction
      HIVE_MAX_SIG_CHECK_DEPTH );
  }
  catch( fc::exception& ) { result.valid = false; }

  return result;
}

DEFINE_API_IMPL( database_api_impl, is_known_transaction )
{
  is_known_transaction_return result;
  result.is_known = _db.is_known_transaction(args.id);
  return result;
}

} } } // hive::plugins::database_api
