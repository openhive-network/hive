#include <hive/protocol/transaction_util.hpp>

namespace hive { namespace protocol {

void verify_authority(const required_authorities_type& required_authorities, 
                      const flat_set<public_key_type>& sigs,
                      const authority_getter& get_active,
                      const authority_getter& get_owner,
                      const authority_getter& get_posting,
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
  dupa::log("mario-12");
  if( required_authorities.required_posting.size() ) {
    dupa::log("mario-13");
    FC_ASSERT( required_authorities.required_active.size() == 0 );
    FC_ASSERT( required_authorities.required_owner.size() == 0 );
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

  dupa::log("mario-14");
  flat_set< public_key_type > avail;
  sign_state s(sigs,get_active,avail);
  dupa::log("mario-15");
  s.max_recursion = max_recursion_depth;
  dupa::log("mario-16");
  s.max_membership = max_membership;
  s.max_account_auths = max_account_auths;
  for( auto& id : active_approvals )
    s.approved_by.insert( id );
  dupa::log("mario-17");
  for( auto& id : owner_approvals )
    s.approved_by.insert( id );
  dupa::log("mario-18");

  for( const auto& auth : required_authorities.other )
  {
    dupa::log("mario-19");
    HIVE_ASSERT( s.check_authority(auth), tx_missing_other_auth, "Missing Authority", ("auth",auth)("sigs",sigs) );
    wlog("mario-19a");
  }

  dupa::log("mario-20");
  // fetch all of the top level authorities
  for( const auto& id : required_authorities.required_active )
  {
    dupa::log("mario-21");
    HIVE_ASSERT( s.check_authority(id) ||
                s.check_authority(get_owner(id)),
                tx_missing_active_auth, "Missing Active Authority ${id}", ("id",id)("auth",get_active(id))("owner",get_owner(id)) );
    dupa::log("mario-21a");
  }

  for( const auto& id : required_authorities.required_owner )
  {
    dupa::log("mario-22");
    HIVE_ASSERT( owner_approvals.find(id) != owner_approvals.end() ||
                s.check_authority(get_owner(id)),
                tx_missing_owner_auth, "Missing Owner Authority ${id}", ("id",id)("auth",get_owner(id)) );
    dupa::log("mario-22a");
  }

  HIVE_ASSERT(
    !s.remove_unused_signatures(),
    tx_irrelevant_sig,
    "Unnecessary signature(s) detected"
    );
  dupa::log("mario-23");
} FC_CAPTURE_AND_RETHROW((sigs)) }

} } // hive::protocol
