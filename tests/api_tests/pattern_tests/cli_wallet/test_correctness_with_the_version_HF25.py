import json

import pytest

from test_tools import paths_to_executables, RemoteNode, Wallet

"""
NO TESTED METHODS
1. 'get_private_key' - less of key in test wallet
2. 'get_private_key_from_password' - less of key in test wallet
3. 'list_my_accounts' - there is no account created with this wallet
4. 'get_conversion_requests' - there is no conversion request in 5m blocklog
5. 'get_collateralized_conversion_requests' - there is no collateralized conversion request in 5m blocklog
6. 'get_withdraw_routes' there is withdraw route in 5m blocklog
7. 'find_recurrent_transfers'  # work but return nothing
8. 'get_open_orders'  # work but return nothing
9. 'get_owner_history' # work but return nothing
10. 'get_encrypted_memo' # no work, less of operation of memo
11. 'list_proposals' # work but return nothing
12. 'find_proposals' # work but return nothing ( there is not proposals
13. 'list_proposal_votes'  # work but return nothing
14. 'help'  # no work
"""


@pytest.fixture
def remote_node_wallet():
    paths_to_executables.set_path_of('cli_wallet',
                                     '/home/dev/ComparationHF25HF26Mainnet/src/hive_HF26/build/programs/cli_wallet/cli_wallet')
    remote_node = RemoteNode(http_endpoint='0.0.0.0:18091', ws_endpoint='0.0.0.0:18090')
    return Wallet(attach_to=remote_node)


METHODS = [
    ('gethelp', ('get_block',)),
    ('about', ()),
    ('help', ()),
    ('is_new', ()),
    ('is_locked', ()),
    ('list_keys', ()),
    ('list_accounts', ('hiveio', 10)),
    ('list_witnesses', ('gtg', 10)),
    ('get_witness', ('gtg',)),
    ('get_account', ('gtg',)),
    ('get_block', (10,)),
    ('get_ops_in_block', (10, True)),
    ('get_feed_history', ()),
    ('get_account_history', ('gtg', -1, 10)),
    ('get_order_book', (100,)),
    ('get_prototype_operation', ('account_create_operation',)),
    ('get_active_witnesses', ()),
    ('get_transaction', ('82d2c772db5312024f572c9dfbe926e45391f8e9',)),

    # Methods impossible to test on blocklog 5m or cli_wallet hf26
    ('get_private_key', ()),
    ('get_private_key_from_password', ()),
    ('list_my_accounts', ()),
    ('get_conversion_requests', ('gtg',)),
    ('get_collateralized_conversion_requests', ('gtg',)),
    ('get_withdraw_routes', ('gtg', 'all')),
    ('find_recurrent_transfers', ('gtg',)),
    ('get_open_orders', ('gtg',)),
    ('get_owner_history', ('gtg',)),
    ('get_encrypted_memo', ('gtg', 'blocktrades', 'memo')),
    ('get_prototype_operation', ('account_create_operation',)),
    ('list_proposals', ([""], 100, 29, 0, 0)),
    ('find_proposals', ([0],)),
    ('list_proposal_votes', ([""], 100, 33, 0, 0)),
    ('info', ()),
]

PROTOTYPE_OPERATIONS = [
    'vote_operation',
    'comment_operation',
    'transfer_operation',
    'transfer_to_vesting_operation',
    'withdraw_vesting_operation',
    'limit_order_create_operation',
    'limit_order_cancel_operation',
    'feed_publish_operation',
    'convert_operation',
    'account_create_operation',
    'account_update_operation',
    'witness_update_operation',
    'account_witness_vote_operation',
    'account_witness_proxy_operation',
    'pow_operation',
    'custom_operation',
    'report_over_production_operation',
    'delete_comment_operation',
    'custom_json_operation',
    'comment_options_operation',
    'set_withdraw_vesting_route_operation',
    'limit_order_create2_operation',
    'claim_account_operation',
    'create_claimed_account_operation',
    'request_account_recovery_operation',
    'recover_account_operation',
    'change_recovery_account_operation',
    'escrow_transfer_operation',
    'escrow_dispute_operation',
    'escrow_release_operation',
    'pow2_operation',
    'escrow_approve_operation',
    'transfer_to_savings_operation',
    'transfer_from_savings_operation',
    'cancel_transfer_from_savings_operation',
    'custom_binary_operation',
    'decline_voting_rights_operation',
    'reset_account_operation',
    'set_reset_account_operation',
    'claim_reward_balance_operation',
    'delegate_vesting_shares_operation',
    'account_create_with_delegation_operation',
    'witness_set_properties_operation',
    'account_update2_operation',
    'create_proposal_operation',
    'update_proposal_votes_operation',
    'remove_proposal_operation',
    'update_proposal_operation',
    'collateralized_convert_operation',
    'recurrent_transfer_operation',
    'fill_convert_request_operation',
    'author_reward_operation',
    'curation_reward_operation',
    'comment_reward_operation',
    'liquidity_reward_operation',
    'interest_operation',
    'fill_vesting_withdraw_operation',
    'fill_order_operation',
    'shutdown_witness_operation',
    'fill_transfer_from_savings_operation',
    'hardfork_operation',
    'comment_payout_update_operation',
    'return_vesting_delegation_operation',
    'comment_benefactor_reward_operation',
    'producer_reward_operation',
    'clear_null_account_balance_operation',
    'proposal_pay_operation',
    'sps_fund_operation',
    'hardfork_hive_operation',
    'hardfork_hive_restore_operation',
    'delayed_voting_operation',
    'consolidate_treasury_balance_operation',
    'effective_comment_vote_operation',
    'ineffective_delete_comment_operation',
    'sps_convert_operation',
    'expired_account_notification_operation',
    'changed_recovery_account_operation',
    'transfer_to_vesting_completed_operation',
    'pow_reward_operation',
    'vesting_shares_split_operation',
    'account_created_operation',
    'fill_collateralized_convert_request_operation',
    'system_warning_operation',
    'fill_recurrent_transfer_operation',
    'failed_recurrent_transfer_operation',
    'limit_order_cancelled_operation'
]


@pytest.mark.parametrize(
    'cli_wallet_method, arguments', [
        *METHODS[0:18],
    ]
)
def test_comparison_of_get_methods(remote_node_wallet, cli_wallet_method, arguments):
    response_hf25 = read_from_json('output_files_hf25', cli_wallet_method)
    response_hf26 = getattr(remote_node_wallet.api, cli_wallet_method)(*arguments)

    assert response_hf25 == response_hf26


# @pytest.mark.parametrize(
#     'cli_wallet_method, arguments', [
#         *METHODS,
#     ]
# )
# def test_comparison_of_gethelp(remote_node_wallet, cli_wallet_method, arguments):
#     response_hf25 = read_from_json('output_files_hf25_gethelp', cli_wallet_method)
#     response_hf26 = getattr(remote_node_wallet.api, 'gethelp')(cli_wallet_method)
#
#     assert response_hf25 == response_hf26
#
#
# @pytest.mark.parametrize(
#     'operation', [
#         *PROTOTYPE_OPERATIONS,
#     ]
# )
# def test_comparison_of_get_prototype_operation(remote_node_wallet, operation):
#     response_hf25 = read_from_json('output_files_hf25_get_prototype_operation', operation)
#     response_hf26 = getattr(remote_node_wallet.api, 'get_prototype_operation')(operation)
#
#     assert response_hf25 == response_hf26


def read_from_json(folder_name, method_name):
    with open(f'{folder_name}/{method_name}.json', 'r') as json_file:
        data = json.load(json_file)
        json_file.close()
    return data
