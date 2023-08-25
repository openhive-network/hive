import pytest
import test_tools as tt


# all test cases from https://gitlab.syncad.com/hive/hive/-/issues/503
@pytest.mark.testnet
def test_create_post(prepared_node, wallet, alice):
    # check RC after creating post
    alice.post_comment('test-permlink', '', 'example-category', 'test-title', 'this is a body', '{}')
    assert len(prepared_node.api.database.find_comments(comments=[['alice', 'test-permlink']])['comments']) == 1, \
        "Post wasn't created"


def test_comment_post(prepared_node, wallet, alice, bob):
    # check RC after creating post
    alice.post_comment('test-permlink', '', 'example-category', 'test-title', 'this is a body', '{}')


