import pytest

import test_tools as tt

from ....local_tools import create_account_and_fund_it


def test_delete_existing_comment(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.post_comment('alice', 'test-permlink', '', 'someone', 'test-title', 'this is a body', '{}')

    assert len(node.api.database.find_comments(comments=[['alice', 'test-permlink']])['comments']) == 1
    wallet.api.delete_comment('alice', 'test-permlink')
    assert len(node.api.database.find_comments(comments=[['alice', 'test-permlink']])['comments']) == 0


def test_delete_nonexistent_comment(node, wallet):
    with pytest.raises(tt.exceptions.CommunicationError):
        wallet.api.delete_comment('initminer', 'fake-permlink')


def test_delete_comment_twice(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.post_comment('alice', 'test-permlink', '', 'someone', 'test-title', 'this is a body', '{}')
    wallet.api.delete_comment('alice', 'test-permlink')
    with pytest.raises(tt.exceptions.CommunicationError):
        wallet.api.delete_comment('alice', 'test-permlink')
