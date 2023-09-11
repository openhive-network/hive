import test_tools as tt
from hive_local_tools.functional.python.operation import Comment

def test_step_by_step(prepared_node, wallet):
    # prepared_node = tt.InitNode()
    # prepared_node.config.plugin.append("account_history_api")
    
    # prepared_node.run()
    # wallet = tt.Wallet(attach_to=prepared_node)

    alice_comment = Comment(prepared_node, wallet, 'alice')

    alice_comment.post()

    alice_max_mana = int(prepared_node.api.rc.find_rc_accounts(accounts=['alice'])['rc_accounts'][0]["max_rc"])
    one_block_regeneration = (alice_max_mana * 3 / ( 5 * 60 * 60 * 24 ))

    alice0_mana = int(prepared_node.api.rc.find_rc_accounts(accounts=['alice'])['rc_accounts'][0]['rc_manabar']['current_mana'])
    alice_comment.delete()
    alice1_mana = int(prepared_node.api.rc.find_rc_accounts(accounts=['alice'])['rc_accounts'][0]['rc_manabar']['current_mana'])
    alice1_mana_without_delete = alice0_mana + one_block_regeneration

    assert alice_comment.is_rc_mana_decrease_after_comment()
    assert alice_comment.is_comment_post(prepared_node)
    assert alice_comment.is_comment_deleted()
    assert alice1_mana_without_delete > alice1_mana
