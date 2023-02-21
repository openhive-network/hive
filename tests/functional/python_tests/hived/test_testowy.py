import pytest

import test_tools as tt
from hive_local_tools import run_for


@pytest.fixture
def node():
    node = tt.InitNode()
    node.run()
    return node


@run_for("testnet")
def test_transfer_between_initminer_and_user(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.api.create_account("initminer", "alice", "{}")
    wallet.api.transfer("initminer", "alice", tt.Asset.Test(100), "")

    assert node.api.condenser.get_accounts(["alice"])[0]["balance"] == tt.Asset.Test(100)


@run_for("testnet")
def test_count_created_few_accounts(node):
    wallet = tt.Wallet(attach_to=node)
    accounts_before = node.api.condenser.get_account_count() - 6
    wallet.create_accounts(100)
    accounts_after = node.api.condenser.get_account_count() - 6

    assert accounts_after == accounts_before + 100


@run_for("testnet")
def test_create_comment_by_user(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.api.create_account("initminer", "alice", "{}")
    count_before_comment = (node.api.condenser.get_accounts(["alice"])[0]["post_count"],)
    wallet.api.post_comment("alice", "test-permlink", "", "test-parent-permlink", "First title", "First body", "{}")
    count_after_comment = (node.api.condenser.get_accounts(["alice"])[0]["post_count"],)

    assert count_after_comment[0] - count_before_comment[0] == 1


@pytest.mark.parametrize(
    "accounts", [
        100,
        1000,
        10000,
    ]
)
@run_for("testnet")
def test_multiple_creation_of_accounts(node, accounts):
    wallet = tt.Wallet(attach_to=node)
    accounts_before = (node.api.condenser.get_account_count() - 6,)
    wallet.create_accounts(accounts)
    accounts_after = (node.api.condenser.get_account_count() - 6,)

    assert not(accounts_after[0] == accounts_before[0])
    assert accounts_after[0] - accounts == 0


