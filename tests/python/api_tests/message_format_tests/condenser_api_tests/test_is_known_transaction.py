from __future__ import annotations

from typing import TYPE_CHECKING

from hive_local_tools import run_for

if TYPE_CHECKING:
    import test_tools as tt


@run_for("testnet")
def test_is_known_transaction_in_testnet(node: tt.InitNode, wallet: tt.Wallet) -> None:
    transaction = wallet.api.create_account("initminer", "alice", "{}")
    node.api.condenser.is_known_transaction(transaction["transaction_id"])


@run_for("mainnet_5m", "live_mainnet")
def test_is_known_transaction_in_mainnet(node: tt.RemoteNode) -> None:
    block = node.api.wallet_bridge.get_block(3652254).block
    node.api.condenser.is_known_transaction(block.transaction_ids[0])
