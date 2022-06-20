import pytest

import test_tools as tt

from .local_tools import as_string, run_for


ACCOUNTS = ['initminer', *[f'account-{i}' for i in range(3)]]


CORRECT_VALUES = [
    [ACCOUNTS[0]],
    ACCOUNTS,
    [True],  # in mainnet it corresponds to an account named 'true'
]

VALUES_EXPECT_EMPTY_RESPONSES = [
    [''],
    ['non-exist-acc'],
    [100]
]


@pytest.mark.parametrize(
    'rc_accounts', [
        *CORRECT_VALUES,
        *as_string(CORRECT_VALUES),
    ]
)
@run_for('testnet', 'mainnet_64m')
def test_find_rc_accounts_with_correct_values_and_existing_accounts(prepared_node, should_prepare, rc_accounts):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        wallet.create_accounts(len(ACCOUNTS))
        wallet.api.create_account('initminer', 'true', '{}')
    found_accounts = prepared_node.api.wallet_bridge.find_rc_accounts(rc_accounts)
    assert len(found_accounts) > 0


@pytest.mark.parametrize(
    'rc_accounts', [
        *VALUES_EXPECT_EMPTY_RESPONSES,
        *as_string(VALUES_EXPECT_EMPTY_RESPONSES),
    ]
)
@run_for('testnet', 'mainnet_64m')
def test_find_rc_accounts_with_correct_values_and_not_existing_accounts(prepared_node, should_prepare, rc_accounts):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        wallet.create_accounts(len(ACCOUNTS))
    rc_account = prepared_node.api.wallet_bridge.find_rc_accounts(rc_accounts)
    assert len(rc_account) == 0


@pytest.mark.parametrize(
    'rc_accounts', [
        "['non-exist-acc']",
        True,
        100,
        '100',
        'incorrect_string_argument',
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_find_rc_accounts_with_incorrect_type_of_argument(prepared_node, rc_accounts):
    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.find_rc_accounts(rc_accounts)
