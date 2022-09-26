import test_tools as tt

from ..local_tools import run_for


@run_for('testnet')
def test_find_limit_orders_in_testnet(prepared_node):
    wallet = tt.Wallet(attach_to=prepared_node)
    wallet.create_account('alice', hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.create_order('alice', 431, tt.Asset.Test(50), tt.Asset.Tbd(5), False, 3600)
    orders = prepared_node.api.database.find_limit_orders(account='alice')['orders']
    assert len(orders) != 0


@run_for('mainnet_5m', 'mainnet_64m')
def test_find_limit_orders_in_mainnet(prepared_node):
    account = prepared_node.api.database.list_limit_orders(start=['alice', 0], limit=100,
                                                           order='by_account')['orders'][0]['seller']
    orders = prepared_node.api.database.find_limit_orders(account=account)['orders']
    assert len(orders) != 0
