import test_tools as tt


def test_transfer_to_savings(wallet):
    wallet.api.create_account('initminer', 'alice', '{}')

    wallet.api.transfer_to_savings('initminer', 'alice', tt.Asset.Test(100), 'memo')
