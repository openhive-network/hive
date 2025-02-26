from __future__ import annotations

import pytest
from beekeepy.exceptions import ErrorInResponseError

import test_tools as tt
from hive_local_tools import run_for


@run_for("testnet")
def test_broadcast_transaction_with_correct_value(node: tt.InitNode) -> None:
    wallet = tt.Wallet(attach_to=node)
    transaction = wallet.api.create_account("initminer", "alice", "{}", broadcast=False)
    node.api.wallet_bridge.broadcast_transaction(transaction)


@pytest.mark.parametrize("transaction_name", [["non-exist-transaction"], "non-exist-transaction", 100, True])
@run_for("testnet")
def test_broadcast_transaction_with_incorrect_type_of_argument(
    node: tt.InitNode, transaction_name: bool | int | list | str
) -> None:
    with pytest.raises(ErrorInResponseError):
        node.api.wallet_bridge.broadcast_transaction(transaction_name)


@run_for("testnet")
def test_broadcast_transaction_with_additional_argument(node: tt.InitNode) -> None:
    wallet = tt.Wallet(attach_to=node)
    transaction = wallet.api.create_account("initminer", "alice", "{}", broadcast=False)

    node.api.wallet_bridge.broadcast_transaction(transaction, "additional_argument")
