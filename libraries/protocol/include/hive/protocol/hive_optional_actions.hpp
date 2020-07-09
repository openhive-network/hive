#pragma once
#include <hive/protocol/types.hpp>
#include <hive/protocol/base.hpp>

namespace hive { namespace protocol {

#ifdef HIVE_ENABLE_SMT
  struct example_optional_action : public base_operation
  {
    account_name_type account;

    void validate()const;
    void get_required_active_authorities( flat_set<account_name_type>& a )const{ a.insert(account); }
  };
#endif

} } // hive::protocol

#ifdef HIVE_ENABLE_SMT
FC_REFLECT( hive::protocol::example_optional_action, (account) )
#endif
