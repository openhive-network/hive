import test_tools as tt

from ....local_tools import create_account_and_fund_it


def test_find_vesting_delegations(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.create_account('alice', 'bob', '{}')
    wallet.api.delegate_vesting_shares('alice', 'bob', tt.Asset.Vest(5))
    delegations = node.api.database.find_vesting_delegations(account='alice')
    assert len(delegations) != 0
