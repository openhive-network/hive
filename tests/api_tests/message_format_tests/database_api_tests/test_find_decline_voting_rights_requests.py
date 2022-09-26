import test_tools as tt

from ..local_tools import run_for


# This test cannot be performed on 5 million blocklog and most recent (current) blocklog because they don't contain any
# decline voting rights requests. See the readme.md file in this directory for further explanation.
@run_for('testnet')
def test_find_decline_voting_rights_requests(prepared_node):
    wallet = tt.Wallet(attach_to=prepared_node)
    wallet.create_account('alice', hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.decline_voting_rights('alice', True)
    requests = prepared_node.api.database.find_decline_voting_rights_requests(accounts=['alice'])['requests']
    assert len(requests) != 0
