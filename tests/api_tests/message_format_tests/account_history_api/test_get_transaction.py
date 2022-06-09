import pytest


@pytest.mark.testnet
def test_get_transaction_response_testnet(node, wallet):
    transaction_id = wallet.api.create_account('initminer', 'bob', '{}')['transaction_id']
    node.wait_number_of_blocks(21)
    response = node.api.account_history.get_transaction(id=transaction_id, include_reversible=True)


@pytest.mark.remote_node_5m
def test_get_transaction_response_5m(node5m):
    response = node5m.api.account_history.get_transaction(id="9f4639be729f8ca436ac5bd01b5684cbc126d44d", include_reversible=True)


@pytest.mark.remote_node_64m
def test_get_transaction_response_64m(node64m):
    response = node64m.api.account_history.get_transaction(id="9f4639be729f8ca436ac5bd01b5684cbc126d44d", include_reversible=True)
