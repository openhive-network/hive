from __future__ import annotations

import pytest

import test_tools as tt
from hive_local_tools.functional.python.operation import create_account_with_different_keys
from python.functional.operation_tests.conftest import Account


@pytest.fixture()
def create_alice_and_bob(prepared_node: tt.InitNode, wallet: tt.Wallet) -> tuple(Account, Account):
    for name in ("alice", "bob"):
        create_account_with_different_keys(wallet, name, "initminer")
        wallet.api.transfer_to_vesting("initminer", name, tt.Asset.Test(50), broadcast=True)
    return (
        Account("alice", prepared_node, wallet),
        Account("bob", prepared_node, wallet),
    )
