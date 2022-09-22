import test_tools as tt

from ....local_tools import create_account_and_fund_it


def test_list_witnesses(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    # mark alice as new witness
    wallet.api.update_witness('alice', 'http://url.html',
                              tt.Account('alice').public_key,
                              {'account_creation_fee': tt.Asset.Test(28), 'maximum_block_size': 131072,
                               'hbd_interest_rate': 1000})
    witnesses = node.api.database.list_witnesses(start='', limit=100, order='by_name')['witnesses']
    assert len(witnesses) != 0
