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
  bool strict_authority_level,
  bool allow_mixed_authorities,
  bool allow_redundant_signatures,
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

  sign_state s( sigs, get_posting, { max_recursion_depth, max_membership, max_account_auths } );

  if( not required_authorities.required_posting.empty() )
  {
    if( !allow_mixed_authorities )
    {
    /**
      *  Up to HF28 transactions with operations required posting authority cannot be combined
      *  with transactions requiring active or owner authority. This is for ease of
      *  implementation. Future versions of authority verification may be able to
      *  check for the merged authority of active and posting.
      */
      VERIFY_AUTHORITY_CHECK( required_authorities.required_active.empty() &&
        required_authorities.required_owner.empty() &&
        required_authorities.required_witness.empty() &&
        required_authorities.other.empty(),
        verify_authority_problem::mixed_with_posting, account_name_type() );
    }

    s.add_approved( posting_approvals );

    for( const auto& id : required_authorities.required_posting )
    {
      if( strict_authority_level )
      {
        VERIFY_AUTHORITY_CHECK( s.check_authority( id ), verify_authority_problem::missing_posting, id );
      }
      else
      {
        VERIFY_AUTHORITY_CHECK( s.check_authority( id ) || s.check_authority( get_active( id ) ) ||
          s.check_authority( get_owner( id ) ), verify_authority_problem::missing_posting, id );
      }
    }

    if( !allow_redundant_signatures )
    {
      VERIFY_AUTHORITY_CHECK( !s.remove_unused_signatures(),
        verify_authority_problem::unused_signature, account_name_type() );
    }
    if( !allow_mixed_authorities )
    {
      return;
    }
  }

  s.change_current_authority( get_active );

  s.init_approved();
  s.add_approved( active_approvals );
  s.add_approved( owner_approvals );;

  for( const auto& auth : required_authorities.other )
  {
    VERIFY_AUTHORITY_CHECK_OTHER_AUTH( s.check_authority( auth ), auth );
  }

  // fetch all of the top level authorities
  for( const auto& id : required_authorities.required_active )
  {
    if( strict_authority_level )
    {
      VERIFY_AUTHORITY_CHECK( s.check_authority( id ),
        verify_authority_problem::missing_active, id );
    }
    else
    {
      VERIFY_AUTHORITY_CHECK( s.check_authority( id ) || s.check_authority( get_owner( id ) ),
        verify_authority_problem::missing_active, id );
    }
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

  if( !allow_redundant_signatures )
  {
    VERIFY_AUTHORITY_CHECK( !s.remove_unused_signatures(),
      verify_authority_problem::unused_signature, account_name_type() );
  }

#undef VERIFY_AUTHORITY_CHECK
#undef VERIFY_AUTHORITY_CHECK_OTHER_AUTH
}

void verify_authority(bool strict_authority_level,
                      bool allow_mixed_authorities,
                      bool allow_redundant_signatures,
                      const required_authorities_type& required_authorities,
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
  verify_authority_impl( strict_authority_level, allow_mixed_authorities, allow_redundant_signatures, required_authorities, sigs,
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
        VERIFY_AUTHORITY_THROW( tx_combined_auths_with_posting,
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

bool has_authorization( bool strict_authority_level,
  bool allow_mixed_authorities,
  bool allow_redundant_signatures,
  const required_authorities_type& required_authorities,
  const flat_set<public_key_type>& sigs,
  const authority_getter& get_active,
  const authority_getter& get_owner,
  const authority_getter& get_posting,
  const witness_public_key_getter& get_witness_key )
{
  bool result = true;
  verify_authority_impl( strict_authority_level, allow_mixed_authorities, allow_redundant_signatures, required_authorities, sigs,
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

void signing_keys_collector::use_active_authority( const account_name_type& account_name )
{
  automatic_detection = false;
  use_active.emplace( account_name );
  // note: we are not enforcing rule that says posting and active/owner are mutually exclusive
}

void signing_keys_collector::use_owner_authority( const account_name_type& account_name )
{
  automatic_detection = false;
  use_owner.emplace( account_name );
  // note: we are not enforcing rule that says posting and active/owner are mutually exclusive
}

void signing_keys_collector::use_posting_authority( const account_name_type& account_name )
{
  automatic_detection = false;
  use_posting.emplace( account_name );
  // note: we are not enforcing rule that says posting and active/owner are mutually exclusive
}

void signing_keys_collector::use_automatic_authority()
{
  automatic_detection = true;
  use_active.clear();
  use_owner.clear();
  use_posting.clear();
}

void signing_keys_collector::collect_signing_keys( flat_set< public_key_type >* keys, const transaction& tx )
{
  flat_set< account_name_type > req_active_approvals;
  flat_set< account_name_type > req_owner_approvals;
  flat_set< account_name_type > req_posting_approvals;
  flat_set< account_name_type > req_witness_approvals;
  vector< authority > other_auths;

  if( automatic_detection )
  {
    tx.get_required_authorities( req_active_approvals, req_owner_approvals, req_posting_approvals, req_witness_approvals, other_auths );
  }
  else
  {
    req_active_approvals.insert( use_active.begin(), use_active.end() );
    req_owner_approvals.insert( use_owner.begin(), use_owner.end() );
    req_posting_approvals.insert( use_posting.begin(), use_posting.end() );
  }

  for( const auto& auth : other_auths )
    for( const auto& a : auth.account_auths )
      req_active_approvals.insert( a.first );

  // std::merge lets us de-duplicate account_id's that occur in both sets, and dump them into
  // a vector at the same time
  vector< account_name_type > v_approving_accounts;
  std::merge( req_active_approvals.begin(), req_active_approvals.end(),
    req_owner_approvals.begin(), req_owner_approvals.end(),
    std::back_inserter( v_approving_accounts ) );
  // in correct case above and below are mutually exclusive (at least for now, since mixing posting
  // with active/owner is forbidden), but we are not enforcing it here
  for( const auto& a : req_posting_approvals )
    v_approving_accounts.push_back( a );

  /// TODO: handle the op that must be signed using witness keys

  prepare_account_authority_data( v_approving_accounts );

  auto get_authorities = [&]( flat_set< account_name_type > authorities_names,
    authority::classification type ) -> std::vector< account_name_type >
  {
    std::vector< account_name_type > v_authorities_names_next_level, all_extra_approving_accounts;

    uint32_t depth = 1;
    while( depth <= HIVE_MAX_SIG_CHECK_DEPTH )
    {
      flat_set< account_name_type > authorities_names_next_level;

      for( const auto& name : authorities_names )
      {
        const authority* current_auth = nullptr;
        switch( type )
        {
        case authority::owner: current_auth = &get_owner( name ); break;
        case authority::active: current_auth = &get_active( name ); break;
        case authority::posting: current_auth = &get_posting( name ); break;
        }
        for( const auto& kv : current_auth->account_auths )
          authorities_names_next_level.insert( kv.first );
      }

      if( authorities_names_next_level.empty() )
        break;

      for( const auto& name : authorities_names_next_level )
      {
        v_authorities_names_next_level.emplace_back( name );
        all_extra_approving_accounts.emplace_back( name );
      }
      authorities_names = std::move( authorities_names_next_level );

      prepare_account_authority_data( v_authorities_names_next_level );
      ++depth;
      if( type != authority::posting )
        type = authority::active;
    }

    return all_extra_approving_accounts;
  };

  if( automatic_detection )
  {
    auto additional_approving_accounts = get_authorities( req_active_approvals, authority::active );
    req_active_approvals.insert( additional_approving_accounts.begin(), additional_approving_accounts.end() );
    additional_approving_accounts = get_authorities( req_owner_approvals, authority::owner );
    req_active_approvals.insert( additional_approving_accounts.begin(), additional_approving_accounts.end() );
    additional_approving_accounts = get_authorities( req_posting_approvals, authority::posting );
    req_posting_approvals.insert( additional_approving_accounts.begin(), additional_approving_accounts.end() );
  }

  for( const auto& name : req_active_approvals )
  {
    auto v_approving_keys = get_active( name ).get_keys();
    wdump( ( v_approving_keys ) );
    keys->insert( v_approving_keys.begin(), v_approving_keys.end() );
  }
  for( const auto& name : req_owner_approvals )
  {
    auto v_approving_keys = get_owner( name ).get_keys();
    wdump( ( v_approving_keys ) );
    keys->insert( v_approving_keys.begin(), v_approving_keys.end() );
  }

  for( const auto& name : req_posting_approvals )
  {
    auto v_approving_keys = get_posting( name ).get_keys();
    wdump( ( v_approving_keys ) );
    keys->insert( v_approving_keys.begin(), v_approving_keys.end() );
  }

  for( const authority& a : other_auths )
  {
    for( const auto& kv : a.key_auths )
    {
      wdump( ( kv.first ) );
      keys->insert( kv.first );
    }
  }
}

} } // hive::protocol
