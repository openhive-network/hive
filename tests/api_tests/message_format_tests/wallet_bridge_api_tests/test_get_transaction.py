import pytest

import test_tools as tt
from hive_local_tools import run_for


@run_for("testnet")
def test_get_transaction_with_correct_value(node):
    wallet = tt.Wallet(attach_to=node)
    transaction_id = wallet.api.create_account('initminer', 'alice', '{}')['transaction_id']
    node.wait_for_irreversible_block()
    node.api.wallet_bridge.get_transaction(transaction_id)



@pytest.mark.parametrize(
    'transaction_id', [
        '0000000000000000000000000000000000000000',  # non exist transaction
        'incorrect_string_argument',
        '100',
    ]
)
@run_for("testnet")
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
@run_for("testnet")
def test_get_transaction_with_incorrect_type_of_argument(node, transaction_id):
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.get_transaction(transaction_id)


@run_for("testnet")
def test_get_transaction_with_additional_argument(node):
    wallet = tt.Wallet(attach_to=node)
    transaction_id = wallet.api.create_account('initminer', 'alice', '{}')['transaction_id']
    node.wait_for_irreversible_block()

    node.api.wallet_bridge.get_transaction(transaction_id, 'additional_argument')
