from ..local_tools import run_for


@run_for('testnet')
def test_get_potential_signatures_in_testnet(node, wallet):
    transaction = wallet.api.create_account('initminer', 'alice', '{}')
    keys = node.api.condenser.get_potential_signatures(transaction)
    assert len(keys) != 0


@run_for('mainnet_5m', 'mainnet_64m')
def test_get_potential_signatures_in_mainnet(node):
    block = node.api.condenser.get_block(4450001)
    transaction = block['transactions'][0]
    keys = node.api.condenser.get_potential_signatures(transaction)
    assert len(keys) != 0
