#pragma once
#include <hive/chain/account_object.hpp>
#include <hive/chain/block_summary_object.hpp>
#include <hive/chain/comment_object.hpp>
#include <hive/chain/global_property_object.hpp>
#include <hive/chain/hive_objects.hpp>
#include <hive/chain/smt_objects.hpp>
#include <hive/chain/dhf_objects.hpp>
#include <hive/chain/transaction_object.hpp>
#include <hive/chain/witness_objects.hpp>
#include <hive/chain/database.hpp>

namespace hive { namespace plugins { namespace database_api {

using namespace hive::chain;

struct api_reward_fund_object
{
  api_reward_fund_object() = default;
  api_reward_fund_object( const reward_fund_object& o, const database& db ):
    id( o.get_id() ),
    name( o.name ),
    reward_balance( o.reward_balance ),
    recent_claims( o.recent_claims ),
    last_update( o.last_update ),
    content_constant( o.content_constant ),
    percent_curation_rewards( o.percent_curation_rewards ),
    percent_content_rewards( o.percent_content_rewards ),
    author_reward_curve( o.author_reward_curve ),
    curation_reward_curve( o.curation_reward_curve )
  {}

  reward_fund_id_type     id;
  reward_fund_name_type   name;
  asset                   reward_balance;
  uint128_t               recent_claims = 0;
  time_point_sec          last_update;
  uint128_t               content_constant = 0;
  uint16_t                percent_curation_rewards = 0;
  uint16_t                percent_content_rewards = 0;
  protocol::curve_id      author_reward_curve = protocol::quadratic;
  protocol::curve_id      curation_reward_curve = protocol::quadratic;
};

struct api_witness_vote_object
{
  api_witness_vote_object( const witness_vote_object& o, const database& db ):
    id( o.get_id() ),
    witness( o.witness ),
    account( o.account )
  {}

  witness_vote_id_type id;
  account_name_type    witness;
  account_name_type    account;
};

struct api_escrow_object
{
  api_escrow_object( const escrow_object& o, const database& db ):
    id( o.get_id() ),
    escrow_id( o.escrow_id ),
    from( o.from ),
    to( o.to ),
    agent( o.agent ),
    ratification_deadline( o.ratification_deadline ),
    escrow_expiration( o.escrow_expiration ),
    hbd_balance( o.hbd_balance ),
    hive_balance( o.hive_balance ),
    pending_fee( o.pending_fee ),
    to_approved( o.to_approved ),
    agent_approved( o.agent_approved ),
    disputed( o.disputed )
  {}

  escrow_id_type    id;
  uint32_t          escrow_id;
  account_name_type from;
  account_name_type to;
  account_name_type agent;
  time_point_sec    ratification_deadline;
  time_point_sec    escrow_expiration;
  asset             hbd_balance;
  asset             hive_balance;
  asset             pending_fee;
  bool              to_approved;
  bool              agent_approved;
  bool              disputed;
};

struct api_withdraw_vesting_route_object
{
  api_withdraw_vesting_route_object( const withdraw_vesting_route_object& o, const database& db ):
    id( o.get_id() ),
    from_account( o.from_account ),
    to_account( o.to_account ),
    percent( o.percent ),
    auto_vest( o.auto_vest )
  {}

  api_withdraw_vesting_route_object() {}

  withdraw_vesting_route_id_type id;
  account_name_type              from_account;
  account_name_type              to_account;
  uint16_t                       percent;
  bool                           auto_vest;
};

struct api_vesting_delegation_object
{
  api_vesting_delegation_object( const vesting_delegation_object& o, const database& db ):
    id( o.get_id() ),
    delegator( db.get_account( o.get_delegator() ).get_name() ),
    delegatee( db.get_account( o.get_delegatee() ).get_name() ),
    vesting_shares( o.get_vesting() ),
    min_delegation_time( o.get_min_delegation_time() )
  {}

  vesting_delegation_id_type id;
  account_name_type          delegator;
  account_name_type          delegatee;
  asset                      vesting_shares;
  time_point_sec             min_delegation_time;
};

struct api_vesting_delegation_expiration_object
{
  api_vesting_delegation_expiration_object( const vesting_delegation_expiration_object& o, const database& db ):
    id( o.get_id() ),
    delegator( db.get_account( o.get_delegator() ).get_name() ),
    vesting_shares( o.get_vesting() ),
    expiration( o.get_expiration_time() )
  {}

  vesting_delegation_expiration_id_type id;
  account_name_type                     delegator;
  asset                                 vesting_shares;
  time_point_sec                        expiration;
};

struct api_convert_request_object
{
  api_convert_request_object() {}
  api_convert_request_object( const convert_request_object& o, const database& db ):
    id( o.get_id() ),
    owner( db.get_account( o.get_owner() ).get_name() ),
    requestid( o.get_request_id() ),
    amount( o.get_convert_amount() ),
    conversion_date( o.get_conversion_date() )
  {}

  convert_request_id_type id;
  account_name_type       owner;
  uint32_t                requestid;
  asset                   amount;
  time_point_sec          conversion_date;
};

struct api_collateralized_convert_request_object
{
  api_collateralized_convert_request_object() {}
  api_collateralized_convert_request_object( const collateralized_convert_request_object& o, const database& db ) :
    id( o.get_id() ),
    owner( db.get_account( o.get_owner() ).get_name() ),
    requestid( o.get_request_id() ),
    collateral_amount( o.get_collateral_amount() ),
    converted_amount( o.get_converted_amount() ),
    conversion_date( o.get_conversion_date() )
  {}

  collateralized_convert_request_id_type id;
  account_name_type                      owner;
  uint32_t                               requestid;
  asset                                  collateral_amount;
  asset                                  converted_amount;
  time_point_sec                         conversion_date;
};

struct api_decline_voting_rights_request_object
{
  api_decline_voting_rights_request_object( const decline_voting_rights_request_object& o, const database& db ):
    id( o.get_id() ),
    account( o.account ),
    effective_date( o.effective_date )
  {}

  decline_voting_rights_request_id_type id;
  account_name_type                     account;
  time_point_sec                        effective_date;
};

struct api_limit_order_object
{
  api_limit_order_object( const limit_order_object& o, const database& db ):
    id( o.get_id() ),
    created( o.created ),
    expiration( o.expiration ),
    seller( o.seller ),
    orderid( o.orderid ),
    for_sale( o.for_sale ),
    sell_price( o.sell_price )
  {}
  api_limit_order_object() {}

  limit_order_id_type id;
  time_point_sec      created;
  time_point_sec      expiration;
  account_name_type   seller;
  uint32_t            orderid;
  share_type          for_sale;
  price               sell_price;
};

struct api_dynamic_global_property_object
{
  api_dynamic_global_property_object( const dynamic_global_property_object& o, const database& db ) :
    id( o.get_id() ),
    head_block_number( o.head_block_number ),
    head_block_id( o.head_block_id ),
    time( o.time ),
    current_witness( o.current_witness ),
    total_pow( o.total_pow ),
    num_pow_witnesses( o.num_pow_witnesses ),
    virtual_supply( o.virtual_supply ),
    current_supply( o.current_supply ),
    init_hbd_supply( o.init_hbd_supply ),
    current_hbd_supply( o.current_hbd_supply ),
    total_vesting_fund_hive( o.total_vesting_fund_hive ),
    total_vesting_shares( o.total_vesting_shares ),
    total_reward_fund_hive( o.total_reward_fund_hive ),
    total_reward_shares2( o.total_reward_shares2 ),
    pending_rewarded_vesting_shares( o.pending_rewarded_vesting_shares ),
    pending_rewarded_vesting_hive( o.pending_rewarded_vesting_hive ),
    hbd_interest_rate( o.hbd_interest_rate ),
    hbd_print_rate( o.hbd_print_rate ),
    maximum_block_size( o.maximum_block_size ),
    required_actions_partition_percent( o.required_actions_partition_percent ),
    current_aslot( o.current_aslot ),
    recent_slots_filled( o.recent_slots_filled ),
    participation_count( o.participation_count ),
    last_irreversible_block_num( db.get_last_irreversible_block_num()),
    vote_power_reserve_rate( o.vote_power_reserve_rate ),
    delegation_return_period( o.delegation_return_period ),
    reverse_auction_seconds( o.reverse_auction_seconds ),
    available_account_subsidies( o.available_account_subsidies ),
    hbd_stop_percent( o.hbd_stop_percent ),
    hbd_start_percent( o.hbd_start_percent ),
    next_maintenance_time( o.next_maintenance_time ),
    last_budget_time( o.last_budget_time ),
    next_daily_maintenance_time( o.next_daily_maintenance_time ),
    content_reward_percent( o.content_reward_percent ),
    vesting_reward_percent( o.vesting_reward_percent ),
    proposal_fund_percent( o.proposal_fund_percent ),
    dhf_interval_ledger( o.dhf_interval_ledger ),
    downvote_pool_percent( o.downvote_pool_percent ),
    current_remove_threshold( o.current_remove_threshold ),
    early_voting_seconds( o.early_voting_seconds ),
    mid_voting_seconds( o.mid_voting_seconds ),
    max_consecutive_recurrent_transfer_failures( o.max_consecutive_recurrent_transfer_failures ),
    max_recurrent_transfer_end_date( o.max_recurrent_transfer_end_date ),
    min_recurrent_transfers_recurrence( o.min_recurrent_transfers_recurrence ),
    max_open_recurrent_transfers( o.max_open_recurrent_transfers )
#ifdef HIVE_ENABLE_SMT
    , smt_creation_fee( o.smt_creation_fee )
#endif
  {}

  api_dynamic_global_property_object() {}

  dynamic_global_property_id_type id;
  uint32_t                        head_block_number                   = 0;
  block_id_type                   head_block_id;
  time_point_sec                  time;
  account_name_type               current_witness;
  uint64_t                        total_pow  = 0;
  uint32_t                        num_pow_witnesses                   = 0;
  asset                           virtual_supply;
  asset                           current_supply;
  asset                           init_hbd_supply;
  asset                           current_hbd_supply;
  asset                           total_vesting_fund_hive;
  asset                           total_vesting_shares;
  asset                           total_reward_fund_hive;
  fc::uint128                     total_reward_shares2;
  asset                           pending_rewarded_vesting_shares;
  asset                           pending_rewarded_vesting_hive;
  uint16_t                        hbd_interest_rate                   = 0;
  uint16_t                        hbd_print_rate                      = 0;
  uint32_t                        maximum_block_size                  = 0;
  uint16_t                        required_actions_partition_percent  = 0;
  uint64_t                        current_aslot                       = 0;
  fc::uint128_t                   recent_slots_filled;
  uint8_t                         participation_count                 = 0;
  uint32_t                        last_irreversible_block_num         = 0;
  uint32_t                        vote_power_reserve_rate             = 0;
  uint32_t                        delegation_return_period            = 0;
  uint64_t                        reverse_auction_seconds             = 0;
  int64_t                         available_account_subsidies         = 0;
  uint16_t                        hbd_stop_percent                    = 0;
  uint16_t                        hbd_start_percent                   = 0;
  time_point_sec                  next_maintenance_time;
  time_point_sec                  last_budget_time;
  time_point_sec                  next_daily_maintenance_time;
  uint16_t                        content_reward_percent              = 0;
  uint16_t                        vesting_reward_percent              = 0;
  uint16_t                        proposal_fund_percent               = 0;
  asset                           dhf_interval_ledger;
  uint16_t                        downvote_pool_percent               = 0;
  int16_t                         current_remove_threshold            = 0;
  uint64_t                        early_voting_seconds                = 0;
  uint64_t                        mid_voting_seconds                  = 0;
  uint8_t                        max_consecutive_recurrent_transfer_failures = HIVE_MAX_CONSECUTIVE_RECURRENT_TRANSFER_FAILURES;
  uint16_t                        max_recurrent_transfer_end_date = HIVE_MAX_RECURRENT_TRANSFER_END_DATE;
  uint8_t                        min_recurrent_transfers_recurrence = HIVE_MIN_RECURRENT_TRANSFERS_RECURRENCE;
  uint16_t                        max_open_recurrent_transfers = HIVE_MAX_OPEN_RECURRENT_TRANSFERS;
#ifdef HIVE_ENABLE_SMT
  asset                           smt_creation_fee;
#endif
};

struct api_change_recovery_account_request_object
{
  api_change_recovery_account_request_object( const change_recovery_account_request_object& o, const database& db ):
    id( o.get_id() ),
    account_to_recover( o.get_account_to_recover() ),
    recovery_account( o.get_recovery_account() ),
    effective_on( o.get_execution_time() )
  {}

  change_recovery_account_request_id_type id;
  account_name_type                       account_to_recover;
  account_name_type                       recovery_account;
  time_point_sec                          effective_on;
};

struct api_commment_cashout_info
{
  api_commment_cashout_info(const comment_cashout_object& o, const database& db);

  uint64_t       total_vote_weight = 0;
  asset          total_payout_value;
  asset          curator_payout_value;
  asset          max_accepted_payout;

  share_type     author_rewards = 0;
  share_type     children_abs_rshares = 0;
  share_type     net_rshares = 0;
  share_type     abs_rshares = 0;
  share_type     vote_rshares = 0;

  int32_t        net_votes = 0;

  time_point_sec last_payout;
  time_point_sec cashout_time;
  time_point_sec max_cashout_time;

  uint16_t       percent_hbd = 0;
  uint16_t       reward_weight = 0;
  bool           allow_replies = false;
  bool           allow_votes = false;
  bool           allow_curation_rewards = false;
  bool           was_voted_on = false;
};

struct api_comment_object
{
  api_comment_object( const comment_object& o, const database& db ):
    id( o.get_id() ),
    depth( o.get_depth() )
  {
    const comment_cashout_object* cc = db.find_comment_cashout( o );
    if( cc )
    {
      total_vote_weight       = cc->get_total_vote_weight();
      reward_weight           = HIVE_100_PERCENT; // since HF17 reward is not limited if posts are too frequent
      author_rewards          = 0; // since HF19 it was always 0 or cc did not exist
      net_votes               = cc->get_net_votes();
      last_payout             = time_point_sec::min(); // since HF19 it is the only value possible
      children                = cc->get_number_of_replies();
      net_rshares             = cc->get_net_rshares();
      abs_rshares             = 0; // value was only used for comments created before HF6
      vote_rshares            = cc->get_vote_rshares();
      children_abs_rshares    = 0; // value not accumulated after HF17
      created                 = cc->get_creation_time();
      last_update             = created; // edit time not available here (Hivemind has it)
      cashout_time            = cc->get_cashout_time();
      max_cashout_time        = time_point_sec::maximum(); // since HF17 it is the only possible value
      max_accepted_payout     = cc->get_max_accepted_payout();
      percent_hbd             = cc->get_percent_hbd();
      allow_votes             = cc->allows_votes();
      allow_curation_rewards  = cc->allows_curation_rewards();
      was_voted_on            = cc->has_votes();

      for( auto& route : cc->get_beneficiaries() )
      {
        beneficiaries.emplace_back( db.get_account( route.account_id ).get_name(), route.weight );
      }

    }

  }

  api_comment_object(){}

  comment_id_type   id;
  string            category;
  string            parent_author;
  string            parent_permlink;
  string            author;
  string            permlink;

  string            title;
  string            body;
  string            json_metadata;
  time_point_sec    last_update;
  time_point_sec    created;
  time_point_sec    last_payout;

  uint8_t           depth = 0;
  uint32_t          children = 0;

  share_type        net_rshares;
  share_type        abs_rshares;
  share_type        vote_rshares;

  share_type        children_abs_rshares;
  time_point_sec    cashout_time;
  time_point_sec    max_cashout_time;
  uint64_t          total_vote_weight = 0;

  uint16_t          reward_weight = 0;

  asset             total_payout_value = HBD_asset(); // since HF19 it was either default 0 or cc did not exist
  asset             curator_payout_value = HBD_asset(); // since HF19 it was either default 0 or cc did not exist

  share_type        author_rewards;

  int32_t           net_votes = 0;

  account_name_type root_author;
  string            root_permlink;

  asset             max_accepted_payout = HBD_asset(); 
  uint16_t          percent_hbd = 0;
  bool              allow_replies = false;
  bool              allow_votes = false;
  bool              allow_curation_rewards = false;
  bool              was_voted_on = false;
  vector< beneficiary_route_type > beneficiaries;
};

struct api_comment_vote_object
{
  api_comment_vote_object( const comment_vote_object& cv, const database& db ) :
    id( cv.get_id() ),
    weight( cv.get_weight() ),
    rshares( cv.get_rshares() ),
    vote_percent( cv.get_vote_percent() ),
    last_update( cv.get_last_update() ),
    num_changes( cv.get_number_of_changes() )
  {
    voter = db.get( cv.get_voter() ).name;
    const comment_cashout_object* cc = db.find_comment_cashout( cv.get_comment() );
    assert( cc != nullptr ); //votes should not exist after cashout
    author = db.get_account( cc->get_author_id() ).name;
    permlink = to_string( cc->get_permlink() );
  }

  comment_vote_id_type id;

  account_name_type    voter;
  account_name_type    author;
  string               permlink;
  uint64_t             weight = 0;
  int64_t              rshares = 0;
  int16_t              vote_percent = 0;
  time_point_sec       last_update;
  int8_t               num_changes = 0;
};

struct api_account_object
{
  api_account_object( const account_object& a, const database& db, bool delayed_votes_active ) :
    id( a.get_id() ),
    name( a.get_name() ),
    memo_key( a.memo_key ),
    proxy( HIVE_PROXY_TO_SELF_ACCOUNT ),
    last_account_update( a.last_account_update ),
    created( a.created ),
    mined( a.mined ),
    recovery_account( a.get_recovery_account() ),
    reset_account( HIVE_NULL_ACCOUNT ),
    last_account_recovery( a.last_account_recovery ),
    comment_count( a.comment_count ),
    lifetime_vote_count( a.lifetime_vote_count ),
    post_count( a.post_count ),
    can_vote( a.can_vote ),
    voting_manabar( a.voting_manabar ),
    downvote_manabar( a.downvote_manabar ),
    balance( a.get_balance() ),
    savings_balance( a.get_savings() ),
    hbd_balance( a.hbd_balance ),
    hbd_seconds( a.hbd_seconds ),
    hbd_seconds_last_update( a.hbd_seconds_last_update ),
    hbd_last_interest_payment( a.hbd_last_interest_payment ),
    savings_hbd_balance( a.get_hbd_savings() ),
    savings_hbd_seconds( a.savings_hbd_seconds ),
    savings_hbd_seconds_last_update( a.savings_hbd_seconds_last_update ),
    savings_hbd_last_interest_payment( a.savings_hbd_last_interest_payment ),
    savings_withdraw_requests( a.savings_withdraw_requests ),
    reward_hbd_balance( a.get_hbd_rewards() ),
    reward_hive_balance( a.get_rewards() ),
    reward_vesting_balance( a.get_vest_rewards() ),
    reward_vesting_hive( a.get_vest_rewards_as_hive() ),
    curation_rewards( a.curation_rewards ),
    posting_rewards( a.posting_rewards ),
    vesting_shares( a.vesting_shares ),
    delegated_vesting_shares( a.delegated_vesting_shares ),
    received_vesting_shares( a.received_vesting_shares ),
    vesting_withdraw_rate( a.vesting_withdraw_rate ),
    next_vesting_withdrawal( a.next_vesting_withdrawal ),
    withdrawn( a.withdrawn ),
    to_withdraw( a.to_withdraw ),
    withdraw_routes( a.withdraw_routes ),
    pending_transfers( a.pending_transfers ),
    witnesses_voted_for( a.witnesses_voted_for ),
    last_post( a.last_post ),
    last_root_post( a.last_root_post ),
    last_post_edit( a.last_post_edit ),
    last_vote_time( a.last_vote_time ),
    post_bandwidth( a.post_bandwidth ),
    pending_claimed_accounts( a.pending_claimed_accounts ),
    open_recurrent_transfers( a.open_recurrent_transfers ),
    governance_vote_expiration_ts( a.get_governance_vote_expiration_ts())
  {
    if( a.has_proxy() )
      proxy = db.get_account( a.get_proxy() ).get_name();

    size_t n = a.proxied_vsf_votes.size();
    proxied_vsf_votes.reserve( n );
    for( size_t i=0; i<n; i++ )
      proxied_vsf_votes.push_back( a.proxied_vsf_votes[i] );

    const auto& auth = db.get< account_authority_object, by_account >( name );
    owner = authority( auth.owner );
    active = authority( auth.active );
    posting = authority( auth.posting );
    previous_owner_update = auth.previous_owner_update;
    last_owner_update = auth.last_owner_update;
#ifdef COLLECT_ACCOUNT_METADATA
    const auto* maybe_meta = db.find< account_metadata_object, by_account >( id );
    if( maybe_meta )
    {
      json_metadata = to_string( maybe_meta->json_metadata );
      posting_json_metadata = to_string( maybe_meta->posting_json_metadata );
    }
#endif

#ifdef HIVE_ENABLE_SMT
    const auto& by_control_account_index = db.get_index<smt_token_index>().indices().get<by_control_account>();
    auto smt_obj_itr = by_control_account_index.find( name );
    is_smt = smt_obj_itr != by_control_account_index.end();
#endif

    if( delayed_votes_active )
      delayed_votes = vector< delayed_votes_data >{ a.delayed_votes.begin(), a.delayed_votes.end() };

    post_voting_power = db.get_effective_vesting_shares(a, VESTS_SYMBOL);
  }

  api_account_object(){}

  account_id_type   id;

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
  uint128_t         hbd_seconds;
  time_point_sec    hbd_seconds_last_update;
  time_point_sec    hbd_last_interest_payment;

  asset             savings_hbd_balance;
  uint128_t         savings_hbd_seconds;
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
};

struct api_owner_authority_history_object
{
  api_owner_authority_history_object( const owner_authority_history_object& o, const database& db ) :
    id( o.get_id() ),
    account( o.account ),
    previous_owner_authority( authority( o.previous_owner_authority ) ),
    last_valid_time( o.last_valid_time )
  {}

  api_owner_authority_history_object() {}

  owner_authority_history_id_type  id;

  account_name_type                account;
  authority                        previous_owner_authority;
  time_point_sec                   last_valid_time;
};

struct api_account_recovery_request_object
{
  api_account_recovery_request_object( const account_recovery_request_object& o, const database& db ) :
    id( o.get_id() ),
    account_to_recover( o.get_account_to_recover() ),
    new_owner_authority( authority( o.get_new_owner_authority() ) ),
    expires( o.get_expiration_time() )
  {}

  api_account_recovery_request_object() {}

  account_recovery_request_id_type id;
  account_name_type                account_to_recover;
  authority                        new_owner_authority;
  time_point_sec                   expires;
};

struct api_account_history_object
{

};

struct api_savings_withdraw_object
{
  api_savings_withdraw_object( const savings_withdraw_object& o, const database& db ) :
    id( o.get_id() ),
    from( o.from ),
    to( o.to ),
    memo( to_string( o.memo ) ),
    request_id( o.request_id ),
    amount( o.amount ),
    complete( o.complete )
  {}

  api_savings_withdraw_object() {}

  savings_withdraw_id_type   id;
  account_name_type          from;
  account_name_type          to;
  string                     memo;
  uint32_t                   request_id = 0;
  asset                      amount;
  time_point_sec             complete;
};

struct api_feed_history_object
{
  api_feed_history_object( const feed_history_object& f ) :
    id( f.get_id() ),
    current_median_history( f.current_median_history ),
    market_median_history( f.market_median_history ),
    current_min_history( f.current_min_history ),
    current_max_history( f.current_max_history ),
    price_history( f.price_history.begin(), f.price_history.end() )
  {}

  api_feed_history_object() {}

  feed_history_id_type id;
  price                current_median_history;
  price                market_median_history;
  price                current_min_history;
  price                current_max_history;
  deque< price >       price_history;
};

struct api_witness_object
{
  api_witness_object( const witness_object& w, const database& db ) :
    id( w.get_id() ),
    owner( w.owner ),
    created( w.created ),
    url( to_string( w.url ) ),
    total_missed( w.total_missed ),
    last_aslot( w.last_aslot ),
    last_confirmed_block_num( w.last_confirmed_block_num ),
    pow_worker( w.pow_worker ),
    signing_key( w.signing_key ),
    props( w.props ),
    hbd_exchange_rate( w.get_hbd_exchange_rate() ),
    last_hbd_exchange_update( w.get_last_hbd_exchange_update() ),
    votes( w.votes ),
    virtual_last_update( w.virtual_last_update ),
    virtual_position( w.virtual_position ),
    virtual_scheduled_time( w.virtual_scheduled_time ),
    last_work( w.last_work ),
    running_version( w.running_version ),
    hardfork_version_vote( w.hardfork_version_vote ),
    hardfork_time_vote( w.hardfork_time_vote ),
    available_witness_account_subsidies( w.available_witness_account_subsidies )
  {}

  api_witness_object() {}

  witness_id_type   id;
  account_name_type owner;
  time_point_sec    created;
  string            url;
  uint32_t          total_missed = 0;
  uint64_t          last_aslot = 0;
  uint64_t          last_confirmed_block_num = 0;
  uint64_t          pow_worker = 0;
  public_key_type   signing_key;
  chain_properties  props;
  price             hbd_exchange_rate;
  time_point_sec    last_hbd_exchange_update;
  share_type        votes;
  fc::uint128       virtual_last_update;
  fc::uint128       virtual_position;
  fc::uint128       virtual_scheduled_time;
  digest_type       last_work;
  version           running_version;
  hardfork_version  hardfork_version_vote;
  time_point_sec    hardfork_time_vote;
  int64_t           available_witness_account_subsidies = 0;
};

// only filled parts that are different between active and future wso
struct future_chain_properties
{
  future_chain_properties() {}

  bool fill( const chain_properties& active, const chain_properties& future );

  fc::optional<asset>    account_creation_fee;
  fc::optional<uint32_t> maximum_block_size;
  fc::optional<uint16_t> hbd_interest_rate;
  fc::optional<int32_t>  account_subsidy_budget;
  fc::optional<uint32_t> account_subsidy_decay;
};

// only filled parts that are different between active and future wso
struct future_witness_schedule
{
  future_witness_schedule() {}

  bool fill( const witness_schedule_object& active, const witness_schedule_object& future );

  fc::optional<uint8_t>                 num_scheduled_witnesses;
  fc::optional<uint8_t>                 elected_weight;
  fc::optional<uint8_t>                 timeshare_weight;
  fc::optional<uint8_t>                 miner_weight;
  fc::optional<uint8_t>                 witness_pay_normalization_factor;
  fc::optional<future_chain_properties> median_props;
  fc::optional<version>                 majority_version;

  fc::optional<uint8_t>                 max_voted_witnesses;
  fc::optional<uint8_t>                 max_miner_witnesses;
  fc::optional<uint8_t>                 max_runner_witnesses;
  fc::optional<uint8_t>                 hardfork_required_witnesses;

  fc::optional<rd_dynamics_params>      account_subsidy_rd;
  fc::optional<rd_dynamics_params>      account_subsidy_witness_rd;
  fc::optional<int64_t>                 min_witness_account_subsidy_decay;
};

struct api_witness_schedule_object
{
  api_witness_schedule_object() {}

  api_witness_schedule_object( const witness_schedule_object& wso,
    const witness_schedule_object& future_wso, bool include_future, const database& db ) :
    id( wso.get_id() ),
    current_virtual_time( future_wso.current_virtual_time ),
    next_shuffle_block_num( future_wso.next_shuffle_block_num ),
    num_scheduled_witnesses( wso.num_scheduled_witnesses ),
    elected_weight( wso.elected_weight ),
    timeshare_weight( wso.timeshare_weight ),
    miner_weight( wso.miner_weight ),
    witness_pay_normalization_factor( wso.witness_pay_normalization_factor ),
    median_props( wso.median_props ),
    majority_version( wso.majority_version ),
    max_voted_witnesses( wso.max_voted_witnesses ),
    max_miner_witnesses( wso.max_miner_witnesses ),
    max_runner_witnesses( wso.max_runner_witnesses ),
    hardfork_required_witnesses( wso.hardfork_required_witnesses ),
    account_subsidy_rd( wso.account_subsidy_rd ),
    account_subsidy_witness_rd( wso.account_subsidy_witness_rd ),
    min_witness_account_subsidy_decay( wso.min_witness_account_subsidy_decay )
  {
    size_t n = wso.current_shuffled_witnesses.size();
    current_shuffled_witnesses.reserve( n );
    std::transform(wso.current_shuffled_witnesses.begin(), wso.current_shuffled_witnesses.end(),
              std::back_inserter(current_shuffled_witnesses),
              [](const account_name_type& s) -> std::string { return s; } );
              // ^ fixed_string std::string operator used here.
    if( include_future )
    {
      n = future_wso.current_shuffled_witnesses.size();
      future_shuffled_witnesses = vector<string>();
      future_shuffled_witnesses->reserve( n );

      std::transform( future_wso.current_shuffled_witnesses.begin(), future_wso.current_shuffled_witnesses.end(),
        std::back_inserter( future_shuffled_witnesses.value() ),
        []( const account_name_type& s ) -> std::string { return s; } );

      future_changes = future_witness_schedule();
      if( !future_changes->fill( wso, future_wso ) )
        future_changes.reset();
    }
  }

  witness_schedule_id_type              id; //always from active wso

  fc::uint128                           current_virtual_time; //always from future wso
  uint32_t                              next_shuffle_block_num; //always from future wso
  vector<string>                        current_shuffled_witnesses; //from active wso
  fc::optional< vector<string> >        future_shuffled_witnesses; //from future wso (only filled on request)
  uint8_t                               num_scheduled_witnesses;
  uint8_t                               elected_weight;
  uint8_t                               timeshare_weight;
  uint8_t                               miner_weight;
  uint32_t                              witness_pay_normalization_factor;
  chain_properties                      median_props;
  version                               majority_version;

  uint8_t                               max_voted_witnesses;
  uint8_t                               max_miner_witnesses;
  uint8_t                               max_runner_witnesses;
  uint8_t                               hardfork_required_witnesses;

  rd_dynamics_params                    account_subsidy_rd;
  rd_dynamics_params                    account_subsidy_witness_rd;
  int64_t                               min_witness_account_subsidy_decay = 0;

  fc::optional<future_witness_schedule> future_changes; //only filled on request when there are changes
};

struct api_signed_block_object : public signed_block
{
  api_signed_block_object(const std::shared_ptr<full_block_type>& full_block) : 
    signed_block(full_block->get_block()),
    block_id(full_block->get_block_id()),
    signing_key(full_block->get_signing_key())
  {
    const std::vector<std::shared_ptr<full_transaction_type>>& full_transactions = full_block->get_full_transactions();
    transaction_ids.reserve(transactions.size());
    for (const std::shared_ptr<full_transaction_type>& full_transaction : full_transactions)
      transaction_ids.push_back(full_transaction->get_transaction_id());
  }
  api_signed_block_object() {}

  block_id_type                 block_id;
  public_key_type               signing_key;
  vector< transaction_id_type > transaction_ids;
};

struct api_hardfork_property_object
{
  api_hardfork_property_object( const hardfork_property_object& h ) :
    id( h.get_id() ),
    last_hardfork( h.last_hardfork ),
    current_hardfork_version( h.current_hardfork_version ),
    next_hardfork( h.next_hardfork ),
    next_hardfork_time( h.next_hardfork_time )
  {
    size_t n = h.processed_hardforks.size();
    processed_hardforks.reserve( n );

    for( size_t i = 0; i < n; i++ )
      processed_hardforks.push_back( h.processed_hardforks[i] );
  }

  api_hardfork_property_object() {}

  hardfork_property_id_type     id;
  vector< fc::time_point_sec >  processed_hardforks;
  uint32_t                      last_hardfork;
  protocol::hardfork_version    current_hardfork_version;
  protocol::hardfork_version    next_hardfork;
  fc::time_point_sec            next_hardfork_time;
};

#ifdef HIVE_ENABLE_SMT

struct api_smt_token_object
{
  api_smt_token_object( const smt_token_object& token, const database& db )
    : token( token.copy_chain_object() ) //FIXME: exposes internal chain object as API result
  {
    const smt_ico_object* ico = db.find< smt_ico_object, by_symbol >( token.liquid_symbol );
    if ( ico != nullptr )
      this->ico = ico->copy_chain_object(); //FIXME: exposes internal chain object as API result
  }

  smt_token_object                token;
  fc::optional< smt_ico_object >  ico;
};

struct api_smt_token_emissions_object
{
  api_smt_token_emissions_object( const smt_token_emissions_object& o, const database& db ):
    id( o.get_id() ),
    symbol( o.symbol ),
    schedule_time( o.schedule_time ),
    emissions_unit( o.emissions_unit ),
    interval_seconds( o.interval_seconds ),
    interval_count( o.interval_count ),
    lep_time( o.lep_time ),
    rep_time( o.rep_time ),
    lep_abs_amount( o.lep_abs_amount ),
    rep_abs_amount( o.rep_abs_amount ),
    lep_rel_amount_numerator( o.lep_rel_amount_numerator ),
    rep_rel_amount_numerator( o.rep_rel_amount_numerator ),
    rel_amount_denom_bits( o.rel_amount_denom_bits )
  {}

  smt_token_emissions_id_type           id;
  asset_symbol_type                     symbol;
  time_point_sec                        schedule_time;
  hive::protocol::smt_emissions_unit    emissions_unit;
  uint32_t                              interval_seconds;
  uint32_t                              interval_count;
  time_point_sec                        lep_time;
  time_point_sec                        rep_time;
  asset                                 lep_abs_amount;
  asset                                 rep_abs_amount;
  uint32_t                              lep_rel_amount_numerator;
  uint32_t                              rep_rel_amount_numerator;
  uint8_t                               rel_amount_denom_bits;
};

struct api_smt_contribution_object
{
  api_smt_contribution_object( const smt_contribution_object& o, const database& db ):
    id( o.get_id() ),
    symbol( o.symbol ),
    contributor( o.contributor ),
    contribution_id( o.contribution_id ),
    contribution( o.contribution )
  {}

  smt_contribution_id_type id;
  asset_symbol_type        symbol;
  account_name_type        contributor;
  uint32_t                 contribution_id;
  asset                    contribution;
};

#endif

enum proposal_status
{
  all,
  inactive,
  active,
  expired,
  votable
};

proposal_status get_proposal_status( const proposal_object& po, const time_point_sec current_time );

typedef uint64_t api_id_type;

struct api_proposal_object
{
  api_proposal_object() = default;

  api_proposal_object(const proposal_object& po, const time_point_sec& current_time) :
    id(po.get_id()),
    proposal_id(po.proposal_id),
    creator(po.creator),
    receiver(po.receiver),
    start_date(po.start_date),
    end_date(po.end_date),
    daily_pay(po.daily_pay),
    subject(to_string(po.subject)),
    permlink(to_string(po.permlink)),
    total_votes(po.total_votes),
    status(get_proposal_status(po,current_time))
  {}

  api_id_type       id;

  uint32_t          proposal_id;
  account_name_type creator;
  account_name_type receiver;
  time_point_sec    start_date;
  time_point_sec    end_date;
  asset             daily_pay;
  string            subject;
  string            permlink;
  uint64_t          total_votes = 0;
  proposal_status   status = proposal_status::all;
};

struct api_proposal_vote_object
{
  api_proposal_vote_object() = default;

  api_proposal_vote_object( const proposal_vote_object& pvo, const database& db ) :
    id( pvo.get_id() ),
    voter( pvo.voter ),
    proposal( db.get< proposal_object, by_proposal_id >( pvo.proposal_id ), db.head_block_time() )
  {}

  proposal_vote_id_type   id;
  account_name_type       voter;
  api_proposal_object     proposal;
};


struct api_recurrent_transfer_object
{
  api_recurrent_transfer_object() = default;

  api_recurrent_transfer_object( const recurrent_transfer_object& o, const account_name_type from_name, const account_name_type to_name ):
    id( o.get_id() ),
    trigger_date( o.get_trigger_date() ),
    from( from_name ),
    to( to_name ),
    amount( o.amount ),
    memo( to_string(o.memo) ),
    recurrence( o.recurrence ),
    consecutive_failures( o.consecutive_failures ),
    remaining_executions( o.remaining_executions )
    {}

    recurrent_transfer_id_type id;
    time_point_sec    trigger_date;
    account_name_type from;
    account_name_type to;
    asset             amount;
    string            memo;
    uint16_t          recurrence = 0;
    uint8_t           consecutive_failures = 0;
    uint16_t          remaining_executions = 0;
};


struct order
{
  price                order_price;
  double               real_price; // dollars per HIVE
  share_type           hive;
  share_type           hbd;
  fc::time_point_sec   created;
};

struct order_book
{
  vector< order >      asks;
  vector< order >      bids;
};

} } } // hive::plugins::database_api

FC_REFLECT(hive::plugins::database_api::api_commment_cashout_info,
  (total_vote_weight)
  (total_payout_value)
  (curator_payout_value)
  (max_accepted_payout)

  (author_rewards)
  (children_abs_rshares)
  (net_rshares)
  (abs_rshares)
  (vote_rshares)

  (net_votes)

  (last_payout)
  (cashout_time)
  (max_cashout_time)

  (percent_hbd)
  (reward_weight)
  (allow_replies)
  (allow_votes)
  (allow_curation_rewards)
  (was_voted_on)
)

FC_REFLECT( hive::plugins::database_api::api_reward_fund_object,
          (id)(name)(reward_balance)(recent_claims)(last_update)(content_constant)
          (percent_curation_rewards)(percent_content_rewards)(author_reward_curve)
          (curation_reward_curve)
        )

FC_REFLECT( hive::plugins::database_api::api_witness_vote_object,
          (id)(witness)(account)
        )

FC_REFLECT( hive::plugins::database_api::api_escrow_object,
          (id)(escrow_id)(from)(to)(agent)(ratification_deadline)(escrow_expiration)
          (hbd_balance)(hive_balance)(pending_fee)(to_approved)(agent_approved)(disputed)
        )

FC_REFLECT( hive::plugins::database_api::api_withdraw_vesting_route_object,
          (id)(from_account)(to_account)(percent)(auto_vest)
        )

FC_REFLECT( hive::plugins::database_api::api_vesting_delegation_object,
          (id)(delegator)(delegatee)(vesting_shares)(min_delegation_time)
        )

FC_REFLECT( hive::plugins::database_api::api_vesting_delegation_expiration_object,
          (id)(delegator)(vesting_shares)(expiration)
        )

FC_REFLECT( hive::plugins::database_api::api_convert_request_object,
          (id)(owner)(requestid)(amount)(conversion_date)
        )

FC_REFLECT( hive::plugins::database_api::api_collateralized_convert_request_object,
          (id)(owner)(requestid)(collateral_amount)(converted_amount)(conversion_date)
        )

FC_REFLECT( hive::plugins::database_api::api_decline_voting_rights_request_object,
          (id)(account)(effective_date)
        )

FC_REFLECT( hive::plugins::database_api::api_limit_order_object,
          (id)(created)(expiration)(seller)(orderid)(for_sale)(sell_price)
        )

FC_REFLECT( hive::plugins::database_api::api_dynamic_global_property_object,
          (id)(head_block_number)(head_block_id)(time)(current_witness)(total_pow)
          (num_pow_witnesses)(virtual_supply)(current_supply)(init_hbd_supply)(current_hbd_supply)
          (total_vesting_fund_hive)(total_vesting_shares)(total_reward_fund_hive)
          (total_reward_shares2)(pending_rewarded_vesting_shares)(pending_rewarded_vesting_hive)
          (hbd_interest_rate)(hbd_print_rate)(maximum_block_size)(required_actions_partition_percent)
          (current_aslot)(recent_slots_filled)(participation_count)(last_irreversible_block_num)
          (vote_power_reserve_rate)(delegation_return_period)(reverse_auction_seconds)
          (available_account_subsidies)(hbd_stop_percent)(hbd_start_percent)(next_maintenance_time)
          (last_budget_time)(next_daily_maintenance_time)(content_reward_percent)(vesting_reward_percent)(proposal_fund_percent)
          (dhf_interval_ledger)(downvote_pool_percent)(current_remove_threshold)(early_voting_seconds)(mid_voting_seconds)
          (max_consecutive_recurrent_transfer_failures)(max_recurrent_transfer_end_date)(min_recurrent_transfers_recurrence)
          (max_open_recurrent_transfers)
#ifdef HIVE_ENABLE_SMT
          (smt_creation_fee)
#endif
        )

FC_REFLECT( hive::plugins::database_api::api_change_recovery_account_request_object,
          (id)(account_to_recover)(recovery_account)(effective_on)
        )

FC_REFLECT( hive::plugins::database_api::api_comment_object,
          (id)(author)(permlink)
          (category)(parent_author)(parent_permlink)
          (title)(body)(json_metadata)(last_update)(created)(last_payout)
          (depth)(children)
          (net_rshares)(abs_rshares)(vote_rshares)
          (children_abs_rshares)(cashout_time)(max_cashout_time)
          (total_vote_weight)(reward_weight)(total_payout_value)(curator_payout_value)(author_rewards)(net_votes)
          (root_author)(root_permlink)
          (max_accepted_payout)(percent_hbd)(allow_replies)(allow_votes)(allow_curation_rewards)(was_voted_on)
          (beneficiaries)
        )

FC_REFLECT( hive::plugins::database_api::api_comment_vote_object,
          (id)(voter)(author)(permlink)(weight)(rshares)(vote_percent)(last_update)(num_changes)
        )

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

FC_REFLECT( hive::plugins::database_api::api_owner_authority_history_object,
          (id)
          (account)
          (previous_owner_authority)
          (last_valid_time)
        )

FC_REFLECT( hive::plugins::database_api::api_account_recovery_request_object,
          (id)
          (account_to_recover)
          (new_owner_authority)
          (expires)
        )

FC_REFLECT( hive::plugins::database_api::api_savings_withdraw_object,
          (id)
          (from)
          (to)
          (memo)
          (request_id)
          (amount)
          (complete)
        )

FC_REFLECT( hive::plugins::database_api::api_feed_history_object,
          (id)
          (current_median_history)
          (market_median_history)
          (current_min_history)
          (current_max_history)
          (price_history)
        )

FC_REFLECT( hive::plugins::database_api::api_witness_object,
          (id)
          (owner)
          (created)
          (url)(votes)(virtual_last_update)(virtual_position)(virtual_scheduled_time)(total_missed)
          (last_aslot)(last_confirmed_block_num)(pow_worker)(signing_key)
          (props)
          (hbd_exchange_rate)(last_hbd_exchange_update)
          (last_work)
          (running_version)
          (hardfork_version_vote)(hardfork_time_vote)
          (available_witness_account_subsidies)
        )

FC_REFLECT( hive::plugins::database_api::future_chain_properties,
          (account_creation_fee)
          (maximum_block_size)
          (hbd_interest_rate)
          (account_subsidy_budget)
          (account_subsidy_decay)
        )

FC_REFLECT( hive::plugins::database_api::future_witness_schedule,
          (num_scheduled_witnesses)
          (elected_weight)
          (timeshare_weight)
          (miner_weight)
          (witness_pay_normalization_factor)
          (median_props)
          (majority_version)
          (max_voted_witnesses)
          (max_miner_witnesses)
          (max_runner_witnesses)
          (hardfork_required_witnesses)
          (account_subsidy_rd)
          (account_subsidy_witness_rd)
          (min_witness_account_subsidy_decay)
        )

FC_REFLECT( hive::plugins::database_api::api_witness_schedule_object,
          (id)
          (current_virtual_time)
          (next_shuffle_block_num)
          (current_shuffled_witnesses)
          (future_shuffled_witnesses)
          (num_scheduled_witnesses)
          (elected_weight)
          (timeshare_weight)
          (miner_weight)
          (witness_pay_normalization_factor)
          (median_props)
          (majority_version)
          (max_voted_witnesses)
          (max_miner_witnesses)
          (max_runner_witnesses)
          (hardfork_required_witnesses)
          (account_subsidy_rd)
          (account_subsidy_witness_rd)
          (min_witness_account_subsidy_decay)
          (future_changes)
        )

FC_REFLECT_DERIVED( hive::plugins::database_api::api_signed_block_object, (hive::protocol::signed_block),
              (block_id)
              (signing_key)
              (transaction_ids)
            )

FC_REFLECT( hive::plugins::database_api::api_hardfork_property_object,
        (id)
        (processed_hardforks)
        (last_hardfork)
        (current_hardfork_version)
        (next_hardfork)
        (next_hardfork_time)
        )

#ifdef HIVE_ENABLE_SMT

FC_REFLECT( hive::plugins::database_api::api_smt_token_object,
  (token)
  (ico)
)

FC_REFLECT( hive::plugins::database_api::api_smt_token_emissions_object,
          (id)(symbol)(schedule_time)(emissions_unit)(interval_seconds)(interval_count)
          (lep_time)(rep_time)(lep_abs_amount)(rep_abs_amount)(lep_rel_amount_numerator)
          (rep_rel_amount_numerator)(rel_amount_denom_bits)
        )

FC_REFLECT( hive::plugins::database_api::api_smt_contribution_object,
          (id)(symbol)(contributor)(contribution_id)(contribution)
        )

#endif

FC_REFLECT_ENUM( hive::plugins::database_api::proposal_status,
            (all)
            (inactive)
            (active)
            (expired)
            (votable)
          )

FC_REFLECT( hive::plugins::database_api::api_proposal_object,
        (id)
        (proposal_id)
        (creator)
        (receiver)
        (start_date)
        (end_date)
        (daily_pay)
        (subject)
        (permlink)
        (total_votes)
        (status)
        )

FC_REFLECT( hive::plugins::database_api::api_proposal_vote_object,
        (id)
        (voter)
        (proposal)
        )

FC_REFLECT( hive::plugins::database_api::order, (order_price)(real_price)(hive)(hbd)(created) );

FC_REFLECT( hive::plugins::database_api::order_book, (asks)(bids) );
FC_REFLECT(hive::plugins::database_api::api_recurrent_transfer_object, (id)(trigger_date)(from)(to)(amount)(memo)(recurrence)(consecutive_failures)(remaining_executions))
