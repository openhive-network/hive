import test_tools as tt

from ..local_tools import run_for


# This test is not performed on 5 million block log because of lack of accounts with collateralized conversion requests.
# remote node update. See the readme.md file in this directory for further explanation.
@run_for('testnet', 'mainnet_64m')
def test_list_collateralized_conversion_requests(prepared_node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        wallet.create_account('alice', hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
        wallet.api.convert_hive_with_collateral('alice', tt.Asset.Test(4))
    requests = prepared_node.api.database.list_collateralized_conversion_requests(start=[''], limit=100, order='by_account')['requests']
    assert len(requests) != 0
