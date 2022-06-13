import pytest


@pytest.mark.testnet
def test_get_feed_history(node):
    node.api.wallet_bridge.get_feed_history()


@pytest.mark.remote_ndoe_5m
def test_get_feed_history_5m(node5m):
    node5m.api.wallet_bridge.get_feed_history()


@pytest.mark.remote_ndoe_64m
def test_get_feed_history_64m(node64m):
    node64m.api.wallet_bridge.get_feed_history()
