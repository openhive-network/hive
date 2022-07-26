import test_tools as tt

from ....local_tools import create_account_and_fund_it


def test_create_order2(wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))

    wallet.api.create_order2('alice', 0, tt.Asset.Test(1), {"base":tt.Asset.Test(1), "quote":tt.Asset.Tbd(1)},
                             False, 10)

    open_orders = wallet.api.get_open_orders('alice')

    assert len(open_orders) == 1
