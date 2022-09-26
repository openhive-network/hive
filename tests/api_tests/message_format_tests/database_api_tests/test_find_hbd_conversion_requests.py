import test_tools as tt

from ..local_tools import run_for


@run_for('testnet')
def test_find_hbd_conversion_requests_in_testnet(prepared_node):
    wallet = tt.Wallet(attach_to=prepared_node)
    wallet.create_account('alice', hives=tt.Asset.Test(100), vests=tt.Asset.Test(100), hbds=tt.Asset.Tbd(100))
    wallet.api.convert_hbd('alice', tt.Asset.Tbd(1.25))
    requests = prepared_node.api.database.find_hbd_conversion_requests(account='alice')['requests']
    assert len(requests) != 0


@run_for('mainnet_5m', 'mainnet_64m')
def test_find_hbd_conversion_requests_in_mainnet(prepared_node):
    account = prepared_node.api.database.list_hbd_conversion_requests(start=['alice', 0], limit=100,
                                                                      order='by_account')['requests'][0]['owner']
    requests = prepared_node.api.database.find_hbd_conversion_requests(account=account)['requests']
    assert len(requests) != 0
