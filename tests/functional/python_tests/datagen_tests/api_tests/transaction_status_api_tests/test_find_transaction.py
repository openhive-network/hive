import time
from ......local_tools import date_from_now


def test_find_existing_transaction(node, wallet):
    wallet.api.set_transaction_expiration(90)
    transaction = wallet.api.create_account('initminer', 'alice', '{}', broadcast=False)
    transaction_id = transaction['transaction_id']
    assert node.api.transaction_status.find_transaction(transaction_id=transaction_id)['status'] == 'unknown'
    node.api.network_broadcast.broadcast_transaction(trx=transaction)
    assert node.api.transaction_status.find_transaction(transaction_id=transaction_id)['status'] == 'within_mempool'

    node.wait_number_of_blocks(1)
    assert node.api.database.is_known_transaction(id=transaction_id)['is_known'] is True
    assert node.api.transaction_status.find_transaction(transaction_id=transaction_id)['status'] == 'within_reversible_block'

    node.wait_number_of_blocks(22)
    assert node.api.transaction_status.find_transaction(transaction_id=transaction_id)['status'] == 'within_irreversible_block'

    time.sleep(90)
    '''
        In this test case we can't really use find_transaction from transaction_status API to check status of 
        transaction (after expiration) because it holds transactions for one hour before it flag it as expired. Instead 
        of this is_known_transaction from database API is used. 
    '''
    assert node.api.database.is_known_transaction(id=transaction_id)['is_known'] is False


def test_find_too_old_transaction(node, wallet):
    unknown_id = '0000000000000000000000000000000000000000'
    response = node.api.transaction_status.find_transaction(transaction_id=unknown_id, expiration=date_from_now(weeks=-24))
    assert response['status'] == 'too_old'


def test_find_unknown_transaction(node, wallet):
    unknown_id = '0000000000000000000000000000000000000000'
    response = node.api.transaction_status.find_transaction(transaction_id=unknown_id)
    assert response['status'] == 'unknown'


def test_find_expired_irreversible_transaction(sped_up_node, wallet):
    unknown_id = '0000000000000000000000000000000000000001'
    sped_up_node.wait_number_of_blocks(1260)
    node_time = sped_up_node.api.database.get_dynamic_global_properties()['time']
    response = sped_up_node.api.transaction_status.find_transaction(transaction_id=unknown_id, expiration=node_time)
    assert response['status'] == 'expired_irreversible'
