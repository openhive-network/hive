import test_tools as tt

from ....local_tools import create_account_and_fund_it


def test_list_hbd_conversion_requests(wallet, node):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100),
                               tbds=tt.Asset.Tbd(100))
    wallet.api.convert_hbd('alice', tt.Asset.Tbd(1.25))
    requests = node.api.database.list_hbd_conversion_requests(start=['alice', 0], limit=100, order='by_account')['requests']
    assert len(requests) != 0
