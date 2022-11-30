from pathlib import Path

import pytest

import test_tools as tt


def pytest_configure(config):
    config.addinivalue_line('markers', 'enabled_plugins: Enabled plugins in node from `node` fixture')


@pytest.fixture
def replayed_node():
    api_node = tt.ApiNode()
    api_node.run(replay_from=Path(__file__).parent.joinpath('block_log/block_log'), wait_for_live=False)
    return api_node


@pytest.fixture
def wallet(node):
    return tt.Wallet(attach_to=node)
