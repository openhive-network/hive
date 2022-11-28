import test_tools as tt

from hive_local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'live_mainnet')
def test_find_limit_orders(node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.create_account('alice', hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
        wallet.api.create_order('alice', 431, tt.Asset.Test(50), tt.Asset.Tbd(5), False, 3600)
    account = node.api.database.list_limit_orders(start=['', 0], limit=100, order='by_account')['orders'][0]['seller']
    orders = node.api.database.find_limit_orders(account=account)['orders']
    assert len(orders) != 0
