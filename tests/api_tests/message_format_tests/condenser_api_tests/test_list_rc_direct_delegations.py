import pytest

import test_tools as tt
from ..local_tools import as_string, run_for


ACCOUNTS = ['initminer', 'alice']

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
@run_for('testnet')
def test_list_rc_direct_delegations_with_correct_value(node, wallet, from_, to, limit):
    create_account_and_delegate_its_rc(wallet, accounts=ACCOUNTS)
    node.api.condenser.list_rc_direct_delegations([from_, to], limit)


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
@run_for('testnet')
def test_list_rc_direct_delegations_with_incorrect_value(node, wallet, from_, to, limit):
    create_account_and_delegate_its_rc(wallet, accounts=ACCOUNTS)

    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.condenser.list_rc_direct_delegations([from_, to], limit)


@run_for('testnet')
def test_list_rc_direct_delegations_with_additional_argument(node, wallet):
    create_account_and_delegate_its_rc(wallet, accounts=ACCOUNTS)

    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.condenser.list_rc_direct_delegations([ACCOUNTS[0], ACCOUNTS[1]], 100, 'additional-argument')


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
@run_for('testnet')
def test_list_rc_direct_delegations_with_incorrect_type_of_arguments(node, wallet, from_, to, limit):
    create_account_and_delegate_its_rc(wallet, accounts=ACCOUNTS)

    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.condenser.list_rc_direct_delegations([from_, to], limit)


@run_for('testnet')
def test_list_rc_direct_delegations_with_missing_argument(node, wallet):
    create_account_and_delegate_its_rc(wallet, accounts=ACCOUNTS)

    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.condenser.list_rc_direct_delegations([ACCOUNTS[0], ACCOUNTS[1]])


def create_account_and_delegate_its_rc(wallet, accounts):
    wallet.create_account(accounts[1], vests=tt.Asset.Test(50))
    wallet.api.delegate_rc(accounts[0], [accounts[1]], 5)
