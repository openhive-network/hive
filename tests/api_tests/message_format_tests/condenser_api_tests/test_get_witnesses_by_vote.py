from ..local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_witnesses_by_vote(prepared_node):
    response = prepared_node.api.condenser.get_witnesses_by_vote('', 100)
    assert len(response) != 0
