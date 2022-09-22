import test_tools as tt

from ....local_tools import create_account_and_fund_it


def test_find_comments(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.post_comment('alice', 'test-permlink', '', 'someone', 'test-title', 'this is a body', '{}')
    comments = node.api.database.find_comments(comments=[['alice', 'test-permlink']])['comments']
    assert len(comments) != 0
