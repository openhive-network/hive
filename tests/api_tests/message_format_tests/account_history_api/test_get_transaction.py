import pytest

import test_tools as tt

from ..local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_transaction_with_correct_values(prepared_node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        wallet.api.create_account('initminer', 'gtg', '{}')
    account_history = prepared_node.api.account_history.get_account_history(account='gtg', start=1, limit=1, include_reversible=True)
    transaction_id = account_history['history'][0][1]['trx_id']
    prepared_node.api.account_history.get_transaction(id=transaction_id, include_reversible=True)


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_transaction_with_incorrect_values(prepared_node):
    wrong_transaction_id = -1
    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.account_history.get_transaction(id=wrong_transaction_id, include_reversible=True)
