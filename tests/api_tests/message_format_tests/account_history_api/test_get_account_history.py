import pytest


@pytest.mark.testnet
def test_get_account_history_response_testnet(node, wallet):
    node.wait_number_of_blocks(21)
    response = node.api.account_history.get_account_history(account='initminer', start=-1, limit=100)


@pytest.mark.remote_node_5m
def test_get_account_history_response_5m(node5m):
    response = node5m.api.account_history.get_account_history(account='gtg', start=-1, limit=100)


@pytest.mark.remote_node_64m
def test_get_account_history_response_64m(node64m):
    response = node64m.api.account_history.get_account_history(account='gtg', start=-1, limit=100)
