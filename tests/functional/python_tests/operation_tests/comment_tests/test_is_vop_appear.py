import test_tools as tt
from hive_local_tools.functional.python.operation import Comment
from test_tools.__private.exceptions import CommunicationError


def test_ineffective_delete_comment_operation():
    prepared_node = tt.InitNode()
    prepared_node.config.plugin.append("account_history_api")
    prepared_node.run()
    wallet = tt.Wallet(attach_to=prepared_node)

    alice_comment = Comment(prepared_node, wallet, 'alice')

    alice_comment.post()
    alice_comment.post_top_comment(False)
    try:
        alice_comment.delete()
    except CommunicationError as error:
        pass

    prepared_node.wait_number_of_blocks(5)

    alice_AH_vops = prepared_node.api.account_history.enum_virtual_ops(block_range_begin=0 ,block_range_end=1000, include_reversible=True, group_by_block=False)["ops"]
    assert not alice_comment.is_comment_deleted()
