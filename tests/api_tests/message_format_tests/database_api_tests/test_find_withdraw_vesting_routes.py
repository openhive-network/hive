import test_tools as tt

from ....local_tools import create_account_and_fund_it


def test_find_withdraw_vesting_routes(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.create_account('alice', 'bob', '{}')
    wallet.api.set_withdraw_vesting_route('alice', 'bob', 15, True)
    routes = node.api.database.find_withdraw_vesting_routes(account='bob', order='by_destination')['routes']
    assert len(routes) != 0
