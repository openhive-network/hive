import test_tools as tt

from ..local_tools import run_for, create_proposal
from ....local_tools import create_account_and_fund_it


# This test is not performed on 5 million blocklog because proposals feature was introduced after block with number
# 5000000. See the readme.md file in this directory for further explanation.
@run_for('testnet', 'mainnet_64m')
def test_list_proposal_votes(node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100), tbds=tt.Asset.Tbd(300))
        create_proposal(wallet, 'alice')
        wallet.api.update_proposal_votes('alice', [0], True)
    proposal_votes = node.api.condenser.list_proposal_votes(['alice'], 100, 'by_voter_proposal', 'ascending',
                                                                     'all')
    assert len(proposal_votes) != 0
