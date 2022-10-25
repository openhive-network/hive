from ..local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_witnesses(node):
    witness = node.api.condenser.get_witnesses([0])
    assert len(witness) != 0
