from __future__ import annotations

import pytest
from helpy.exceptions import ErrorInResponseError

import test_tools as tt
from hive_local_tools import run_for


@pytest.mark.parametrize(
    "include_reversible",
    [
        True,
        False,
    ],
)
@run_for("testnet", enable_plugins=["account_history_api"])
def test_get_transaction_in_reversible_block(node: tt.InitNode, include_reversible: bool) -> None:
    wallet = tt.Wallet(attach_to=node)
    transaction = wallet.create_account("alice")
    if not include_reversible:
        node.wait_for_irreversible_block()
    # delete one additional key to compare transactions
    del transaction.rc_cost
    response = node.api.account_history.get_transaction(
        id=transaction["transaction_id"], include_reversible=include_reversible
    )
    assert transaction == response


@pytest.mark.parametrize(
    "incorrect_id",
    [
        # too short hex, correct hex but unknown transaction, too long hex
        "123",
        "1000000000000000000000000000000000000000",
        "10000000000000000000000000000000000000001",
    ],
)
@pytest.mark.parametrize(
    "include_reversible",
    [
        False,
        True,
    ],
)
@run_for("testnet", enable_plugins=["account_history_api"])
def test_wrong_transaction_id(node: tt.InitNode, incorrect_id: str, include_reversible: bool) -> None:
    with pytest.raises(ErrorInResponseError):
        node.api.account_history.get_transaction(id=incorrect_id, include_reversible=include_reversible)
