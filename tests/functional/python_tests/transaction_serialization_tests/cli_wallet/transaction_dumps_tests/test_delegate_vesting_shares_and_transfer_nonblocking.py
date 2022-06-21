import pytest

import test_tools as tt

from ......local_tools import create_account_and_fund_it


@pytest.mark.testnet
def test_delegate_vesting_shares_and_transfer_nonblocking(wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), tbds=tt.Asset.Tbd(100),
                               vests=tt.Asset.Test(100))
    create_account_and_fund_it(wallet, 'bob', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.delegate_vesting_shares_and_transfer_nonblocking('alice', 'bob', tt.Asset.Vest(1), tt.Asset.Tbd(6),
                                                                'memo')
