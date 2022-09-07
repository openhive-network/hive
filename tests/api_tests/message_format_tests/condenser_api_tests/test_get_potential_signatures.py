from ..local_tools import run_for


@run_for('testnet')
def test_get_potential_signatures_in_testnet(prepared_node, wallet):
    transaction = wallet.api.create_account('initminer', 'alice', '{}')
    keys = prepared_node.api.condenser.get_potential_signatures(transaction)
    assert len(keys) != 0


@run_for('mainnet_5m', 'mainnet_64m')
def test_get_potential_signatures_in_mainnet(prepared_node):
    block = prepared_node.api.condenser.get_block(4450001)
    transaction = block['transactions'][0]
    keys = prepared_node.api.condenser.get_potential_signatures(transaction)
    assert len(keys) != 0
