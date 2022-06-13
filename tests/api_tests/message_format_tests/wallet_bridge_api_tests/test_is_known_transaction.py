import pytest

import test_tools as tt


@pytest.mark.testnet
def test_is_know_transaction_with_correct_value_and_existing_transaction(node, wallet):
    transaction_id = wallet.api.create_account('initminer', 'alice', '{}')['transaction_id']
    node.api.wallet_bridge.is_known_transaction(transaction_id)


@pytest.mark.remote_node_5m
def test_is_know_transaction_with_correct_value_and_existing_transaction_5m(node5m):
    # Transaction id of the 5 million block
    transaction_id = node5m.api.wallet_bridge.get_block(5000000)['block']['transaction_ids'][0]
    node5m.api.wallet_bridge.is_known_transaction(transaction_id)


@pytest.mark.remote_node_64m
def test_is_know_transaction_with_correct_value_and_existing_transaction_64m(node64m):
    # Transaction id of the 64 million block
    transaction_id = node64m.api.wallet_bridge.get_block(64000000)['block']['transaction_ids'][0]
    node64m.api.wallet_bridge.is_known_transaction(transaction_id)


@pytest.mark.parametrize(
    'transaction_id', [
        # transaction id should be HEX
        '0000000000000000000000000000000000000000',
        '',
        '0',
        'adfafa',
        '100',
        100,
    ]
)
@pytest.mark.testnet
def test_is_know_transaction_with_correct_value_and_non_existing_transaction(node, transaction_id):
    node.api.wallet_bridge.is_known_transaction(transaction_id)


@pytest.mark.parametrize(
    'transaction_id', [
        # transaction id should be HEX
        ['example-array'],
        'non-hex-string-argument',
        True,
        'true',
    ]
)
@pytest.mark.testnet
def test_is_know_transaction_with_incorrect_type_of_argument(node, transaction_id):
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.is_known_transaction(transaction_id)
