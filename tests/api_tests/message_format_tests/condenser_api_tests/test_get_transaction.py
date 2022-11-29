import test_tools as tt

from ....local_tools import run_for


@run_for('testnet')
def test_get_transaction_in_testnet(node, wallet):
    transaction = wallet.api.create_account('initminer', 'alice', '{}')

    # Wait until block containing above transaction will become irreversible.
    node.api.debug_node.debug_generate_blocks(debug_key=tt.Account('initminer').private_key, count=22, skip=0,
                                                       miss_blocks=0, edit_if_needed=True)
    node.api.condenser.get_transaction(transaction['transaction_id'])


@run_for('mainnet_5m', 'mainnet_64m')
def test_get_transaction_in_mainnet(node):
    block = node.api.condenser.get_block(4450001)
    transaction = block['transactions'][0]
    node.api.condenser.get_transaction(transaction['transaction_id'])
