from __future__ import annotations

import pytest

import test_tools as tt
from hive_local_tools import run_for


# This test cannot be performed on 5 million blocklog because it doesn't contain any recurrent transfers - they were
# introduced in HF25, after block with number 5000000. In this case testing live_mainnet blocklog is problematic
# too. There is no listing method for recurrent_transfers, and they could run out of executions after some time.
# See the readme.md file in this directory for further explanation.
@run_for("testnet")
@pytest.mark.parametrize("asset", [tt.Asset.Test(5), tt.Asset.Tbd(5)])
def test_find_recurrent_transfers(node: tt.InitNode, asset: tt.Asset) -> None:
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account("alice", hives=tt.Asset.Test(100), hbds=tt.Asset.Tbd(100), vests=tt.Asset.Test(100))
    wallet.api.create_account("initminer", "bob", "{}")
    # create transfer from alice to bob for amount 5 test hives every 720 hours, repeat 12 times
    wallet.api.recurrent_transfer("alice", "bob", asset, "memo", 720, 12)
    # "from" is a Python keyword and needs workaround
    recurrent_transfers = node.api.database.find_recurrent_transfers(**{"from": "alice"}).recurrent_transfers
    assert len(recurrent_transfers) != 0
