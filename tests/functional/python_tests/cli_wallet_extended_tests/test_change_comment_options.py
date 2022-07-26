import test_tools as tt

from ....local_tools import create_account_and_fund_it


def test_change_comment_options(node, wallet):
    beneficiaries = {"beneficiaries": [{"account": "alice", "weight": 1000}]}

    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.post_comment('alice', 'permlink', '', 'parent-permlink', 'title', 'body', '{}')

    comment_before = node.api.database.find_comments(comments=[['alice', 'permlink']])['comments'][0]

    wallet.api.change_comment_options('alice', 'permlink', tt.Asset.Tbd(10), 10, False, False, beneficiaries)

    comment_after = node.api.database.find_comments(comments=[['alice', 'permlink']])['comments'][0]

    assert comment_before['max_accepted_payout'] != tt.Asset.Tbd(10)
    assert comment_after['max_accepted_payout'] == tt.Asset.Tbd(10)

    assert comment_before['percent_hbd'] != 10
    assert comment_after['percent_hbd'] == 10

    assert comment_before['allow_votes'] is True
    assert comment_after['allow_votes'] is False

    assert comment_before['allow_curation_rewards'] is True
    assert comment_after['allow_curation_rewards'] is False

    assert comment_before['beneficiaries'] != [{"account": "alice", "weight": 1000}]
    assert comment_after['beneficiaries'] == [{"account": "alice", "weight": 1000}]
