import test_tools as tt

from ....local_tools import create_account_and_fund_it


def test_find_change_recovery_account_requests(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.change_recovery_account('alice', 'steem.dao')
    requests = node.api.database.find_change_recovery_account_requests(accounts=['alice'])['requests']
    assert len(requests) != 0
