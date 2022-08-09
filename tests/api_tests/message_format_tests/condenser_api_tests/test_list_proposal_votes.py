import test_tools as tt

from ..local_tools import run_for, create_proposal
from ....local_tools import create_account_and_fund_it


@run_for('testnet', 'mainnet_64m')
def test_list_proposal_votes_in_testnet_and_mainnet_64m(prepared_node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100), tbds=tt.Asset.Tbd(300))
        create_proposal(wallet, 'alice')
        wallet.api.update_proposal_votes('alice', [0], True)
    proposal_votes = prepared_node.api.condenser.list_proposal_votes(['alice'], 100, 'by_voter_proposal', 'ascending',
                                                                     'all')
    assert len(proposal_votes) != 0
