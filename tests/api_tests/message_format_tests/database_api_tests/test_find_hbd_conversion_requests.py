import test_tools as tt

from ....local_tools import create_account_and_fund_it


def test_find_hbd_conversion_requests(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100),
                               tbds=tt.Asset.Tbd(100))
    wallet.api.convert_hbd('alice', tt.Asset.Tbd(1.25))
    requests = node.api.database.find_hbd_conversion_requests(account='alice')['requests']
    assert len(requests) != 0
