import pytest

from test_tools.node_config import NodeConfig


@pytest.fixture
def config():
    return NodeConfig()
