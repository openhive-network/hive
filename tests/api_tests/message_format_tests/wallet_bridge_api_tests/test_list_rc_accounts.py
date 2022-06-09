import pytest

import test_tools as tt

from .local_tools import as_string, run_for


ACCOUNTS = [f'account-{i}' for i in range(3)]

CORRECT_VALUES = [
        # RC ACCOUNT
        (ACCOUNTS[0], 100),
        (ACCOUNTS[-1], 100),
        ('non-exist-acc', 100),
        ('true', 100),
        ('', 100),
        (100, 100),
        (True, 100),

        # LIMIT
        (ACCOUNTS[0], 0),
        (ACCOUNTS[0], 1000),
]


@pytest.mark.parametrize(
    'rc_account, limit', [
        *CORRECT_VALUES,
        *as_string(CORRECT_VALUES),
        (ACCOUNTS[0], True),  # bool is treated like numeric (0:1)
    ]
)
@run_for('testnet')
def test_list_rc_accounts_with_correct_values_in_testnet(prepared_node, rc_account, limit):
    wallet = tt.Wallet(attach_to=prepared_node)
    wallet.create_accounts(len(ACCOUNTS))
    prepared_node.api.wallet_bridge.list_rc_accounts(rc_account, limit)


@pytest.mark.parametrize(
    'rc_account, limit', [
        # LIMIT
        (ACCOUNTS[0], -1),
        (ACCOUNTS[0], 1001),
    ]
)
@run_for('testnet')
def test_list_rc_accounts_with_incorrect_values(prepared_node, rc_account, limit):
    wallet = tt.Wallet(attach_to=prepared_node)
    wallet.create_accounts(len(ACCOUNTS))

    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.list_rc_accounts(rc_account, limit)


@pytest.mark.parametrize(
    'rc_account, limit', [
        # WITNESS
        (['example-array'], 100),

        # LIMIT
        (ACCOUNTS[0], 'incorrect_string_argument'),
        (ACCOUNTS[0], [100]),
        (ACCOUNTS[0], 'true'),
    ]
)
@run_for('testnet')
def test_list_rc_accounts_with_incorrect_type_of_arguments(prepared_node, rc_account, limit):
    wallet = tt.Wallet(attach_to=prepared_node)
    wallet.create_accounts(len(ACCOUNTS))
    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.list_rc_accounts(rc_account, limit)
