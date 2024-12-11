#pragma once
#include <hive/protocol/sign_state.hpp>
#include <hive/protocol/exceptions.hpp>

namespace hive { namespace protocol {

template< typename AuthContainerType >
required_authorities_type get_required_authorities(const vector<AuthContainerType>& auth_containers)
{ try {
  required_authorities_type result;
  hive::protocol::get_required_auth_visitor auth_visitor(result.required_active, result.required_owner, 
                                                         result.required_posting, result.required_witness, result.other);

  for( const auto& a : auth_containers )
    auth_visitor( a );

  return result;
} FC_CAPTURE_AND_RETHROW((auth_containers)) }

void verify_authority(bool allow_strict_and_mixed_authorities,
                      bool allow_redundant_signatures,
                      const required_authorities_type& required_authorities,
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

bool has_authorization( bool allow_strict_and_mixed_authorities,
                        bool allow_redundant_signatures,
                        const required_authorities_type& required_authorities,
                        const flat_set<public_key_type>& sigs,
                        const authority_getter& get_active,
                        const authority_getter& get_owner,
                        const authority_getter& get_posting,
                        const witness_public_key_getter& get_witness_key );

template< typename AuthContainerType >
void verify_authority(bool allow_strict_and_mixed_authorities,
                      bool allow_redundant_signatures,
                      const vector<AuthContainerType>& auth_containers, 
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
  verify_authority(allow_strict_and_mixed_authorities,
                   allow_redundant_signatures,
                   get_required_authorities(auth_containers),
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

/**
 * Class that encapsulates process of detecting which keys need to be used for signing transaction.
 */
class signing_keys_collector
{
public:
  virtual ~signing_keys_collector() {}

  // disables automatic detection and adds given account's active authority to set of used authorities
  void use_active_authority( const account_name_type& account_name );
  // disables automatic detection and adds given account's owner authority to set of used authorities
  void use_owner_authority( const account_name_type& account_name );
  // disables automatic detection and adds given account's posting authority to set of used authorities
  void use_posting_authority( const account_name_type& account_name );
  // clears set of used authorities and enables automatic detection
  void use_automatic_authority();

  bool automatic_detection_enabled() const { return automatic_detection; }

  virtual void collect_signing_keys( flat_set< public_key_type >* keys, const transaction& tx );

  // called before get_active/get_owner/get_posting for each given account
  virtual void prepare_account_authority_data( const std::vector< account_name_type >& accounts ) = 0;

  virtual const authority& get_active( const account_name_type& account_name ) const = 0;
  virtual const authority& get_owner( const account_name_type& account_name ) const = 0;
  virtual const authority& get_posting( const account_name_type& account_name ) const = 0;

protected:
  flat_set<account_name_type> use_active;
  flat_set<account_name_type> use_owner;
  flat_set<account_name_type> use_posting;
  bool automatic_detection = true;
};

} } // hive::protocol

