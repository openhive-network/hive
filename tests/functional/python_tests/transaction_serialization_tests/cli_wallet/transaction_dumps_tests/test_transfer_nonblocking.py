import test_tools as tt

def test_transfer_nonblocking(wallet):
    wallet.api.create_account('initminer', 'alice', '{}')

    wallet.api.transfer_nonblocking('initminer', 'alice', tt.Asset.Test(10), 'memo')
