import pytest

from test_library.node_config import NodeConfig


@pytest.fixture
def config():
    return NodeConfig()


def test_single_value_loading(config):
    config.load_from_lines(['block_log_info_print_file = ILOG'])
    assert config.block_log_info_print_file == 'ILOG'
