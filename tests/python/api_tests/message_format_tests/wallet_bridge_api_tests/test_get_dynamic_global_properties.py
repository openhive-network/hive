from __future__ import annotations

from typing import TYPE_CHECKING

import pytest

from hive_local_tools import run_for

if TYPE_CHECKING:
    import test_tools as tt


@pytest.mark.skip(
    reason="wallet_bridge uses deprecated schemas package which still requires removed DGPO fields "
    "(total_reward_fund_hive, total_reward_shares2); test should be migrated to database API"
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_dynamic_global_properties(node: tt.InitNode | tt.RemoteNode) -> None:
    node.api.wallet_bridge.get_dynamic_global_properties()
