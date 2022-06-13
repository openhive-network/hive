import pytest


@pytest.mark.testnet
def test_get_hardfork_version(node):
    r = node.api.wallet_bridge.get_hardfork_version()
    print()


@pytest.mark.remote_node_5m
def test_get_hardfork_version_5m(node5m):
    node5m.api.wallet_bridge.get_hardfork_version()


@pytest.mark.remote_node_64m
def test_get_hardfork_version_64(node64m):
    node64m.api.wallet_bridge.get_hardfork_version()
