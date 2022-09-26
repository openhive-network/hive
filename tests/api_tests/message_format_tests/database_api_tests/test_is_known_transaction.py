import test_tools as tt

from ..local_tools import run_for


@run_for('testnet')
def test_is_known_transaction_in_testnet(prepared_node):
    wallet = tt.Wallet(attach_to=prepared_node)
    transaction = wallet.api.create_account('initminer', 'alice', '{}')
    prepared_node.api.database.is_known_transaction(id=transaction['transaction_id'])


@run_for('mainnet_5m', 'mainnet_64m')
def test_is_known_transaction_in_mainnet(prepared_node):
    block = prepared_node.api.wallet_bridge.get_block(3652254)['block']
    prepared_node.api.database.is_known_transaction(id=block['transaction_ids'][0])
