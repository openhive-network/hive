import test_tools as tt

from hive_local_tools import run_for


@run_for('testnet')
def test_verify_authority_in_testnet(node):
    wallet = tt.Wallet(attach_to=node, additional_arguments=['--transaction-serialization=hf26'])
    transaction = wallet.api.create_account('initminer', 'alice', '{}')
    node.api.database.verify_authority(trx=transaction, pack='hf26')


@run_for('mainnet_5m')
def test_verify_authority_in_mainnet_5m(node):
    block = node.api.wallet_bridge.get_block(4_800_119)['block']
    transaction = block['transactions'][1]
    node.api.database.verify_authority(trx=transaction)


@run_for('live_mainnet')
def test_verify_authority_in_live_mainnet(node):
    block = node.api.wallet_bridge.get_block(48_000_034)['block']
    transaction = block['transactions'][17]
    node.api.database.verify_authority(trx=transaction)
