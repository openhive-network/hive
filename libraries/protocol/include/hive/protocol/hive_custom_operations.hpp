#pragma once

#include <hive/protocol/base.hpp>
#include <fc/static_variant.hpp>

namespace hive { namespace protocol {

struct follow_operation : public base_operation
{
  account_name_type   follower;
  account_name_type   following;
  std::set< string >  what; /// blog, mute

  void validate()const;
  void get_required_posting_authorities( flat_set<account_name_type>& a )const { a.insert( follower ); }
};

struct reblog_operation : public base_operation
{
  account_name_type account;
  account_name_type author;
  string            permlink;

  void validate()const;
  void get_required_posting_authorities( flat_set<account_name_type>& a )const { a.insert( account ); }
};

struct delegate_rc_operation : public base_operation
{
  account_name_type             from;
  flat_set< account_name_type > delegatees;
  int64_t                       max_rc = 0;
  extensions_type               extensions;

  void validate()const;
  void get_required_posting_authorities( flat_set<account_name_type>& a )const { a.insert( from ); }
};

typedef fc::static_variant<
  delegate_rc_operation
> rc_custom_operation;

#define HIVE_RC_CUSTOM_OPERATION_ID "rc"

} } // hive::protocol


FC_REFLECT( hive::protocol::delegate_rc_operation,
  (from)
  (delegatees)
  (max_rc)
  (extensions)
  )

FC_REFLECT( hive::protocol::follow_operation, (follower)(following)(what) )
FC_REFLECT( hive::protocol::reblog_operation, (account)(author)(permlink) )

FC_REFLECT_TYPENAME( hive::protocol::rc_custom_operation )