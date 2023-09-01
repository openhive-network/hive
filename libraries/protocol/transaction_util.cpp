#include <hive/protocol/transaction_util.hpp>

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

} } // hive::protocol
