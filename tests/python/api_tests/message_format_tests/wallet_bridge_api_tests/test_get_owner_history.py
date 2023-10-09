import pytest

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.api.message_format import as_string

CORRECT_VALUES = [
    "",
    "non-exist-acc",
    "alice",
    100,
    True,
]


@pytest.mark.parametrize(
    "account_name",
    [
        *CORRECT_VALUES,
        *as_string(CORRECT_VALUES),
    ],
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_owner_history_with_correct_value(node, should_prepare, account_name):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        create_and_update_account(wallet, account_name="alice")
    node.api.wallet_bridge.get_owner_history(account_name)


@pytest.mark.parametrize("account_name", [["alice"]])
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_owner_history_with_incorrect_type_of_argument(node, should_prepare, account_name):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        create_and_update_account(wallet, account_name="alice")
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.get_owner_history(account_name)


def create_and_update_account(wallet, account_name):
    wallet.api.create_account("initminer", account_name, "{}")
    wallet.api.transfer_to_vesting("initminer", account_name, tt.Asset.Test(500))
    key = "TST8grZpsMPnH7sxbMVZHWEu1D26F3GwLW1fYnZEuwzT4Rtd57AER"
    wallet.api.update_account(account_name, "{}", key, key, key, key)
