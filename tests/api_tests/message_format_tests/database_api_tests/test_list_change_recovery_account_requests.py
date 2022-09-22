import test_tools as tt

from ....local_tools import create_account_and_fund_it


def test_list_change_recovery_account_requests(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.change_recovery_account('initminer', 'hive.fund')
    requests = node.api.database.list_change_recovery_account_requests(start='', limit=100, order='by_account')['requests']
    assert len(requests) != 0
