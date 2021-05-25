import pytest

from test_tools.node_config import NodeConfig


@pytest.fixture
def configs():
    return NodeConfig(), NodeConfig(), NodeConfig()


def test_different_shared_file_size_values(configs):
    first, second, empty = configs

    first.shared_file_size = '5G'
    second.shared_file_size = '42G'

    assert first != second and first != empty and second != empty


def test_different_required_participation_values(configs):
    first, second, empty = configs

    first.required_participation = 1
    second.required_participation = 2

    assert first != second and first != empty and second != empty


def test_different_enable_stale_production_values(configs):
    first, second, empty = configs

    first.enable_stale_production = True
    second.enable_stale_production = False

    assert first != second and first != empty and second != empty
