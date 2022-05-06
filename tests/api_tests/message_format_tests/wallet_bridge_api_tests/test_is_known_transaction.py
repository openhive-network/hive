import pytest

from test_tools import exceptions


def test_is_know_transaction_with_correct_value_and_existing_transaction(node, wallet):
    transaction_id = wallet.api.create_account('initminer', 'alice', '{}')['transaction_id']
    node.api.wallet_bridge.is_known_transaction(transaction_id)


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
def test_is_know_transaction_with_incorrect_type_of_argument(node, transaction_id):
    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.is_known_transaction(transaction_id)
