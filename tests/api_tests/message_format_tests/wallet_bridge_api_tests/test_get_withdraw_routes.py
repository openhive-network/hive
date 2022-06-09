import pytest

import test_tools as tt

from .local_tools import as_string, run_for


CORRECT_VALUES = [
    #  ACCOUNT NAME
    ('alice', 'all'),
    ('non-exist-acc', 'all'),
    ('', 'all'),
    (100, 'all'),
    (True, 'all'),

    #  WITHDRAW ROUTE TYPE
    ('alice', 'all'),
    ('alice', 'incoming'),
    ('alice', 'outgoing'),
    ('alice', 0),
    ('alice', 2),
]


@pytest.mark.parametrize(
    'account_name, withdraw_route_type', [
        *CORRECT_VALUES,
        *as_string(CORRECT_VALUES),
        ('alice', False),
        ('alice', True),
    ]
)
@run_for('testnet')
def test_get_withdraw_routes_with_correct_value_in_testnet(prepared_node, account_name, withdraw_route_type):
    wallet = tt.Wallet(attach_to=prepared_node)
    create_accounts_and_set_withdraw_vesting_route_between_them(wallet, from_='alice', to='bob')
    prepared_node.api.wallet_bridge.get_withdraw_routes(account_name, withdraw_route_type)


@pytest.mark.parametrize(
    'account_name, withdraw_route_type', [
        #  WITHDRAW ROUTE TYPE
        ('alice', 'non-exist-argument'),
        ('alice', 'true'),
        ('alice', ''),
        ('alice', '100'),
        ('alice', '-1'),
        ('alice', '3'),
    ]
)
@run_for('testnet')
def test_get_withdraw_routes_with_incorrect_value(prepared_node, account_name, withdraw_route_type):
    wallet = tt.Wallet(attach_to=prepared_node)
    create_accounts_and_set_withdraw_vesting_route_between_them(wallet, from_='alice', to='bob')

    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.get_withdraw_routes(account_name, withdraw_route_type)


@pytest.mark.parametrize(
    'account_name, withdraw_route_type', [
        #  ACCOUNT NAME
        (['alice'], 'all'),

        #  WITHDRAW ROUTE TYPE
        ('alice', ['all']),
        ('alice', 100),
    ]
)
@run_for('testnet')
def test_get_withdraw_routes_with_incorrect_type_of_arguments(prepared_node, account_name, withdraw_route_type):
    wallet = tt.Wallet(attach_to=prepared_node)
    create_accounts_and_set_withdraw_vesting_route_between_them(wallet, from_='alice', to='bob')

    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.get_withdraw_routes(account_name, withdraw_route_type)


@run_for('testnet')
def test_get_withdraw_routes_with_additional_argument(prepared_node):
    wallet = tt.Wallet(attach_to=prepared_node)
    create_accounts_and_set_withdraw_vesting_route_between_them(wallet, from_='alice', to='bob')

    prepared_node.api.wallet_bridge.get_withdraw_routes('alice', 'all', 'additional_argument')


@run_for('testnet')
def test_get_withdraw_routes_with_missing_argument(prepared_node):
    wallet = tt.Wallet(attach_to=prepared_node)
    create_accounts_and_set_withdraw_vesting_route_between_them(wallet, from_='alice', to='bob')

    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.get_withdraw_routes('alice')


def create_accounts_and_set_withdraw_vesting_route_between_them(wallet, from_, to):
    with wallet.in_single_transaction():
        wallet.api.create_account('initminer', from_, '{}')
        wallet.api.create_account('initminer', to, '{}')

    wallet.api.transfer_to_vesting('initminer', from_, tt.Asset.Test(500))
    wallet.api.set_withdraw_vesting_route(from_, to, 30, True)
