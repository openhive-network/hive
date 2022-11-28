import test_tools as tt

from hive_local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'live_mainnet')
def test_find_hbd_conversion_requests(node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.create_account('alice', hives=tt.Asset.Test(100), vests=tt.Asset.Test(100), hbds=tt.Asset.Tbd(100))
        wallet.api.convert_hbd('alice', tt.Asset.Tbd(1.25))
    account = node.api.database.list_hbd_conversion_requests(start=['', 0], limit=100,
                                                                      order='by_account')['requests'][0]['owner']
    requests = node.api.database.find_hbd_conversion_requests(account=account)['requests']
    assert len(requests) != 0
