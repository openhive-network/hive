import pytest

import test_tools as tt


@pytest.mark.testnet
def test_broadcast_transaction_with_correct_value(node, wallet):
    transaction = wallet.api.create_account('initminer', 'alice', '{}', broadcast=False)
    node.api.wallet_bridge.broadcast_transaction(transaction)


@pytest.mark.parametrize(
    'transaction_name', [
        ['non-exist-transaction'],
        'non-exist-transaction',
        100,
        True
    ]
)
@pytest.mark.testnet
def test_broadcast_transaction_with_incorrect_type_of_argument(node, transaction_name):
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.broadcast_transaction(transaction_name)


@pytest.mark.testnet
def test_broadcast_transaction_with_additional_argument(node, wallet):
    transaction = wallet.api.create_account('initminer', 'alice', '{}', broadcast=False)

    node.api.wallet_bridge.broadcast_transaction(transaction, 'additional_argument')
