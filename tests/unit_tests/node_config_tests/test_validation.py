import pytest

from test_tools.node_config import NodeConfig


@pytest.fixture
def config():
    return NodeConfig()


def test_detection_of_duplicated_plugins(config):
    config.plugin.extend(['condenser_api', 'condenser_api'])
    with pytest.raises(RuntimeError):
        config.validate()
