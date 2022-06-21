import pytest

import test_tools as tt


@pytest.mark.testnet
def test_create_funded_account_with_keys(wallet):
    key = 'TST8grZpsMPnH7sxbMVZHWEu1D26F3GwLW1fYnZEuwzT4Rtd57AER'
    wallet.api.create_funded_account_with_keys('initminer', 'alice', tt.Asset.Test(10), 'memo', '{}', key, key, key,
                                               key)
