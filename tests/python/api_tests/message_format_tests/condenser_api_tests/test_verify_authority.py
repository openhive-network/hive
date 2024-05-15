from __future__ import annotations

import test_tools as tt
from hive_local_tools import run_for


@run_for("testnet")
def test_verify_authority_in_testnet(node: tt.InitNode) -> None:
    wallet = tt.OldWallet(attach_to=node)
    transaction = wallet.api.create_account("initminer", "alice", "{}")
    node.api.condenser.verify_authority(transaction)


@run_for("mainnet_5m")
def test_verify_authority_in_mainnet_5m(node: tt.RemoteNode) -> None:
    block = node.api.condenser.get_block(4800119)
    transaction = block.transactions[1]
    node.api.condenser.verify_authority(transaction)


@run_for("live_mainnet")
def test_verify_authority_in_live_mainnet(node: tt.RemoteNode) -> None:
    block = node.api.condenser.get_block(48000034)
    transaction = block.transactions[17]
    node.api.condenser.verify_authority(transaction)
