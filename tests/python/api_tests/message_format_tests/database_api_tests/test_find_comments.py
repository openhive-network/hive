from __future__ import annotations

import test_tools as tt
from hive_local_tools import run_for

import pytest
from beekeepy.exceptions import ErrorInResponseError

@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_find_comments(node: tt.InitNode | tt.RemoteNode, should_prepare: bool) -> None:
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.create_account("herverisson", hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
        wallet.api.post_comment(
            "herverisson", "homefront-a-one-act-play-last-part", "", "someone", "test-title", "this is a body", "{}"
        )

    with pytest.raises(ErrorInResponseError) as exception:
        node.api.database.find_comments(
        comments=[["herverisson", "homefront-a-one-act-play-last-part"]])

    expected_error_message = "Supported by Hivemind"
    assert expected_error_message in exception.value.error