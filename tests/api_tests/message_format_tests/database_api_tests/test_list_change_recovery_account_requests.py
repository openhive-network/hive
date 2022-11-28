import test_tools as tt

from hive_local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'live_mainnet')
def test_list_change_recovery_account_requests(node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.create_account('alice', hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
        wallet.api.change_recovery_account('initminer', 'hive.fund')
    requests = node.api.database.list_change_recovery_account_requests(start='', limit=100, order='by_account')['requests']
    assert len(requests) != 0
