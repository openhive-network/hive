from __future__ import annotations

import pytest

import test_tools as tt


@pytest.fixture()
def queen_chain_specification() -> tt.AlternateChainSpecs:
    return tt.AlternateChainSpecs(
        genesis_time=int(tt.Time.now(serialize=False).timestamp()),
        hardfork_schedule=[tt.HardforkSchedule(hardfork=28, block_num=1)],
    )


@pytest.fixture()
def queen_node() -> tt.InitNode:
    node = tt.InitNode()
    node.config.plugin.append("queen")
    return node
