import test_tools as tt

from ....local_tools import create_account_and_fund_it


def test_update_account_auth_key(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.update_account_memo_key('alice', 'TST84oS1GW3yb9QaRaGztrqH5cHqeFFyLgLGSK4FoLEoDFBJqCnSJ')
