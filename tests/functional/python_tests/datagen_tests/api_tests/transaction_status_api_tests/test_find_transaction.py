import pytest

from ......local_tools import date_from_now


def test_find_transaction_before_broadcasting_it(node, wallet):
    transaction = wallet.api.create_account('initminer', 'alice', '{}', broadcast=False)
    assert node.api.transaction_status.find_transaction(transaction_id=transaction['transaction_id'])['status'] == 'unknown'


def test_find_transaction_right_after_broadcasting_it_manually(node, wallet):
    transaction = wallet.api.create_account('initminer', 'alice', '{}', broadcast=False)
    node.api.network_broadcast.broadcast_transaction(trx=transaction)
    assert node.api.transaction_status.find_transaction(transaction_id=transaction['transaction_id'])['status'] == 'within_mempool'


def test_find_transaction_in_reversible_block_pool(node, wallet):
    transaction = wallet.api.create_account('initminer', 'alice', '{}')
    node.wait_number_of_blocks(1)
    assert node.api.database.is_known_transaction(id=transaction['transaction_id'])['is_known'] is True
    assert node.api.transaction_status.find_transaction(transaction_id=transaction['transaction_id'])['status'] == 'within_reversible_block'


def test_find_transaction_in_irreversible_block_pool(node, wallet):
    transaction = wallet.api.create_account('initminer', 'alice', '{}')
    assert node.api.database.is_known_transaction(id=transaction['transaction_id'])['is_known'] is True

    # waiting 22 blocks for the block with the transaction to become irreversible
    node.wait_number_of_blocks(22)
    assert node.api.transaction_status.find_transaction(transaction_id=transaction['transaction_id'])['status'] == 'within_irreversible_block'


@pytest.mark.parametrize(
    'unknown_id', [
        '0000000000000000000000000000000000000000',
        '0000000000000000000000000000000000000001',
    ]
)
def test_find_transaction_with_expiration_set_half_year_earlier(node, wallet, unknown_id):
    response = node.api.transaction_status.find_transaction(transaction_id=unknown_id, expiration=date_from_now(weeks=-24))
    assert response['status'] == 'too_old'


@pytest.mark.parametrize(
    'unknown_id', [
        '0000000000000000000000000000000000000000',
        '0000000000000000000000000000000000000001',
    ]
)
def test_find_transaction_with_unknown_id(node, wallet, unknown_id):
    response = node.api.transaction_status.find_transaction(transaction_id=unknown_id)
    assert response['status'] == 'unknown'


@pytest.mark.parametrize(
    'unknown_id', [
        '0000000000000000000000000000000000000000',
        '0000000000000000000000000000000000000001',
    ]
)
def test_find_transaction_after_producing_empty_blocks_for_hour(sped_up_node, unknown_id):
    sped_up_node.wait_number_of_blocks(1260)
    node_time = sped_up_node.api.database.get_dynamic_global_properties()['time']
    response = sped_up_node.api.transaction_status.find_transaction(transaction_id=unknown_id, expiration=node_time)
    assert response['status'] == 'expired_irreversible'
