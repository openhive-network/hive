import pytest

METHODS_WITH_CORRECT_ARGUMENTS = [
    ['find_account_recovery_requests', {'accounts': ['hive.fund', 'initminer', 'miners', 'null', 'steem.dao', 'temp']}],
    ['find_accounts', {'accounts': ['hive.fund', 'initminer', 'miners', 'null', 'steem.dao', 'temp']}],
    ['find_change_recovery_account_requests', {'accounts': ['hive.fund', 'initminer', 'miners', 'null', 'steem.dao', 'temp']}],
    ['find_collateralized_conversion_requests', {'account': 'hive.fund'}],
    ['find_comments', {'comments': [['hive.fund', 'https://api.hive.blog']]}],
    ['find_decline_voting_rights_requests', {'accounts': ['hive.fund', 'initminer', 'miners', 'null', 'steem.dao', 'temp']}],
    ['find_escrows', {'from': 'initminer'}],
    ['find_hbd_conversion_requests', {'account': 'hive.fund'}],
    ['find_limit_orders', {'account': 'hive.fund'}],
    ['find_owner_histories', {'owner': 'hive.fund'}],
    ['find_proposals', {'proposal_ids': [0, 1, 2, 3]}],
    ['find_recurrent_transfers', {'from': 'initminer'}],
    ['find_savings_withdrawals', {'account': 'hive.fund'}],
    ['find_vesting_delegation_expirations', {'account': 'hive.fund'}],
    ['find_vesting_delegations', {'account': 'hive.fund'}],
    ['find_withdraw_vesting_routes', {'account': 'hive.fund', 'order': 'by_destination'}],
    ['find_witnesses', {'owners': ['initminer']}],
    ['get_active_witnesses', {}],
    ['get_active_witnesses', {'include_future':True}],
    ['get_comment_pending_payouts', {'comments': [['initminer', 'test-permlink']]}],
    ['get_config', {}],
    ['get_current_price_feed', {}],
    ['get_dynamic_global_properties', {}],
    ['get_feed_history', {}],
    ['get_hardfork_properties', {}],
    ['get_order_book', {'limit': 100}],
    ['get_potential_signatures', {'trx': {'ref_block_num': 0, 'ref_block_prefix': 0, 'expiration': '2022-04-11T10:29:00',
                                          'operations': [], 'extensions': [], 'signatures': []}}],
    ['get_required_signatures', {'trx': {'ref_block_num': 0, 'ref_block_prefix': 0, 'expiration': '2022-04-11T10:29:00',
                                         'operations': [], 'extensions': [], 'signatures': []}, 'available_keys': []}],
    ['get_reward_funds', {}],
    ['get_transaction_hex', {'trx': {'ref_block_num': 0, 'ref_block_prefix': 0, 'expiration': '2022-04-11T10:29:00',
                                     'operations': [], 'extensions': [], 'signatures': []}}], ['get_version', {}],
    ['get_witness_schedule', {}],
    ['get_witness_schedule', {'include_future':True}],
    ['is_known_transaction', {'id': '0000000000000000000000000000000000000000'}],
    ['list_account_recovery_requests', {'start': '', 'limit': 100, 'order': 'by_account'}],
    ['list_accounts', {'start': '', 'limit': 100, 'order': 'by_name'}],
    ['list_change_recovery_account_requests', {'start': '', 'limit': 100, 'order': 'by_account'}],
    ['list_collateralized_conversion_requests', {'start': [''], 'limit': 100, 'order': 'by_account'}],
    ['list_decline_voting_rights_requests', {'start': '', 'limit': 100, 'order': 'by_account'}],
    ['list_escrows', {'start': ['initminer', 0], 'limit': 100, 'order': 'by_from_id'}],
    ['list_hbd_conversion_requests', {'start': ['initminer', 0], 'limit': 100, 'order': 'by_account'}],
    ['list_limit_orders', {'start': ['initminer', 0], 'limit': 100, 'order': 'by_account'}],
    ['list_owner_histories', {'start': ['initminer', '2022-04-11T10:29:00'], 'limit': 100}],
    ['list_proposal_votes', {'start': ['initminer'], 'limit': 100, 'order': 'by_voter_proposal',
                             'order_direction': 'ascending', 'status': 'all'}],
    ['list_proposals', {'start': ['initminer'], 'limit': 100, 'order': 'by_creator', 'order_direction': 'ascending',
                        'status': 'all'}],
    ['list_savings_withdrawals', {'start': ['2022-04-11T10:29:00', 'initminer', 0], 'limit': 100, 'order': 'by_complete_from_id'}],
    ['list_vesting_delegation_expirations', {'start': ['initminer', '2022-04-11T10:29:00', 0], 'limit': 100, 'order': 'by_account_expiration'}],
    ['list_vesting_delegations', {'start': ['hive.fund', 'initminer'], 'limit': 100, 'order': 'by_delegation'}],
    ['list_withdraw_vesting_routes', {'start': ['hive.fund', 'initminer'], 'limit': 100, 'order': 'by_withdraw_route'}],
    ['list_witness_votes', {'start': ['initminer', 'hive.fund'], 'limit': 100, 'order': 'by_witness_account'}],
    ['list_witnesses', {'start': '', 'limit': 100, 'order': 'by_name'}],
    ['verify_authority', {'trx': {'ref_block_num': 0, 'ref_block_prefix': 0, 'expiration': '2022-04-11T10:29:00',
                                  'operations': [], 'extensions': [], 'signatures': []}}],
    ['verify_signatures', {'hash': '0000000000000000000000000000000000000000000000000000000000000000', 'signatures': [],
                           'required_owner': [], 'required_active': [], 'required_posting': [], 'required_other': []}]]


@pytest.mark.parametrize(
    'api_database_method, arguments',
    METHODS_WITH_CORRECT_ARGUMENTS
)
def test_run_method_with_additional_argument(node, api_database_method, arguments):
    getattr(node.api.database, api_database_method)(**arguments, arg=True)


@pytest.mark.parametrize(
    'api_database_method, arguments',
    METHODS_WITH_CORRECT_ARGUMENTS
)
def test_run_method_with_correct_arguments(node, api_database_method, arguments):
    getattr(node.api.database, api_database_method)(**arguments)
