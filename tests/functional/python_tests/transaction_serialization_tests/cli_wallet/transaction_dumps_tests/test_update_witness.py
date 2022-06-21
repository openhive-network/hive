import test_tools as tt

from ....local_tools import create_account_and_fund_it


def test_list_witness_votes(node, wallet):
    create_account_and_fund_it(wallet, 'alice', vests=tt.Asset.Test(100))

    wallet.api.update_witness('alice', 'http://url.html', tt.Account('alice').public_key,
                              {'account_creation_fee': tt.Asset.Test(10)})
