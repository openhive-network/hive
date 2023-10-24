from __future__ import annotations

from typing import TYPE_CHECKING

from hive_local_tools import run_for

if TYPE_CHECKING:
    import test_tools as tt


# Resource credits (RC) were introduced after block with number 5000000, that's why this test is performed only on
# testnet and current mainnet.
@run_for("testnet", "live_mainnet")
def test_get_resource_params(node: tt.InitNode | tt.RemoteNode) -> None:
    node.api.rc.get_resource_params()
