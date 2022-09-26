import test_tools as tt

from ..local_tools import run_for


@run_for('testnet')
def test_get_transaction_hex_in_testnet(prepared_node):
    wallet = tt.Wallet(attach_to=prepared_node, additional_arguments=['--transaction-serialization=hf26'])
    transaction = wallet.api.create_account('initminer', 'alice', '{}')
    output_hex = prepared_node.api.database.get_transaction_hex(trx=transaction)['hex']
    assert len(output_hex) != 0


@run_for('mainnet_5m', 'mainnet_64m')
def test_get_transaction_hex_in_mainnet(prepared_node):
    block = prepared_node.api.wallet_bridge.get_block(4450001)['block']
    transaction = block['transactions'][0]
    output_hex = prepared_node.api.database.get_transaction_hex(trx=transaction)['hex']
    assert len(output_hex) != 0
