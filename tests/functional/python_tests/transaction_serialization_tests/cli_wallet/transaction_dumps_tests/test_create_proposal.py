import test_tools as tt

from ....local_tools import create_account_and_fund_it, date_from_now


def test_create_proposal(wallet):
    create_account_and_fund_it(wallet, 'alice', tbds=tt.Asset.Tbd(1000000), vests=tt.Asset.Test(1000000))

    wallet.api.post_comment('alice', 'permlink', '', 'parent-permlink', 'title', 'body', '{}')

    wallet.api.create_proposal('alice', 'alice', date_from_now(weeks=16), date_from_now(weeks=20),
                                        tt.Asset.Tbd(1000), 'subject-1', 'permlink')
