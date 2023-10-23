from __future__ import annotations

from typing import TYPE_CHECKING

from hive_local_tools import run_for

if TYPE_CHECKING:
    import test_tools as tt


@run_for("testnet")
def test_get_potential_signatures_in_testnet(node: tt.InitNode, wallet: tt.Wallet) -> None:
    transaction = wallet.api.create_account("initminer", "alice", "{}")
    keys = node.api.condenser.get_potential_signatures(transaction)
    assert len(keys) != 0


@run_for("mainnet_5m", "live_mainnet")
def test_get_potential_signatures_in_mainnet(node: tt.RemoteNode) -> None:
    block = node.api.condenser.get_block(4450001)
    transaction = block.transactions[0]
    keys = node.api.condenser.get_potential_signatures(transaction)
    assert len(keys) != 0
