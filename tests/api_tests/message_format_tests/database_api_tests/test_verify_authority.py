import test_tools as tt

from ..local_tools import run_for


@run_for('testnet')
def test_verify_authority_in_testnet(prepared_node):
    wallet = tt.Wallet(attach_to=prepared_node, additional_arguments=['--transaction-serialization=hf26'])
    transaction = wallet.api.create_account('initminer', 'alice', '{}')
    prepared_node.api.database.verify_authority(trx=transaction, pack='hf26')


@run_for('mainnet_5m')
def test_verify_authority_in_mainnet_5m(prepared_node):
    block = prepared_node.api.wallet_bridge.get_block(4_800_119)['block']
    transaction = block['transactions'][1]
    prepared_node.api.database.verify_authority(trx=transaction)


@run_for('mainnet_64m')
def test_verify_authority_in_mainnet_64m(prepared_node):
    block = prepared_node.api.wallet_bridge.get_block(48_000_034)['block']
    transaction = block['transactions'][17]
    prepared_node.api.database.verify_authority(trx=transaction)
