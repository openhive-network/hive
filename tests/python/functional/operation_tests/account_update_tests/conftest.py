from __future__ import annotations

import pytest

import test_tools as tt
from hive_local_tools.functional.python.operation import create_account_with_different_keys
from python.functional.operation_tests.conftest import UpdateAccount


@pytest.fixture()
def alice(prepared_node: tt.InitNode, wallet: tt.Wallet) -> UpdateAccount:
    # slow down node - speeding up time caused random fails (it's not possible to do "+0h x1")
    prepared_node.restart(time_control=tt.OffsetTimeControl(offset="+1h"))
    # wallet.create_account creates account with 4 the same keys which is not wanted in this kind of tests
    create_account_with_different_keys(wallet, "alice", "initminer")
    wallet.api.transfer_to_vesting("initminer", "alice", tt.Asset.Test(50), broadcast=True)
    return UpdateAccount("alice", prepared_node, wallet)
