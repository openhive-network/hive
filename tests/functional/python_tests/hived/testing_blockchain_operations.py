import pytest

import test_tools as tt
from hive_local_tools import run_for


@pytest.fixture
def node():
    node = tt.InitNode()
    node.run()
    return node


@run_for("testnet")
def test_account_created(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.api.create_account("initminer", "alice", "{}")
    wallet.api.create_account("alice", "bob", "{}")
    response = node.api.database.list_accounts(start="", limit=10, order="by_name")
    names = set()
    for name in response["accounts"]:
        names.add(name["name"])

    assert "alice" and "bob" in names


@run_for("testnet")
def test_transfer_operation(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.api.create_account("initminer", "alice", "{}")
    wallet.api.transfer("initminer", "alice", tt.Asset.Test(100), "memo")
    wallet.api.create_account("initminer", "bob", "{}")
    wallet.api.m
    response = node.api.condenser.get_accounts(["alice"])

    assert tt.Asset.Test(50) == response[0]["balance"]
