from __future__ import annotations

from typing import TYPE_CHECKING

from hive_local_tools import run_for

if TYPE_CHECKING:
    import test_tools as tt


@run_for("testnet")
def test_get_transaction_hex_in_testnet(node: tt.InitNode, wallet: tt.Wallet) -> None:
    transaction = wallet.api.create_account("initminer", "alice", "{}")
    output_hex = node.api.condenser.get_transaction_hex(transaction)
    assert len(output_hex) != 0


@run_for("mainnet_5m", "live_mainnet")
def test_get_transaction_hex_in_mainnet(node: tt.RemoteNode) -> None:
    block = node.api.condenser.get_block(4450001)
    transaction = block.transactions[0]
    output_hex = node.api.condenser.get_transaction_hex(transaction)
    assert len(output_hex) != 0
