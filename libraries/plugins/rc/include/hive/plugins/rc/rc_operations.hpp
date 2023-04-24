#pragma once
#include <hive/protocol/base.hpp>

#include <hive/chain/evaluator.hpp>

namespace hive { namespace plugins { namespace rc {

using hive::protocol::account_name_type;
using hive::protocol::asset;
using hive::protocol::base_operation;
using hive::protocol::asset_symbol_type;
using hive::protocol::extensions_type;
using hive::protocol::account_name_type;

class rc_plugin;

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
