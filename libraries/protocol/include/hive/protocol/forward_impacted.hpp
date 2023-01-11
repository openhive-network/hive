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


enum class authority_t : std::int32_t
{
  OWNER,
  ACTIVE,
  POSTING,
  WITNESS,
  NEW_OWNER_AUTHORITY,
  RECENT_OWNER_AUTHORITY,
};

struct collected_keyauth_t
{
  std::string key_auth;
  authority_t authority_kind;
  std::string account_name;

};

typedef std::set<std::string> stringset;
typedef std::vector<collected_keyauth_t> collected_keyauth_collection_t;
collected_keyauth_collection_t operation_get_keyauths(const hive::protocol::operation& op);
stringset get_operations_used_in_get_keyauths();

typedef std::vector<std::pair<protocol::account_name_type, protocol::asset>> impacted_balance_data;
impacted_balance_data operation_get_impacted_balances(const hive::protocol::operation& op, const bool is_hf01);
stringset get_operations_used_in_get_balance_impacting_operations();

protocol::account_name_type get_created_from_account_create_operations( const protocol::operation& op );

void transaction_get_impacted_accounts(
  const hive::protocol::transaction& tx,
  fc::flat_set<protocol::account_name_type>& result
  );

} } // hive::app
