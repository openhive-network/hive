import pytest

import test_tools as tt

from .local_tools import as_string


ACCOUNTS = [f'account-{i}' for i in range(3)]

CORRECT_VALUES = [
        # FROM_, TO
        (ACCOUNTS[0], ACCOUNTS[1], 100),
        (ACCOUNTS[0], '', 100),

        # LIMIT
        (ACCOUNTS[0], ACCOUNTS[1], 0),
        (ACCOUNTS[0], ACCOUNTS[1], 1000),
]


@pytest.mark.parametrize(
    'from_, to, limit', [
        *CORRECT_VALUES,
        *as_string(CORRECT_VALUES),
        (ACCOUNTS[0], ACCOUNTS[1], True),  # bool is treated like numeric (0:1)
    ]
)
@pytest.mark.testnet
def test_list_rc_direct_delegations_with_correct_value(node, wallet, from_, to, limit):
    create_accounts_and_delegate_rc_from_account0_to_account1(wallet, accounts=ACCOUNTS)

    r = node.api.wallet_bridge.list_rc_direct_delegations([from_, to], limit)
    print()


@pytest.mark.rmeote_node_5m
def test_list_rc_direct_delegations_with_correct_value_5m(node5m):
    r = node5m.api.wallet_bridge.list_rc_direct_delegations(['gtg', ''], 10)
    print()


@pytest.mark.parametrize(
    'from_, to, limit', [
        # FROM_
        ('', '', 100),
        ('non-exist-acc', '', 100),
        ('true', '', 100),

        # TO
        (ACCOUNTS[0], 'true', 100),
        (ACCOUNTS[0], 'non-exist-acc', 100),

        # LIMIT
        (ACCOUNTS[0], ACCOUNTS[1], -1),
        (ACCOUNTS[0], '', 1001),
    ]
)
@pytest.mark.testnet
def test_list_rc_direct_delegations_with_incorrect_value(node, wallet, from_, to, limit):
    create_accounts_and_delegate_rc_from_account0_to_account1(wallet, accounts=ACCOUNTS)

    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.list_rc_direct_delegations([from_, to], limit)


@pytest.mark.parametrize(
    'from_, to, limit', [
        # FROM_
        ('100', ACCOUNTS[1], 100),
        (100, ACCOUNTS[1], 100),
        (True, ACCOUNTS[1], 100),
        ('', ACCOUNTS[1], 100),
        ([ACCOUNTS[0]], ACCOUNTS[1], 100),

        # TO
        (ACCOUNTS[0], 100, 100),
        (ACCOUNTS[0], '100', 100),
        (ACCOUNTS[0], True, 100),
        (ACCOUNTS[0], [ACCOUNTS[1]], 100),

        # LIMIT
        (ACCOUNTS[0], ACCOUNTS[1], 'incorrect_string_argument'),
        (ACCOUNTS[0], ACCOUNTS[1], 'true'),
        (ACCOUNTS[0], ACCOUNTS[1], [100]),

    ]
)
@pytest.mark.testnet
def test_list_rc_direct_delegations_with_incorrect_type_of_arguments(node, wallet, from_, to, limit):
    create_accounts_and_delegate_rc_from_account0_to_account1(wallet, accounts=ACCOUNTS)

    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.list_rc_direct_delegations([from_, to], limit)


def create_accounts_and_delegate_rc_from_account0_to_account1(wallet, accounts):
    wallet.create_accounts(len(accounts))
    wallet.api.transfer_to_vesting('initminer', accounts[0], tt.Asset.Test(0.1))
    wallet.api.delegate_rc(accounts[0], [accounts[1]], 5)
    