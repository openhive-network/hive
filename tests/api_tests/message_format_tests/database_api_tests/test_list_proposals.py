import test_tools as tt


from ..local_tools import create_proposal
from ....local_tools import create_account_and_fund_it


def test_list_proposals(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100),
                               tbds=tt.Asset.Tbd(300))
    create_proposal(wallet, 'alice')
    proposals = node.api.database.list_proposals(start=["alice"], limit=100, order='by_creator',
                                                 order_direction='ascending', status='all')['proposals']
    assert len(proposals) != 0
