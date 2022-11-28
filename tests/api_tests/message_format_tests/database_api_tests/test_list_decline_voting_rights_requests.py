import test_tools as tt

from hive_local_tools import run_for


# This test cannot be performed on 5 million blocklog and most recent (live_mainnet) blocklog because they don't contain any
# decline voting rights requests.
@run_for('testnet')
def test_list_decline_voting_rights_requests(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account('alice', hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.decline_voting_rights('alice', True)
    requests = node.api.database.list_decline_voting_rights_requests(start='', limit=100, order='by_account')['requests']
    assert len(requests) != 0
