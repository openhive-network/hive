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


struct collected_metadata_t
{
  std::string account_name;
  std::string json_metadata;
  std::string posting_json_metadata;
};

typedef std::vector<collected_metadata_t> collected_metadata_collection_t;
collected_metadata_collection_t operation_get_metadata(const hive::protocol::operation& op);
stringset get_operations_used_in_get_metadata();



typedef std::vector<std::pair<protocol::account_name_type, protocol::asset>> impacted_balance_data;
impacted_balance_data operation_get_impacted_balances(const hive::protocol::operation& op, const bool is_hf01);
stringset get_operations_used_in_get_balance_impacting_operations();

protocol::account_name_type get_created_from_account_create_operations( const protocol::operation& op );

void transaction_get_impacted_accounts(
  const hive::protocol::transaction& tx,
  fc::flat_set<protocol::account_name_type>& result
  );

struct collected_account_balances_t
{
  std::string account_name;
  long long balance;
  long long hbd_balance;
  long long vesting_shares;
  long long savings_hbd_balance;
  long long reward_hbd_balance;
};


typedef std::vector<collected_account_balances_t> collected_account_balances_collection_t;
collected_account_balances_collection_t collect_current_all_accounts_balances(const char* context);


bool is_keyauths_operation( const protocol::operation& op );

bool is_metadata_operation( const protocol::operation& op );

int consensus_state_provider_get_expected_block_num_impl(const char* context, const char* shared_memory_bin_path);
int consume_variant_block_impl(const fc::variant& v, const char* context, int block_num, const char* shared_memory_bin_path);
void consensus_state_provider_finish_impl(const char* context, const char* shared_memory_bin_path);


} } // hive::app
