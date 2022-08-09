import test_tools as tt

from ..local_tools import run_for


@run_for('testnet')
def test_get_transaction_in_testnet(prepared_node, wallet):
    transaction = wallet.api.create_account('initminer', 'alice', '{}')

    # Wait until block containing above transaction will become irreversible.
    prepared_node.wait_number_of_blocks(22)
    prepared_node.api.condenser.get_transaction(transaction['transaction_id'])


@run_for('mainnet_5m', 'mainnet_64m')
def test_get_transaction_in_mainnet(prepared_node):
    block = prepared_node.api.condenser.get_block(4450001)
    transaction = block['transactions'][0]
    prepared_node.api.condenser.get_transaction(transaction['transaction_id'])
