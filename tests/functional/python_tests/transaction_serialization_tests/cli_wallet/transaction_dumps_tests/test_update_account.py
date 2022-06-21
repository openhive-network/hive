import test_tools as tt

from ....local_tools import create_account_and_fund_it


def test_update_account(wallet):
    create_account_and_fund_it(wallet, 'alice', vests=tt.Asset.Test(100))
    key = 'TST8grZpsMPnH7sxbMVZHWEu1D26F3GwLW1fYnZEuwzT4Rtd57AER'
    wallet.api.update_account('alice', '{}', key, key, key, key)
