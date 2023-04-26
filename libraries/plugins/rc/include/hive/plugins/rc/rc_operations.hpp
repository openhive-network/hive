#pragma once
#include <hive/protocol/base.hpp>

#include <hive/chain/evaluator.hpp>
#include <hive/chain/hive_object_types.hpp>

namespace hive { namespace chain {
class account_object;
class database;
class remove_guard;
} }

namespace hive { namespace plugins { namespace rc {

using hive::protocol::account_name_type;
using hive::protocol::asset;
using hive::protocol::base_operation;
using hive::protocol::asset_symbol_type;
using hive::protocol::extensions_type;
using hive::chain::account_id_type;
using hive::chain::account_object;
using hive::chain::database;
using hive::chain::remove_guard;

class rc_plugin;

void update_account_after_rc_delegation( database& _db, const account_object& account, uint32_t now, int64_t delta, bool regenerate_mana = false );
bool has_expired_delegation( const database& _db, const account_object& account );
void remove_delegations( database& _db, int64_t& delegation_overflow, account_id_type delegator_id, uint32_t now, remove_guard& obj_perf );

struct delegate_rc_operation : base_operation
{
  account_name_type     from;
  flat_set< account_name_type > delegatees;
  int64_t              max_rc = 0;
  extensions_type       extensions;

  void validate()const;
  void get_required_posting_authorities( flat_set<account_name_type>& a )const { a.insert( from ); }
};

typedef fc::static_variant<
        delegate_rc_operation
  > rc_custom_operation;

HIVE_DEFINE_PLUGIN_EVALUATOR( rc_plugin, rc_custom_operation, delegate_rc );

} } } // hive::plugins::rc

FC_REFLECT( hive::plugins::rc::delegate_rc_operation,
  (from)
  (delegatees)
  (max_rc)
  (extensions)
  )

HIVE_DECLARE_OPERATION_TYPE( hive::plugins::rc::rc_custom_operation )
FC_REFLECT_TYPENAME( hive::plugins::rc::rc_custom_operation )
