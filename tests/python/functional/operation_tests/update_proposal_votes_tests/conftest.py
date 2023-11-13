from __future__ import annotations

from pathlib import Path

import pytest

import test_tools as tt
from hive_local_tools.functional.python.operation import Account


@pytest.fixture()
def node() -> tt.InitNode:
    node = tt.InitNode()
    block_log_directory = Path(__file__).parent.joinpath("block_log")

    with open(block_log_directory / "timestamp", encoding="utf-8") as file:
        timestamp = tt.Time.parse(file.read())

    node.run(
        time_offset=f"{tt.Time.serialize(timestamp, format_=tt.TimeFormats.TIME_OFFSET_FORMAT)}",
        replay_from=block_log_directory / "block_log",
    )
    return node


@pytest.fixture()
def wallet(node) -> tt.Wallet:
    return tt.Wallet(attach_to=node)


@pytest.fixture()
def voter(node, wallet):
    wallet.create_account("voter", vests=tt.Asset.Test(10))
    return Account("voter", node, wallet)
