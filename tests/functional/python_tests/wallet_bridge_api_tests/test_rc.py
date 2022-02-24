from pykwalify.core import Core
import pytest

from test_tools import Asset, logger, Wallet, World
import test_tools.exceptions

import schemas


@pytest.mark.parametrize(
    'wallet_bridge_api_command', [
        'get_block',
        'get_ops_in_block',
        'get_withdraw_routes',
        'list_my_accounts',
        'list_accounts',
        'get_account',
        'get_accounts',
        'get_transaction',
        'list_witnesses',
        'get_witness',
        'get_conversion_requests',
        'get_collateralized_conversion_requests',
        'get_order_book',
        'get_open_orders',
        'get_owner_history',
        'get_account_history',
        'list_proposals',
        'find_proposals',
        'is_known_transaction',
        'list_proposal_votes',
        'get_reward_fund',
        'broadcast_transaction_synchronous',
        'broadcast_transaction',
        'find_recurrent_transfers',
        'find_rc_accounts',
        'list_rc_accounts',
        'list_rc_direct_delegations',
    ]
)
def test_run_command_without_arguments_where_arguments_are_required(node, wallet_bridge_api_command):
    getattr(node.api.wallet_bridge, wallet_bridge_api_command)()

    with pytest.raises(test_tools.exceptions.CommunicationError):
        getattr(node.api.wallet_bridge, wallet_bridge_api_command)()


def get_accounts_name(accounts):
    return [account.name for account in accounts]


def validate_message(message, schema):
    return Core(source_data=message, schema_data=schema).validate(raise_exception=True)


@pytest.mark.parametrize(
    'find_rc_accounts_wrong_parameters', [
        '',
        None,
        1,
        'random_string',
        False,
        (2, 'tup', -1),
        [2],
        [],
    ]
)
def test_find_rc_accounts_with_incorrect_values(node, wallet, find_rc_accounts_wrong_parameters):
    get_accounts_name(wallet.create_accounts(5, 'account'))
    wallet.api.create_account('initminer', 'alice', '{}')

    with pytest.raises(test_tools.exceptions.CommunicationError):
        node.api.wallet_bridge.find_rc_accounts([find_rc_accounts_wrong_parameters])
    # x = node.api.wallet_bridge.find_rc_accounts([find_rc_accounts_wrong_parameters])

    all_acc = node.api.wallet_bridge.list_accounts('', 1000)
    x = node.api.wallet_bridge.find_rc_accounts('account-0')
    print()


def test_find_rc_accounts_output_data_schema(node, wallet):
    get_accounts_name(wallet.create_accounts(5, 'account'))
    wallet.api.create_account('initminer', 'alice', '{}')

    validate_message(
        node.api.wallet_bridge.find_rc_accounts('account-0', 100),
        schemas.find_rc_accounts,
    )


def test_list_rc_accounts_output_data_schema(node, wallet):
    accounts = get_accounts_name(wallet.create_accounts(5, 'account'))
    wallet.api.transfer_to_vesting('initminer', accounts[0], Asset.Test(0.1))
    wallet.api.delegate_rc(accounts[0], [accounts[1]], 10)

    validate_message(
        node.api.wallet_bridge.list_rc_accounts('', 100),
        schemas.list_rc_accounts
    )


def test_list_rc_direct_delegations_output_data_schema(node, wallet):
    accounts = get_accounts_name(wallet.create_accounts(5, 'account'))
    wallet.api.create_account('initminer', 'alice', '{}')

    wallet.api.transfer_to_vesting('initminer', accounts[0], Asset.Test(0.1))
    wallet.api.transfer_to_vesting('initminer', accounts[1], Asset.Test(0.1))
    wallet.api.delegate_rc(accounts[0], [accounts[1], accounts[2]], 100)
    wallet.api.delegate_rc(accounts[0], [accounts[1]], 12)
    wallet.api.delegate_rc(accounts[0], [accounts[2], accounts[3], accounts[1]], 55)

    validate_message(
        node.api.wallet_bridge.list_rc_direct_delegations([accounts[0], accounts[1]], 100),
        schemas.list_rc_direct_delegations
    )


# def test_broadcast_transaction(node, wallet):
#     creator = 'initminer'
#     transaction = wallet.api.create_account(creator, 'jonas', '{}', broadcast=False)
#     transaction = wallet.api.sign_transaction(transaction, broadcast=False)
#
#     node.api.wallet_bridge.broadcast_transaction(transaction)
#     print()
#
#
# def test_broadcast_transaction_synchronous(node, wallet):
#     creator = 'initminer'
#     transaction = wallet.api.create_account(creator, 'jonas', '{}', broadcast=False)
#     transaction = wallet.api.sign_transaction(transaction, broadcast=False)
#
#     node.api.wallet_bridge.broadcast_transaction_synchronous(transaction)
#     print()
