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
    wallet = tt.Wallet(attach_to=node)
    wallet.api.create_account("initminer", "alice", "{}")
    wallet.api.transfer("initminer", "alice", tt.Asset.Test(100), "memo")
    response = node.api.condenser.get_accounts(["alice"])

    assert tt.Asset.Test(100) == response[0]["balance"]


@run_for("testnet")
def test_transfer_to_vests_and_withdraw_vesting(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.api.create_account("initminer", "alice", "{}")
    wallet.api.transfer_to_vesting('initminer', "alice", tt.Asset.Test(100))

    vests_balance = node.api.condenser.get_accounts(["alice"])[0]["vesting_shares"]
    wallet.api.withdraw_vesting("alice", tt.Asset.Vest(16))
    vests_to_withdraw = node.api.condenser.get_accounts(["alice"])[0]["vesting_withdraw_rate"].split()
    vests_to_withdraw = float(vests_to_withdraw[0]) * 13
    vests_to_withdraw = tt.Asset.Vest(int(vests_to_withdraw))

    today_date = datetime.date.today()
    next_withdrawal_date = str(today_date + datetime.timedelta(days=7))

    assert vests_balance != tt.Asset.Vest(0)
    assert vests_to_withdraw == tt.Asset.Vest(16)
    assert node.api.condenser.get_accounts(["alice"])[0]["next_vesting_withdrawal"][:10] == next_withdrawal_date


@run_for("testnet")
def test_transfer_to_saving_operation(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.api.create_account("initminer", "alice", "{}")
    wallet.api.transfer_to_savings("initminer", "alice", tt.Asset.Test(10), "lemon")

    assert node.api.condenser.get_accounts(["alice"])[0]["savings_balance"] == tt.Asset.Test(10)


@run_for("testnet")
def test_transfer_from_saving_operation(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.api.create_account("initminer", "alice", "{}")
    wallet.api.transfer_to_savings("initminer", "alice", tt.Asset.Test(10), "lemon")
    wallet.api.transfer_from_savings("alice", 1000, "alice", tt.Asset.Test(10), "lemon")

    assert node.api.condenser.get_accounts(["alice"])[0]["savings_withdraw_requests"] == 1


@run_for("testnet")
def test_cancel_transfer_from_saving_operation(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.api.create_account("initminer", "alice", "{}")
    wallet.api.transfer_to_vesting('initminer', "alice", tt.Asset.Test(1))
    wallet.api.transfer_to_savings("initminer", "alice", tt.Asset.Test(10), "lemon")
    wallet.api.transfer_from_savings("alice", 1000, "alice", tt.Asset.Test(10), "lemon")
    wallet.api.cancel_transfer_from_savings("alice", 1000)

    assert node.api.condenser.get_accounts(["alice"])[0]["savings_withdraw_requests"] == 0


@run_for("testnet")
def test_convert_hbd_to_hive_operation(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.api.create_account("initminer", "alice", "{}")
    wallet.api.transfer("initminer", "alice", tt.Asset.Tbd(100), "memo")
    wallet.api.convert_hbd("alice", tt.Asset.Tbd(50))

    assert node.api.condenser.get_accounts(["alice"])[0]["hbd_balance"] == tt.Asset.Tbd(50)


@run_for("testnet")
def test_convert_hbd_to_hive_operation(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.api.create_account("initminer", "alice", "{}")
    wallet.api.transfer("initminer", "alice", tt.Asset.Test(100), "memo")
    wallet.api.convert_hive_with_collateral("alice", tt.Asset.Test(50))

    assert all([node.api.condenser.get_accounts(["alice"])[0]["balance"] == tt.Asset.Test(50),
                node.api.condenser.get_accounts(["alice"])[0]["hbd_balance"] != tt.Asset.Tbd(0)])


@run_for("testnet")
def test_recurrent_transfer_operation(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.api.create_account("initminer", "alice", "{}")
    wallet.api.create_account("initminer", "bob", "{}")
    wallet.api.transfer("initminer", "alice", tt.Asset.Test(100), "memo")
    wallet.api.transfer_to_vesting('initminer', "alice", tt.Asset.Test(100))
    response = node.api.condenser.find_recurrent_transfers('alice')
    print()