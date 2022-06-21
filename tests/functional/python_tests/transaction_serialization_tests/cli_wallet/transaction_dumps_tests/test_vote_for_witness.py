import test_tools as tt

from ....local_tools import create_account_and_fund_it


def test_vote_for_witness(wallet):
    create_account_and_fund_it(wallet, 'alice', vests=tt.Asset.Test(100))
    wallet.api.vote_for_witness('alice', 'initminer', True)
