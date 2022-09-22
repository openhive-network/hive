import test_tools as tt

from ....local_tools import create_account_and_fund_it



def test_get_comment_pending_payouts(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.post_comment('alice', 'test-permlink', '', 'test-parent-permlink', 'test-title', 'test-body', '{}')
    cashout_info = node.api.database.get_comment_pending_payouts(comments=[['alice', 'test-permlink']])['cashout_infos']
    assert len(cashout_info) != 0
