from __future__ import annotations

from typing import TYPE_CHECKING

from hive_local_tools import run_for

if TYPE_CHECKING:
    import test_tools as tt


@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_lookup_witness_accounts(node: tt.InitNode | tt.RemoteNode) -> None:
    response = node.api.condenser.lookup_witness_accounts("", 100)
    assert len(response) != 0
