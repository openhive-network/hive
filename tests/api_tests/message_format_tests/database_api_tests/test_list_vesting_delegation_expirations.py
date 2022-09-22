import test_tools as tt

from ..local_tools import create_and_cancel_vesting_delegation
from ....local_tools import create_account_and_fund_it, date_from_now


def test_list_vesting_delegation_expirations(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.create_account('alice', 'bob', '{}')
    create_and_cancel_vesting_delegation(wallet, 'alice', 'bob')
    delegations = node.api.database.list_vesting_delegation_expirations(start=['alice', date_from_now(weeks=-1), 0],
                                                                        limit=100, order='by_account_expiration')['delegations']
    assert len(delegations) != 0
