from __future__ import annotations

from typing import TYPE_CHECKING

from hive_local_tools import run_for

if TYPE_CHECKING:
    import test_tools as tt


@run_for("testnet", "mainnet_5m", "live_mainnet", enable_plugins=["reputation_api"])
def test_get_account_reputations(node: tt.InitNode | tt.RemoteNode) -> None:
    node.api.condenser.get_account_reputations("", 100)


@run_for("testnet", "mainnet_5m", "live_mainnet", enable_plugins=["reputation_api"])
def test_get_account_reputations_with_default_second_argument(node: tt.InitNode | tt.RemoteNode) -> None:
    node.api.condenser.get_account_reputations("")
