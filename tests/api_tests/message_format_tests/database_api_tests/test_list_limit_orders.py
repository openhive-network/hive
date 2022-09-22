import test_tools as tt

from ....local_tools import create_account_and_fund_it


def test_list_limit_orders(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.create_order('alice', 431, tt.Asset.Test(50), tt.Asset.Tbd(5), False, 3600)
    orders = node.api.database.list_limit_orders(start=['alice', 0], limit=100, order='by_account')['orders']
    assert len(orders) != 0
