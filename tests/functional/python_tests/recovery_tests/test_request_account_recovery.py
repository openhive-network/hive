import pytest

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.constants import ACCOUNT_RECOVERY_REQUEST_EXPIRATION_PERIOD, OWNER_AUTH_RECOVERY_PERIOD, OWNER_UPDATE_LIMIT
from hive_local_tools.functional.python.recovery import get_authority, get_owner_key, get_recovery_agent


@run_for("testnet")
def test_request_account_recovery(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account("alice")

    new_key = tt.Account("alice", secret="new_key").public_key
    new_authority = get_authority(new_key)
    wallet.api.request_account_recovery("initminer", "alice", new_authority)

    assert len(node.api.database.find_account_recovery_requests(accounts=["alice"])["requests"]) == 1


@run_for("testnet")
def test_account_recovery_request_expiration(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account("alice")

    recovery_account_key = tt.Account("initminer").public_key
    authority = get_authority(recovery_account_key)
    wallet.api.request_account_recovery("initminer", "alice", authority)

    node.wait_number_of_blocks(ACCOUNT_RECOVERY_REQUEST_EXPIRATION_PERIOD)

    assert len(node.api.database.find_account_recovery_requests(accounts=["alice"])["requests"]) == 0


@run_for("testnet")
def test_remove_account_recovery_request(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account("alice")

    recovery_account_key = tt.Account("initminer").public_key

    authority = get_authority(recovery_account_key)
    wallet.api.request_account_recovery("initminer", "alice", authority)

    authority = get_authority(recovery_account_key)
    # to remove existing recovery request, recovery_agent broadcast request_account_operation with weight_threshold = 0
    authority["weight_threshold"] = 0

    wallet.api.request_account_recovery("initminer", "alice", authority)
    assert len(node.api.database.find_account_recovery_requests(accounts=["alice"])["requests"]) == 0


@run_for("testnet")
def test_account_recovery_process(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account("alice", vests=100)

    alice_original_owner_key = get_owner_key(node, "alice")
    alice_original_authority = get_authority(alice_original_owner_key)

    # thief changes key to account "alice"
    wallet.api.update_account_auth_key("alice", "owner", str(tt.PublicKey("alice", secret="thief_key")), 3)

    # alice asks her recovery_agent (initminer) to create a recovery request
    new_authority_key = tt.PublicKey("alice", secret="alice_new_key")
    new_authority = get_authority(new_authority_key)
    wallet.api.import_key(tt.PrivateKey("alice", secret="alice_new_key"))
    wallet.api.request_account_recovery("initminer", "alice", new_authority)

    # alice confirms the request made by her agent
    wallet.api.recover_account("alice", alice_original_authority, new_authority)

    assert get_owner_key(node, "alice") == new_authority_key


@run_for("testnet")
def test_recovery_account_process_without_changing_owner_key(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account("alice")

    primary_key = get_owner_key(node, "alice")
    primary_authority = get_authority(primary_key)

    new_key = tt.PublicKey("alice", secret="new_key")
    new_authority = get_authority(new_key)
    wallet.api.import_key(tt.PrivateKey("alice", secret="new_key"))

    wallet.api.request_account_recovery("initminer", "alice", new_authority)
    assert len(node.api.database.find_account_recovery_requests(accounts=["alice"])["requests"]) == 1

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        wallet.api.recover_account("alice", primary_authority, new_authority)

    assert "Recent authority not found in authority history." in exception.value.response["error"]["message"]


@run_for("testnet")
def test_confirm_account_recovery_without_account_recovery_request(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account("alice")

    primary_key = get_owner_key(node, "alice")
    primary_authority = get_authority(primary_key)

    new_key = tt.PublicKey("alice", secret="new_key")
    new_authority = get_authority(new_key)
    wallet.api.import_key(tt.PrivateKey("alice", secret="new_key"))

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        wallet.api.recover_account("alice", primary_authority, new_authority)

    assert "There are no active recovery requests for this account." in exception.value.response["error"]["message"]


@run_for("testnet")
def test_account_recovery_process_with_mismatched_key(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account("alice", vests=100)

    primary_key = get_owner_key(node, "alice")
    primary_authority = get_authority(primary_key)

    # thief changes owner key to account "alice"
    wallet.api.update_account_auth_key("alice", "owner", tt.PublicKey("alice", secret="thief_key"), 3)

    # alice asks her recovery_agent (initminer) to create a recovery request
    new_key = tt.PublicKey("alice", secret="alice_new_key")
    new_authority = get_authority(new_key)
    wallet.api.import_key(tt.PrivateKey("alice", secret="alice_new_key"))
    wallet.api.request_account_recovery("initminer", "alice", new_authority)

    random_key = tt.PublicKey("alice", secret="random_key")
    random_authority = get_authority(random_key)
    wallet.api.import_key(tt.PrivateKey("alice", secret="random_key"))

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        # The key does not match the key provided in the recovery request
        wallet.api.recover_account("alice", primary_authority, random_authority)

    assert "New owner authority does not match recovery request." in exception.value.response["error"]["message"]


@run_for("testnet")
def test_account_recovery_process_with_most_trust_witness_as_recovery_agent(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account("alice", creator="temp")
    assert get_recovery_agent(node, account_name="alice") == ""

    primary_key = get_owner_key(node, "alice")
    primary_authority = get_authority(primary_key)

    # alice asks her recovery_agent (most trusted witness - initminer) to create a recovery request
    new_key = tt.PublicKey("alice", secret="alice_new_key")
    new_authority = get_authority(new_key)
    wallet.api.import_key(tt.PrivateKey("alice", secret="alice_new_key"))

    wallet.api.update_account_auth_key("alice", "owner", tt.PublicKey("alice", secret="thief_key"), 3)

    wallet.api.request_account_recovery("initminer", "alice", new_authority)
    assert len(node.api.database.find_account_recovery_requests(accounts=["alice"])["requests"]) == 1

    # alice confirms the request made by her agent
    wallet.api.recover_account("alice", primary_authority, new_authority)
    assert get_owner_key(node, "alice") == new_key


@run_for("testnet")
def test_create_account_recovery_request_from_random_account_as_recovery_agent(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account("alice")
    wallet.create_account("random-account")

    recovery_account_key = tt.Account("random-account").public_key
    authority = get_authority(recovery_account_key)

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        # alice confirms the request made by random account
        wallet.api.request_account_recovery("random-account", "alice", authority)

    assert (
        "Cannot recover an account that does not have you as their recovery partner."
        in exception.value.response["error"]["message"]
    )


@run_for("testnet")
def test_create_recovery_account_request_from_future_recovery_agent(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account("alice", vests=tt.Asset.Test(100))
    wallet.create_account("future-agent")

    wallet.api.change_recovery_account("alice", "future-agent")
    new_key = tt.Account("alice", secret="new_key").public_key
    new_authority = get_authority(new_key)

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        wallet.api.request_account_recovery("future-agent", "alice", new_authority)

    assert (
        "Cannot recover an account that does not have you as their recovery partner."
        in exception.value.response["error"]["message"]
    )


@run_for("testnet")
def test_recover_account_using_current_agent_while_waiting_for_the_approval_of_the_new_recovery_agent(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account("alice", vests=tt.Asset.Test(100))

    alice_original_key = get_owner_key(node, "alice")
    alice_original_authority = get_authority(alice_original_key)

    wallet.create_account("future-agent")

    wallet.api.change_recovery_account("alice", "future-agent")

    # it is impossible to recover an account without at least one key change
    key = tt.PublicKey("alice", secret="thief_key")
    wallet.api.update_account_auth_key("alice", "owner", key, 3)

    new_key = tt.Account("alice", secret="new_key").public_key
    wallet.api.import_key(tt.Account("alice", secret="new_key").private_key)
    new_authority = get_authority(new_key)

    wallet.api.request_account_recovery("initminer", "alice", new_authority)
    assert len(node.api.database.find_account_recovery_requests(accounts=["alice"])["requests"]) == 1

    wallet.api.recover_account("alice", alice_original_authority, new_authority)


@run_for("testnet")
def test_try_to_confirm_account_recovery_request_with_expired_key(node):
    """
    Timeline:
                                      The time during which the original key works
           │‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│
    ───────●───────────────────────────────────────────────●──────────────────────────────────────────────────●─●───>[t]
           ↓                                               ↓                                                    ↓
    alice loses 'owner'                          alice recovery agent (initminer)                       alice confirm
    authority (thief                             create account recovery request                        recovery request
    change authority)                            for alice account                                      - unsuccessfully
    """
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account("alice", vests=100)

    alice_original_owner_key = get_owner_key(node, "alice")
    alice_original_authority = get_authority(alice_original_owner_key)

    # thief changes key to account "alice"
    thief_key = tt.PublicKey("alice", secret="thief_key")
    wallet.api.update_account_auth_key("alice", "owner", thief_key, 3)

    node.wait_number_of_blocks(OWNER_AUTH_RECOVERY_PERIOD - 2)

    # alice asks her recovery_agent (initminer) to create a recovery request
    new_authority_key = tt.PublicKey("alice", secret="alice_new_key")
    new_authority = get_authority(new_authority_key)
    wallet.api.import_key(tt.PrivateKey("alice", secret="alice_new_key"))
    wallet.api.request_account_recovery("initminer", "alice", new_authority)

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        # alice confirms the request made by her agent
        wallet.api.recover_account("alice", alice_original_authority, new_authority)

    assert "Recent authority not found in authority history." in exception.value.response["error"]["message"]


@run_for("testnet")
def test_use_account_recovery_system_twice(node):
    """
    Timeline:                               0    minimum time to reconfirm recovery account    6s
                                            │‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│
    ───────●────────────────────────────────●──────────────────────────────────────────────────●────●───────────────>[t]
           ↓                                ↓                                                       ↓
    alice loses 'owner'           alice recovery agent (initminer)               alice recovery agent (initminer)
    authority (thief              creates account recovery request,              second time creates account recovery
    change authority)             alice confirms it                              request, alice confirms it
    """
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account("alice", vests=100)

    alice_original_owner_key = get_owner_key(node, "alice")
    alice_original_authority = get_authority(alice_original_owner_key)

    # thief changes key to account "alice"
    thief_key = tt.PublicKey("alice", secret="thief_key")
    wallet.api.update_account_auth_key("alice", "owner", thief_key, 3)

    # alice asks her recovery_agent (initminer) to create a recovery request
    new_authority_key = tt.PublicKey("alice", secret="alice_new_key")
    new_authority = get_authority(new_authority_key)
    wallet.api.import_key(tt.PrivateKey("alice", secret="alice_new_key"))
    wallet.api.request_account_recovery("initminer", "alice", new_authority)

    wallet.api.recover_account("alice", alice_original_authority, new_authority)

    # alice second time asks her recovery_agent (initminer) to create a recovery request
    new_authority_key = tt.PublicKey("alice", secret="alice_second_key")
    new_authority = get_authority(new_authority_key)
    wallet.api.import_key(tt.PrivateKey("alice", secret="alice_second_key"))
    wallet.api.request_account_recovery("initminer", "alice", new_authority)

    # minimum time to reconfirm recovery account operation (6 seconds on testnet /  60 minutes on the mainnet)
    node.wait_number_of_blocks(OWNER_UPDATE_LIMIT)

    wallet.api.recover_account("alice", alice_original_authority, new_authority)


@run_for("testnet")
def test_use_account_recovery_system_twice_within_a_short_period_of_time(node):
    """
    Timeline:                               0    minimum time to reconfirm recovery account    6s
                                            │‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│
    ───────●────────────────────────────────●─────────────────────────────────────────────●────●────────────────────>[t]
           ↓                                ↓                                             ↓
    alice loses 'owner'           alice recovery agent (initminer)               alice recovery agent (initminer)
    authority (thief              creates account recovery request,              second time creates account recovery
    change authority)             alice confirms it                              request, alice tries to confirm it
    """
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account("alice", vests=100)

    alice_original_owner_key = get_owner_key(node, "alice")
    alice_original_authority = get_authority(alice_original_owner_key)

    # thief changes key to account "alice"
    thief_key = tt.PublicKey("alice", secret="thief_key")
    wallet.api.update_account_auth_key("alice", "owner", thief_key, 3)

    # alice asks her recovery_agent (initminer) to create a recovery request
    new_authority_key = tt.PublicKey("alice", secret="alice_new_key")
    new_authority = get_authority(new_authority_key)
    wallet.api.import_key(tt.PrivateKey("alice", secret="alice_new_key"))
    wallet.api.request_account_recovery("initminer", "alice", new_authority)

    wallet.api.recover_account("alice", alice_original_authority, new_authority)

    # alice second time asks her recovery_agent (initminer) to create a recovery request
    new_authority_key = tt.PublicKey("alice", secret="alice_second_key")
    new_authority = get_authority(new_authority_key)
    wallet.api.import_key(tt.PrivateKey("alice", secret="alice_second_key"))
    wallet.api.request_account_recovery("initminer", "alice", new_authority)

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        wallet.api.recover_account("alice", alice_original_authority, new_authority)
    # FIXME: After repair (https://gitlab.syncad.com/hive/hive/-/issues/469) uncomment assert and change error message
    # assert "EXPECTED ERROR MESSAGE" in exception.value.response["error"]["message"]
