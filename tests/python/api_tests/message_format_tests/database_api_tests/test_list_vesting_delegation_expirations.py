from __future__ import annotations

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.api.message_format import create_and_cancel_vesting_delegation


# This test cannot be performed on 5 million blocklog because it doesn't contain any vesting delegation expirations.
# See the readme.md file in this directory for further explanation.
@run_for("testnet", "live_mainnet")
def test_list_vesting_delegation_expirations(node: tt.InitNode | tt.RemoteNode, should_prepare: bool) -> None:
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.create_account("alice", hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
        wallet.api.create_account("alice", "bob", "{}")
        create_and_cancel_vesting_delegation(wallet, "alice", "bob")
    delegations = node.api.database.list_vesting_delegation_expirations(
        start=["", tt.Time.from_now(weeks=-100), 0], limit=100, order="by_account_expiration"
    ).delegations
    assert len(delegations) != 0
