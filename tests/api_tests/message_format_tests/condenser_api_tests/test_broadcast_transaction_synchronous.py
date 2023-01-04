import pytest

import test_tools as tt

from hive_local_tools import run_for


@run_for("testnet")
def test_broadcast_transaction_synchronous(node):
    wallet = tt.Wallet(attach_to=node)
    transaction = wallet.api.create_account("initminer", "alice", "{}", broadcast=False)
    node.api.condenser.broadcast_transaction_synchronous(transaction)


@pytest.mark.parametrize("transaction_name", [["non-exist-transaction"], "non-exist-transaction", 100, True])
@run_for("testnet")
def test_broadcast_transaction_synchronous_with_incorrect_type_of_argument(node, transaction_name):
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.condenser.broadcast_transaction_synchronous(transaction_name)


@run_for("testnet")
def test_broadcast_transaction_synchronous_with_additional_argument(node, wallet):
    transaction = wallet.api.create_account("initminer", "alice", "{}", broadcast=False)

    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.condenser.broadcast_transaction_synchronous(transaction, "additional_argument")
