import pytest


@pytest.mark.testnet
def test_get_version(node):
    node.api.wallet_bridge.get_version()


@pytest.mark.remote_node_5m
def test_get_version_5m(node5m):
    node5m.api.wallet_bridge.get_version()


@pytest.mark.remote_node_64m
def test_get_version_64m(node64m):
    node64m.api.wallet_bridge.get_version()

