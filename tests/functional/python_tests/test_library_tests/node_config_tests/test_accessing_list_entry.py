import pytest

from test_library.node_config import NodeConfig


@pytest.fixture
def config():
    return NodeConfig()


def test_adding_item_to_empty_list(config):
    config.witness += ['A']
    assert config.witness == ['A']


def test_adding_multiple_items_in_single_line(config):
    config.private_key += ['1', '2', '3']
    assert config.private_key == ['1', '2', '3']


def test_adding_multiple_items_in_few_lines(config):
    config.private_key += ['1']
    config.private_key += ['2']
    config.private_key += ['3']
    assert config.private_key == ['1', '2', '3']
