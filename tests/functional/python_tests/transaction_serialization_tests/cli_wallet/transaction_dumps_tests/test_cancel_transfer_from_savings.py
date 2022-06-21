import test_tools as tt

from ....local_tools import create_account_and_fund_it

def test_transfer_from_savings(wallet):
    create_account_and_fund_it(wallet, 'alice', vests=tt.Asset.Test(10))
    wallet.api.create_account('initminer', 'bob', '{}')

    wallet.api.transfer_to_savings('initminer', 'alice', tt.Asset.Test(10), 'memo')

    transfer = wallet.api.transfer_from_savings('alice', 1000, 'bob', tt.Asset.Test(1), 'memo')

    request_id = transfer['operations'][0][1]['request_id']

    wallet.api.cancel_transfer_from_savings('alice', request_id)
