
#include <hive/plugins/rc/resource_sizes.hpp>

#include <hive/chain/comment_object.hpp>
#include <hive/chain/hive_objects.hpp>
#include <hive/chain/dhf_objects.hpp>
#include <hive/chain/transaction_object.hpp>
#include <hive/plugins/rc/rc_objects.hpp>

namespace hive { namespace plugins { namespace rc {

using namespace hive::chain;

// when uncommented the sizes of multiindex nodes will not be calculated dynamically, but will use fixed given values;
// this is how it should be on production; dynamic sizes should only be compared with fixed values used temporarily
// when we plan to update state RC costs for new hardfork after some significant changes in chain objects
//#define USE_FIXED_SIZE

#ifdef USE_FIXED_SIZE
#define SIZE( type, fixed_value ) ( fixed_value )
#else
template< size_t dynamic_value, int fixed_value >
struct compare
{
  static_assert( dynamic_value == fixed_value, "Update fixed size" ); //error should tell you correct value
};
template< typename type, int fixed_value >
struct validate_size
{
  static const compare< sizeof( type ), fixed_value > _;
  static const size_t value = fixed_value;
};
#define SIZE( type, fixed_value ) validate_size< type, ( fixed_value ) >::value
#endif

state_object_size_info::state_object_size_info()
  // cost is expressed in hour-bytes - amount of bytes taken multiplied by
  // amount of hours the object is going to live (average time in case of lasting
  // objects with no definite lifetime limit or half of persistent objects when
  // average time is hard to predict) rounded up when necessary (could be
  // block-bytes but then scale would be very large)
  // cost of persistent objects is calculated with assumption that average node
  // doubles its memory every 5 years
:
  TEMPORARY_STATE_BYTE( 1 ),
  LASTING_STATE_BYTE( TEMPORARY_STATE_BYTE * 5 * 365 * 12 ), //2.5 years
  PERSISTENT_STATE_BYTE( TEMPORARY_STATE_BYTE * 5 * 365 * 24 ), //5 years

  // account create (all versions)
  account_create_base_size(
    SIZE( account_index::MULTIINDEX_NODE_TYPE, 616 ) * PERSISTENT_STATE_BYTE +
    //SIZE( account_metadata_index::MULTIINDEX_NODE_TYPE, 136 ) * PERSISTENT_STATE_BYTE +
    SIZE( account_authority_index::MULTIINDEX_NODE_TYPE, 312 ) * PERSISTENT_STATE_BYTE +
    SIZE( rc_account_index::MULTIINDEX_NODE_TYPE, 144 ) * PERSISTENT_STATE_BYTE ),
  //account_json_metadata_char_size(
  //  PERSISTENT_STATE_BYTE ),
  authority_account_member_size(
    SIZE( shared_authority::account_authority_map::value_type, 24 ) * PERSISTENT_STATE_BYTE ),
  authority_key_member_size(
    SIZE( shared_authority::key_authority_map::value_type, 36 ) * PERSISTENT_STATE_BYTE ),

  // account update (both versions)
  owner_authority_history_object_size(
    SIZE( owner_authority_history_index::MULTIINDEX_NODE_TYPE, 168 ) * TEMPORARY_STATE_BYTE * 30 * 24 ), //HIVE_OWNER_AUTH_RECOVERY_PERIOD

  // transfer to vesting
  transfer_to_vesting_size(
    SIZE( delayed_votes_data, 16 ) * TEMPORARY_STATE_BYTE * 30 * 24 ), //HIVE_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS

  // request account recovery
  request_account_recovery_size(
    SIZE( account_recovery_request_index::MULTIINDEX_NODE_TYPE, 192 ) * TEMPORARY_STATE_BYTE * 24 ), //HIVE_ACCOUNT_RECOVERY_REQUEST_EXPIRATION_PERIOD

  // change recovery account
  change_recovery_account_size(
    SIZE( change_recovery_account_request_index::MULTIINDEX_NODE_TYPE, 136 ) * TEMPORARY_STATE_BYTE * 30 * 24 ), //HIVE_OWNER_AUTH_RECOVERY_PERIOD

  // comment
  comment_base_size(
    SIZE( comment_index::MULTIINDEX_NODE_TYPE, 96 ) * PERSISTENT_STATE_BYTE +
    SIZE( comment_cashout_index::MULTIINDEX_NODE_TYPE, 224 ) * TEMPORARY_STATE_BYTE * 7 * 24 ), //HIVE_CASHOUT_WINDOW_SECONDS
  comment_permlink_char_size(
    TEMPORARY_STATE_BYTE * 7 * 24 ), //HIVE_CASHOUT_WINDOW_SECONDS
  comment_beneficiaries_member_size(
    SIZE( comment_cashout_object::stored_beneficiary_route_type, 8 ) *
    TEMPORARY_STATE_BYTE * 7 * 24 ), //HIVE_CASHOUT_WINDOW_SECONDS

  // vote
  vote_size(
    SIZE( comment_vote_index::MULTIINDEX_NODE_TYPE, 144 ) * TEMPORARY_STATE_BYTE * 7 * 24 ), //HIVE_CASHOUT_WINDOW_SECONDS

  // convert
  convert_size(
    SIZE( convert_request_index::MULTIINDEX_NODE_TYPE, 120 ) * TEMPORARY_STATE_BYTE * 7 * 12 ), //HIVE_CONVERSION_DELAY
  // collateralized convert
  collateralized_convert_size(
    SIZE( collateralized_convert_request_index::MULTIINDEX_NODE_TYPE, 128 ) * TEMPORARY_STATE_BYTE * 7 * 12 ), //HIVE_COLLATERALIZED_CONVERSION_DELAY

  // decline voting rights
  decline_voting_rights_size(
    SIZE( decline_voting_rights_request_index::MULTIINDEX_NODE_TYPE, 128 ) * TEMPORARY_STATE_BYTE * 30 * 24 ), //HIVE_OWNER_AUTH_RECOVERY_PERIOD

  // escrow transfer
  escrow_transfer_size(
    SIZE( escrow_index::MULTIINDEX_NODE_TYPE, 216 ) * LASTING_STATE_BYTE ),

  // limit order create (both versions)
  limit_order_create_size(
    SIZE( limit_order_index::MULTIINDEX_NODE_TYPE, 208 ) * TEMPORARY_STATE_BYTE * 28 * 24 ), //HIVE_MAX_LIMIT_ORDER_EXPIRATION

  // transfer from savings
  transfer_from_savings_size(
    SIZE( savings_withdraw_index::MULTIINDEX_NODE_TYPE, 232 ) * TEMPORARY_STATE_BYTE * 3 * 24 ), //HIVE_SAVINGS_WITHDRAW_TIME

  // transaction
  transaction_base_size(
    SIZE( transaction_index::MULTIINDEX_NODE_TYPE, 128 ) * TEMPORARY_STATE_BYTE * 1 ), //HIVE_MAX_TIME_UNTIL_EXPIRATION
  transaction_byte_size(
    TEMPORARY_STATE_BYTE * 1 ), //HIVE_MAX_TIME_UNTIL_EXPIRATION

  // delegate vesting shares
  vesting_delegation_object_size(
    SIZE( vesting_delegation_index::MULTIINDEX_NODE_TYPE, 88 ) * LASTING_STATE_BYTE ),
  vesting_delegation_expiration_object_size(
    SIZE( vesting_delegation_expiration_index::MULTIINDEX_NODE_TYPE, 120 ) * TEMPORARY_STATE_BYTE * 5 * 24 ), //HIVE_DELEGATION_RETURN_PERIOD_HF20
  delegate_vesting_shares_size(
    std::max( vesting_delegation_object_size, vesting_delegation_expiration_object_size ) ),

  // set withdraw vesting route
  set_withdraw_vesting_route_size(
    SIZE( withdraw_vesting_route_index::MULTIINDEX_NODE_TYPE, 144 ) * LASTING_STATE_BYTE ),

  // witness update
  witness_update_base_size(
    SIZE( witness_index::MULTIINDEX_NODE_TYPE, 544 ) * PERSISTENT_STATE_BYTE ),
  witness_update_url_char_size(
    PERSISTENT_STATE_BYTE ),

  // account witness vote
  account_witness_vote_size(
    SIZE( witness_vote_index::MULTIINDEX_NODE_TYPE, 136 ) * TEMPORARY_STATE_BYTE * 365 * 24 ), //HIVE_GOVERNANCE_VOTE_EXPIRATION_PERIOD

  // create proposal
  create_proposal_base_size(
    SIZE( proposal_index::MULTIINDEX_NODE_TYPE, 336 ) * TEMPORARY_STATE_BYTE ), //multiply by actual lifetime
  create_proposal_subject_permlink_char_size(
    TEMPORARY_STATE_BYTE ), //multiply by actual lifetime

  // update proposal votes
  update_proposal_votes_member_size(
    SIZE( proposal_vote_index::MULTIINDEX_NODE_TYPE, 128 ) * TEMPORARY_STATE_BYTE * 365 * 24), //HIVE_GOVERNANCE_VOTE_EXPIRATION_PERIOD

  // recurrent transfer
  recurrent_transfer_base_size(
    SIZE( recurrent_transfer_index::MULTIINDEX_NODE_TYPE, 200 ) * TEMPORARY_STATE_BYTE ), //multiply by actual lifetime

  // direct rc delegation
  delegate_rc_base_size(
    SIZE( rc_direct_delegation_index::MULTIINDEX_NODE_TYPE, 88 ) * LASTING_STATE_BYTE ) //multiply by number of delegatees
{}

operation_exec_info::operation_exec_info()
  //times taken from https://gitlab.syncad.com/hive/hive/-/issues/205 results
  //NOTE1: those operations that have two separate results (from clean and plugin
  //run) use clean run values, especially that ops that produce vops have their
  //measurements broken in plugin run
  //NOTE2: some operations depend heavily on state which may lead to some
  //discrepancies if the operations were only used in specific way or very
  //limited number of times (f.e. other than that there is really no reason
  //for rc related components of account_create_time be that much different from
  //those for account_create_with_delegation_time)

  //the time components used below are:
  //validate + exec + pre->rc + post->rc [+ automatic processing]
  //times from witness not included since it runs empty during replay (no valid measurements)
:
  account_create_time( 587 + 31859 + 236 + 6401 ),
  account_create_with_delegation_time( 3292 + 41583 + 139 + 66 ),
  account_witness_vote_time( 436 + 14005 + 173 + 145 ),
  comment_time( 16949 + 20931 + 221 + 166 + 41857 ), //cashout
  comment_options_time( 464 + 5550 + 142 + 117 ),
  convert_time( 403 + 10763 + 207 + 143 + 7928 ), //conversion
  collateralized_convert_time( 513 + 12888 + 332 + 239 + 8755 ), //conversion
  create_claimed_account_time( 6562 + 45969 + 342 + 10778 ),
  decline_voting_rights_time( 513 + 46887 + 179 + 187 + 24301 ), //finalization
  delegate_vesting_shares_time( 376 + 11480 + 3110 + 1208 ),
  escrow_transfer_time( 10420 + 13628 + 309 + 204 ),
  limit_order_create_time( 367 + 13436 + 187 + 124 ),
  limit_order_create2_time( 569 + 23169 + 310 + 118 ),
  request_account_recovery_time( 625 + 6555 + 149 + 127 ),
  set_withdraw_vesting_route_time( 412 + 8762 + 632 + 121 ),
  vote_time( 391 + 17530 + 187 + 78 ),
  witness_update_time( 524 + 3326 + 154 + 121 ),
  transfer_time( 513 + 4959 + 204 + 134 ),
  transfer_to_vesting_time( 399 + 17085 + 2174 + 721 + 24064 ), //delayed voting
  transfer_to_savings_time( 720 + 5073 + 205 + 155 ),
  transfer_from_savings_time( 444 + 6923 + 111 + 106 + 8028 ), //savings withdraws
  claim_reward_balance_time( 352 + 22648 + 2798 + 970 ),
  withdraw_vesting_time( 351 + 6715 + 2068 + 567 + 15950 * HIVE_VESTING_WITHDRAW_INTERVALS ), //vesting withdrawals
  account_update_time( 18668 + 12004 + 206 + 152 ),
  account_update2_time( 22417 + 10389 + 405 + 257 ),
  account_witness_proxy_time( 557 + 54013 + 170 + 166 ),
  cancel_transfer_from_savings_time( 127 + 2696 + 72 + 66 ),
  change_recovery_account_time( 551 + 12819 + 297 + 202 ),
  claim_account_time( 336 + 7861 + 267 + 150 ),
  custom_time( 115 + 150 + 238 + 121 ),
  custom_json_time( 13550 + 183 + 321 + 167 ),
  custom_binary_time( 0 + 0 + 0 + 0 + 100000 ), //never executed (artificial time added for safety)
  delete_comment_time( 436 + 17725 + 175 + 146 ),
  escrow_approve_time( 850 + 5970 + 260 + 181 ),
  escrow_dispute_time( 826 + 9759 + 261 + 176 ),
  escrow_release_time( 730 + 13320 + 252 + 189 ),
  feed_publish_time( 372 + 2283 + 213 + 144 ),
  limit_order_cancel_time( 299 + 5160 + 204 + 142 ),
  witness_set_properties_time( 3258 + 4210 + 310 + 234 ),
#ifdef HIVE_ENABLE_SMT
  claim_reward_balance2_time( 0 ),
  smt_setup_time( 0 ),
  smt_setup_emissions_time( 0 ),
  smt_set_setup_parameters_time( 0 ),
  smt_set_runtime_parameters_time( 0 ),
  smt_create_time( 0 ),
  smt_contribute_time( 0 ),
#endif
  create_proposal_time( 5917 + 59896 + 5666 + 2168 ), //processing not counted since it depends on too many variables
  update_proposal_time( 762 + 12673 + 4501 + 267 ),
  update_proposal_votes_time( 404 + 10625 + 2993 + 1206 ),
  remove_proposal_time( 594 + 129492 + 3886 + 1692 ),
  recurrent_transfer_base_time( 811 + 19349 + 330 + 238 ), //processing separately below
  recurrent_transfer_processing_time( 13023 ), //multiply by number of transfers
  delegate_rc_time( 75000 ), //this is cost over custom json processing (difference between RC delegation and dummy custom json of the same size);
    //while it depends on number of delegatees, the differences should be neglegible so it was collected for 10 delegatees
    //once we have real uses of RC delegation we can correct actual average time consumption

  verify_authority_time( 95230 ), //multiply by number of signatures
  transaction_time(
    4405 + //transaction dupe check
    364 + //tapos check
    3415 ), //post->rc::transaction

  //not used but included since we have measurements
  recover_account_time( 687 + 24296 + 149 + 145 ),
  pow_time( 177891 + 23343 + 165 + 57 ),
  pow2_time( 86439 + 17341 + 160 + 92 )

{}

} } } // hive::plugins::rc
