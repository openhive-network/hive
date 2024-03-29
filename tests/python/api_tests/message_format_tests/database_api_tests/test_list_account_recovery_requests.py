from __future__ import annotations

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.api.message_format import request_account_recovery


# This test is not performed on 5 million block log because of lack of accounts with recovery requests within it. In
# case of most recent blocklog (live_mainnet) there is a lot of recovery requests, but they are changed after every
# remote node update. See the readme.md file in this directory for further explanation.
@run_for("testnet")
def test_list_account_recovery_requests(node: tt.InitNode) -> None:
    wallet = tt.Wallet(attach_to=node)
    wallet.api.create_account("initminer", "alice", "{}")
    request_account_recovery(wallet, "alice")
    requests = node.api.database.list_account_recovery_requests(start="", limit=100, order="by_account").requests
    assert len(requests) != 0
