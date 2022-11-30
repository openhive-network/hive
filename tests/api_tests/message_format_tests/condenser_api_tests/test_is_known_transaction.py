from ....local_tools import run_for


@run_for('testnet')
def test_is_known_transaction_in_testnet(node, wallet):
    transaction = wallet.api.create_account('initminer', 'alice', '{}')
    node.api.condenser.is_known_transaction(transaction['transaction_id'])


@run_for('mainnet_5m', 'mainnet_64m')
def test_is_known_transaction_in_mainnet(node):
    block = node.api.wallet_bridge.get_block(3652254)['block']
    node.api.condenser.is_known_transaction(block['transaction_ids'][0])
