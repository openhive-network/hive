import test_tools as tt


def test_transfer_to_vesting(wallet):
    wallet.api.create_account('initminer', 'alice', '{}')
    wallet.api.transfer_to_vesting('initminer', 'alice', tt.Asset.Test(100))
