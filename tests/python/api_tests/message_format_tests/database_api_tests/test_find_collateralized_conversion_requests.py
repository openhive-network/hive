from __future__ import annotations

import test_tools as tt
from hive_local_tools import run_for


# This test is not performed on 5 million block log because of lack of accounts with collateralized conversion requests.
# remote node update. See the readme.md file in this directory for further explanation.
@run_for("testnet")
def test_find_collateralized_conversion_requests_in_testnet(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account("alice", hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    # convert_hive_with_collateral changes hives for hbd, this process takes three and half days
    wallet.api.convert_hive_with_collateral("alice", tt.Asset.Test(4))
    requests = node.api.database.find_collateralized_conversion_requests(account="alice")["requests"]
    assert len(requests) != 0


@run_for("live_mainnet")
def test_find_collateralized_conversion_requests_in_live_mainnet(node):
    account = node.api.database.list_collateralized_conversion_requests(start=[""], limit=100, order="by_account")[
        "requests"
    ][0]["owner"]
    requests = node.api.database.find_collateralized_conversion_requests(account=account)["requests"]
    assert len(requests) != 0
