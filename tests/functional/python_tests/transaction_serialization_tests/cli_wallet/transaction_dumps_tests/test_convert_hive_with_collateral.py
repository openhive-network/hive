import pytest

import test_tools as tt


@pytest.mark.testnet
def test_convert_hive_with_collateral(wallet):
    wallet.api.create_account('initminer', 'alice', '{}')
    wallet.api.transfer('initminer', 'alice', tt.Asset.Test(100), 'memo')
    wallet.api.transfer_to_vesting('initminer', 'alice', tt.Asset.Test(100))
    wallet.api.convert_hive_with_collateral('alice', tt.Asset.Test(10))
