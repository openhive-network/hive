import test_tools as tt

from ....local_tools import create_account_and_fund_it


def test_find_collateralized_conversion_requests(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    # convert_hive_with_collateral changes hives for hbd, this process takes three and half days
    wallet.api.convert_hive_with_collateral('alice', tt.Asset.Test(4))
    requests = node.api.database.find_collateralized_conversion_requests(account='alice')['requests']
    assert len(requests) != 0
