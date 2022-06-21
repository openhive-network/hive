import test_tools as tt

from ....local_tools import create_account_and_fund_it


def test_find_recurrent_transfers(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.create_account('initminer', 'bob', '{}')

    wallet.api.recurrent_transfer('alice', 'bob', tt.Asset.Test(5), 'memo', 720, 12)

    wallet.api.find_recurrent_transfers('alice')
    