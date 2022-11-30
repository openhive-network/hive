from ....local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_lookup_witness_accounts(node):
    response = node.api.condenser.lookup_witness_accounts('', 100)
    assert len(response) != 0
