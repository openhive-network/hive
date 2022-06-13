import pytest

from test_tools import exceptions


@pytest.mark.testnet
def test_get_transaction_with_correct_value(node, wallet):
    transaction_id = wallet.api.create_account('initminer', 'alice', '{}')['transaction_id']
    node.wait_number_of_blocks(21)   # waiting 21 blocks for the block with the transaction to become irreversible
    node.api.wallet_bridge.get_transaction(transaction_id)


@pytest.mark.remote_node_5m
def test_get_transaction_with_correct_value_5m(node5m):
    # Transaction id of the 5 millionth block
    transaction_id = node5m.api.wallet_bridge.get_block(5 * 1000**2)['block']['transaction_ids'][0]
    node5m.api.wallet_bridge.get_transaction(transaction_id)


@pytest.mark.remote_node_64m
def test_get_transaction_with_correct_value_64m(node64m):
    # Transaction id of the 64 millionth block
    transaction_id = node64m.api.wallet_bridge.get_block(64 * 1000**2)['block']['transaction_ids'][0]
    node64m.api.wallet_bridge.get_transaction(transaction_id)


@pytest.mark.parametrize(
    'transaction_id', [
        '0000000000000000000000000000000000000000',  # non exist transaction
        'incorrect_string_argument',
        '100',
    ]
)
@pytest.mark.testnet
def test_get_transaction_with_incorrect_value(node, transaction_id):
    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.get_transaction(transaction_id)


@pytest.mark.parametrize(
    'transaction_id', [
        ['example-array'],
        100,
        True,
    ]
)
@pytest.mark.testnet
def test_get_transaction_with_incorrect_type_of_argument(node, transaction_id):
    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.get_transaction(transaction_id)


@pytest.mark.testnet
def test_get_transaction_with_additional_argument(node, wallet):
    transaction_id = wallet.api.create_account('initminer', 'alice', '{}')['transaction_id']
    node.wait_number_of_blocks(21)   # waiting 21 blocks for the block with the transaction to become irreversible

    node.api.wallet_bridge.get_transaction(transaction_id, 'additional_argument')
