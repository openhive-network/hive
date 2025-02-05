from __future__ import annotations

import pytest
from helpy.exceptions import ErrorInResponseError

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.api.message_format.wallet_bridge_api import get_transaction_id_from_head_block


@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_is_know_transaction_with_correct_value_and_existing_transaction(
    node: tt.InitNode | tt.RemoteNode, should_prepare: bool
) -> None:
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.api.create_account("initminer", "alice", "{}")

    transaction_id = get_transaction_id_from_head_block(node)
    node.api.wallet_bridge.is_known_transaction(transaction_id)


@pytest.mark.parametrize(
    "transaction_id",
    [
        # transaction id should be HEX
        "0000000000000000000000000000000000000000",
        "",
        "0",
        "adfafa",
        "100",
        100,
    ],
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_is_know_transaction_with_correct_value_and_non_existing_transaction(
    node: tt.InitNode | tt.RemoteNode, transaction_id: int | str
) -> None:
    node.api.wallet_bridge.is_known_transaction(transaction_id)


@pytest.mark.parametrize(
    "transaction_id",
    [
        # transaction id should be HEX
        ["example-array"],
        "non-hex-string-argument",
        True,
        "true",
    ],
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_is_know_transaction_with_incorrect_type_of_argument(
    node: tt.InitNode | tt.RemoteNode, transaction_id: bool | list | str
) -> None:
    with pytest.raises(ErrorInResponseError):
        node.api.wallet_bridge.is_known_transaction(transaction_id)
