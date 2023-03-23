import pytest

import test_tools as tt
from hive_local_tools import run_for

def get_commands(commands_with_arguments):
    commands = []
    for command_with_argument in commands_with_arguments:
        command = command_with_argument[0]
        commands.append(command)
    return commands


def is_mainnet_5m_node(node) -> bool:
    return True if isinstance(node, tt.RemoteNode) and node.get_last_block_number() <= 5_000_000 else False


COMMANDS_WITH_CORRECT_ARGUMENTS = [
    ('list_proposals', ([""], 100, 29, 0, 0)),
    ('list_proposal_votes', ([""], 100, 33, 0, 0)),
    ('find_proposals', ([0],)),
    ('find_rc_accounts', ([''],)),
    ('list_rc_accounts', ('initminer', 100)),
    ('list_rc_direct_delegations', (['initminer', ''], 100)),
    ('get_account', ('get_account',)),
    ('get_account_history', ('non-exist-acc', -1, 1000)),
    ('get_accounts', (['non-exist-acc'],)),
    ('get_block', (0,)),
    ('get_ops_in_block', (0, True)),
    ('get_open_orders', ('non-exist-acc',)),
    ('get_owner_history', ('non-exist-acc',)),
    ('is_known_transaction', ('0000000000000000000000000000000000000000',)),
    ('get_reward_fund', ('post',)),
    ('get_order_book', (10,)),
    ('get_witness', ('non-exist-acc',)),
    ('get_active_witnesses', (False,)),
    ('get_witness_schedule', (False,)),
    ('list_accounts', ('non-exist-acc', 100)),
]


@pytest.mark.parametrize(
    'wallet_bridge_api_command', [
        *get_commands(COMMANDS_WITH_CORRECT_ARGUMENTS),
        'get_transaction',
        'get_conversion_requests',
        'get_collateralized_conversion_requests',
        'find_recurrent_transfers',
        'broadcast_transaction',
        'broadcast_transaction_synchronous',
        'list_my_accounts',
        'get_withdraw_routes',
    ]
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_run_command_without_arguments_where_arguments_are_required(node, wallet_bridge_api_command):
    with pytest.raises(tt.exceptions.CommunicationError):
        getattr(node.api.wallet_bridge, wallet_bridge_api_command)()


@pytest.mark.parametrize(
    'wallet_bridge_api_command, arguments', [
        *COMMANDS_WITH_CORRECT_ARGUMENTS,
        ('get_chain_properties', ()),
        ('get_feed_history', ()),
        ('get_current_median_history_price', ()),
        ('get_dynamic_global_properties', ()),
        ('get_hardfork_version', ()),
    ]
)
@run_for("testnet", "mainnet_5m", "live_mainnet", enable_plugins=['account_history_api'])
def test_run_command_with_additional_argument(node, should_prepare, wallet_bridge_api_command, arguments):
    if is_mainnet_5m_node(node) and wallet_bridge_api_command == "get_reward_fund":
        with pytest.raises(tt.exceptions.CommunicationError):
            getattr(node.api.wallet_bridge, wallet_bridge_api_command)(*arguments, 'additional_string_argument')
    else:
        getattr(node.api.wallet_bridge, wallet_bridge_api_command)(*arguments, 'additional_string_argument')


@pytest.mark.parametrize(
    'wallet_bridge_api_command, arguments', [
        *COMMANDS_WITH_CORRECT_ARGUMENTS,
        ('get_transaction', ('transaction_id',)),
        ('get_conversion_requests', ('alice',)),
        ('get_collateralized_conversion_requests', ('alice',)),
        ('find_recurrent_transfers', ('alice',)),
        ('broadcast_transaction', ('transaction',)),
        ('broadcast_transaction_synchronous', ('transaction',)),
    ]
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_run_command_with_missing_argument(node, wallet_bridge_api_command, arguments):
    with pytest.raises(tt.exceptions.CommunicationError):
        getattr(node.api.wallet_bridge, wallet_bridge_api_command)(*arguments[:-1])
