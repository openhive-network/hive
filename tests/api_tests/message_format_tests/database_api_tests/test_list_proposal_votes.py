import test_tools as tt

from ..local_tools import create_proposal
from ....local_tools import create_account_and_fund_it


def test_list_proposal_votes(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100),
                               tbds=tt.Asset.Tbd(300))
    create_proposal(wallet, 'alice')
    wallet.api.update_proposal_votes('alice', [0], True)
    proposal_votes = node.api.database.list_proposal_votes(start=['alice'], limit=100, order='by_voter_proposal',
                                                           order_direction='ascending', status='all')['proposal_votes']
    assert len(proposal_votes) != 0
