import pytest

import test_tools as tt

from hive_local_tools.functional.python.operation import Comment


def test_if_comment_exist(prepared_node: tt.InitNode, wallet: tt.Wallet):
    alice_comment = Comment(prepared_node, wallet, 'alice', bottom_comment=False)
    alice_comment.post()

    assert alice_comment.is_rc_mana_decrease_after_comment()
    assert alice_comment.is_comment_post(prepared_node)


@pytest.mark.parametrize("bottom_comment, self_bottom_comment", [(False, True), (True, False)], ids=['comment own post', 'comment someone else post'])
def test_if_comment_with_parent_exist(prepared_node: tt.InitNode, wallet: tt.Wallet, bottom_comment, self_bottom_comment):
    alice_comment = Comment(prepared_node, wallet, 'alice', bottom_comment=bottom_comment, self_bottom_comment=self_bottom_comment)
    alice_comment.post()

    assert alice_comment.is_rc_mana_decrease_after_comment
    assert alice_comment.is_comment_post(prepared_node)


@pytest.mark.parametrize("bottom_comment, self_bottom_comment", [(True, False), (False, False)], ids=['update comment', 'update post'])
def test_update_without_comment(prepared_node: tt.InitNode, wallet: tt.Wallet, bottom_comment, self_bottom_comment):
    alice_comment = Comment(prepared_node, wallet, 'alice', bottom_comment=bottom_comment, self_bottom_comment=self_bottom_comment)
    alice_comment.post()
    alice_comment.update()

    assert alice_comment.is_rc_mana_decrease_after_comment_update()
    assert alice_comment.is_comment_post(prepared_node)


@pytest.mark.parametrize("bottom_comment, self_bottom_comment", [(True, False), (False, False)], ids=['update comment', 'update post'])
def test_update_with_comment(prepared_node: tt.InitNode, wallet: tt.Wallet, bottom_comment, self_bottom_comment):
    alice_comment = Comment(prepared_node, wallet, 'alice', bottom_comment=bottom_comment, self_bottom_comment=self_bottom_comment)
    alice_comment.post()
    alice_comment.post_top_comment(self_comment=False)
    alice_comment.update()

    assert alice_comment.is_rc_mana_decrease_after_comment_update()
    assert alice_comment.is_comment_post(prepared_node)


@pytest.mark.parametrize("bottom_comment, self_bottom_comment", [(True, False), (False, False)], ids=['update comment with comment', 'update post with comment'])
def test_update_with_comment_after_cashout(prepared_node: tt.InitNode, wallet: tt.Wallet, bottom_comment, self_bottom_comment):
    alice_comment = Comment(prepared_node, wallet, 'alice', bottom_comment=bottom_comment, self_bottom_comment=self_bottom_comment)
    alice_comment.post()
    alice_comment.post_top_comment(self_comment=False)

    # vote is necessary to cashout
    alice_comment.vote()

    # waiting for cashout 7 days
    prepared_node.restart(time_offset="+7d")
    # WALKAROUND: refresh RC state after restart of node by creating 'junk operation'
    wallet.api.create_account(alice_comment.author_name, 'junk-acc', '{}')

    alice_comment.update()

    assert alice_comment.is_rc_mana_decrease_after_comment_update()
    assert alice_comment.is_comment_post(prepared_node)


@pytest.mark.parametrize("bottom_comment, self_bottom_comment", [(True, False), (False, False)], ids=['update comment', 'update post'])
def test_update_with_comment_votes_and_downvotes(wallet, prepared_node, bottom_comment, self_bottom_comment):
    author0_comment = Comment(prepared_node, wallet, 'author0', bottom_comment=bottom_comment, self_bottom_comment=self_bottom_comment)
    author1_comment = Comment(prepared_node, wallet, 'author1', bottom_comment=bottom_comment, self_bottom_comment=self_bottom_comment)
    author2_comment = Comment(prepared_node, wallet, 'author2', bottom_comment=bottom_comment, self_bottom_comment=self_bottom_comment)
    author3_comment = Comment(prepared_node, wallet, 'author3', bottom_comment=bottom_comment, self_bottom_comment=self_bottom_comment)

    comments = [author0_comment, author1_comment, author2_comment, author3_comment]

    with wallet.in_single_transaction():
        for comment in comments:
            comment.post()

    for comment in comments:
        comment.post_top_comment(self_comment=False)

    with wallet.in_single_transaction():
        for comment in comments:
            comment.update()

  # Not in single transaction, because is possible t ocomment only every 3 seconds
    for comment in comments:
        comment.vote()

    author0_comment.downvote()
    author2_comment.downvote()

    author0_comment.update()
    author1_comment.update()

    # waiting for cashout 7 days
    prepared_node.restart(time_offset="+7d")

    reward_hbd_balances = []
    for comment in comments:
        reward_hbd_balances.append(tt.Asset.from_(wallet.api.get_account(comment.author_name)['reward_hbd_balance']))

    # verify is downvoting work correctly after post update
    assert reward_hbd_balances[2] < reward_hbd_balances[3]
    # verify is downvoting work correctly
    assert reward_hbd_balances[0] < reward_hbd_balances[1]
    # verify is account update have no impact on voting
    assert reward_hbd_balances[0] == reward_hbd_balances[2]
    # verify is account update have no impact on downvoting
    assert reward_hbd_balances[1] == reward_hbd_balances[3]
