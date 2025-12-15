#include <hive/chain/smt_objects.hpp>

#include <hive/chain/index.hpp>
#include <chainbase/chainbase.inl>

#include <hive/chain/util/type_registrar_definition.hpp>

namespace hive { namespace chain {

void initialize_core_indexes_10( database& db )
{
#ifdef HIVE_ENABLE_SMT
  HIVE_ADD_CORE_INDEX(db, smt_token_index);
  HIVE_ADD_CORE_INDEX(db, account_regular_balance_index);
  HIVE_ADD_CORE_INDEX(db, account_rewards_balance_index);
  HIVE_ADD_CORE_INDEX(db, nai_pool_index);
  HIVE_ADD_CORE_INDEX(db, smt_token_emissions_index);
  HIVE_ADD_CORE_INDEX(db, smt_contribution_index);
  HIVE_ADD_CORE_INDEX(db, smt_ico_index);
#endif
}

} }

#ifdef HIVE_ENABLE_SMT
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::smt_token_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::account_regular_balance_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::account_rewards_balance_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::nai_pool_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::smt_token_emissions_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::smt_contribution_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::smt_ico_index)

// Explicit template instantiations for chainbase::database methods
template const chainbase::generic_index<hive::chain::smt_token_index>& chainbase::database::get_index<hive::chain::smt_token_index>() const;
template chainbase::generic_index<hive::chain::smt_token_index>& chainbase::database::get_mutable_index<hive::chain::smt_token_index>();

template const chainbase::generic_index<hive::chain::account_regular_balance_index>& chainbase::database::get_index<hive::chain::account_regular_balance_index>() const;
template chainbase::generic_index<hive::chain::account_regular_balance_index>& chainbase::database::get_mutable_index<hive::chain::account_regular_balance_index>();

template const chainbase::generic_index<hive::chain::account_rewards_balance_index>& chainbase::database::get_index<hive::chain::account_rewards_balance_index>() const;
template chainbase::generic_index<hive::chain::account_rewards_balance_index>& chainbase::database::get_mutable_index<hive::chain::account_rewards_balance_index>();

template const chainbase::generic_index<hive::chain::nai_pool_index>& chainbase::database::get_index<hive::chain::nai_pool_index>() const;
template chainbase::generic_index<hive::chain::nai_pool_index>& chainbase::database::get_mutable_index<hive::chain::nai_pool_index>();

template const chainbase::generic_index<hive::chain::smt_token_emissions_index>& chainbase::database::get_index<hive::chain::smt_token_emissions_index>() const;
template chainbase::generic_index<hive::chain::smt_token_emissions_index>& chainbase::database::get_mutable_index<hive::chain::smt_token_emissions_index>();

template const chainbase::generic_index<hive::chain::smt_contribution_index>& chainbase::database::get_index<hive::chain::smt_contribution_index>() const;
template chainbase::generic_index<hive::chain::smt_contribution_index>& chainbase::database::get_mutable_index<hive::chain::smt_contribution_index>();

template const chainbase::generic_index<hive::chain::smt_ico_index>& chainbase::database::get_index<hive::chain::smt_ico_index>() const;
template chainbase::generic_index<hive::chain::smt_ico_index>& chainbase::database::get_mutable_index<hive::chain::smt_ico_index>();
#endif