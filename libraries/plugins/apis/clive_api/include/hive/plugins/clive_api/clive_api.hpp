#pragma once
#include <hive/plugins/json_rpc/utility.hpp>

#include <hive/protocol/asset.hpp>
#include <hive/protocol/authority.hpp>
#include <hive/protocol/types.hpp>

#include <fc/optional.hpp>

#include <map>
#include <vector>

namespace hive { namespace plugins { namespace clive_api {

struct api_manabar
{
  int64_t  current_mana = 0;
  uint32_t last_update_time = 0;
};

struct decline_voting_rights_request_info
{
  hive::protocol::account_name_type account;
  time_point_sec                    effective_date;
};

struct change_recovery_account_request_info
{
  hive::protocol::account_name_type account_to_recover;
  time_point_sec                    effective_on;
};

struct owner_authority_change_info
{
  hive::protocol::authority previous_owner_authority;
  time_point_sec            last_valid_time;
};

struct collect_dashboard_data_args
{
  /// Set of account names to collect account infos for. If given name is not found, it is skipped and no info is retrieved.
  std::vector<hive::protocol::account_name_type> accounts;
};

struct account_dashboard_data
{
  api_manabar voting_manabar;
  api_manabar downvote_manabar;
  api_manabar rc_manabar;

  hive::protocol::asset      max_rc_creation_adjustment = hive::protocol::asset(0, VESTS_SYMBOL);
  int64_t                    max_rc = 0;
  uint64_t                   delegated_rc = 0;
  uint64_t                   received_delegated_rc = 0;

  hive::protocol::asset      balance;
  hive::protocol::asset      hbd_balance;
  hive::protocol::asset      savings_balance;
  hive::protocol::asset      savings_hbd_balance;
  hive::protocol::asset      reward_hbd_balance;
  hive::protocol::asset      reward_hive_balance;
  hive::protocol::asset      reward_vesting_balance;
  hive::protocol::asset      vesting_shares;
  hive::protocol::asset      delegated_vesting_shares;
  hive::protocol::asset      received_vesting_shares;

  hive::protocol::asset      post_voting_power;

  uint16_t                   open_recurrent_transfers = 0;
  time_point_sec             governance_vote_expiration_ts;

  /// Timestamp of last REAL operation really done by given account
  fc::time_point_sec         last_operation_timestamp;
  hive::protocol::share_type reputation;

  fc::optional<decline_voting_rights_request_info>    decline_voting_rights_request;
  fc::optional< change_recovery_account_request_info> change_recovery_account_request;

  owner_authority_change_info owner_authority_change;
};

struct collect_dashboard_data_return
{
  uint32_t                        head_block_number = 0;
  time_point_sec                  head_block_time;
  hive::protocol::asset           total_vesting_fund_hive;
  hive::protocol::asset           total_vesting_shares;

  /** Holds collected data for any known account which name has been specified at input args.
  */
  std::map<hive::protocol::account_name_type, account_dashboard_data> collected_account_infos;
};

class clive_api final
{
  public:
    clive_api();
    ~clive_api() = default;

    /** Allows to collect all informations required to periodically display dashboard view containing both general blockchain properties like also basic account info.
    */
    DECLARE_API(
      (collect_dashboard_data) )
    
  private:
    class impl;
    struct deleter
    {
      void operator()(impl*) const;
    };

    std::unique_ptr< impl, deleter> my;
};

} } } // hive::plugins::chain

FC_REFLECT(hive::plugins::clive_api::api_manabar, (current_mana)(last_update_time))
FC_REFLECT(hive::plugins::clive_api::decline_voting_rights_request_info, (account)(effective_date))
FC_REFLECT(hive::plugins::clive_api::change_recovery_account_request_info, (account_to_recover)(effective_on))
FC_REFLECT(hive::plugins::clive_api::owner_authority_change_info, (previous_owner_authority)(last_valid_time))
FC_REFLECT(hive::plugins::clive_api::account_dashboard_data, (voting_manabar)(downvote_manabar)(rc_manabar)(max_rc_creation_adjustment)(max_rc)(delegated_rc)(received_delegated_rc)
          (balance)(savings_balance)(savings_hbd_balance)(reward_hbd_balance)(reward_hive_balance)(reward_vesting_balance)(vesting_shares)(delegated_vesting_shares)(received_vesting_shares)
          (post_voting_power)(open_recurrent_transfers)(governance_vote_expiration_ts)(last_operation_timestamp)(reputation)(decline_voting_rights_request)(change_recovery_account_request)
          (owner_authority_change)
)

FC_REFLECT(hive::plugins::clive_api::collect_dashboard_data_args, (accounts) )
FC_REFLECT(hive::plugins::clive_api::collect_dashboard_data_return, (head_block_number)(head_block_time)(total_vesting_fund_hive)(total_vesting_shares)(collected_account_infos))
