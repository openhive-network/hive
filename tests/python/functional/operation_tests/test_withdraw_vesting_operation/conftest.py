from __future__ import annotations

import pytest

import test_tools as tt
from hive_local_tools.functional.python.operation.withdrawe_vesting import PowerDownAccount


@pytest.fixture()
def alice(prepared_node, wallet):
    wallet.create_account("alice")
    alice = PowerDownAccount("alice", prepared_node, wallet)
    alice.fund_vests(tt.Asset.Test(13_000))
    alice.update_account_info()
    return alice
