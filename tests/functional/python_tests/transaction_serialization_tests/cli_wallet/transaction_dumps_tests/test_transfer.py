import test_tools as tt


def test_transfer(wallet):
    wallet.api.create_account('initminer', 'alice', '{}')

    wallet.api.transfer('initminer', 'alice', tt.Asset.Test(10), 'memo')
