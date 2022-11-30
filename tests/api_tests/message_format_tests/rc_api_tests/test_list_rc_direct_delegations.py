import pytest

import test_tools as tt

from ..local_tools import as_string
from ....local_tools import run_for


ACCOUNTS = ['initminer', 'alice']

CORRECT_VALUES = [
        # FROM_, TO
        (ACCOUNTS[0], ACCOUNTS[1], 100),
        (ACCOUNTS[0], '', 100),

        # LIMIT
        (ACCOUNTS[0], ACCOUNTS[1], 0),
        (ACCOUNTS[0], ACCOUNTS[1], 1000),
]


@pytest.fixture
def ready_node(node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        create_account_and_delegate_its_rc(wallet, accounts=ACCOUNTS)
    return node


@pytest.mark.parametrize(
    'from_, to, limit', [
        *CORRECT_VALUES,
        *as_string(CORRECT_VALUES),
        (ACCOUNTS[0], ACCOUNTS[1], True),  # bool is treated like numeric (0:1)
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_list_rc_direct_delegations_with_correct_value(ready_node, from_, to, limit):
    ready_node.api.rc.list_rc_direct_delegations(start=[from_, to], limit=limit)


@pytest.mark.parametrize(
    'from_, to, limit', [
        # FROM_
        ('', '', 100),
        ('non-exist-acc', '', 100),

        # TO
        (ACCOUNTS[0], 'non-exist-acc', 100),

        # LIMIT
        (ACCOUNTS[0], ACCOUNTS[1], -1),
        (ACCOUNTS[0], '', 1001),
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_list_rc_direct_delegations_with_incorrect_value(ready_node, from_, to, limit):
    with pytest.raises(tt.exceptions.CommunicationError):
        ready_node.api.rc.list_rc_direct_delegations(start=[from_, to], limit=limit)


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_list_rc_direct_delegations_with_additional_argument(ready_node):
    ready_node.api.rc.list_rc_direct_delegations(start=[ACCOUNTS[0], ACCOUNTS[1]],
                                                 limit=100,
                                                 additional_argument='additional-argument',
                                                 )


@pytest.mark.parametrize(
    'from_, to, limit', [
        # FROM_
        ('100', ACCOUNTS[1], 100),
        (100, ACCOUNTS[1], 100),
        ('', ACCOUNTS[1], 100),
        ([ACCOUNTS[0]], ACCOUNTS[1], 100),

        # TO
        (ACCOUNTS[0], 100, 100),
        (ACCOUNTS[0], '100', 100),
        (ACCOUNTS[0], [ACCOUNTS[1]], 100),

        # LIMIT
        (ACCOUNTS[0], ACCOUNTS[1], 'incorrect_string_argument'),
        (ACCOUNTS[0], ACCOUNTS[1], 'true'),
        (ACCOUNTS[0], ACCOUNTS[1], [100]),

    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_list_rc_direct_delegations_with_incorrect_type_of_arguments(ready_node, from_, to, limit):
    with pytest.raises(tt.exceptions.CommunicationError):
        ready_node.api.rc.list_rc_direct_delegations(start=[from_, to], limit=limit)


@pytest.mark.skip(reason="https://gitlab.syncad.com/hive/hive/-/issues/422")
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_list_rc_direct_delegations_with_missing_argument(ready_node):
    with pytest.raises(tt.exceptions.CommunicationError):
        ready_node.api.rc.list_rc_direct_delegations(start=[ACCOUNTS[0], ACCOUNTS[1]])


def create_account_and_delegate_its_rc(wallet, accounts):
    wallet.create_account(accounts[1], vests=tt.Asset.Test(50))
    wallet.api.delegate_rc(accounts[0], [accounts[1]], 5)
