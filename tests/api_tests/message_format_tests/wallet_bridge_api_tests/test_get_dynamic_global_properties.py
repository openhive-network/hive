import pytest


@pytest.mark.testnet
def test_get_dynamic_global_properties(node):
    node.api.wallet_bridge.get_dynamic_global_properties()


@pytest.mark.remote_node_5m
def test_get_dynamic_global_properties_5m(node5m):
    node5m.api.wallet_bridge.get_dynamic_global_properties()


@pytest.mark.remote_node_64m
def test_get_dynamic_global_properties_64m(node64m):
    node64m.api.wallet_bridge.get_dynamic_global_properties()
