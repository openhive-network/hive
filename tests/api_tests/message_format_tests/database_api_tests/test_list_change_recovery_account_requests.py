import test_tools as tt

from ..local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_list_change_recovery_account_requests(prepared_node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        wallet.create_account('alice', hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
        wallet.api.change_recovery_account('initminer', 'hive.fund')
    requests = prepared_node.api.database.list_change_recovery_account_requests(start='', limit=100, order='by_account')['requests']
    assert len(requests) != 0
