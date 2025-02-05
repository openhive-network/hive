from __future__ import annotations

import pytest
from helpy.exceptions import ErrorInResponseError

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.api.message_format.wallet_bridge_api import get_transaction_id_from_head_block


@run_for("testnet", "mainnet_5m", "live_mainnet", enable_plugins=["account_history_api"])
def test_get_transaction_with_correct_value(node: tt.InitNode | tt.RemoteNode, should_prepare: bool) -> None:
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.api.create_account("initminer", "alice", "{}")
    transaction_id = get_transaction_id_from_head_block(node)

    node.api.wallet_bridge.get_transaction(transaction_id)


@pytest.mark.parametrize(
    "transaction_id",
    [
        "0000000000000000000000000000000000000000",  # non exist transaction
        "incorrect_string_argument",
        "100",
    ],
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_transaction_with_incorrect_value(node: tt.InitNode | tt.RemoteNode, transaction_id: str) -> None:
    with pytest.raises(ErrorInResponseError):
        node.api.wallet_bridge.get_transaction(transaction_id)


@pytest.mark.parametrize(
    "transaction_id",
    [
        ["example-array"],
        100,
        True,
    ],
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_transaction_with_incorrect_type_of_argument(
    node: tt.InitNode | tt.RemoteNode, transaction_id: bool | int | list
) -> None:
    with pytest.raises(ErrorInResponseError):
        node.api.wallet_bridge.get_transaction(transaction_id)


@run_for("testnet", "mainnet_5m", "live_mainnet", enable_plugins=["account_history_api"])
def test_get_transaction_with_additional_argument(node: tt.InitNode | tt.RemoteNode, should_prepare: bool) -> None:
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.api.create_account("initminer", "alice", "{}")
    transaction_id = get_transaction_id_from_head_block(node)

    node.api.wallet_bridge.get_transaction(transaction_id, "additional_argument")
