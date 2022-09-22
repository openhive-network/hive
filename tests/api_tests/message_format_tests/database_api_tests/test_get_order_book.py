import test_tools as tt

from ....local_tools import create_account_and_fund_it


def test_get_order_book(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    create_account_and_fund_it(wallet, 'bob', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100),
                               tbds=tt.Asset.Tbd(100))

    wallet.api.create_order('alice', 0, tt.Asset.Test(100), tt.Asset.Tbd(100), False, 3600)  # Sell 100 HIVE for 100 HBD
    wallet.api.create_order('bob', 0, tt.Asset.Tbd(50), tt.Asset.Test(100), False, 3600)  # Buy 100 HIVE for 50 HBD

    response = node.api.database.get_order_book(limit=100)
    assert len(response['asks']) != 0
    assert len(response['bids']) != 0
