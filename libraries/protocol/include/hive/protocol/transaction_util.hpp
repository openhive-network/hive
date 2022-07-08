#pragma once
#include <hive/protocol/sign_state.hpp>
#include <hive/protocol/exceptions.hpp>
#include <thread>

namespace hive { namespace protocol {

struct dupa
{
    static void log( const std::string& message )
    {
      std::ostringstream ss;

      ss << std::this_thread::get_id();

      std::string idstr = ss.str();

      wlog( "${a} ${b}", ("a", message.c_str())("b", idstr.c_str()) );
    }
};

struct required_authorities_type
{
  flat_set<hive::protocol::account_name_type> required_active;
  flat_set<hive::protocol::account_name_type> required_owner;
  flat_set<hive::protocol::account_name_type> required_posting;
  vector<hive::protocol::authority> other;
};

template< typename AuthContainerType >
required_authorities_type get_required_authorities(const vector<AuthContainerType>& auth_containers)
{ try {
  required_authorities_type result;
  flat_set<hive::protocol::account_name_type> required_active;
  flat_set<hive::protocol::account_name_type> required_owner;
  flat_set<hive::protocol::account_name_type> required_posting;
  vector<hive::protocol::authority> other;

  hive::protocol::get_required_auth_visitor auth_visitor(result.required_active, result.required_owner, 
                                                         result.required_posting, result.other);

  for( const auto& a : auth_containers )
    auth_visitor( a );

  return result;
} FC_CAPTURE_AND_RETHROW((auth_containers)) }

void verify_authority(const required_authorities_type& required_authorities, 
                      const flat_set<public_key_type>& sigs,
                      const authority_getter& get_active,
                      const authority_getter& get_owner,
                      const authority_getter& get_posting,
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
                   max_recursion_depth,
                   max_membership,
                   max_account_auths,
                   allow_committe,
                   active_approvals,
                   owner_approvals,
                   posting_approvals);
}

} } // hive::protocol
