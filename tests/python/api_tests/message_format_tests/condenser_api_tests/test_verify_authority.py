from __future__ import annotations

from typing import TYPE_CHECKING

from hive_local_tools import run_for

if TYPE_CHECKING:
    import test_tools as tt


@run_for("testnet")
def test_verify_authority_in_testnet(node, wallet):
    transaction = wallet.api.create_account("initminer", "alice", "{}")
    node.api.condenser.verify_authority(transaction)


@run_for("mainnet_5m")
def test_verify_authority_in_mainnet_5m(node: tt.RemoteNode):
    block = node.api.condenser.get_block(4800119)
    transaction = block["transactions"][1]
    node.api.condenser.verify_authority(transaction)


@run_for("live_mainnet")
def test_verify_authority_in_live_mainnet(node):
    block = node.api.condenser.get_block(48000034)
    transaction = block["transactions"][17]
    node.api.condenser.verify_authority(transaction)
