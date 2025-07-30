from __future__ import annotations

from typing import Final

import test_tools as tt
from hive_local_tools import run_for


@run_for("testnet")
def test_change_vesting_price(node: tt.InitNode | tt.RemoteNode) -> None:
    vest_per_hive_ratio: Final[int] = 100
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account(name="alice", hives=tt.Asset.Test(1))

    report_vest_price(node, "before price updating")
    node.set_vest_price(quote=tt.Asset.Vest(vest_per_hive_ratio), base=tt.Asset.Test(1), invest=tt.Asset.Test(0))
    report_vest_price(node, "after price updating")

    wallet.api.transfer_to_vesting("alice", "alice", tt.Asset.Test(1))

    assert (
        node.api.condenser.get_accounts(["alice"])[0].vesting_shares == tt.Asset.Vest(vest_per_hive_ratio).as_legacy()
    )


def report_vest_price(node: tt.InitNode | tt.RemoteNode, msg: str) -> None:
    dgpo = node.api.database.get_dynamic_global_properties()

    tt.logger.info(f"{msg} vests price (from dgpo): {dgpo['total_vesting_shares']}/{dgpo['total_vesting_fund_hive']}.")
