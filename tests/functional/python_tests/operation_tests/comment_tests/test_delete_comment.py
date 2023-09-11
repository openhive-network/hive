import pytest

import test_tools as tt

from hive_local_tools.functional.python.operation import Comment
from test_tools.__private.exceptions import CommunicationError
from hive_local_tools.functional.python.operation import get_current_mana, create_transaction_with_any_operation


@pytest.mark.parametrize("bottom_comment, self_bottom_comment", [(False, False)])
def test_delete_comment(prepared_node, wallet, bottom_comment, self_bottom_comment):

    # prepared_node = tt.InitNode()
    # prepared_node.run()
    # wallet = tt.Wallet(attach_to=prepared_node)

    alice_comment = Comment(prepared_node, wallet, 'alice', bottom_comment=bottom_comment, self_bottom_comment=self_bottom_comment)
    bob_comment = Comment(prepared_node, wallet, 'bob', bottom_comment=bottom_comment, self_bottom_comment=self_bottom_comment)

    alice_comment.post()
    bob_comment.post()

    alice0_mana = prepared_node.api.rc.find_rc_accounts(accounts=['alice'])['rc_accounts'][0]['rc_manabar']['current_mana']
    bob0_mana = prepared_node.api.rc.find_rc_accounts(accounts=['bob'])['rc_accounts'][0]['rc_manabar']['current_mana']


    # create_transaction_with_any_operation(wallet, 'delete_comment', author='alice', permlink='permlink-alice')
    alice_comment.delete()

    with wallet.in_single_transaction():
            wallet.api.create_account('alice', 'junk-acc', '{}')
            wallet.api.create_account('bob', 'junk-acc1', '{}')


    alice1_mana = prepared_node.api.rc.find_rc_accounts(accounts=['alice'])['rc_accounts'][0]['rc_manabar']['current_mana']
    bob1_mana = prepared_node.api.rc.find_rc_accounts(accounts=['bob'])['rc_accounts'][0]['rc_manabar']['current_mana']

    assert alice0_mana > alice1_mana
    assert bob0_mana == bob1_mana

    assert alice_comment.is_rc_mana_decrease_after_comment()
    assert bob_comment.is_rc_mana_decrease_after_comment()

    assert alice_comment.is_comment_deleted()
    assert alice_comment.is_rc_mana_decrease_after_comment_delete()







    # try:
    #     alice_comment.delete()
    # except CommunicationError:
    #     pass

    # ops_in_block0 = prepared_node.api.account_history.get_ops_in_block(block_num=block)
    # ops_in_block2 = prepared_node.api.account_history.get_ops_in_block(block_num=block + 1)
    # ops_in_block3 = prepared_node.api.account_history.get_ops_in_block(block_num=block + 2)
    # ops_in_block4 = prepared_node.api.account_history.get_ops_in_block(block_num=block + 3)

    # assert alice_comment.is_rc_mana_decrease_after_comment_delete
 