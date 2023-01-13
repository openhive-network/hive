from pathlib import Path

import pytest

import test_tools as tt


def pytest_configure(config):
    config.addinivalue_line('markers', 'transaction_status_track_after_block: set argument in config.ini')
    config.addinivalue_line('markers', 'transaction_status_block_depth: set argument in config.ini')


@pytest.fixture
def wallet(node):
    return tt.Wallet(attach_to=node)


@pytest.fixture
def replayed_node(request):
    api_node = tt.ApiNode()
    transaction_status_track_after_block = request.node.get_closest_marker('transaction_status_track_after_block')
    transaction_status_block_depth = request.node.get_closest_marker('transaction_status_block_depth')

    if transaction_status_track_after_block:
        api_node.config.transaction_status_track_after_block = int(list(transaction_status_track_after_block.args)[0])

    if transaction_status_block_depth:
        api_node.config.transaction_status_block_depth = int(list(transaction_status_block_depth.args)[0])

    api_node.run(replay_from=Path(__file__).parent.joinpath('block_log/block_log'), wait_for_live=False, time_offset="2022-12-22 10:47:05")
    return api_node
