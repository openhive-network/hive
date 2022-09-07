from ..local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_lookup_account_names(prepared_node):
    response = prepared_node.api.condenser.lookup_account_names(['initminer'])
    assert len(response) != 0
