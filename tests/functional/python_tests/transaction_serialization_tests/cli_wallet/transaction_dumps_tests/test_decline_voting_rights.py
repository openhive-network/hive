import test_tools as tt

from ....local_tools import create_account_and_fund_it


def test_decline_voting_rights(wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.decline_voting_rights('alice', True)
