#include <hive/chain/account_object_multiindex.hpp>
#include <hive/chain/detail/state/reward_fund_object_multiindex.hpp>

#include <hive/chain/index.hpp>
#include <chainbase/chainbase.inl>

#include <hive/chain/util/type_registrar_definition.hpp>

namespace hive { namespace chain {

void initialize_core_indexes_09( database& db )
{
  HIVE_ADD_CORE_INDEX(db, reward_fund_index);
  HIVE_ADD_CORE_INDEX(db, vesting_delegation_index);
  HIVE_ADD_CORE_INDEX(db, vesting_delegation_expiration_index);
}

} }

HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::reward_fund_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::vesting_delegation_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::vesting_delegation_expiration_index)

// Explicit template instantiations for chainbase::database methods
template const chainbase::generic_index<hive::chain::reward_fund_index>& chainbase::database::get_index<hive::chain::reward_fund_index>() const;
template chainbase::generic_index<hive::chain::reward_fund_index>& chainbase::database::get_mutable_index<hive::chain::reward_fund_index>();

template const chainbase::generic_index<hive::chain::vesting_delegation_index>& chainbase::database::get_index<hive::chain::vesting_delegation_index>() const;
template chainbase::generic_index<hive::chain::vesting_delegation_index>& chainbase::database::get_mutable_index<hive::chain::vesting_delegation_index>();

template const chainbase::generic_index<hive::chain::vesting_delegation_expiration_index>& chainbase::database::get_index<hive::chain::vesting_delegation_expiration_index>() const;
template chainbase::generic_index<hive::chain::vesting_delegation_expiration_index>& chainbase::database::get_mutable_index<hive::chain::vesting_delegation_expiration_index>();