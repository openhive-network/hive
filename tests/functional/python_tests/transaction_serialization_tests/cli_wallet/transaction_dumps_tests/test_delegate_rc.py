import test_tools as tt

from ....local_tools import create_account_and_fund_it


def test_delegate_rc(wallet):
    create_account_and_fund_it(wallet, 'alice', vests=tt.Asset.Test(10))
    wallet.api.create_account('initminer', 'bob', '{}')

    wallet.api.delegate_rc('alice', ['bob'], 10)
