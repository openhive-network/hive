from __future__ import annotations

from typing import TYPE_CHECKING

from hive_local_tools import run_for

if TYPE_CHECKING:
    import test_tools as tt


@run_for("testnet")
def test_find_transaction_with_extended_expiration(prepare_environment: tuple[tt.InitNode, tt.Wallet]) -> None:
    node, wallet = prepare_environment
    wallet.api.set_transaction_expiration(2 * 3600)
    transaction = wallet.api.create_account("initminer", "alice", "{}", broadcast=False)
    transaction_id = transaction["transaction_id"]
    node.api.network_broadcast.broadcast_transaction(trx=transaction)

    node.wait_number_of_blocks(1)

    node.wait_for_irreversible_block()

    assert (
        node.api.transaction_status.find_transaction(transaction_id=transaction_id).status
        == "within_irreversible_block"
    )
