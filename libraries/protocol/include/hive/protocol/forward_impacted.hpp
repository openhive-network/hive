#pragma once

#include <fc/container/flat.hpp>
#include <hive/protocol/operations.hpp>
#include <hive/protocol/transaction.hpp>

#include <fc/string.hpp>

#include <vector>

namespace hive { namespace app {

void operation_get_impacted_accounts(
  const hive::protocol::operation& op,
  fc::flat_set<protocol::account_name_type>& result );

void operation_get_owner_impacted_accounts(
  const hive::protocol::operation& op,
  fc::flat_set<protocol::account_name_type>& result,
  fc::flat_set<protocol::account_name_type>& result2 );

typedef std::set<std::string> stringset;

struct collected_metadata_t
{
  std::string account_name;
  std::string json_metadata;
  std::string posting_json_metadata;
};

typedef std::vector<collected_metadata_t> collected_metadata_collection_t;
collected_metadata_collection_t operation_get_metadata(const hive::protocol::operation& op, const bool is_hardfork_21);

typedef std::vector<std::pair<protocol::account_name_type, protocol::asset>> impacted_balance_data;
impacted_balance_data operation_get_impacted_balances(const hive::protocol::operation& op, const bool is_hf01);
stringset get_operations_used_in_get_balance_impacting_operations();

protocol::account_name_type get_created_from_account_create_operations( const protocol::operation& op );

void transaction_get_impacted_accounts(
  const hive::protocol::transaction& tx,
  fc::flat_set<protocol::account_name_type>& result
  );

// Helper for metatdata and keyayuth collectors
template <typename T>
void exclude_from_used_operations(hive::app::stringset& used_operations)
{
  used_operations.erase(fc::get_typename<T>::name());
}

} } // hive::app
