import test_tools as tt

from ..local_tools import create_proposal
from ....local_tools import create_account_and_fund_it


def test_find_proposals(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100),
                               tbds=tt.Asset.Tbd(300))
    create_proposal(wallet, 'alice')
    proposals = node.api.database.find_proposals(proposal_ids=[0])['proposals']
    assert len(proposals) != 0
