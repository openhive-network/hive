from __future__ import annotations

from typing import TYPE_CHECKING

from hive_local_tools import run_for

if TYPE_CHECKING:
    import test_tools as tt


@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_witness_schedule(node: tt.InitNode | tt.RemoteNode) -> None:
    node.api.condenser.get_witness_schedule()


@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_witness_schedule_current(node: tt.InitNode | tt.RemoteNode) -> None:
    node.api.condenser.get_witness_schedule(False)


@run_for("testnet")
def test_get_witness_schedule_future(node: tt.InitNode | tt.RemoteNode) -> None:
    node.api.condenser.get_witness_schedule(True)
