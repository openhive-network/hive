import pytest
import datetime

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
    # ARRANGE
    wallet = tt.Wallet(attach_to=node)
    wallet.api.create_account("initminer", "alice", "{}")

    # ACT
    wallet.api.transfer("initminer", "alice", tt.Asset.Test(100), "memo")

    # ASSERT
    response = node.api.condenser.get_accounts(["alice"])
    assert tt.Asset.Test(100) == response[0]["balance"]





@run_for("testnet")
def test_transfer_to_saving_operation(node):
    # ARRANGE
    wallet = tt.Wallet(attach_to=node)
    wallet.api.create_account("initminer", "alice", "{}")

    # ACT
    wallet.api.transfer_to_savings("initminer", "alice", tt.Asset.Test(10), "lemon")

    # ASSERT
    assert node.api.condenser.get_accounts(["alice"])[0]["savings_balance"] == tt.Asset.Test(10)


@run_for("testnet")
def test_transfer_from_saving_operation(node):
    # ARRANGE
    wallet = tt.Wallet(attach_to=node)
    wallet.api.create_account("initminer", "alice", "{}")

    # ACT
    wallet.api.transfer_to_savings("initminer", "alice", tt.Asset.Test(10), "lemon")
    wallet.api.transfer_from_savings("alice", 1000, "alice", tt.Asset.Test(10), "lemon")

    # ASSERT
    assert node.api.condenser.get_accounts(["alice"])[0]["savings_withdraw_requests"] == 1


@run_for("testnet")
def test_cancel_transfer_from_saving_operation(node):
    # ARRANGE
    wallet = tt.Wallet(attach_to=node)
    wallet.api.create_account("initminer", "alice", "{}")

    # ACT
    wallet.api.transfer_to_vesting('initminer', "alice", tt.Asset.Test(1))
    wallet.api.transfer_to_savings("initminer", "alice", tt.Asset.Test(10), "lemon")
    wallet.api.transfer_from_savings("alice", 1000, "alice", tt.Asset.Test(10), "lemon")
    wallet.api.cancel_transfer_from_savings("alice", 1000)

    # ASSERT
    assert node.api.condenser.get_accounts(["alice"])[0]["savings_withdraw_requests"] == 0


@run_for("testnet")
def test_convert_hbd_to_hive_operation(node):
    # ARRANGE
    wallet = tt.Wallet(attach_to=node)
    wallet.api.create_account("initminer", "alice", "{}")

    # ACT
    wallet.api.transfer("initminer", "alice", tt.Asset.Tbd(100), "memo")
    wallet.api.convert_hbd("alice", tt.Asset.Tbd(50))

    # ASSERT
    assert node.api.condenser.get_accounts(["alice"])[0]["hbd_balance"] == tt.Asset.Tbd(50)


@run_for("testnet")
def test_convert_hive_to_hbd_operation(node):
    # ARRANGE
    wallet = tt.Wallet(attach_to=node)
    wallet.api.create_account("initminer", "alice", "{}")

    # ACT
    wallet.api.transfer("initminer", "alice", tt.Asset.Test(100), "memo")
    wallet.api.convert_hive_with_collateral("alice", tt.Asset.Test(50))

    # ASSERT
    assert all([node.api.condenser.get_accounts(["alice"])[0]["balance"] == tt.Asset.Test(50),
                node.api.condenser.get_accounts(["alice"])[0]["hbd_balance"] != tt.Asset.Tbd(0)])


@run_for("testnet")
def test_recurrent_transfer_operation(node):
    # ARRANGE
    wallet = tt.Wallet(attach_to=node)
    wallet.api.create_account("initminer", "alice", "{}")
    wallet.api.create_account("initminer", "bob", "{}")

    # ACT
    wallet.api.transfer("initminer", "alice", tt.Asset.Test(100), "memo")
    wallet.api.transfer_to_vesting('initminer', "alice", tt.Asset.Test(100))
    wallet.api.recurrent_transfer("alice", "bob", tt.Asset.Test(10), "{}", 24, 10)

    # ASSERT
    response = node.api.condenser.find_recurrent_transfers('alice')
    assert all([response[0]["from"] == "alice",
                response[0]["to"] == "bob",
                response[0]["amount"] == tt.Asset.Test(10),
                response[0]["recurrence"] == 24
                ])
