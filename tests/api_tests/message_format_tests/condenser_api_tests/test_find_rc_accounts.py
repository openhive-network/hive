import pytest

import test_tools as tt

from ..local_tools import as_string, run_for

ACCOUNTS = [f'account-{i}' for i in range(3)]

CORRECT_VALUES = [
    [''],
    ['non-exist-acc'],
    [ACCOUNTS[0]],
    ACCOUNTS,
    [100],
    [True],
]


@pytest.mark.parametrize(
    'rc_accounts', [
        *CORRECT_VALUES,
        *as_string(CORRECT_VALUES),
    ]
)
@run_for('testnet')
def test_find_rc_accounts_with_correct_value(prepared_node, wallet, rc_accounts):
    wallet.create_accounts(len(ACCOUNTS))
    prepared_node.api.condenser.find_rc_accounts(rc_accounts)


@pytest.mark.parametrize(
    'rc_accounts', [
        "['non-exist-acc']",
        True,
        100,
        '100',
        'incorrect_string_argument',
    ]
)
@run_for('testnet')
def test_find_rc_accounts_with_incorrect_type_of_argument(prepared_node, rc_accounts):
    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.condenser.find_rc_accounts(rc_accounts)


@run_for('testnet')
def test_find_rc_accounts_with_missing_argument(prepared_node):
    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.condenser.find_rc_accounts()


@run_for('testnet')
def test_find_rc_accounts_with_additional_argument(prepared_node):
    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.condenser.find_rc_accounts([ACCOUNTS[0]], 'additional_argument')
