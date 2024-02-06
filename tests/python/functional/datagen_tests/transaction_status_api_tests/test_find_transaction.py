from __future__ import annotations

import time
from typing import TYPE_CHECKING

from hive_local_tools import run_for

if TYPE_CHECKING:
    import test_tools as tt


def test_find_existing_transaction(node_with_genesis_time, wallet_with_genesis_time) -> None:
    transaction = wallet_with_genesis_time.api.create_account("initminer", "alice", "{}", broadcast=False)
    transaction_id = transaction["transaction_id"]
    node_with_genesis_time.api.condenser.broadcast_transaction(transaction)

    assert (
        node_with_genesis_time.api.transaction_status.find_transaction(transaction_id=transaction_id).status
        == "within_mempool"
    )

    node_with_genesis_time.wait_number_of_blocks(1)
    assert node_with_genesis_time.api.database.is_known_transaction(id=transaction_id).is_known is True
    assert (
        node_with_genesis_time.api.transaction_status.find_transaction(transaction_id=transaction_id).status
        == "within_reversible_block"
    )

    node_with_genesis_time.wait_for_irreversible_block()
    assert (
        node_with_genesis_time.api.transaction_status.find_transaction(transaction_id=transaction_id).status
        == "within_irreversible_block"
    )

    time.sleep(90)
    """
        In this test case we can't really use find_transaction from transaction_status API to check status of
        transaction (after expiration) because it holds transactions for one hour before it flag it as expired. Instead
        of this is_known_transaction from database API is used.
    """
    assert node_with_genesis_time.api.database.is_known_transaction(id=transaction_id).is_known is False


@run_for("testnet", enable_plugins=["transaction_status_api"])
def test_find_unknown_transaction(node: tt.ApiNode, wallet: tt.Wallet) -> None:
    response = node.api.transaction_status.find_transaction(transaction_id="0000000000000000000000000000000000000000")
    assert response.status == "unknown"
