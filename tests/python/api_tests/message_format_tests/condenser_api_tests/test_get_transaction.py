from __future__ import annotations

from typing import TYPE_CHECKING

from hive_local_tools import run_for

if TYPE_CHECKING:
    import test_tools as tt


@run_for("testnet", enable_plugins=["account_history_api"])
def test_get_transaction_in_testnet(node: tt.InitNode, wallet: tt.Wallet) -> None:
    transaction = wallet.api.create_account("initminer", "alice", "{}")

    node.wait_for_irreversible_block()
    node.api.condenser.get_transaction(transaction["transaction_id"])


@run_for("mainnet_5m", "live_mainnet")
def test_get_transaction_in_mainnet(node: tt.RemoteNode) -> None:
    block = node.api.condenser.get_block(4450001)
    transaction = block.transactions[0]
    node.api.condenser.get_transaction(transaction["transaction_id"])
