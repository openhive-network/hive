import pytest


@pytest.mark.testnet
def test_get_chain_properties(node):
    node.api.wallet_bridge.get_chain_properties()


@pytest.mark.remote_node_5m
def test_get_chain_properties_5m(node5m):
    node5m.api.wallet_bridge.get_chain_properties()


@pytest.mark.remote_node_64m
def test_get_chain_properties_64m(node64m):
    node64m.api.wallet_bridge.get_chain_properties()
