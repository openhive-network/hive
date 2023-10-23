from __future__ import annotations

from typing import TYPE_CHECKING

from hive_local_tools import run_for

if TYPE_CHECKING:
    import test_tools as tt


@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_dynamic_global_properties(node: tt.InitNode | tt.RemoteNode) -> None:
    node.api.condenser.get_dynamic_global_properties()
