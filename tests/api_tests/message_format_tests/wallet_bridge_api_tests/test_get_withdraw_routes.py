import pytest

import test_tools as tt

from .local_tools import as_string, run_for

ACCOUNT_NAMES = ['blocktrades', 'alice']

CORRECT_VALUES = [
    #  ACCOUNT NAME
    (ACCOUNT_NAMES[0], 'all'),
    ('non-exist-acc', 'all'),
    ('', 'all'),
    (100, 'all'),
    (True, 'all'),

    #  WITHDRAW ROUTE TYPE
    (ACCOUNT_NAMES[0], 'all'),
    (ACCOUNT_NAMES[0], 'incoming'),
    (ACCOUNT_NAMES[0], 'outgoing'),
    (ACCOUNT_NAMES[0], 0),
    (ACCOUNT_NAMES[0], 2),
]


@pytest.mark.parametrize(
    'account_name, withdraw_route_type', [
        *CORRECT_VALUES,
        *as_string(CORRECT_VALUES),
        (ACCOUNT_NAMES[0], False),
        (ACCOUNT_NAMES[0], True),
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_withdraw_routes_with_correct_value(prepared_node, should_prepare, account_name, withdraw_route_type):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        create_accounts_and_set_withdraw_vesting_route_between_them(wallet, from_=ACCOUNT_NAMES[0], to=ACCOUNT_NAMES[1])

    prepared_node.api.wallet_bridge.get_withdraw_routes(account_name, withdraw_route_type)


@pytest.mark.parametrize(
    'account_name, withdraw_route_type', [
        #  WITHDRAW ROUTE TYPE
        (ACCOUNT_NAMES[0], 'non-exist-argument'),
        (ACCOUNT_NAMES[0], 'true'),
        (ACCOUNT_NAMES[0], ''),
        (ACCOUNT_NAMES[0], '100'),
        (ACCOUNT_NAMES[0], '-1'),
        (ACCOUNT_NAMES[0], '3'),
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_withdraw_routes_with_incorrect_value(prepared_node, should_prepare, account_name, withdraw_route_type):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        create_accounts_and_set_withdraw_vesting_route_between_them(wallet, from_=ACCOUNT_NAMES[0], to=ACCOUNT_NAMES[1])

    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.get_withdraw_routes(account_name, withdraw_route_type)


@pytest.mark.parametrize(
    'account_name, withdraw_route_type', [
        #  ACCOUNT NAME
        ([ACCOUNT_NAMES[0]], 'all'),

        #  WITHDRAW ROUTE TYPE
        (ACCOUNT_NAMES[0], ['all']),
        (ACCOUNT_NAMES[0], 100),
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_withdraw_routes_with_incorrect_type_of_arguments(prepared_node, should_prepare, account_name, withdraw_route_type):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        create_accounts_and_set_withdraw_vesting_route_between_them(wallet, from_=ACCOUNT_NAMES[0], to=ACCOUNT_NAMES[1])

    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.get_withdraw_routes(account_name, withdraw_route_type)


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_withdraw_routes_with_additional_argument(prepared_node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        create_accounts_and_set_withdraw_vesting_route_between_them(wallet, from_=ACCOUNT_NAMES[0], to=ACCOUNT_NAMES[1])

    prepared_node.api.wallet_bridge.get_withdraw_routes(ACCOUNT_NAMES[0], 'all', 'additional_argument')


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_withdraw_routes_with_missing_argument(prepared_node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        create_accounts_and_set_withdraw_vesting_route_between_them(wallet, from_=ACCOUNT_NAMES[0], to=ACCOUNT_NAMES[1])

    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.get_withdraw_routes(ACCOUNT_NAMES[0])


def create_accounts_and_set_withdraw_vesting_route_between_them(wallet, from_, to):
    with wallet.in_single_transaction():
        wallet.api.create_account('initminer', from_, '{}')
        wallet.api.create_account('initminer', to, '{}')

    wallet.api.transfer_to_vesting('initminer', from_, tt.Asset.Test(500))
    wallet.api.set_withdraw_vesting_route(from_, to, 30, True)
