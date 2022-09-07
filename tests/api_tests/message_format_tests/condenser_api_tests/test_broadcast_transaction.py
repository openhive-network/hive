import test_tools as tt

from ..local_tools import run_for


@run_for('testnet')
def test_broadcast_transaction(prepared_node):
    wallet = tt.Wallet(attach_to=prepared_node)
    transaction = wallet.api.create_account('initminer', 'alice', '{}', broadcast=False)
    prepared_node.api.condenser.broadcast_transaction(transaction)
