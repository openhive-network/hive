import pytest

from test_tools.node_config import NodeConfig


def test_detection_of_duplicated_plugins(config):
    config.plugin.extend(['condenser_api', 'condenser_api'])
    with pytest.raises(RuntimeError):
        config.validate()


def test_detection_of_unsupported_plugin():
    for incorrect_plugin in ['UNDEFINED_PLUGIN', 'witnness', 'p3p']:
        config = NodeConfig()
        config.plugin.append(incorrect_plugin)

        with pytest.raises(ValueError):
            config.validate()
