import pytest


@pytest.mark.testnet
def test_get_ops_in_block_response_testnet(node, wallet):
    node.wait_number_of_blocks(21)
    response = node.api.account_history.get_ops_in_block(block_num=1, only_virtual=False)


@pytest.mark.remote_node_5m
def test_get_ops_in_block_response_5m(node5m):
    response = node5m.api.account_history.get_ops_in_block(block_num=1, only_virtual=False)
    print()

@pytest.mark.remote_node_64m
def test_get_ops_in_block_response_64m(node64m):
    response = node64m.api.account_history.get_ops_in_block(block_num=1, only_virtual=False)
    print()


