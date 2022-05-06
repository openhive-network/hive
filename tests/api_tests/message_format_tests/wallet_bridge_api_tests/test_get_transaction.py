import pytest

from test_tools import exceptions


def test_get_transaction_with_correct_value(node, wallet):
    transaction_id = wallet.api.create_account('initminer', 'alice', '{}')['transaction_id']
    node.wait_number_of_blocks(21)   # waiting 21 blocks for the block with the transaction to become irreversible
    node.api.wallet_bridge.get_transaction(transaction_id)


@pytest.mark.parametrize(
    'transaction_id', [
        '0000000000000000000000000000000000000000',  # non exist transaction
        'incorrect_string_argument',
        '100',
    ]
)
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
def test_get_transaction_with_incorrect_type_of_argument(node, transaction_id):
    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.get_transaction(transaction_id)


def test_get_transaction_with_additional_argument(node, wallet):
    transaction_id = wallet.api.create_account('initminer', 'alice', '{}')['transaction_id']
    node.wait_number_of_blocks(21)   # waiting 21 blocks for the block with the transaction to become irreversible

    node.api.wallet_bridge.get_transaction(transaction_id, 'additional_argument')
