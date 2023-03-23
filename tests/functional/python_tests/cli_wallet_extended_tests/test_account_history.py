from hive_local_tools import run_for

@run_for('testnet', enable_plugins=['account_history_api'])
def test_getting_empty_history(wallet):
    assert len(wallet.api.get_account_history('initminer', -1, 1)) == 0
