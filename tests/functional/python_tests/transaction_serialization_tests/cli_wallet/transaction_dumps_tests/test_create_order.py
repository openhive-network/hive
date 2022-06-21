import pytest

import test_tools as tt

from ......local_tools import create_account_and_fund_it


@pytest.mark.testnet
def test_create_order(wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.create_order('alice', 1000, tt.Asset.Test(1), tt.Asset.Tbd(1), False, 1000)
