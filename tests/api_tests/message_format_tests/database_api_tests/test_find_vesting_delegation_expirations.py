import test_tools as tt

from ..local_tools import create_and_cancel_vesting_delegation
from ....local_tools import create_account_and_fund_it


def test_find_vesting_delegation_expirations(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.create_account('alice', 'bob', '{}')
    create_and_cancel_vesting_delegation(wallet, 'alice', 'bob')
    delegations = node.api.database.find_vesting_delegation_expirations(account='alice')['delegations']
    assert len(delegations) != 0
