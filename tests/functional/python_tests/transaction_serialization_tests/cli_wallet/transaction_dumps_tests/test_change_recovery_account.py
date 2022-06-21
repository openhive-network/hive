import pytest

import test_tools as tt

from ......local_tools import create_account_and_fund_it


@pytest.mark.testnet
def test_change_recovery_account(wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    with wallet.in_single_transaction():
        wallet.api.create_account('alice', 'bob', '{}')
        wallet.api.transfer_to_vesting('alice', 'bob', tt.Asset.Test(100))
        wallet.api.transfer('initminer', 'bob', tt.Asset.Test(100), 'memo')

    wallet.api.change_recovery_account('alice', 'bob')
