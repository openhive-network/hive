from hive_local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'live_mainnet')
def test_find_witnesses(node):
    witnesses = node.api.database.find_witnesses(owners=['initminer'])['witnesses']
    assert len(witnesses) != 0
