import test_tools as tt

from ....local_tools import create_account_and_fund_it


def test_find_recurrent_transfers(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.create_account('initminer', 'bob', '{}')
    # create transfer from alice to bob for amount 5 test hives every 720 hours, repeat 12 times
    wallet.api.recurrent_transfer('alice', 'bob', tt.Asset.Test(5), 'memo', 720, 12)
    # "from" is a Python keyword and needs workaround
    recurrent_transfers = node.api.database.find_recurrent_transfers(**{'from': 'alice'})['recurrent_transfers']
    assert len(recurrent_transfers) != 0
