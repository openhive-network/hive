from __future__ import annotations

import test_tools as tt
from hive_local_tools import run_for


@run_for("testnet", enable_plugins=["account_history_api"])
def test_transaction_timestamp_consistency_before_and_after_irreversibility(node):
    wallet = tt.Wallet(attach_to=node)

    trx = wallet.api.create_account("initminer", "alice", "{}")

    reversible_ah_trx = node.api.account_history_api.get_ops_in_block(block_num=trx.block_num, include_reversible=True)

    node.wait_for_block_with_number(30)  # wait for activate obi
    assert node.api.database.get_dynamic_global_properties().last_irreversible_block_num >= 2

    irreversible_ah_trx = node.api.account_history_api.get_ops_in_block(
        block_num=trx.block_num, include_reversible=False
    )

    assert reversible_ah_trx.ops[0].timestamp == irreversible_ah_trx.ops[0].timestamp, "Timestamps isn't the same"
