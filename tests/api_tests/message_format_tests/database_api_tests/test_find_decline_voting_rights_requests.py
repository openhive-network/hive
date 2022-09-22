import test_tools as tt

from ....local_tools import create_account_and_fund_it


def test_find_decline_voting_rights_requests(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.decline_voting_rights('alice', True)
    requests = node.api.database.find_decline_voting_rights_requests(accounts=['alice'])
    assert len(requests) != 0
