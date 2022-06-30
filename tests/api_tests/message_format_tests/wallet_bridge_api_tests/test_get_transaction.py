import pytest

import test_tools as tt

from .local_tools import run_for


@run_for('testnet')
def test_get_transaction_with_correct_value_in_testnet(prepared_node):
    wallet = tt.Wallet(attach_to=prepared_node)
    transaction_id = wallet.api.create_account('initminer', 'alice', '{}')['transaction_id']
    prepared_node.wait_number_of_blocks(21)  # waiting 21 blocks for the block with the transaction to become irreversible
    prepared_node.api.wallet_bridge.get_transaction(transaction_id)


@run_for('mainnet_5m', 'mainnet_64m')
def test_get_transaction_with_correct_value_in_mainnet(prepared_node):
    # Transaction id of the 5 millionth block
    transaction_id = prepared_node.api.wallet_bridge.get_block(5 * 1000**2)['block']['transaction_ids'][0]
    prepared_node.api.wallet_bridge.get_transaction(transaction_id)


@pytest.mark.parametrize(
    'transaction_id', [
        '0000000000000000000000000000000000000000',  # non exist transaction
        'incorrect_string_argument',
        '100',
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_transaction_with_incorrect_value(prepared_node, transaction_id):
    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.get_transaction(transaction_id)


@pytest.mark.parametrize(
    'transaction_id', [
        ['example-array'],
        100,
        True,
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_transaction_with_incorrect_type_of_argument(prepared_node, transaction_id):
    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.get_transaction(transaction_id)


@run_for('testnet')
def test_get_transaction_with_additional_argument_in_testnet(prepared_node):
    wallet = tt.Wallet(attach_to=prepared_node)
    transaction_id = wallet.api.create_account('initminer', 'alice', '{}')['transaction_id']
    prepared_node.wait_number_of_blocks(21)   # waiting 21 blocks for the block with the transaction to become irreversible
    prepared_node.api.wallet_bridge.get_transaction(transaction_id, 'additional_argument')


@run_for('mainnet_5m', 'mainnet_64m')
def test_get_transaction_with_additional_argument_in_mainnet(prepared_node):
    # Transaction id of the 5 millionth block
    transaction_id = prepared_node.api.wallet_bridge.get_block(5 * 1000**2)['block']['transaction_ids'][0]
    prepared_node.api.wallet_bridge.get_transaction(transaction_id, 'additional_argument')
