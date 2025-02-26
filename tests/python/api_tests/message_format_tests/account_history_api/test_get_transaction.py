from __future__ import annotations

import pytest
from beekeepy.exceptions import ErrorInResponseError

import test_tools as tt
from hive_local_tools import run_for


@run_for("testnet", "mainnet_5m", "live_mainnet", enable_plugins=["account_history_api"])
def test_get_transaction_with_correct_values(node: tt.InitNode | tt.RemoteNode, should_prepare: bool) -> None:
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.api.create_account("initminer", "gtg", "{}")
    account_history = node.api.account_history.get_account_history(
        account="gtg", start=1, limit=1, include_reversible=True
    )
    transaction_id = account_history.history[0][1].trx_id
    node.api.account_history.get_transaction(id=transaction_id, include_reversible=True)


@run_for("testnet", "mainnet_5m", "live_mainnet", enable_plugins=["account_history_api"])
def test_get_transaction_with_incorrect_values(node: tt.InitNode | tt.RemoteNode) -> None:
    wrong_transaction_id = -1
    with pytest.raises(ErrorInResponseError):
        node.api.account_history.get_transaction(id=wrong_transaction_id, include_reversible=True)
