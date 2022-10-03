import pytest

import test_tools as tt

from ..local_tools import run_for


# network broadcast API can only be tested on the `testnet` network.
@run_for('testnet')
def test_broadcast_transaction(prepared_node):
    wallet = tt.Wallet(attach_to=prepared_node)
    alice_creation_transaction = wallet.api.create_account('initminer', 'alice', '{}', broadcast=False)

    prepared_node.api.network_broadcast.broadcast_transaction(trx=alice_creation_transaction)


@pytest.mark.parametrize(
    'transaction_name', [
        ['non-exist-transaction'],
        'non-exist-transaction',
        100,
        True
    ]
)
@run_for('testnet')
def test_broadcast_transaction_with_incorrect_type_of_argument(prepared_node, transaction_name):
    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.network_broadcast.broadcast_transaction(trx=transaction_name)
