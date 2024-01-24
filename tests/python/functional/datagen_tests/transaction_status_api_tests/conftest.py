from __future__ import annotations

from pathlib import Path

import pytest

import test_tools as tt


def pytest_configure(config):
    # config.addinivalue_line("markers", "transaction_status_track_after_block: set argument in config.ini")
    config.addinivalue_line("markers", "transaction_status_block_depth: set argument in config.ini")


@pytest.fixture()
def wallet(node: tt.InitNode) -> tt.Wallet:
    return tt.Wallet(attach_to=node)


@pytest.fixture()
def replayed_node(request: pytest.FixtureRequest) -> tt.ApiNode:
    api_node = tt.ApiNode()
    # transaction_status_track_after_block = request.node.get_closest_marker("transaction_status_track_after_block")
    transaction_status_block_depth = request.node.get_closest_marker("transaction_status_block_depth")

    # if transaction_status_track_after_block:
    #    api_node.config.transaction_status_track_after_block = next(iter(transaction_status_track_after_block.args))

    if transaction_status_block_depth:
        api_node.config.transaction_status_block_depth = next(iter(transaction_status_block_depth.args))
    block_log = tt.BlockLog(Path(__file__).parent.joinpath("block_log/block_log"))
    api_node.run(
        replay_from=block_log,
        wait_for_live=False,
        time_control=tt.StartTimeControl(start_time="head_block_time"),
    )
    return api_node
