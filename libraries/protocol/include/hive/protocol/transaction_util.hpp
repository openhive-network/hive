#pragma once
#include <hive/protocol/sign_state.hpp>
#include <hive/protocol/exceptions.hpp>

namespace hive { namespace protocol {

struct required_authorities_type
{
  flat_set<hive::protocol::account_name_type> required_active;
  flat_set<hive::protocol::account_name_type> required_owner;
  flat_set<hive::protocol::account_name_type> required_posting;
  flat_set<hive::protocol::account_name_type> required_witness;
  vector<hive::protocol::authority> other;
};

template< typename AuthContainerType >
required_authorities_type get_required_authorities(const vector<AuthContainerType>& auth_containers)
{ try {
  required_authorities_type result;
  flat_set<hive::protocol::account_name_type> required_active;
  flat_set<hive::protocol::account_name_type> required_owner;
  flat_set<hive::protocol::account_name_type> required_posting;
  flat_set<hive::protocol::account_name_type> required_witness;
  vector<hive::protocol::authority> other;

  hive::protocol::get_required_auth_visitor auth_visitor(result.required_active, result.required_owner, 
                                                         result.required_posting, result.required_witness, result.other);

  for( const auto& a : auth_containers )
    auth_visitor( a );

  return result;
} FC_CAPTURE_AND_RETHROW((auth_containers)) }

void verify_authority(const required_authorities_type& required_authorities, 
                      const flat_set<public_key_type>& sigs,
                      const authority_getter& get_active,
                      const authority_getter& get_owner,
                      const authority_getter& get_posting,
                      const witness_public_key_getter& get_witness_key,
                      uint32_t max_recursion_depth = HIVE_MAX_SIG_CHECK_DEPTH,
                      uint32_t max_membership = HIVE_MAX_AUTHORITY_MEMBERSHIP,
                      uint32_t max_account_auths = HIVE_MAX_SIG_CHECK_ACCOUNTS,
                      bool allow_committe = false,
                      const flat_set<account_name_type>& active_approvals = flat_set<account_name_type>(),
                      const flat_set<account_name_type>& owner_approvals = flat_set<account_name_type>(),
                      const flat_set<account_name_type>& posting_approvals = flat_set<account_name_type>());

template< typename AuthContainerType >
void verify_authority(const vector<AuthContainerType>& auth_containers, 
                      const flat_set<public_key_type>& sigs,
                      const authority_getter& get_active,
                      const authority_getter& get_owner,
                      const authority_getter& get_posting,
                      const witness_public_key_getter& get_witness_key,
                      uint32_t max_recursion_depth = HIVE_MAX_SIG_CHECK_DEPTH,
                      uint32_t max_membership = HIVE_MAX_AUTHORITY_MEMBERSHIP,
                      uint32_t max_account_auths = HIVE_MAX_SIG_CHECK_ACCOUNTS,
                      bool allow_committe = false,
                      const flat_set<account_name_type>& active_approvals = flat_set<account_name_type>(),
                      const flat_set<account_name_type>& owner_approvals = flat_set<account_name_type>(),
                      const flat_set<account_name_type>& posting_approvals = flat_set<account_name_type>())
{ 
  verify_authority(get_required_authorities(auth_containers),
                   sigs,
                   get_active,
                   get_owner,
                   get_posting,
                   get_witness_key,
                   max_recursion_depth,
                   max_membership,
                   max_account_auths,
                   allow_committe,
                   active_approvals,
                   owner_approvals,
                   posting_approvals);
}

/**
 * Builds public keys paired with private keys generated through typical methods of deriving key from given string.
 * Caller can use the collection to test it against actual keys associated with the account in order to detect leak,
 * block user action and warn about the problem.
 */
void collect_potential_keys( std::vector< public_key_type >* keys, const account_name_type& account, const std::string& str );

struct memo_data
{
  public_key_type from;
  public_key_type to;
  uint64_t        nonce = 0;
  uint32_t        check = 0;
  vector<char>    encrypted;

  static std::optional<memo_data> from_string( std::string str );
  operator std::string() const;
};

/**
 * Encrypts given memo (it has to start with #).
 */
std::string encrypt_memo( const private_key_type& from_priv, const public_key_type& to_key, std::string memo );

/**
 * Decrypts given memo (it has to start with #).
 */
std::string decrypt_memo( std::function<fc::optional<private_key_type>( const public_key_type& )> key_finder, std::string encrypted_memo );

} } // hive::protocol

FC_REFLECT( hive::protocol::memo_data, (from)(to)(nonce)(check)(encrypted) )
