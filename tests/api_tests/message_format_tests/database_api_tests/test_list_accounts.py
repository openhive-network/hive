import test_tools as tt

from hive_local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'live_mainnet')
def test_list_accounts(node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.api.create_account('initminer', 'alice', '{}')
    accounts = node.api.database.list_accounts(start='', limit=100, order='by_name')['accounts']
    assert len(accounts) != 0
