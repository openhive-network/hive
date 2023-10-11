from __future__ import annotations

from hive_local_tools import run_for


@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_witness_count(node):
    node.api.condenser.get_witness_count()
