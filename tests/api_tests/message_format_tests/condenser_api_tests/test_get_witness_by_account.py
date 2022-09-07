from ..local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_witness_by_account(prepared_node):
    prepared_node.api.condenser.get_witness_by_account('initminer')
