import pytest
from time import sleep

import test_tools as tt


@pytest.mark.parametrize(
    'max_block_age_value', [-1, 2000, -2000, 20000, -20000]
)
def test_broadcast_account_creating_with_correct_values(node, wallet, max_block_age_value):
    transaction = wallet.api.create_account('initminer', 'alice', '{}', broadcast=False)
    node.api.network_broadcast.broadcast_transaction(trx=transaction,  max_block_age=max_block_age_value)
    node.wait_number_of_blocks(22)

    assert transaction != node.api.account_history.get_transaction(id=transaction['transaction_id'],
                                                                   include_reversible=True)
    assert wallet.api.get_account('alice')


def test_broadcast_account_creating_with_incorrect_value(node, wallet):
    with pytest.raises(tt.exceptions.CommunicationError):
        for i in range(6):
            transaction = wallet.api.create_account('initminer', f'alice{i}', '{}', broadcast=False)
            node.api.network_broadcast.broadcast_transaction(trx=transaction,  max_block_age=0)
            sleep(1)
    node.wait_number_of_blocks(22)

    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.account_history.get_transaction(id=transaction['transaction_id'], include_reversible=True)

    with pytest.raises(tt.exceptions.CommunicationError):
        wallet.api.get_account(f'alice{i}')


def test_broadcast_same_transaction_twice(node, wallet):
    transaction = wallet.api.create_account('initminer', 'alice', '{}', broadcast=False)
    node.api.network_broadcast.broadcast_transaction(trx=transaction)
    node.wait_number_of_blocks(22)

    assert transaction != node.api.account_history.get_transaction(id=transaction['transaction_id'], include_reversible=True)

    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.network_broadcast.broadcast_transaction(trx=transaction)


@pytest.mark.parametrize(
    "way_of_broadcasting",
    [
        "node.api.condenser.broadcast_transaction(transaction)",
        "node.api.network_broadcast.broadcast_transaction(trx=transaction)",
        "node.api.wallet_bridge.broadcast_transaction(transaction)",
    ]
)
def test_broadcasting_manually_signed_transaction(node, wallet, way_of_broadcasting):
    transaction = wallet.api.create_account('initminer', 'alice', '{}', broadcast=False)
    transaction = wallet.api.sign_transaction(transaction, broadcast=False)

    eval(way_of_broadcasting)
    assert 'alice' in wallet.list_accounts()
