#include <hive/plugins/database_api/database_api_impl.hpp>

#include <hive/chain/database_authority_getter.hpp>

#include <hive/protocol/transaction_util.hpp>

namespace hive { namespace plugins { namespace database_api {

namespace {

/**
 * Provides authorities from state like its base, but also collects all keys of authorities it was asked for.
 */
class potential_signatures_collector : public chain::database_authority_getter
{
public:
  potential_signatures_collector( const chain::database& db, std::set< public_key_type >& keys )
    : chain::database_authority_getter( db ), _keys( keys ) {}

  virtual std::optional<authority> get_active( const std::string& account_name ) const override
  {
    return collect_keys( chain::database_authority_getter::get_active( account_name ) );
  }
  virtual std::optional<authority> get_owner( const std::string& account_name ) const override
  {
    return collect_keys( chain::database_authority_getter::get_owner( account_name ) );
  }
  virtual std::optional<authority> get_posting( const std::string& account_name ) const override
  {
    return collect_keys( chain::database_authority_getter::get_posting( account_name ) );
  }

private:
  std::optional<authority> collect_keys( std::optional<authority>&& auth ) const
  {
    if( auth )
    {
      for( const auto& k : auth->key_auths )
        _keys.insert( k.first );
    }
    return std::move( auth );
  }

  std::set< public_key_type >& _keys;
};

} // namespace

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
    chain::database_authority_getter( _db ),
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
    potential_signatures_collector( _db, result.keys ),
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
    chain::database_authority_getter( _db ),
    args.pack,
    HIVE_MAX_SIG_CHECK_DEPTH,
    HIVE_MAX_AUTHORITY_MEMBERSHIP,
    HIVE_MAX_SIG_CHECK_ACCOUNTS );
  return verify_authority_return( { true } );
}

DEFINE_API_IMPL( database_api_impl, verify_account_authority )
{
  auto account = _db.find< chain::account_object, chain::by_name >( args.account );
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
    chain::database_authority_getter( _db ) );

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
      chain::database_authority_getter( _db ),
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
