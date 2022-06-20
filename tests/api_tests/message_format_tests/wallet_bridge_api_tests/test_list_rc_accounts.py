import pytest

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.api.message_format import as_string
from hive_local_tools.api.message_format.wallet_bridge_api.constants import ACCOUNTS, MAINNET_ACCOUNT


CORRECT_VALUES = [
        # RC ACCOUNT
        (ACCOUNTS[0], 100),
        (MAINNET_ACCOUNT, 100),
        (ACCOUNTS[-1], 100),
        ('non-exist-acc', 100),
        ('true', 100),
        ('', 100),
        (100, 100),
        (True, 100),

        # LIMIT
        (ACCOUNTS[0], 1),
        (ACCOUNTS[0], 1000),
]


@pytest.mark.parametrize(
    'rc_account, limit', [
        *CORRECT_VALUES,
        *as_string(CORRECT_VALUES),
        (ACCOUNTS[0], True),  # bool is treated like numeric (0:1)
    ]
)
@run_for("testnet", "live_mainnet")
def test_list_rc_accounts_with_correct_values(node, should_prepare, rc_account, limit):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.create_account("true")
        wallet.create_accounts(len(ACCOUNTS))
        node.api.wallet_bridge.list_rc_accounts(rc_account, limit)

    rc_accounts = node.api.wallet_bridge.list_rc_accounts(rc_account, limit)

    # Ensure that response is not empty when requested non-zero number of accounts
    if int(limit) > 0:
        assert len(rc_accounts) > 0


@pytest.mark.parametrize(
    'rc_account, limit', [
        *CORRECT_VALUES,
        *as_string(CORRECT_VALUES),
        (ACCOUNTS[0], True),  # bool is treated like numeric (0:1)
    ]
)
@run_for("mainnet_5m")
def test_list_rc_account_with_correct_values_in_mainnet_5m(node, rc_account, limit):
    # rc service wasn't available until the HF20. The expected response for 5 million block node is always an empty list
    rc_accounts = node.api.wallet_bridge.list_rc_accounts(rc_account, limit)
    assert rc_accounts == []


@pytest.mark.parametrize(
    'rc_account, limit', [
        # LIMIT
        (ACCOUNTS[0], -1),
        (ACCOUNTS[0], 0),
        (ACCOUNTS[0], 1001),
    ]
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_list_rc_accounts_with_incorrect_values(node, should_prepare, rc_account, limit):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.create_accounts(len(ACCOUNTS))

    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.list_rc_accounts(rc_account, limit)


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
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_list_rc_accounts_with_incorrect_type_of_arguments(node, should_prepare, rc_account, limit):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.create_accounts(len(ACCOUNTS))

    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.list_rc_accounts(rc_account, limit)
