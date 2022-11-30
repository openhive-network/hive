import pytest

import test_tools as tt

from ..local_tools import as_string

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
def test_get_withdraw_routes_with_correct_value(node, wallet, account_name, withdraw_route_type):
    create_accounts_and_set_withdraw_vesting_route_between_them(wallet, from_='alice', to='bob')
    node.api.wallet_bridge.get_withdraw_routes(account_name, withdraw_route_type)


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
def test_get_withdraw_routes_with_incorrect_value(node, wallet, account_name, withdraw_route_type):
    create_accounts_and_set_withdraw_vesting_route_between_them(wallet, from_='alice', to='bob')

    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.get_withdraw_routes(account_name, withdraw_route_type)


@pytest.mark.parametrize(
    'account_name, withdraw_route_type', [
        #  ACCOUNT NAME
        (['alice'], 'all'),

        #  WITHDRAW ROUTE TYPE
        ('alice', ['all']),
        ('alice', 100),
    ]
)
def test_get_withdraw_routes_with_incorrect_type_of_arguments(node, wallet, account_name, withdraw_route_type):
    create_accounts_and_set_withdraw_vesting_route_between_them(wallet, from_='alice', to='bob')

    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.get_withdraw_routes(account_name, withdraw_route_type)


def test_get_withdraw_routes_with_additional_argument(node, wallet):
    create_accounts_and_set_withdraw_vesting_route_between_them(wallet, from_='alice', to='bob')

    node.api.wallet_bridge.get_withdraw_routes('alice', 'all', 'additional_argument')


def test_get_withdraw_routes_with_missing_argument(node, wallet):
    create_accounts_and_set_withdraw_vesting_route_between_them(wallet, from_='alice', to='bob')

    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.get_withdraw_routes('alice')


def create_accounts_and_set_withdraw_vesting_route_between_them(wallet, from_, to):
    with wallet.in_single_transaction():
        wallet.api.create_account('initminer', from_, '{}')
        wallet.api.create_account('initminer', to, '{}')

    wallet.api.transfer_to_vesting('initminer', from_, tt.Asset.Test(500))
    wallet.api.set_withdraw_vesting_route(from_, to, 30, True)
