from __future__ import annotations

from typing import TYPE_CHECKING

import pytest
from helpy.exceptions import ErrorInResponseError

if TYPE_CHECKING:
    import test_tools as tt


def test_sending_transaction_with_duplicated_signature(node: tt.InitNode | tt.RemoteNode, wallet: tt.Wallet):
    transaction = wallet.api.create_account("initminer", "alice", "{}", broadcast=False)
    transaction["signatures"] = 2 * transaction["signatures"]  # Duplicate signature

    with pytest.raises(ErrorInResponseError) as error:
        node.api.wallet_bridge.broadcast_transaction(transaction)

    response = error.value.error
    assert "duplicate signature included:" in response
    assert "Duplicate signature detected" in response
