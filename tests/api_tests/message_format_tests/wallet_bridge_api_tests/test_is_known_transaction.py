import pytest

import test_tools as tt

from .local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_is_know_transaction_with_correct_value_and_existing_transaction(prepared_node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        wallet.api.create_account('initminer', 'alice', '{}')
    transaction_id = prepared_node.api.account_history.get_account_history(
        account='alice',
        start=-1,
        limit=1,
        include_reversible=True,
    )['history'][0][1]['trx_id']

    prepared_node.api.wallet_bridge.is_known_transaction(transaction_id)


@pytest.mark.parametrize(
    'transaction_id', [
        # transaction id should be HEX
        '0000000000000000000000000000000000000000',
        '',
        '0',
        'adfafa',
        '100',
        100,
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_is_know_transaction_with_correct_value_and_non_existing_transaction(prepared_node, transaction_id):
    prepared_node.api.wallet_bridge.is_known_transaction(transaction_id)


@pytest.mark.parametrize(
    'transaction_id', [
        # transaction id should be HEX
        ['example-array'],
        'non-hex-string-argument',
        True,
        'true',
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_is_know_transaction_with_incorrect_type_of_argument(prepared_node, transaction_id):
    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.is_known_transaction(transaction_id)
