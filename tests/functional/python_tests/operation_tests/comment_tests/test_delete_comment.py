import pytest

import test_tools as tt

from hive_local_tools.functional.python.operation import Comment



@pytest.mark.parametrize("bottom_comment, self_bottom_comment", [(True, False), (False, False)], ids=['delete comment', 'delete post'])
def test_delete_comment(prepared_node: tt.InitNode, wallet: tt.Wallet, bottom_comment, self_bottom_comment):
    alice_comment = Comment(prepared_node, wallet, 'alice', bottom_comment=bottom_comment, self_bottom_comment=self_bottom_comment)

    alice_comment.post()
    alice_comment.delete()
