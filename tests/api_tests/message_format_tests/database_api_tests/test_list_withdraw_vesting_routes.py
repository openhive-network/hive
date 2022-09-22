import test_tools as tt

from ....local_tools import create_account_and_fund_it


def test_list_withdraw_vesting_routes(node, wallet):
    create_account_and_fund_it(wallet, 'alice', vests=tt.Asset.Test(100))
    wallet.api.create_account('alice', 'bob', '{}')
    wallet.api.set_withdraw_vesting_route('alice', 'bob', 15, True)
    routes = node.api.database.list_withdraw_vesting_routes(start=["alice", "bob"], limit=100, order="by_withdraw_route")['routes']
    assert len(routes) != 0
