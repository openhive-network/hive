import pytest

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.api.message_format import as_string
from hive_local_tools.api.message_format.wallet_bridge_api.constants import ACCOUNTS, MAINNET_ACCOUNT


CORRECT_VALUES = [
    [ACCOUNTS[0]],
    [MAINNET_ACCOUNT],
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
@run_for("testnet", "live_mainnet")
def test_find_rc_accounts_with_correct_value(node, should_prepare, rc_accounts):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.create_account("true")
        wallet.create_accounts(len(ACCOUNTS))
    found_accounts = node.api.wallet_bridge.find_rc_accounts(rc_accounts)
    assert len(found_accounts) > 0


@pytest.mark.parametrize(
    'rc_accounts', [
        *CORRECT_VALUES,
        *as_string(CORRECT_VALUES),
    ]
)
@run_for("mainnet_5m")
def test_find_rc_accounts_with_correct_values_and_existing_accounts_in_mainnet_5m(node, should_prepare, rc_accounts):
    # rc service wasn't available until the HF20. The expected response for 5 million block node is always an empty list
    found_accounts = node.api.wallet_bridge.find_rc_accounts(rc_accounts)
    assert found_accounts == []


@pytest.mark.parametrize(
    'rc_accounts', [
        *VALUES_EXPECT_EMPTY_RESPONSES,
        *as_string(VALUES_EXPECT_EMPTY_RESPONSES),
    ]
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_find_rc_accounts_with_correct_values_and_not_existing_accounts(node, should_prepare, rc_accounts):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.create_accounts(len(ACCOUNTS))
    rc_account = node.api.wallet_bridge.find_rc_accounts(rc_accounts)
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
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_find_rc_accounts_with_incorrect_type_of_argument(node, rc_accounts):
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.find_rc_accounts(rc_accounts)
