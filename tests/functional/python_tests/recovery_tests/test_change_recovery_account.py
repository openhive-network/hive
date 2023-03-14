import pytest

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.constants import OWNER_AUTH_RECOVERY_PERIOD
from hive_local_tools.functional.python.recovery import get_recovery_agent


@run_for("testnet")
def test_default_recovery_agent(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account("alice")

    assert get_recovery_agent(node, account_name="alice") == "initminer"


@run_for("testnet")
def test_change_recovery_agent(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account("alice")
    wallet.create_account("bob")

    block_num = wallet.api.change_recovery_account("alice", "bob")["block_num"]
    node.wait_for_block_with_number(block_num + OWNER_AUTH_RECOVERY_PERIOD)

    assert get_recovery_agent(node, account_name="alice") == "bob"


@run_for("testnet")
def test_resign_from_the_current_recovery_agent(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account("alice")

    with pytest.raises(tt.exceptions.CommunicationError):
        wallet.api.change_recovery_account("alice", "")


@run_for("testnet")
def test_withdrawal_of_the_request_for_a_change_of_recovery_agent(node):
    wallet = tt.Wallet(node)
    wallet.create_account("alice", vests=tt.Asset.Test(100))
    wallet.create_account("bob")

    wallet.api.change_recovery_account("alice", "bob")
    assert len(node.api.database.find_change_recovery_account_requests(accounts=["alice"])["requests"]) == 1

    # to cancel change_recovery_agent_request set previous recovery agent
    wallet.api.change_recovery_account("alice", "initminer")
    assert len(node.api.database.find_change_recovery_account_requests(accounts=["alice"])["requests"]) == 0
    assert get_recovery_agent(node, account_name="alice") == "initminer"


@run_for("testnet")
def test_set_own_account_as_recovery_agent(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account("alice")

    wallet.api.change_recovery_account("alice", "alice")
    node.wait_number_of_blocks(OWNER_AUTH_RECOVERY_PERIOD)

    assert get_recovery_agent(node, account_name="alice") == "alice"


@run_for("testnet")
def test_set_null_account_as_recovery_agent(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account("alice")

    wallet.api.change_recovery_account("alice", "null")
    node.wait_number_of_blocks(OWNER_AUTH_RECOVERY_PERIOD)

    assert get_recovery_agent(node, account_name="alice") == "null"


@run_for("testnet")
def test_set_temp_account_as_recovery_agent(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account("alice")

    wallet.api.change_recovery_account("alice", "temp")
    node.wait_number_of_blocks(OWNER_AUTH_RECOVERY_PERIOD)

    assert get_recovery_agent(node, account_name="alice") == "temp"


@run_for("testnet")
def test_change_recovery_agent_to_non_existing_account(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account("alice")

    with pytest.raises(tt.exceptions.CommunicationError):
        wallet.api.change_recovery_account("alice", "non-existing-acc")


@pytest.mark.skip(reason="https://gitlab.syncad.com/hive/hive/-/issues/466")
@run_for("testnet")
def test_change_recovery_agent_to_the_same_recovery_agent(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account("alice")

    assert get_recovery_agent(node, account_name="alice") == "initminer"

    with pytest.raises(tt.exceptions.CommunicationError):
        wallet.api.change_recovery_account("alice", "initminer")


@run_for("testnet")
def test_that_there_is_only_one_change_recovery_agent_request(node):
    wallet = tt.Wallet(node)
    wallet.create_account("alice", vests=tt.Asset.Test(100))

    wallet.create_accounts(3)

    wallet.api.change_recovery_account("alice", "account-0")
    wallet.api.change_recovery_account("alice", "account-1")
    wallet.api.change_recovery_account("alice", "account-2")
    assert len(node.api.database.find_change_recovery_account_requests(accounts=["alice"])["requests"]) == 1

    node.wait_number_of_blocks(OWNER_AUTH_RECOVERY_PERIOD)
    assert get_recovery_agent(node, account_name="alice") == "account-2"
    assert len(node.api.database.find_change_recovery_account_requests(accounts=["alice"])["requests"]) == 0


@pytest.mark.skip(reason="https://gitlab.syncad.com/hive/hive/-/issues/466")
@run_for("testnet")
def test_change_recovery_agent_twice_while_the_request_is_in_progress(node):
    wallet = tt.Wallet(node)
    wallet.create_account("alice", vests=tt.Asset.Test(100))
    wallet.create_account("bob")

    wallet.api.change_recovery_account("alice", "bob")
    first_request = node.api.database.find_change_recovery_account_requests(accounts=["alice"])["requests"]

    node.wait_number_of_blocks(OWNER_AUTH_RECOVERY_PERIOD//2)

    wallet.api.change_recovery_account("alice", "bob")
    second_request = node.api.database.find_change_recovery_account_requests(accounts=["alice"])["requests"]

    assert len(node.api.database.find_change_recovery_account_requests(accounts=["alice"])["requests"]) == 1
    assert first_request == second_request


@pytest.mark.parametrize("authority", ["active", "posting"])
@run_for("testnet")
def test_change_recovery_agent_with_too_low_threshold_authority(node, authority):
    wallet = tt.Wallet(attach_to=node)
    wallet.api.create_account("initminer", "alice", "{}")
    wallet.api.transfer_to_vesting("initminer", "alice", tt.Asset.Test(100))
    wallet.create_account("bob")

    wallet.api.use_authority(authority, "alice")

    with pytest.raises(tt.exceptions.CommunicationError):
        wallet.api.change_recovery_account("alice", "bob")
