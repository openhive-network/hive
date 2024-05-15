from __future__ import annotations

import test_tools as tt
from hive_local_tools import run_for


@run_for("testnet")
def test_get_potential_signatures_in_testnet(node: tt.InitNode) -> None:
    wallet = tt.Wallet(attach_to=node)
    transaction = wallet.api.create_account("initminer", "alice", "{}")
    keys = node.api.database.get_potential_signatures(trx=transaction).keys
    assert len(keys) != 0


@run_for("mainnet_5m", "live_mainnet")
def test_get_potential_signatures_in_mainnet(node: tt.RemoteNode) -> None:
    block = node.api.wallet_bridge.get_block(4450001).block
    transaction = block.transactions[0]
    keys = node.api.database.get_potential_signatures(trx=transaction).keys
    assert len(keys) != 0
