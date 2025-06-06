from __future__ import annotations

import os
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

    destination_variable = os.environ.get("TESTING_BLOCK_LOGS_DESTINATION")
    assert destination_variable is not None, "Path TESTING_BLOCK_LOGS_DESTINATION must be set!"
    block_logs_location: Path = Path(destination_variable) / "transaction_status_api"

    block_log = tt.BlockLog(block_logs_location, "auto")
    api_node.config.block_log_split = 9999
    api_node.run(
        replay_from=block_log,
        wait_for_live=False,
        time_control=tt.StartTimeControl(start_time=block_log.get_head_block_time()),
    )
    return api_node
