import test_tools as tt

from ....local_tools import create_account_and_fund_it


def test_list_vesting_delegations(node, wallet):
    create_account_and_fund_it(wallet, 'alice', vests=tt.Asset.Test(100))
    wallet.api.create_account('alice', 'bob', '{}')
    wallet.api.delegate_vesting_shares('alice', 'bob', tt.Asset.Vest(5))
    delegations = node.api.database.list_vesting_delegations(start=["alice", "bob"], limit=100, order='by_delegation')['delegations']
    assert len(delegations) != 0
