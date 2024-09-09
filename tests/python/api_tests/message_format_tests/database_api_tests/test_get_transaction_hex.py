from __future__ import annotations

import test_tools as tt
from hive_local_tools import run_for


@run_for("testnet")
def test_get_transaction_hex_in_testnet(node: tt.InitNode) -> None:
    wallet = tt.Wallet(attach_to=node)
    transaction = wallet.api.create_account("initminer", "alice", "{}")
    output_hex = node.api.database.get_transaction_hex(trx=transaction).hex_
    assert len(output_hex) != 0


@run_for("mainnet_5m", "live_mainnet")
def test_get_transaction_hex_in_mainnet(node: tt.RemoteNode) -> None:
    block = node.api.wallet_bridge.get_block(4450001).block
    transaction = block.transactions[0]
    output_hex = node.api.database.get_transaction_hex(trx=transaction).hex_
    assert len(output_hex) != 0
