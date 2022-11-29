import pytest

import test_tools as tt


def test_get_transaction_with_correct_value(node, wallet):
    transaction_id = wallet.api.create_account('initminer', 'alice', '{}')['transaction_id']
    # waiting 21 blocks for the block with the transaction to become irreversible
    node.api.debug_node.debug_generate_blocks(debug_key=tt.Account('initminer').private_key, count=21, skip=0,
                                              miss_blocks=0, edit_if_needed=True)
    node.api.wallet_bridge.get_transaction(transaction_id)


@pytest.mark.parametrize(
    'transaction_id', [
        '0000000000000000000000000000000000000000',  # non exist transaction
        'incorrect_string_argument',
        '100',
    ]
)
def test_get_transaction_with_incorrect_value(node, transaction_id):
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.get_transaction(transaction_id)


@pytest.mark.parametrize(
    'transaction_id', [
        ['example-array'],
        100,
        True,
    ]
)
def test_get_transaction_with_incorrect_type_of_argument(node, transaction_id):
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.get_transaction(transaction_id)


def test_get_transaction_with_additional_argument(node, wallet):
    transaction_id = wallet.api.create_account('initminer', 'alice', '{}')['transaction_id']
    # waiting 21 blocks for the block with the transaction to become irreversible
    node.api.debug_node.debug_generate_blocks(debug_key=tt.Account('initminer').private_key, count=21, skip=0,
                                              miss_blocks=0, edit_if_needed=True)
    node.api.wallet_bridge.get_transaction(transaction_id, 'additional_argument')
