import pytest

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.api.message_format import as_string
from hive_local_tools.api.message_format.wallet_bridge_api.constants import ACCOUNTS


CORRECT_VALUES = [
        # LOWERBOUND ACCOUNT
        (ACCOUNTS[0], 100),
        ('non-exist-acc', 100),
        ('', 100),
        ('true', 100),
        (True, 100),
        (100, 100),

        # LIMIT
        (ACCOUNTS[0], 0),
        (ACCOUNTS[0], 1000),
]


@pytest.mark.parametrize(
    'lowerbound_account, limit', [
        *CORRECT_VALUES,
        *as_string(CORRECT_VALUES),
        (ACCOUNTS[0], True),  # bool is treated like numeric (0:1)
    ]
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_list_accounts_with_correct_values_in_testnet(node, should_prepare, lowerbound_account, limit):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.create_accounts(len(ACCOUNTS))

    node.api.wallet_bridge.list_accounts(lowerbound_account, limit)

@pytest.mark.parametrize(
    'lowerbound_account, limit', [
        # LIMIT
        (ACCOUNTS[0], -1),
        (ACCOUNTS[0], 1001),
    ]
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_list_accounts_with_incorrect_values(node, should_prepare, lowerbound_account, limit):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.create_accounts(len(ACCOUNTS))

    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.list_accounts(lowerbound_account, limit)


@pytest.mark.parametrize(
    'lowerbound_account, limit', [
        # LOWERBOUND ACCOUNT
        (['example-array'], 100),

        # LIMIT
        (ACCOUNTS[0], 'incorrect_string_argument'),
        (ACCOUNTS[0], [100]),
    ]
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_list_accounts_with_incorrect_type_of_argument(node, lowerbound_account, limit):
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.list_accounts(lowerbound_account, limit)
