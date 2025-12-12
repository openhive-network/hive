#include <hive/chain/hardfork_property_object_multiindex.hpp>
#include <hive/chain/detail/state/liquidity_reward_balance_object_multiindex.hpp>
#include <hive/chain/detail/state/withdraw_vesting_route_object_multiindex.hpp>

#include <hive/chain/index.hpp>
#include <chainbase/chainbase.inl>

#include <hive/chain/util/type_registrar_definition.hpp>

namespace hive { namespace chain {

void initialize_core_indexes_06( database& db )
{
  HIVE_ADD_CORE_INDEX(db, liquidity_reward_balance_index);
  HIVE_ADD_CORE_INDEX(db, hardfork_property_index);
  HIVE_ADD_CORE_INDEX(db, withdraw_vesting_route_index);
}

} }

HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::liquidity_reward_balance_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::hardfork_property_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::withdraw_vesting_route_index)

// Explicit template instantiations for chainbase::database methods
template const chainbase::generic_index<hive::chain::liquidity_reward_balance_index>& chainbase::database::get_index<hive::chain::liquidity_reward_balance_index>() const;
template chainbase::generic_index<hive::chain::liquidity_reward_balance_index>& chainbase::database::get_mutable_index<hive::chain::liquidity_reward_balance_index>();

template const chainbase::generic_index<hive::chain::hardfork_property_index>& chainbase::database::get_index<hive::chain::hardfork_property_index>() const;
template chainbase::generic_index<hive::chain::hardfork_property_index>& chainbase::database::get_mutable_index<hive::chain::hardfork_property_index>();

template const chainbase::generic_index<hive::chain::withdraw_vesting_route_index>& chainbase::database::get_index<hive::chain::withdraw_vesting_route_index>() const;
template chainbase::generic_index<hive::chain::withdraw_vesting_route_index>& chainbase::database::get_mutable_index<hive::chain::withdraw_vesting_route_index>();
