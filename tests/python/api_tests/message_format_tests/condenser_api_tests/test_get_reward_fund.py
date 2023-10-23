from __future__ import annotations

from typing import TYPE_CHECKING

from hive_local_tools import run_for

if TYPE_CHECKING:
    import test_tools as tt


# This test is not performed on 5 million block log because reward fund feature was introduced after block with number
# 5000000. See the readme.md file in this directory for further explanation.
@run_for("testnet", "live_mainnet")
def test_get_reward_fund(node: tt.InitNode | tt.RemoteNode) -> None:
    assert node.api.condenser.get_reward_fund("post")
