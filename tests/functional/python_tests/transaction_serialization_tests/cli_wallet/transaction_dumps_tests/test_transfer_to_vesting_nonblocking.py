import pytest

import test_tools as tt


@pytest.mark.testnet
def test_transfer_to_vesting_nonblocking(wallet):
    wallet.api.create_account('initminer', 'alice', '{}')
    wallet.api.transfer_to_vesting_nonblocking('initminer', 'alice', tt.Asset.Test(100))
