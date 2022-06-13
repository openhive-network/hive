import pytest


@pytest.mark.testnet
def test_get_current_median_history_price(node):
    node.api.wallet_bridge.get_current_median_history_price()


@pytest.mark.remote_node_5m
def test_get_current_median_history_price_5m(node5m):
    node5m.api.wallet_bridge.get_current_median_history_price()


@pytest.mark.remote_node_64m
def test_get_current_median_history_price_64m(node64m):
    node64m.api.wallet_bridge.get_current_median_history_price()
