from ....local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_block_header(node):
    head_block_number = node.api.condenser.get_dynamic_global_properties()['head_block_number']
    node.api.condenser.get_block_header(head_block_number)
