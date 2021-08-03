#!/usr/bin/python3

# INSERT INTO hive.blocks
# VALUES
#     ( 1, '\xBADD10', '\xCAFE10', '2016-06-22 19:10:21-07'::timestamp )
#   , ( 2, '\xBADD20', '\xCAFE20', '2016-06-22 19:10:22-07'::timestamp )
#   , ( 3, '\xBADD30', '\xCAFE30', '2016-06-22 19:10:23-07'::timestamp )
#   , ( 4, '\xBADD40', '\xCAFE40', '2016-06-22 19:10:24-07'::timestamp )
#   , ( 5, '\xBADD50', '\xCAFE50', '2016-06-22 19:10:25-07'::timestamp )
# ;

# INSERT INTO hive.transactions
# VALUES
#         ( 1, 0::SMALLINT, '\xDEED10', 101, 100, '2016-06-22 19:10:21-07'::timestamp, '\xBEEF' )
#       , ( 2, 0::SMALLINT, '\xDEED20', 101, 100, '2016-06-22 19:10:22-07'::timestamp, '\xBEEF' )
#       , ( 3, 0::SMALLINT, '\xDEED30', 101, 100, '2016-06-22 19:10:23-07'::timestamp, '\xBEEF' )
#       , ( 4, 0::SMALLINT, '\xDEED40', 101, 100, '2016-06-22 19:10:24-07'::timestamp, '\xBEEF' )
#       , ( 5, 0::SMALLINT, '\xDEED50', 101, 100, '2016-06-22 19:10:25-07'::timestamp, '\xBEEF' )
# ;

# INSERT INTO hive.transactions_multisig
# VALUES
#         ( '\xDEED10', '\xBAAD10' )
#       , ( '\xDEED20', '\xBAAD20' )
#       , ( '\xDEED30', '\xBAAD30' )
#       , ( '\xDEED40', '\xBAAD40' )
#       , ( '\xDEED50', '\xBAAD50' )
# ;

# INSERT INTO hive.operations
# VALUES
#         ( 1, 1, 0, 0, 12, '{"type":"account_witness_vote_operation","value":{"account":"donalddrumpf","witness":"berniesanders","approve":true}}' )
#       , ( 2, 2, 0, 0, 9, '{"type":"account_create_operation","value":{"fee":{"amount":"0","precision":3,"nai":"@@000000021"},"creator":"hello","new_account_name":"fabian","owner":{"weight_threshold":1,"account_auths":[],"key_auths":[["STM8MN3FNBa8WbEpxz3wGL3L1mkt6sGnncH8iuto7r8Wa3T9NSSGT",1]]},"active":{"weight_threshold":1,"account_auths":[],"key_auths":[["STM8HCf7QLUexogEviN8x1SpKRhFwg2sc8LrWuJqv7QsmWrua6ZyR",1]]},"posting":{"weight_threshold":1,"account_auths":[],"key_auths":[["STM8EhGWcEuQ2pqCKkGHnbmcTNpWYZDjGTT7ketVBp4gUStDr2brz",1]]},"memo_key":"STM6Gkj27XMkoGsr4zwEvkjNhh4dykbXmPFzHhT8g86jWsqu3U38X","json_metadata":"{}"}}' )
#       , ( 3, 3, 0, 0, 62, '{"type":"producer_reward_operation","value":{"producer":"initminer","vesting_shares":{"amount":"1000","precision":3,"nai":"@@000000021"}}}' )
#       , ( 4, 4, 0, 0, 75, '{"type":"pow_reward_operation","value":{"worker":"dark","reward":{"amount":"0","precision":3,"nai":"@@000000021"}}}' )
#       , ( 5, 5, 0, 0, 2, '{"type":"transfer_operation","value":{"from":"faddy3","to":"faddy","amount":{"amount":"40000","precision":3,"nai":"@@000000021"},"memo":""}}' )
#       , ( 6, 5, 0, 1, 77, '{"type":"account_created_operation","value":{"new_account_name":"fabian","creator":"hello","initial_vesting_shares":{"amount":"0","precision":6,"nai":"@@000000037"},"initial_delegation":{"amount":"0","precision":6,"nai":"@@000000037"}}}' )
#       , ( 7, 4, 0, 2, 74, '{"type":"transfer_to_vesting_completed_operation","value":{"from_account":"faddy","to_account":"faddy","hive_vested":{"amount":"357000","precision":3,"nai":"@@000000021"},"vesting_shares_received":{"amount":"357000000","precision":6,"nai":"@@000000037"}}}' )
#       , ( 8, 4, 0, 3, 70, '{"type":"effective_comment_vote_operation","value":{"voter":"steemit78","author":"steemit","permlink":"firstpost","weight":"5100000000","rshares":5100,"total_vote_weight":"5100000000","pending_payout":{"amount":"0","precision":3,"nai":"@@000000013"}}}' )
# ;

import json

if __name__ == "__main__":

  operations_types = """
    INSERT INTO hive.operation_types
    VALUES (0, 'vote_operation', FALSE )
        ,(1, 'comment_operation', FALSE )
        ,(2, 'transfer_operation', FALSE )
        ,(3, 'transfer_to_vesting_operation', FALSE )
        ,(4, 'withdraw_vesting_operation', FALSE )
        ,(5, 'limit_order_create_operation', FALSE )
        ,(6, 'limit_order_cancel_operation', FALSE )
        ,(7, 'feed_publish_operation', FALSE )
        ,(8, 'convert_operation', FALSE )
        ,(9, 'account_create_operation', FALSE )
        ,(10, 'account_update_operation', FALSE )
        ,(11, 'witness_update_operation', FALSE )
        ,(12, 'account_witness_vote_operation', FALSE )
        ,(13, 'account_witness_proxy_operation', FALSE )
        ,(14, 'pow_operation', FALSE )
        ,(15, 'custom_operation', FALSE )
        ,(16, 'report_over_production_operation', FALSE )
        ,(17, 'delete_comment_operation', FALSE )
        ,(18, 'custom_json_operation', FALSE )
        ,(19, 'comment_options_operation', FALSE )
        ,(20, 'set_withdraw_vesting_route_operation', FALSE )
        ,(21, 'limit_order_create2_operation', FALSE )
        ,(22, 'claim_account_operation', FALSE )
        ,(23, 'create_claimed_account_operation', FALSE )
        ,(24, 'request_account_recovery_operation', FALSE )
        ,(25, 'recover_account_operation', FALSE )
        ,(26, 'change_recovery_account_operation', FALSE )
        ,(27, 'escrow_transfer_operation', FALSE )
        ,(28, 'escrow_dispute_operation', FALSE )
        ,(29, 'escrow_release_operation', FALSE )
        ,(30, 'pow2_operation', FALSE )
        ,(31, 'escrow_approve_operation', FALSE )
        ,(32, 'transfer_to_savings_operation', FALSE )
        ,(33, 'transfer_from_savings_operation', FALSE )
        ,(34, 'cancel_transfer_from_savings_operation', FALSE )
        ,(35, 'custom_binary_operation', FALSE )
        ,(36, 'decline_voting_rights_operation', FALSE )
        ,(37, 'reset_account_operation', FALSE )
        ,(38, 'set_reset_account_operation', FALSE )
        ,(39, 'claim_reward_balance_operation', FALSE )
        ,(40, 'delegate_vesting_shares_operation', FALSE )
        ,(41, 'account_create_with_delegation_operation', FALSE )
        ,(42, 'witness_set_properties_operation', FALSE )
        ,(43, 'account_update2_operation', FALSE )
        ,(44, 'create_proposal_operation', FALSE )
        ,(45, 'update_proposal_votes_operation', FALSE )
        ,(46, 'remove_proposal_operation', FALSE )
        ,(47, 'update_proposal_operation', FALSE )
        ,(48, 'fill_convert_request_operation', TRUE )
        ,(49, 'author_reward_operation', TRUE )
        ,(50, 'curation_reward_operation', TRUE )
        ,(51, 'comment_reward_operation', TRUE )
        ,(52, 'liquidity_reward_operation', TRUE )
        ,(53, 'interest_operation', TRUE )
        ,(54, 'fill_vesting_withdraw_operation', TRUE )
        ,(55, 'fill_order_operation', TRUE )
        ,(56, 'shutdown_witness_operation', TRUE )
        ,(57, 'fill_transfer_from_savings_operation', TRUE )
        ,(58, 'hardfork_operation', TRUE )
        ,(59, 'comment_payout_update_operation', TRUE )
        ,(60, 'return_vesting_delegation_operation', TRUE )
        ,(61, 'comment_benefactor_reward_operation', TRUE )
        ,(62, 'producer_reward_operation', TRUE )
        ,(63, 'clear_null_account_balance_operation', TRUE )
        ,(64, 'proposal_pay_operation', TRUE )
        ,(65, 'sps_fund_operation', TRUE )
        ,(66, 'hardfork_hive_operation', TRUE )
        ,(67, 'hardfork_hive_restore_operation', TRUE )
        ,(68, 'delayed_voting_operation', TRUE )
        ,(69, 'consolidate_treasury_balance_operation', TRUE )
        ,(70, 'effective_comment_vote_operation', TRUE )
        ,(71, 'ineffective_delete_comment_operation', TRUE )
        ,(72, 'sps_convert_operation', TRUE )
        ,(73, 'expired_account_notification_operation', TRUE )
        ,(74, 'changed_recovery_account_operation', TRUE )
        ,(75, 'transfer_to_vesting_completed_operation', TRUE )
        ,(76, 'pow_reward_operation', TRUE )
        ,(77, 'vesting_shares_split_operation', TRUE )
        ,(78, 'account_created_operation', TRUE )
    ;
"""

  hash          = 'hash'

  offset = 1

  blocks_number = 5000
  transactions_number_per_block = 3
  transactions_multi_sig_number_per_block = 1

  blocks = 'INSERT INTO hive.blocks\n' + 'VALUES\n'
  transactions = 'INSERT INTO hive.transactions\n' + 'VALUES\n'
  transactions_multisig = 'INSERT INTO hive.transactions_multisig\n' + 'VALUES\n'
  operations = 'INSERT INTO hive.operations\n' + 'VALUES\n'

  counter = offset

  for b in range(blocks_number):
    if b % 1000 == 0:
      print("block: {}".format(b))
    actual_block_number = b + offset
    blocks = blocks + "{} ( {}, '{}{}', 'prev-hash-{}', '2016-06-22 19:10:21-07'::timestamp ) \n".format( '' if b == 0  else ',', actual_block_number, hash, b, b - 1 )

    for t in range(transactions_number_per_block):
      transactions = transactions + "{} ( {}, {}::SMALLINT, '{}-{}-{}', 666, 667, '2016-06-22 19:10:21-07'::timestamp, '\xBEEF' ) \n".format( '' if ( b == 0 and t == 0 ) else ',', actual_block_number, t, hash, actual_block_number, t )
      
      _val1 = "axa-" + str(counter)
      _val2 = "bxb-" + str(counter)
      _val3 = "cxc-" + str(counter)
      _val4 = "dxd-" + str(counter)

      op = { "type" : "account_witness_vote_operation", "value" : { "account":_val1, "witness":_val2, "approve":True } }
      operations = operations + "{}( {}, {}, {}, 0, 12, '{}' )\n".format('' if ( counter == offset ) else ',', counter, actual_block_number, t, json.dumps(op) )
      counter = counter + 1

      op2 = {"type":"producer_reward_operation","value":{"producer":_val3,"vesting_shares":{"amount":"1000","precision":3,"nai":"@@000000021"}}}
      operations = operations + "{}( {}, {}, {}, 1, 62, '{}' )\n".format('' if ( counter == offset ) else ',', counter, actual_block_number, t, json.dumps(op2) )
      counter = counter + 1

      op3 = {"type":"transfer_operation","value":{"from":_val4,"to":_val1,"amount":{"amount":"40000","precision":3,"nai":"@@000000021"},"memo":""}}
      operations = operations + "{}( {}, {}, {}, 1, 2, '{}' )\n".format('' if ( counter == offset ) else ',', counter, actual_block_number, t, json.dumps(op3) )
      counter = counter + 1

      op4 = {"type":"effective_comment_vote_operation","value":{"voter":_val1,"author":_val3,"permlink":"firstpost","weight":"5100000000","rshares":5100,"total_vote_weight":"5100000000","pending_payout":{"amount":"0","precision":3,"nai":"@@000000013"}}}
      operations = operations + "{}( {}, {}, {}, 1, 70, '{}' )\n".format('' if ( counter == offset ) else ',', counter, actual_block_number, t, json.dumps(op4) )
      counter = counter + 1

    for t in range(transactions_multi_sig_number_per_block):
      transactions_multisig = transactions_multisig + "{} ( '{}-{}-{}', '\xAAAAA' ) \n".format( '' if ( b == 0 and t == 0 ) else ',', hash, actual_block_number, t )

  blocks              = blocks + ";"
  transactions          = transactions + ";"
  transactions_multisig = transactions_multisig + ";"
  operations            = operations + ";"

  print( blocks )
  print( transactions )
  print( transactions_multisig )
  print( operations )

  with open("records.sql", "w") as f:
    f.write( operations_types )
    f.write( '\n\n' )
    f.write( blocks )
    f.write( '\n\n' )
    f.write( transactions )
    f.write( '\n\n' )
    f.write( transactions_multisig )
    f.write( '\n\n' )
    f.write( operations )
    f.write( '\n\n' )
