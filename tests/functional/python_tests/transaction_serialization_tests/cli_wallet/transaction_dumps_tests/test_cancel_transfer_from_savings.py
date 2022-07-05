import pytest

import test_tools as tt

from .local_tools import import_keys
from ......local_tools import create_account_and_fund_it

@pytest.mark.testnet
def test_cancel_transfer_from_savings(nai_wallet):
    # create_account_and_fund_it(wallet, 'alice', vests=tt.Asset.Test(10))
    # wallet.api.create_account('initminer', 'bob', '{}')
    #
    # wallet.api.transfer_to_savings('initminer', 'alice', tt.Asset.Test(10), 'memo')
    #
    # wallet.api.transfer_from_savings('alice', 1, 'bob', tt.Asset.Test(1), 'memo')
    import_keys(nai_wallet)
    # nai_wallet.api.import_key('5JjLUVc1Az6m4sYRBf3ZXw17oPNiSv4XYL6BHGHHoNNoG518FdT') #alice
    # nai_wallet.api.import_key('5Jwwu32ZCb79g47BG6tybTx3rgU4BjvkeX96qx7kUL6QaS6taYz') #bob

    nai_wallet.api.cancel_transfer_from_savings('alice', 1)
