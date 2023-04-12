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


/*

account current balances (specific to all tokens: 
hive, 
hbd, 
vests)
It should include also 
saving balance and 
reward balance


FC_REFLECT( hive::plugins::database_api::api_account_object,
          (id)(name)(owner)(active)(posting)(memo_key)(json_metadata)(posting_json_metadata)(proxy)(previous_owner_update)(last_owner_update)(last_account_update)
          (created)(mined)
          (recovery_account)(last_account_recovery)(reset_account)
          (comment_count)(lifetime_vote_count)(post_count)(can_vote)(voting_manabar)(downvote_manabar)
          (balance)
          (savings_balance)
          (hbd_balance)(hbd_seconds)(hbd_seconds_last_update)(hbd_last_interest_payment)
          (savings_hbd_balance)(savings_hbd_seconds)(savings_hbd_seconds_last_update)(savings_hbd_last_interest_payment)(savings_withdraw_requests)
          (reward_hbd_balance)(reward_hive_balance)(reward_vesting_balance)(reward_vesting_hive)
          (vesting_shares)(delegated_vesting_shares)(received_vesting_shares)(vesting_withdraw_rate)(post_voting_power)(next_vesting_withdrawal)(withdrawn)(to_withdraw)(withdraw_routes)
          (pending_transfers)(curation_rewards)
          (posting_rewards)
          (proxied_vsf_votes)(witnesses_voted_for)
          (last_post)(last_root_post)(last_post_edit)(last_vote_time)
          (post_bandwidth)(pending_claimed_accounts)(open_recurrent_transfers)
          (is_smt)
          (delayed_votes)
          (governance_vote_expiration_ts)
        )


    //account_id_type
      uint32_t  id;

  account_name_type name;
  authority         owner;
  authority         active;
  authority         posting;
  public_key_type   memo_key;
  string            json_metadata;
  string            posting_json_metadata;
  account_name_type proxy;

  time_point_sec    previous_owner_update;
  time_point_sec    last_owner_update;
  time_point_sec    last_account_update;

  time_point_sec    created;
  bool              mined = false;
  account_name_type recovery_account;
  account_name_type reset_account;
  time_point_sec    last_account_recovery;
  uint32_t          comment_count = 0;
  uint32_t          lifetime_vote_count = 0;
  uint32_t          post_count = 0;

  bool              can_vote = false;
  util::manabar     voting_manabar;
  util::manabar     downvote_manabar;

  asset             balance;
  asset             savings_balance;

  asset             hbd_balance;
  uint128_t         hbd_seconds = 0;
  time_point_sec    hbd_seconds_last_update;
  time_point_sec    hbd_last_interest_payment;

  asset             savings_hbd_balance;
  uint128_t         savings_hbd_seconds = 0;
  time_point_sec    savings_hbd_seconds_last_update;
  time_point_sec    savings_hbd_last_interest_payment;

  uint8_t           savings_withdraw_requests = 0;

  asset             reward_hbd_balance;
  asset             reward_hive_balance;
  asset             reward_vesting_balance;
  asset             reward_vesting_hive;

  share_type        curation_rewards;
  share_type        posting_rewards;

  asset             vesting_shares;
  asset             delegated_vesting_shares;
  asset             received_vesting_shares;
  asset             vesting_withdraw_rate;
  
  asset             post_voting_power;

  time_point_sec    next_vesting_withdrawal;
  share_type        withdrawn;
  share_type        to_withdraw;
  uint16_t          withdraw_routes = 0;
  uint16_t          pending_transfers = 0;

  vector< share_type > proxied_vsf_votes;

  uint16_t          witnesses_voted_for = 0;

  time_point_sec    last_post;
  time_point_sec    last_root_post;
  time_point_sec    last_post_edit;
  time_point_sec    last_vote_time;
  uint32_t          post_bandwidth = 0;

  share_type        pending_claimed_accounts = 0;
  uint16_t          open_recurrent_transfers = 0;

  bool              is_smt = false;

  fc::optional< vector< delayed_votes_data > >  delayed_votes;
  time_point_sec governance_vote_expiration_ts;
*/

typedef std::vector<collected_account_balances_t> collected_account_balances_collection_t;
collected_account_balances_collection_t collect_current_all_accounts_balances(const char* context);
int get_expected_block_num_impl(const char* context);
int consume_json_block_impl(const char *json_block, const char *context, int block_num);
int consume_variant_block_impl(const fc::variant& v, const char* context, int block_num);
void cab_destroy_C_impl(const char* context);


bool is_keyauths_operation( const protocol::operation& op );

bool is_metadata_operation( const protocol::operation& op );

void grab_pqxx_blocks_impl(int from, int to, const char *context, const char *postgres_url);


} } // hive::app
