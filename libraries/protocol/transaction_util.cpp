#include <hive/protocol/transaction_util.hpp>

#include <fc/crypto/aes.hpp>
#include <fc/crypto/base58.hpp>

namespace hive { namespace protocol {

void verify_authority(const required_authorities_type& required_authorities, 
                      const flat_set<public_key_type>& sigs,
                      const authority_getter& get_active,
                      const authority_getter& get_owner,
                      const authority_getter& get_posting,
                      const witness_public_key_getter& get_witness_key,
                      uint32_t max_recursion_depth /* = HIVE_MAX_SIG_CHECK_DEPTH */,
                      uint32_t max_membership /* = HIVE_MAX_AUTHORITY_MEMBERSHIP */,
                      uint32_t max_account_auths /* = HIVE_MAX_SIG_CHECK_ACCOUNTS */,
                      bool allow_committe /* = false */,
                      const flat_set<account_name_type>& active_approvals /* = flat_set<account_name_type>() */,
                      const flat_set<account_name_type>& owner_approvals /* = flat_set<account_name_type>() */,
                      const flat_set<account_name_type>& posting_approvals /* = flat_set<account_name_type>() */)
{ try {
  /**
    *  Transactions with operations required posting authority cannot be combined
    *  with transactions requiring active or owner authority. This is for ease of
    *  implementation. Future versions of authority verification may be able to
    *  check for the merged authority of active and posting.
    */
  if( required_authorities.required_posting.size() ) {
    FC_ASSERT( required_authorities.required_active.size() == 0 );
    FC_ASSERT( required_authorities.required_owner.size() == 0 );
    FC_ASSERT( required_authorities.required_witness.size() == 0 );
    FC_ASSERT( required_authorities.other.size() == 0 );

    flat_set<public_key_type> avail;
    sign_state s(sigs, get_posting, avail);
    s.max_recursion = max_recursion_depth;
    s.max_membership = max_membership;
    s.max_account_auths = max_account_auths;
    for( auto& id : posting_approvals )
      s.approved_by.insert( id );
    for( const auto& id : required_authorities.required_posting )
    {
      HIVE_ASSERT(s.check_authority(id) ||
                  s.check_authority(get_active(id)) ||
                  s.check_authority(get_owner(id)),
                  tx_missing_posting_auth, "Missing Posting Authority ${id}",
                  ("id",id)
                  ("posting",get_posting(id))
                  ("active",get_active(id))
                  ("owner",get_owner(id)));
    }
    HIVE_ASSERT(
      !s.remove_unused_signatures(),
      tx_irrelevant_sig,
      "Unnecessary signature(s) detected"
      );
    return;
  }

  flat_set< public_key_type > avail;
  sign_state s(sigs,get_active,avail);
  s.max_recursion = max_recursion_depth;
  s.max_membership = max_membership;
  s.max_account_auths = max_account_auths;
  for( auto& id : active_approvals )
    s.approved_by.insert( id );
  for( auto& id : owner_approvals )
    s.approved_by.insert( id );

  for( const auto& auth : required_authorities.other )
  {
    HIVE_ASSERT( s.check_authority(auth), tx_missing_other_auth, "Missing Authority", ("auth",auth)("sigs",sigs) );
  }

  // fetch all of the top level authorities
  for( const auto& id : required_authorities.required_active )
  {
    HIVE_FINALIZABLE_ASSERT( s.check_authority(id) || s.check_authority(get_owner(id)),
      tx_missing_active_auth, "Missing Active Authority ${id}", ("id",id)("auth",get_active(id))("owner",get_owner(id)) );
  }

  for( const auto& id : required_authorities.required_owner )
  {
    HIVE_ASSERT( owner_approvals.find(id) != owner_approvals.end() ||
                s.check_authority(get_owner(id)),
                tx_missing_owner_auth, "Missing Owner Authority ${id}", ("id",id)("auth",get_owner(id)) );
  }

  for( const auto& id : required_authorities.required_witness )
  {
    public_key_type signing_key;
    try
    {
      signing_key = get_witness_key(id);
    }
    catch (const fc::exception&)
    {
      FC_THROW_EXCEPTION(tx_missing_witness_auth, "Missing Witness Authority ${id}", (id));
    }
    HIVE_ASSERT(s.signed_by(signing_key),
                tx_missing_witness_auth, "Missing Witness Authority ${id}, key ${signing_key}", (id)(signing_key));
  }

  HIVE_ASSERT(
    !s.remove_unused_signatures(),
    tx_irrelevant_sig,
    "Unnecessary signature(s) detected"
    );
} FC_CAPTURE_AND_RETHROW((sigs)) }

void collect_potential_keys( std::vector< public_key_type >* keys,
  const account_name_type& account, const std::string& str )
{
  // get possible key if str was WIF private key
  {
    auto private_key_ptr = fc::ecc::private_key::wif_to_key( str );
    if( private_key_ptr )
      keys->push_back( private_key_ptr->get_public_key() );
  }

  // get possible key if str was an extended private key
  try
  {
    keys->push_back( fc::ecc::extended_private_key::from_base58( str ).get_public_key() );
  }
  catch( fc::parse_error_exception& ) {}
  catch( fc::assert_exception& ) {}

  // get possible keys if str was an account password
  auto generate_key = [&]( std::string role ) -> public_key_type
  {
    std::string seed = account + role + str;
    auto secret = fc::sha256::hash( seed.c_str(), seed.size() );
    return fc::ecc::private_key::regenerate( secret ).get_public_key();
  };
  keys->push_back( generate_key( "owner" ) );
  keys->push_back( generate_key( "active" ) );
  keys->push_back( generate_key( "posting" ) );
  keys->push_back( generate_key( "memo" ) );
}

std::optional<memo_data> memo_data::from_string( std::string str )
{
  try
  {
    if( str.size() > sizeof( memo_data ) && str[0] == '#' )
    {
      auto data = fc::from_base58( str.substr( 1 ) );
      memo_data m;
      fc::raw::unpack_from_vector( data, m );
      FC_ASSERT( string( m ) == str );
      return m;
    }
  }
  catch( ... ) {}
  return std::optional<memo_data>();
}

memo_data::operator std::string() const
{
  auto data = fc::raw::pack_to_vector( *this );
  auto base58 = fc::to_base58( data );
  return '#' + base58;
}

std::string encrypt_memo( const private_key_type& from_priv, const public_key_type& to_key, std::string memo )
{
  FC_ASSERT( memo.size() > 0 && memo[0] == '#' );
  memo_data m;

  m.from = from_priv.get_public_key();
  m.to = to_key;
  m.nonce = fc::time_point::now().time_since_epoch().count();

  auto shared_secret = from_priv.get_shared_secret( m.to );

  fc::sha512::encoder enc;
  fc::raw::pack( enc, m.nonce );
  fc::raw::pack( enc, shared_secret );
  auto encrypt_key = enc.result();

  m.encrypted = fc::aes_encrypt( encrypt_key, fc::raw::pack_to_vector( memo.substr( 1 ) ) );
  m.check = fc::sha256::hash( encrypt_key )._hash[0];
  return m;
}

std::string decrypt_memo( std::function< fc::optional<private_key_type>( const public_key_type& )> key_finder, std::string encrypted_memo )
{
  FC_ASSERT( encrypted_memo.size() > 0 && encrypted_memo[0] == '#' );
  auto m = memo_data::from_string( encrypted_memo );
  if( !m )
    return encrypted_memo;

  fc::sha512 shared_secret;
  auto from_key = key_finder( m->from );
  if( !from_key )
  {
    auto to_key = key_finder( m->to );
    if( !to_key )
      return encrypted_memo;
    shared_secret = to_key->get_shared_secret( m->from );
  }
  else
  {
    shared_secret = from_key->get_shared_secret( m->to );
  }
  fc::sha512::encoder enc;
  fc::raw::pack( enc, m->nonce );
  fc::raw::pack( enc, shared_secret );
  auto encryption_key = enc.result();

  uint32_t check = fc::sha256::hash( encryption_key )._hash[0];
  if( check != m->check )
    return encrypted_memo;

  try
  {
    vector<char> decrypted = fc::aes_decrypt( encryption_key, m->encrypted );
    std::string decrypted_string;
    fc::raw::unpack_from_vector( decrypted, decrypted_string );
    return decrypted_string;
  }
  catch( ... ) {}

  return encrypted_memo;
}

} } // hive::protocol
