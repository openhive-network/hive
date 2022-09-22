import pytest

import test_tools as tt


def test_sending_transaction_with_duplicated_signature(node, wallet):
    transaction = wallet.api.create_account('initminer', 'alice', '{}', broadcast=False)
    transaction['signatures'] = 2 * transaction['signatures']  # Duplicate signature

    with pytest.raises(tt.exceptions.CommunicationError) as error:
        node.api.wallet_bridge.broadcast_transaction(transaction)

    response = error.value.response
    assert 'duplicate signature included:Duplicate Signature detected' == response['error']['message']
