import pytest

import test_tools as tt

from .local_tools import as_string, run_for


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
@run_for('testnet')
def test_list_rc_direct_delegations_with_correct_value_in_testnet(prepared_node, from_, to, limit):
    wallet = tt.Wallet(attach_to=prepared_node)
    create_accounts_and_delegate_rc_from_account0_to_account1(wallet, accounts=ACCOUNTS)

    prepared_node.api.wallet_bridge.list_rc_direct_delegations([from_, to], limit)


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
def test_list_rc_direct_delegations_with_incorrect_value(prepared_node, from_, to, limit):
    wallet = tt.Wallet(attach_to=prepared_node)
    create_accounts_and_delegate_rc_from_account0_to_account1(wallet, accounts=ACCOUNTS)

    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.list_rc_direct_delegations([from_, to], limit)


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
def test_list_rc_direct_delegations_with_incorrect_type_of_arguments(prepared_node, from_, to, limit):
    wallet = tt.Wallet(attach_to=prepared_node)
    create_accounts_and_delegate_rc_from_account0_to_account1(wallet, accounts=ACCOUNTS)

    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.list_rc_direct_delegations([from_, to], limit)


def create_accounts_and_delegate_rc_from_account0_to_account1(wallet, accounts):
    wallet.create_accounts(len(accounts))
    wallet.api.transfer_to_vesting('initminer', accounts[0], tt.Asset.Test(0.1))
    wallet.api.delegate_rc(accounts[0], [accounts[1]], 5)
    