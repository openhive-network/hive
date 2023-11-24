from __future__ import annotations

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.api.message_format import create_and_cancel_vesting_delegation


@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_expiring_vesting_delegations(node: tt.InitNode | tt.RemoteNode, should_prepare: bool) -> None:
    preparation_for_testnet_node(node, should_prepare)
    node.api.condenser.get_expiring_vesting_delegations("alice", tt.Time.now(), 50)


@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_expiring_vesting_delegations_with_default_third_argument(
    node: tt.InitNode | tt.RemoteNode, should_prepare: bool
) -> None:
    preparation_for_testnet_node(node, should_prepare)
    node.api.condenser.get_expiring_vesting_delegations("alice", tt.Time.now())


def preparation_for_testnet_node(node: tt.InitNode | tt.RemoteNode, should_prepare: bool) -> None:
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.create_account("alice", hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
        wallet.api.create_account("initminer", "bob", "{}")
        create_and_cancel_vesting_delegation(wallet, "alice", "bob")
