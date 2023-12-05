from __future__ import annotations

from typing import TYPE_CHECKING

from hive_local_tools import run_for
from hive_local_tools.functional.python.datagen.api.transaction_status_api import assert_transaction_status

if TYPE_CHECKING:
    import test_tools as tt


@run_for("testnet", enable_plugins=["transaction_status_api"])
def test_transaction_status(node: tt.ApiNode, wallet: tt.Wallet) -> None:
    irreversible_transaction = wallet.api.create_account("initminer", "alice", "{}")
    node.wait_for_irreversible_block()
    reversible_transaction = wallet.api.create_account("initminer", "bob", "{}")

    assert_transaction_status(node, irreversible_transaction["transaction_id"], "within_irreversible_block")
    assert_transaction_status(node, reversible_transaction["transaction_id"], "within_reversible_block")

    node.restart()

    assert_transaction_status(node, irreversible_transaction["transaction_id"], "within_irreversible_block")
    assert_transaction_status(node, reversible_transaction["transaction_id"], "unknown")
