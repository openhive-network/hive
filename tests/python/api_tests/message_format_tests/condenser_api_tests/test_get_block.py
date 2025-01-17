from __future__ import annotations

from typing import TYPE_CHECKING

from hive_local_tools import run_for

if TYPE_CHECKING:
    import test_tools as tt


@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_block(node: tt.InitNode | tt.RemoteNode) -> None:
    head_block_number = node.api.condenser.get_dynamic_global_properties().head_block_number
    node.api.condenser.get_block(head_block_number)
