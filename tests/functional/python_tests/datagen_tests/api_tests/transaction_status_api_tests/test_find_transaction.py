import time

import test_tools as tt

def test_find_existing_transaction(node, wallet):
    wallet.api.set_transaction_expiration(90)
    transaction = wallet.api.create_account('initminer', 'alice', '{}', broadcast=False)
    transaction_id = transaction['transaction_id']
    node.api.network_broadcast.broadcast_transaction(trx=transaction)

    assert node.api.transaction_status.find_transaction(transaction_id=transaction_id)['status'] == 'within_mempool'

    node.wait_number_of_blocks(1)
    assert node.api.database.is_known_transaction(id=transaction_id)['is_known'] is True
    assert node.api.transaction_status.find_transaction(transaction_id=transaction_id)['status'] == 'within_reversible_block'

    node.api.debug_node.debug_generate_blocks(debug_key=tt.Account('initminer').private_key, count=22, skip=0,
                                              miss_blocks=0, edit_if_needed=True)
    assert node.api.transaction_status.find_transaction(transaction_id=transaction_id)['status'] == 'within_irreversible_block'

    time.sleep(90)
    '''
        In this test case we can't really use find_transaction from transaction_status API to check status of 
        transaction (after expiration) because it holds transactions for one hour before it flag it as expired. Instead 
        of this is_known_transaction from database API is used. 
    '''
    assert node.api.database.is_known_transaction(id=transaction_id)['is_known'] is False


def test_find_unknown_transaction(node, wallet):
    response = node.api.transaction_status.find_transaction(transaction_id="0000000000000000000000000000000000000000")
    assert response['status'] == 'unknown'
