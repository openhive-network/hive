import test_tools as tt

from hive_local_tools import run_for
from hive_local_tools.api.message_format import create_proposal


# This test cannot be performed on 5 million blocklog because it doesn't contain any proposals - they were introduced
# after block with number 5000000. See the readme.md file in this directory for further explanation.
@run_for('testnet', 'live_mainnet')
def test_find_proposals(node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.create_account('alice', hives=tt.Asset.Test(100), vests=tt.Asset.Test(100), hbds=tt.Asset.Tbd(300))
        create_proposal(wallet, 'alice')
    proposals = node.api.database.find_proposals(proposal_ids=[0])['proposals']
    assert len(proposals) != 0
