import pytest

import test_tools as tt


@pytest.mark.testnet
def test_withdraw_vesting(wallet):
    wallet.api.create_account('initminer', 'alice', "{}")
    wallet.api.transfer_to_vesting('initminer', 'alice', tt.Asset.Test(100))

    wallet.api.withdraw_vesting('alice', tt.Asset.Vest(10))
