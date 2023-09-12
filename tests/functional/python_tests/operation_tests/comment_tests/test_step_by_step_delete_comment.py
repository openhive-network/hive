import test_tools as tt
from hive_local_tools.functional.python.operation import Comment, get_transaction_timestamp

def test_step_by_step(prepared_node: tt.InitNode, wallet: tt.Wallet):
    # prepared_node = tt.InitNode()
    # prepared_node.config.plugin.append("account_history_api")
    
    # prepared_node.run()
    # wallet = tt.Wallet(attach_to=prepared_node)

    alice_comment = Comment(prepared_node, wallet, 'alice')
    alice_obj = alice_comment.author_obj

    post_transaction = alice_comment.post()

    delete_transaction = alice_comment.delete()

    post_rc_cost = int(post_transaction["rc_cost"])
    post_timestamp = get_transaction_timestamp(prepared_node, post_transaction)

    delete_rc_cost = int(delete_transaction["rc_cost"])
    delete_timestamp = get_transaction_timestamp(prepared_node, delete_transaction)

    alice_obj.rc_manabar.assert_rc_current_mana_is_reduced(operation_rc_cost=post_rc_cost, operation_timestamp=post_timestamp)
    alice_obj.rc_manabar.assert_rc_current_mana_is_reduced(operation_rc_cost=delete_rc_cost, operation_timestamp=delete_timestamp)
