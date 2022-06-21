import pytest

import test_tools as tt

from ......local_tools import create_account_and_fund_it


@pytest.mark.testnet
def test_set_withdraw_vesting_route(wallet):
    create_account_and_fund_it(wallet, 'alice', vests=tt.Asset.Test(100))
    wallet.api.create_account('initminer', 'bob', '{}')

    wallet.api.set_withdraw_vesting_route('alice', 'bob', 30, True)
