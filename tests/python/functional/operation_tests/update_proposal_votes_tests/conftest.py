from __future__ import annotations

from pathlib import Path

import pytest

import test_tools as tt
from hive_local_tools.functional.python.operation import Account


@pytest.fixture()
def node() -> tt.InitNode:
    node = tt.InitNode()
    node.config.log_logger = (
        '{"name": "default", "level": "debug", "appender": "stderr"}',
        '{"name": "user", "level": "debug", "appender": "stderr"}',
        '{"name": "chainlock", "level": "debug", "appender": "p2p"}',
        '{"name": "sync", "level": "debug", "appender": "p2p"}',
        '{"name": "p2p", "level": "debug", "appender": "p2p"}',
        '{"name":"witness","level":"debug","appender":"stderr"}'
    )
    node.config.block_log_split = -1
    block_log_directory = Path(__file__).parent.joinpath("block_log")
    block_log = tt.BlockLog(block_log_directory, "monolithic")

    node.run(
        time_control=tt.StartTimeControl(start_time="head_block_time"),
        replay_from=block_log,
    )
    return node


@pytest.fixture()
def wallet(node: tt.InitNode) -> tt.Wallet:
    return tt.Wallet(attach_to=node)


@pytest.fixture()
def voter(node: tt.InitNode, wallet: tt.Wallet):
    wallet.create_account("voter", vests=tt.Asset.Test(10))
    return Account("voter", node, wallet)
