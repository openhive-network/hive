from __future__ import annotations

import pytest
from beekeepy.exceptions import ErrorInResponseError

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.api.message_format import as_string


@pytest.mark.parametrize(
    "reward_fund_name",
    [
        "alice",
        "bob",
    ],
)
@pytest.mark.parametrize("asset", [tt.Asset.Test(20), tt.Asset.Tbd(20)])
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_find_recurrent_transfers_with_correct_value(
    node: tt.InitNode | tt.RemoteNode, should_prepare: bool, reward_fund_name: str, asset: tt.Asset
) -> None:
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        create_accounts_and_make_recurrent_transfer(wallet, from_account="alice", to_account="bob", asset=asset)

    node.api.wallet_bridge.find_recurrent_transfers(reward_fund_name)


INCORRECT_VALUES = [
    "non-exist-acc",
    "",
    100,
    ["initminer"],
]


@pytest.mark.parametrize(
    "reward_fund_name",
    [
        *INCORRECT_VALUES,
        *as_string(INCORRECT_VALUES),
    ],
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_find_recurrent_transfers_with_incorrect_value(
    node: tt.InitNode | tt.RemoteNode, reward_fund_name: int | list | str
) -> None:
    with pytest.raises(ErrorInResponseError):
        node.api.wallet_bridge.find_recurrent_transfers(reward_fund_name)


@pytest.mark.parametrize("reward_fund_name", [["alice"]])
@pytest.mark.parametrize("asset", [tt.Asset.Test(20), tt.Asset.Tbd(20)])
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_find_recurrent_transfers_with_incorrect_type_of_argument(
    node: tt.InitNode | tt.RemoteNode, should_prepare: bool, reward_fund_name: list, asset: tt.Asset
) -> None:
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        create_accounts_and_make_recurrent_transfer(wallet, from_account="alice", to_account="bob", asset=asset)

    with pytest.raises(ErrorInResponseError):
        node.api.wallet_bridge.find_recurrent_transfers(reward_fund_name)


@run_for("testnet", "mainnet_5m", "live_mainnet")
@pytest.mark.parametrize("asset", [tt.Asset.Test(20), tt.Asset.Tbd(20)])
def test_find_recurrent_transfers_with_additional_argument(
    node: tt.InitNode | tt.RemoteNode, should_prepare: bool, asset: tt.Asset
) -> None:
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        create_accounts_and_make_recurrent_transfer(wallet, from_account="alice", to_account="bob", asset=asset)

    node.api.wallet_bridge.find_recurrent_transfers("alice", "additional_argument")


def create_accounts_and_make_recurrent_transfer(
    wallet: tt.Wallet, from_account: str, to_account: str, asset: tt.Asset
) -> None:
    with wallet.in_single_transaction():
        wallet.api.create_account("initminer", from_account, "{}")
        wallet.api.create_account("initminer", to_account, "{}")

    wallet.api.transfer_to_vesting("initminer", from_account, tt.Asset.Test(100))
    wallet.api.transfer("initminer", from_account, tt.Asset.Test(500), "memo")
    wallet.api.transfer("initminer", from_account, tt.Asset.Tbd(500), "memo")
    wallet.api.recurrent_transfer(from_account, to_account, asset, "memo", 100, 10)
