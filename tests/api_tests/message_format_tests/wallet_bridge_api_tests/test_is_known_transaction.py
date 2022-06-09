import pytest

import test_tools as tt

from .local_tools import run_for


@run_for('testnet')
def test_is_know_transaction_with_correct_value_and_existing_transaction_in_testnet(prepared_node):
    wallet = tt.Wallet(attach_to=prepared_node)
    transaction_id = wallet.api.create_account('initminer', 'alice', '{}')['transaction_id']
    prepared_node.api.wallet_bridge.is_known_transaction(transaction_id)


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
@run_for('testnet')
def test_is_know_transaction_with_correct_value_and_non_existing_transaction(prepared_node, transaction_id):
    prepared_node.api.wallet_bridge.is_known_transaction(transaction_id)


@pytest.mark.parametrize(
    'transaction_id', [
        # transaction id should be HEX
        ['example-array'],
        'non-hex-string-argument',
        True,
        'true',
    ]
)
@run_for('testnet')
def test_is_know_transaction_with_incorrect_type_of_argument(prepared_node, transaction_id):
    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.is_known_transaction(transaction_id)
