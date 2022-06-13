import pytest

from .local_tools import prepare_node_with_witnesses


WITNESSES_NAMES = [f'witness-{i}' for i in range(20)]  # 21-st is initminer


@pytest.mark.testnet
def test_get_active_witnesses_with_correct_value(world):
    node = prepare_node_with_witnesses(world, WITNESSES_NAMES)
    node.api.wallet_bridge.get_active_witnesses()


@pytest.mark.remote_node_5m
def test_get_active_witnesses_with_correct_value_5m(node5m):
    node5m.api.wallet_bridge.get_active_witnesses()


@pytest.mark.remote_node_64m
def test_get_active_witnesses_with_correct_value_64m(node64m):
    node64m.api.wallet_bridge.get_active_witnesses()
