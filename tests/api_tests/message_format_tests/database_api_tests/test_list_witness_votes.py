import test_tools as tt

from ....local_tools import create_account_and_fund_it


def test_list_witness_votes(node, wallet):
    create_account_and_fund_it(wallet, 'alice', vests=tt.Asset.Test(100))
    create_account_and_fund_it(wallet, 'bob', vests=tt.Asset.Test(100))

    # mark alice as new witness
    wallet.api.update_witness('alice', 'http://url.html',
                              tt.Account('alice').public_key,
                              {'account_creation_fee': tt.Asset.Test(28), 'maximum_block_size': 131072,
                               'hbd_interest_rate': 1000})
    wallet.api.vote_for_witness('bob', 'alice', True)
    votes = node.api.database.list_witness_votes(start=["alice", "bob"], limit=100, order='by_witness_account')['votes']
    assert len(votes) != 0
