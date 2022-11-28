import test_tools as tt

from hive_local_tools import run_for


@run_for('testnet')
def test_is_known_transaction_in_testnet(node):
    wallet = tt.Wallet(attach_to=node)
    transaction = wallet.api.create_account('initminer', 'alice', '{}')
    node.api.database.is_known_transaction(id=transaction['transaction_id'])


@run_for('mainnet_5m', 'live_mainnet')
def test_is_known_transaction_in_mainnet(node):
    block = node.api.wallet_bridge.get_block(3652254)['block']
    node.api.database.is_known_transaction(id=block['transaction_ids'][0])
