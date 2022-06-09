import pytest


@pytest.mark.testnet
def test_enum_virtual_ops_response_testnet(node):
    node.wait_number_of_blocks(21)
    response = node.api.account_history.enum_virtual_ops(block_range_begin=1, block_range_end=100)


@pytest.mark.remote_node_5m
def test_enum_virtual_ops_response_5m(node5m):
    response = node5m.api.account_history.enum_virtual_ops(block_range_begin=1, block_range_end=100)


@pytest.mark.remote_node_64m
def test_enum_virtual_ops_response_64m(node64m):
    response = node64m.api.account_history.enum_virtual_ops(block_range_begin=1, block_range_end=100)
