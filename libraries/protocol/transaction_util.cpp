#include <hive/protocol/transaction_util.hpp>

namespace hive { namespace protocol {

enum class verify_authority_problem
{
  mixed_with_posting,
  missing_posting,
  missing_active,
  missing_owner,
  missing_witness,
  missing_witness_key,
  unused_signature
};

template< typename PROBLEM_HANDLER, typename OTHER_AUTH_PROBLEM_HANDLER >
void verify_authority_impl(
  const required_authorities_type& required_authorities,
  const flat_set<public_key_type>& sigs,
  const authority_getter& get_active,
  const authority_getter& get_owner,
  const authority_getter& get_posting,
  const witness_public_key_getter& get_witness_key,
  uint32_t max_recursion_depth,
  uint32_t max_membership,
  uint32_t max_account_auths,
  const flat_set<account_name_type>& active_approvals,
  const flat_set<account_name_type>& owner_approvals,
  const flat_set<account_name_type>& posting_approvals,
  const PROBLEM_HANDLER& handler,
  const OTHER_AUTH_PROBLEM_HANDLER& other_handler)
{
#define VERIFY_AUTHORITY_CHECK( TEST, PROBLEM, ID )     \
FC_EXPAND_MACRO(                                        \
  FC_MULTILINE_MACRO_BEGIN                              \
    if( UNLIKELY( !( TEST ) ) )                         \
    {                                                   \
      handler( #TEST, PROBLEM, ID );                    \
      return;                                           \
    }                                                   \
  FC_MULTILINE_MACRO_END                                \
)
#define VERIFY_AUTHORITY_CHECK_OTHER_AUTH( TEST, AUTH ) \
FC_EXPAND_MACRO(                                        \
  FC_MULTILINE_MACRO_BEGIN                              \
    if( UNLIKELY( !( TEST ) ) )                         \
    {                                                   \
      other_handler( #TEST, AUTH );                     \
      return;                                           \
    }                                                   \
  FC_MULTILINE_MACRO_END                                \
)
  /**
    *  Transactions with operations required posting authority cannot be combined
    *  with transactions requiring active or owner authority. This is for ease of
    *  implementation. Future versions of authority verification may be able to
    *  check for the merged authority of active and posting.
    */
  if( not required_authorities.required_posting.empty() )
  {
    VERIFY_AUTHORITY_CHECK( required_authorities.required_active.empty() &&
      required_authorities.required_owner.empty() &&
      required_authorities.required_witness.empty() &&
      required_authorities.other.empty(),
      verify_authority_problem::mixed_with_posting, account_name_type() );

    flat_set<public_key_type> avail;
    sign_state s( sigs, get_posting, avail );
    s.max_recursion = max_recursion_depth;
    s.max_membership = max_membership;
    s.max_account_auths = max_account_auths;
    for( auto& id : posting_approvals )
      s.approved_by.insert( id );
    for( const auto& id : required_authorities.required_posting )
    {
      VERIFY_AUTHORITY_CHECK( s.check_authority( id ) || s.check_authority( get_active( id ) ) ||
        s.check_authority( get_owner( id ) ), verify_authority_problem::missing_posting, id );
    }
    VERIFY_AUTHORITY_CHECK( !s.remove_unused_signatures(),
      verify_authority_problem::unused_signature, account_name_type() );
    return;
  }

  flat_set< public_key_type > avail;
  sign_state s( sigs, get_active, avail );
  s.max_recursion = max_recursion_depth;
  s.max_membership = max_membership;
  s.max_account_auths = max_account_auths;
  for( auto& id : active_approvals )
    s.approved_by.insert( id );
  for( auto& id : owner_approvals )
    s.approved_by.insert( id );

  for( const auto& auth : required_authorities.other )
  {
    VERIFY_AUTHORITY_CHECK_OTHER_AUTH( s.check_authority( auth ), auth );
  }

  // fetch all of the top level authorities
  for( const auto& id : required_authorities.required_active )
  {
    VERIFY_AUTHORITY_CHECK( s.check_authority( id ) || s.check_authority( get_owner( id ) ),
      verify_authority_problem::missing_active, id );
  }

  for( const auto& id : required_authorities.required_owner )
  {
    VERIFY_AUTHORITY_CHECK( owner_approvals.find( id ) != owner_approvals.end() ||
      s.check_authority( get_owner( id ) ), verify_authority_problem::missing_owner, id );
  }

  for( const auto& id : required_authorities.required_witness )
  {
    public_key_type signing_key;
    try
    {
      signing_key = get_witness_key( id );
    }
    catch (const fc::exception&)
    {
      handler( "get_witness_key( id )", verify_authority_problem::missing_witness_key, id );
      return;
    }
    VERIFY_AUTHORITY_CHECK( s.signed_by( signing_key ),
      verify_authority_problem::missing_witness, id );
  }

  VERIFY_AUTHORITY_CHECK( !s.remove_unused_signatures(),
    verify_authority_problem::unused_signature, account_name_type() );

#undef VERIFY_AUTHORITY_CHECK
#undef VERIFY_AUTHORITY_CHECK_OTHER_AUTH
}

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
  verify_authority_impl( required_authorities, sigs,
    get_active, get_owner, get_posting, get_witness_key,
    max_recursion_depth, max_membership, max_account_auths,
    active_approvals, owner_approvals, posting_approvals,
    [&]( const char* checked_expr, verify_authority_problem problem, const account_name_type& id )
    {
#define VERIFY_AUTHORITY_THROW( EX_TYPE, ... )                        \
FC_EXPAND_MACRO(                                                      \
  FC_MULTILINE_MACRO_BEGIN                                            \
    FC_ASSERT_EXCEPTION_DECL( checked_expr, EX_TYPE, "" __VA_ARGS__ ) \
    throw __e__;                                                      \
  FC_MULTILINE_MACRO_END                                              \
)
      switch( problem )
      {
      case verify_authority_problem::mixed_with_posting:
        VERIFY_AUTHORITY_THROW( fc::assert_exception,
          "Transactions with operations required posting authority cannot be combined "
          "with transactions requiring active or owner authority." );
      case verify_authority_problem::missing_posting:
        VERIFY_AUTHORITY_THROW( tx_missing_posting_auth,
          "Missing Posting Authority ${id}", ( id )( "posting", get_posting( id ) )
          ( "active", get_active( id ) )( "owner", get_owner( id ) ) );
      case verify_authority_problem::missing_active:
        VERIFY_AUTHORITY_THROW( tx_missing_active_auth,
          "Missing Active Authority ${id}", ( id )
          ( "auth", get_active( id ) )( "owner", get_owner( id ) ) );
      case verify_authority_problem::missing_owner:
        VERIFY_AUTHORITY_THROW( tx_missing_owner_auth,
          "Missing Owner Authority ${id}", ( id )( "auth", get_owner( id ) ) );
      case verify_authority_problem::missing_witness:
        VERIFY_AUTHORITY_THROW( tx_missing_witness_auth,
          "Missing Witness Authority ${id}, key ${signing_key}", ( id )
          ( "signing_key", get_witness_key(id) ) );
      case verify_authority_problem::missing_witness_key:
        VERIFY_AUTHORITY_THROW( tx_missing_witness_auth,
          "Missing Witness Authority ${id}", ( id ) );
      case verify_authority_problem::unused_signature:
        VERIFY_AUTHORITY_THROW( tx_irrelevant_sig,
          "Unnecessary signature(s) detected" );
      }
    },
    []( const char* checked_expr, const authority& auth )
    {
      VERIFY_AUTHORITY_THROW( tx_missing_other_auth, "Missing Authority", ( "auth", auth ) );
    } );

#undef VERIFY_AUTHORITY_THROW
} FC_CAPTURE_AND_RETHROW((sigs)) }

bool has_authorization( const required_authorities_type& required_authorities,
  const flat_set<public_key_type>& sigs,
  const authority_getter& get_active,
  const authority_getter& get_owner,
  const authority_getter& get_posting,
  const witness_public_key_getter& get_witness_key )
{
  bool result = true;
  verify_authority_impl( required_authorities, sigs,
    get_active, get_owner, get_posting, get_witness_key,
    HIVE_MAX_SIG_CHECK_DEPTH, HIVE_MAX_AUTHORITY_MEMBERSHIP, HIVE_MAX_SIG_CHECK_ACCOUNTS,
    flat_set<account_name_type>(), flat_set<account_name_type>(), flat_set<account_name_type>(),
    [&]( const char*, verify_authority_problem, const account_name_type& ){ result = false; },
    [&]( const char*, const authority& ){ result = false; } );
  return result;
}

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
