import pytest

from test_tools import exceptions

# TODO BUG LIST!
# 1. Problem with run command wallet_bridge_api.find_proposals() with additional argument. (#BUG1)
# 2. Problem with run command wallet_bridge_api.get_account() with additional argument. (#BUG2)


COMMANDS_WITH_CORRECT_ARGUMENTS = [
    ('list_proposals', ([""], 100, 29, 0, 0)),
    ('list_proposal_votes', ([""], 100, 33, 0, 0)),
    ('find_proposals', ([0])),
    ('get_active_witnesses', ()),
    ('get_current_median_history_price', ()),
    ('get_dynamic_global_properties', ()),
    ('get_feed_history', ()),
    ('get_hardfork_version', ()),
    ('get_witness_schedule', ()),
    ('find_rc_accounts', ([''])),
    ('list_rc_accounts', ('initminer', 100)),
    ('list_rc_direct_delegations', (['initminer', ''], 100)),
    ('get_account', ('get_account')),
    ('get_account_history', ('non-exist-acc', -1, 1000)),
    ('get_accounts', (['non-exist-acc'])),
]


@pytest.mark.parametrize(
    'wallet_bridge_api_command', [
        'broadcast_transaction',
        'broadcast_transaction_synchronous',
        'find_proposals',
        'find_rc_accounts',
        'find_recurrent_transfers',
        'get_account',
        'get_account_history',
        'get_accounts',
        'get_block',
        'get_collateralized_conversion_requests',
        'get_conversion_requests',
        'get_open_orders',
        'get_ops_in_block',
        'get_order_book',
        'get_owner_history',
        'get_reward_fund',
        'get_transaction',
        'get_withdraw_routes',
        'get_witness',
        'is_known_transaction',
        'list_accounts',
        'list_my_accounts',
        'list_proposal_votes',
        'list_proposals',
        'list_rc_accounts',
        'list_rc_direct_delegations',
        'list_witnesses',
    ]
)
def test_run_command_without_arguments_where_arguments_are_required(node, wallet_bridge_api_command):
    # TODO In final version use 'COMMANDS_WITH_CORRECT_ARGUMENTS'
    with pytest.raises(exceptions.CommunicationError):
        getattr(node.api.wallet_bridge, wallet_bridge_api_command)()


@pytest.mark.parametrize(
    'wallet_bridge_api_command, arguments', COMMANDS_WITH_CORRECT_ARGUMENTS
)
def test_run_command_with_additional_argument(node, wallet_bridge_api_command, arguments):
    #'list_my_accounts' is testing in file test_accounts.py
    if wallet_bridge_api_command != 'find_proposals' and wallet_bridge_api_command != 'get_account':  # BUG1, BUG2
        getattr(node.api.wallet_bridge, wallet_bridge_api_command)(*arguments, 'additional_string_argument')


@pytest.mark.parametrize(
    'wallet_bridge_api_command, arguments', COMMANDS_WITH_CORRECT_ARGUMENTS
)
def test_run_command_with_missing_argument(node, wallet_bridge_api_command, arguments):
    #'list_my_accounts' is testing in file test_accounts.py
    if len(arguments) >= 1:
        with pytest.raises(exceptions.CommunicationError) as exception:
            getattr(node.api.wallet_bridge, wallet_bridge_api_command)(*arguments[:-1])
