import test_tools as tt

from ..local_tools import create_proposal, run_for


# This test cannot be performed on 5 million blocklog because it doesn't contain any proposals - they were introduced
# after block with number 5000000. See the readme.md file in this directory for further explanation.
@run_for('testnet', 'mainnet_64m')
def test_list_proposals(prepared_node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        wallet.create_account('alice', hives=tt.Asset.Test(100), vests=tt.Asset.Test(100),
                              hbds=tt.Asset.Tbd(300))
        create_proposal(wallet, 'alice')
    proposals = prepared_node.api.database.list_proposals(start=[''], limit=100, order='by_creator',
                                                          order_direction='ascending', status='all')['proposals']
    assert len(proposals) != 0
