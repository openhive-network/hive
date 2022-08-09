from ..local_tools import run_for


@run_for('testnet')
def test_verify_authority_in_testnet(prepared_node, wallet):
    transaction = wallet.api.create_account('initminer', 'alice', '{}')
    prepared_node.api.condenser.verify_authority(transaction)


@run_for('mainnet_5m')
def test_verify_authority_in_mainnet_5m(prepared_node):
    block = prepared_node.api.condenser.get_block(4800119)
    transaction = block['transactions'][1]
    prepared_node.api.condenser.verify_authority(transaction)


@run_for('mainnet_64m')
def test_verify_authority_in_mainnet_64m(prepared_node):
    block = prepared_node.api.condenser.get_block(48000034)
    transaction = block['transactions'][17]
    prepared_node.api.condenser.verify_authority(transaction)
