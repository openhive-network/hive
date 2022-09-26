from ..local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_find_witnesses(prepared_node):
    witnesses = prepared_node.api.database.find_witnesses(owners=['initminer'])['witnesses']
    assert len(witnesses) != 0
