from __future__ import annotations

import pytest

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
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_find_recurrent_transfers_with_correct_value(
    node: tt.InitNode | tt.RemoteNode, should_prepare: bool, reward_fund_name: str
) -> None:
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        create_accounts_and_make_recurrent_transfer(wallet, from_account="alice", to_account="bob")

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
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.find_recurrent_transfers(reward_fund_name)


@pytest.mark.parametrize("reward_fund_name", [["alice"]])
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_find_recurrent_transfers_with_incorrect_type_of_argument(
    node: tt.InitNode | tt.RemoteNode, should_prepare: bool, reward_fund_name: list
) -> None:
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        create_accounts_and_make_recurrent_transfer(wallet, from_account="alice", to_account="bob")

    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.find_recurrent_transfers(reward_fund_name)


@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_find_recurrent_transfers_with_additional_argument(
    node: tt.InitNode | tt.RemoteNode, should_prepare: bool
) -> None:
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        create_accounts_and_make_recurrent_transfer(wallet, from_account="alice", to_account="bob")

    node.api.wallet_bridge.find_recurrent_transfers("alice", "additional_argument")


def create_accounts_and_make_recurrent_transfer(wallet: tt.Wallet, from_account: str, to_account: str) -> None:
    with wallet.in_single_transaction():
        wallet.api.create_account("initminer", from_account, "{}")
        wallet.api.create_account("initminer", to_account, "{}")

    wallet.api.transfer_to_vesting("initminer", from_account, tt.Asset.Test(100))
    wallet.api.transfer("initminer", from_account, tt.Asset.Test(500), "memo")
    wallet.api.recurrent_transfer(from_account, to_account, tt.Asset.Test(20), "memo", 100, 10)
