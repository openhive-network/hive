import pytest


@pytest.mark.testnet
def test_get_transaction_response_testnet(node, wallet):
    transaction_id = wallet.api.create_account('initminer', 'bob', '{}')['transaction_id']
    node.wait_number_of_blocks(21)
    response = node.api.account_history.get_transaction(id=transaction_id, include_reversible=True)


@pytest.mark.remote_node_5m
def test_get_transaction_response_5m(node5m):
    # Transaction id of the 5 millionth block
    # transaction_id = node5m.api.wallet_bridge.get_block(5 * 1000**2)['block']['transaction_ids'][0]

    # response = node5m.api.account_history.get_transaction(id=transaction_id, include_reversible=True)
    response = node5m.api.account_history.get_transaction(id='6707feb450da66dc223ab5cb3e259937b2fef6bf', include_reversible=True)


@pytest.mark.remote_node_64m
def test_get_transaction_response_64m(node64m):
    # Transaction id of the 5 millionth block
    # transaction_id = node64m.api.wallet_bridge.get_block(64 * 1000**2)['block']['transaction_ids'][0]

    response = node64m.api.account_history.get_transaction(id='ab22a1a151da577566a38acecbfb4551de08c338', include_reversible=True)
    # response = node64m.api.account_history.get_transaction(id=transaction_id, include_reversible=True)
