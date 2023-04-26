#include <hive/chain/dhf_objects.hpp>
#include <hive/chain/pending_optional_action_object.hpp>
#include <hive/chain/pending_required_action_object.hpp>
#include <hive/chain/smt_objects.hpp>

#include <hive/chain/index.hpp>

#include <hive/chain/util/type_registrar_definition.hpp>

namespace hive { namespace chain {

void initialize_core_indexes_10( database& db )
{
  HIVE_ADD_CORE_INDEX(db, pending_required_action_index);
  HIVE_ADD_CORE_INDEX(db, pending_optional_action_index);
#ifdef HIVE_ENABLE_SMT
  HIVE_ADD_CORE_INDEX(db, smt_token_index);
  HIVE_ADD_CORE_INDEX(db, account_regular_balance_index);
  HIVE_ADD_CORE_INDEX(db, account_rewards_balance_index);
  HIVE_ADD_CORE_INDEX(db, nai_pool_index);
  HIVE_ADD_CORE_INDEX(db, smt_token_emissions_index);
  HIVE_ADD_CORE_INDEX(db, smt_contribution_index);
  HIVE_ADD_CORE_INDEX(db, smt_ico_index);
#endif
  HIVE_ADD_CORE_INDEX(db, proposal_index);
}

} }

HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::pending_required_action_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::pending_optional_action_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::proposal_index)
#ifdef HIVE_ENABLE_SMT
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::smt_token_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::account_regular_balance_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::account_rewards_balance_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::nai_pool_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::smt_token_emissions_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::smt_contribution_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::smt_ico_index)
#endif