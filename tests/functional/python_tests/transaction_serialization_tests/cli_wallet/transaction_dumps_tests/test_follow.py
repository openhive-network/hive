import pytest

import test_tools as tt

from ......local_tools import create_account_and_fund_it


@pytest.mark.testnet
def test_follow(wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.create_account('initminer', 'bob', '{}')
    wallet.api.follow('alice', 'bob', ['blog'])
