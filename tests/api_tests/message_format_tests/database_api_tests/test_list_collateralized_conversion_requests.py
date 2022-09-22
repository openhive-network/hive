import test_tools as tt

from ....local_tools import create_account_and_fund_it


def test_list_collateralized_conversion_requests(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.convert_hive_with_collateral('alice', tt.Asset.Test(4))
    requests = node.api.database.list_collateralized_conversion_requests(start=[''], limit=100, order='by_account')['requests']
    assert len(requests) != 0
