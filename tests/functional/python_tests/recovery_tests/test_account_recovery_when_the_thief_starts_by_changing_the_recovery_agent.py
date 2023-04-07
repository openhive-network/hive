import pytest

import test_tools as tt
from hive_local_tools.constants import OWNER_AUTH_RECOVERY_PERIOD
from hive_local_tools.functional.python.recovery import get_authority, get_recovery_agent, get_owner_key


def test_account_theft_after_user_overlooked_change_of_recovery_agent(prepare_environment):
    """
    Timeline:
        0d                            15d                            30d                         45d
                                      │‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│
        │‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│                           │
    ────■─────────────────────────────●──────────────────────────────■──────────────────■────────●──────────────────>[t]
        ↓                             ↓                              ↓                  ↓
    thief changes agent         alice changes                thief agent becomes     thief `recovers` alice
    for alice account           owner key                    alice account agent     account (alice loses her account)
    (duration 30d)              (duration 30d)
    """
    node, wallet_alice, wallet_thief, wallet_alice_agent, wallet_thief_agent = prepare_environment

    alice_original_owner_key = get_owner_key(node, "alice")
    alice_original_authority = get_authority(alice_original_owner_key)

    # thief changes alice recovery agent
    wallet_thief.api.change_recovery_account("alice", "thief")

    node.wait_number_of_blocks(OWNER_AUTH_RECOVERY_PERIOD // 2)

    # alice understands the situation and changes owner key
    alice_new_owner_key = tt.Account("alice_new_owner_key").public_key
    wallet_alice.api.update_account("alice", "{}",
                                    alice_new_owner_key,
                                    alice_new_owner_key,
                                    alice_new_owner_key,
                                    alice_new_owner_key,
                                    )

    # alice isn't aware that in 15 more days thief will become her account recovery agent (after 30 days overall)
    get_recovery_agent(node, "alice", wait_for_agent="thief")

    # thief as the new agent of alice's account creates an account recovery request
    thief_key = tt.Account("thief", secret="thief_key").public_key
    thief_authority = get_authority(thief_key)
    wallet_thief.api.import_key(tt.PrivateKey("thief", secret="thief_key"))
    wallet_thief.api.request_account_recovery("thief", "alice", thief_authority)
    assert len(node.api.database.find_account_recovery_requests(accounts=["alice"])["requests"]) == 1

    # thief "recover" alice account
    wallet_thief.api.recover_account("alice", alice_original_authority, thief_authority)


def test_steal_account_scenario_start_from_change_recovery_agent_by_thief_0(prepare_environment):
    """
    Timeline:
        0d                                                                                         30d
        │‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│
    ────■─────────────────────────────────────────────●────────────────────────────────────────────■───────●────────>[t]
        ↓                                             ↓                                                    ↓
    thief changes agent                          alice deletes                              thief tries unsuccessfully
    for alice account                            agent change                               to `recover` alice's account
    (duration 30d)                               request
    """
    node, wallet_alice, wallet_thief, wallet_alice_agent, wallet_thief_agent = prepare_environment

    # thief changes alice recovery agent
    wallet_thief.api.change_recovery_account("alice", "thief")

    node.wait_number_of_blocks(OWNER_AUTH_RECOVERY_PERIOD//2)

    # alice understands the situation and removes the recovery agent change request
    wallet_alice.api.change_recovery_account("alice", "alice.agent")
    assert len(node.api.database.find_account_recovery_requests(accounts=["alice"])["requests"]) == 0

    node.wait_number_of_blocks(OWNER_AUTH_RECOVERY_PERIOD//2)

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        # thief tries to recover "alice" account
        thief_account_key = tt.Account("thief").public_key
        thief_authority = get_authority(thief_account_key)

        wallet_thief.api.request_account_recovery("thief", "alice", thief_authority)
    assert (
        "Cannot recover an account that does not have you as their recovery partner."
        in exception.value.response["error"]["message"]
    )


def test_steal_account_scenario_start_from_change_recovery_agent_by_thief_1(prepare_environment):
    """
    Timeline:
        0d                            15d                            30d                         45d
                                      │‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│
        │‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│                           │
    ────■─────────────────────────────●──────────────────────────────■───────────────────────────●───────●──────────>[t]
        ↓                             ↓                                                                  ↓
    thief changes agent         thief changes                                                 alice realizes that she
    for alice account           owner authority                                               has lost the owner key,
    (duration 30d)              to alice account                                              tries unsuccessfully
                                (duration 30d)                                                recover her account
    """
    node, wallet_alice, wallet_thief, wallet_alice_agent, wallet_thief_agent = prepare_environment

    # thief changes alice recovery agent
    wallet_thief.api.change_recovery_account("alice", "thief")

    node.wait_number_of_blocks(OWNER_AUTH_RECOVERY_PERIOD//2)

    # the thief changes the owner key of alice's account
    thief_owner_key_to_alice_account = tt.Account("thief_key").public_key
    wallet_thief.api.update_account("alice", "{}",
                                    thief_owner_key_to_alice_account,
                                    thief_owner_key_to_alice_account,
                                    thief_owner_key_to_alice_account,
                                    thief_owner_key_to_alice_account,
                                    )
    assert get_owner_key(node, "alice") == thief_owner_key_to_alice_account

    # alice doesn't notice the recovery agent change, day 45 passes
    node.wait_number_of_blocks(OWNER_AUTH_RECOVERY_PERIOD)

    alice_new_key = tt.Account("alice", secret="new_key").public_key
    alice_new_authority = get_authority(alice_new_key)

    # alice is trying to change the owner authority
    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        wallet_alice.api.update_account("alice", "{}", alice_new_key, alice_new_key, alice_new_key, alice_new_key)
    assert "Missing Owner Authority"  in exception.value.response["error"]["message"]

    # alice tries to change the recovery agent
    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        wallet_alice.api.change_recovery_account("alice", "alice.agent")
    assert "Missing Owner Authority" in exception.value.response["error"]["message"]

    # alice-agent trying to recover an alice account
    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        wallet_alice_agent.api.request_account_recovery("alice.agent", "alice", alice_new_authority)
    error_message = "Cannot recover an account that does not have you as their recovery partner."
    assert error_message  in exception.value.response["error"]["message"]


def test_account_recovery_after_the_thief_changed_the_key_but_before_changing_the_recovery_agent(prepare_environment):
    """
    Timeline:
        0d                            15d                            30d                         45d
                                      │‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│
        │‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│                           │
    ────■─────────────────────────────●──────────────────────────────■─────────────●─────────────●──────────────────>[t]
        ↓                             ↓                                            ↓
    thief changes agent         thief changes                            alice realizes that she
    for alice account           owner authority                          has lost the owner key,
    (duration 30d)              to alice account                         tries unsuccessfully
                                (duration 30d)                           recover her account
    """
    node, wallet_alice, wallet_thief, wallet_alice_agent, wallet_thief_agent = prepare_environment

    # thief changes alice recovery agent
    wallet_thief.api.change_recovery_account("alice", "thief")

    node.wait_number_of_blocks(OWNER_AUTH_RECOVERY_PERIOD // 2)

    # the thief changes the owner key of alice's account
    thief_owner_key_to_alice_account = tt.Account("thief_key").public_key
    wallet_thief.api.update_account("alice", "{}",
                                    thief_owner_key_to_alice_account,
                                    thief_owner_key_to_alice_account,
                                    thief_owner_key_to_alice_account,
                                    thief_owner_key_to_alice_account,
                                    )
    assert get_owner_key(node, "alice") == thief_owner_key_to_alice_account

    # alice doesn't notice the recovery agent change, day 45 passes. Thief becomes alice account recovery agent
    get_recovery_agent(node, "alice", wait_for_agent="thief")

    alice_new_key = tt.Account("alice", secret="new_key").public_key
    alice_new_authority = get_authority(alice_new_key)

    # alice is trying to change the owner authority
    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        wallet_alice.api.update_account("alice", "{}", alice_new_key, alice_new_key, alice_new_key, alice_new_key)
    assert "Missing Owner Authority"  in exception.value.response["error"]["message"]

    # alice tries to change the recovery agent
    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        wallet_alice.api.change_recovery_account("alice", "alice.agent")
    assert "Missing Owner Authority" in exception.value.response["error"]["message"]

    # alice-agent trying to recover an alice account
    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        wallet_alice_agent.api.request_account_recovery("alice.agent", "alice", alice_new_authority)
    error_message = "Cannot recover an account that does not have you as their recovery partner."
    assert error_message  in exception.value.response["error"]["message"]


def test_account_recovery_after_the_key_and_agent_was_changed_by_the_thief(prepare_environment):
    """
    Timeline:
        0d                            15d                            30d                         45d
                                      │‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│
        │‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│                           │
    ────■─────────────────────────────●─────────────────────●────────■─────────────●─────────────●──────────────────>[t]
        ↓                             ↓                     ↓                      ↓
    thief changes agent         thief changes        alice wants to change        alice-agent tries to create
    for alice account           owner authority      change owner authority       account recovery request
    (duration 30d)              to alice account     -  unsuccessfully            - unsuccessfully
                                (duration 30d)
    """

    node, wallet_alice, wallet_thief, wallet_alice_agent, wallet_thief_agent = prepare_environment

    # thief changes alice recovery agent
    wallet_thief.api.change_recovery_account("alice", "thief")
    node.wait_number_of_blocks(OWNER_AUTH_RECOVERY_PERIOD//2)

    # the thief changes the owner key of alice's account
    thief_owner_key_to_alice_account = tt.Account("thief_key").public_key
    wallet_thief.api.update_account("alice", "{}",
                                    thief_owner_key_to_alice_account,
                                    thief_owner_key_to_alice_account,
                                    thief_owner_key_to_alice_account,
                                    thief_owner_key_to_alice_account,
                                    )
    assert get_owner_key(node, "alice") == thief_owner_key_to_alice_account

    alice_new_key = tt.Account("alice", secret="new_key").public_key
    alice_new_authority = get_authority(alice_new_key)

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        wallet_alice.api.update_account("alice", "{}",
                                        alice_new_key,
                                        alice_new_key,
                                        alice_new_key,
                                        alice_new_key,
                                        )
    assert "Missing Owner Authority" in exception.value.response["error"]["message"]

    get_recovery_agent(node, "alice", wait_for_agent="thief")

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        wallet_alice_agent.api.request_account_recovery("alice.agent", "alice", alice_new_authority)
    assert (
        "Cannot recover an account that does not have you as their recovery partner."
        in exception.value.response["error"]["message"]
    )


def test_steal_account_scenario_start_from_change_recovery_agent_by_thief_2(prepare_environment):
    """
    Timeline:
        0d                            15d                            30d                         45d
                                      │‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│
        │‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│                           │
    ────■─────────────────────────────●─────────────────────●────────■─────────────●─────────────●─────────●───────>[t]
        ↓                             ↓                     ↓                      ↓                       ↓
    thief changes agent         thief changes         alice-agent          alice - accept              thief 'recover'
    for alice account           owner authority       create account       account recovery            the alice account
    (duration 30d)              to alice account      recovery request     request
                                (duration 30d)
    """
    node, wallet_alice, wallet_thief, wallet_alice_agent, wallet_thief_agent = prepare_environment

    alice_original_owner_key = get_owner_key(node, "alice")
    alice_original_authority = get_authority(alice_original_owner_key)

    # thief changes alice recovery agent
    wallet_thief.api.change_recovery_account("alice", "thief")

    node.wait_number_of_blocks(15)

    # the thief changes the owner key of alice's account
    thief_owner_key_to_alice_account = tt.Account("thief", secret="thief's_key_to_alice's_account").public_key
    wallet_thief.api.import_key(tt.Account("thief", secret="thief's_key_to_alice's_account").private_key)
    thief_owner_authority_to_alice_account = get_authority(thief_owner_key_to_alice_account)
    wallet_thief.api.update_account("alice", "{}",
                                    thief_owner_key_to_alice_account,
                                    thief_owner_key_to_alice_account,
                                    thief_owner_key_to_alice_account,
                                    thief_owner_key_to_alice_account,
                                    )
    assert get_owner_key(node, "alice") == thief_owner_key_to_alice_account

    alice_new_key = tt.Account("alice", secret="new_key").public_key
    wallet_alice.api.import_key(tt.Account("alice", secret="new_key").private_key)
    alice_new_authority = get_authority(alice_new_key)
    wallet_alice_agent.api.request_account_recovery("alice.agent", "alice", alice_new_authority)

    assert get_recovery_agent(node, "alice", wait_for_agent="thief") == "thief"
    wallet_alice.api.recover_account("alice", alice_original_authority, alice_new_authority)
    node.wait_number_of_blocks(2) # the minimum time between two `recover_account` is in mainnet is 1h/ testnet 6s

    thief_key = tt.Account("thief", secret="it_is_thief").public_key
    wallet_thief.api.import_key(tt.Account("thief", secret="it_is_thief").private_key)
    thief_authority = get_authority(thief_key)

    wallet_thief.api.request_account_recovery("thief", "alice", thief_authority)
    assert len(node.api.database.find_account_recovery_requests(accounts=["alice"])["requests"]) == 1
    wallet_thief.api.recover_account("alice", thief_owner_authority_to_alice_account, thief_authority)


def test_steal_account_scenario_start_from_change_recovery_agent_by_thief_3(prepare_environment):
    """
    Timeline:
        0d                                15d                                30d                             45d
                                          │‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│
        │‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│                               │
    ────■─────────────────────────────────●───────────●────────────────●─────■─────────────────────●─────────●──────>[t]
        ↓                                 ↓           ↓                ↓                           ↓
    thief changes agent         thief changes      alice-agent        alice - accept            thief `recovers`
    for alice account           owner authority    create account     account recovery          the alice account
    (duration 30d)              to alice account   recovery request   request                   (alice did not change
                                (duration 30d)                                                  the recovery agent)
    """
    node, wallet_alice, wallet_thief, wallet_alice_agent, wallet_thief_agent = prepare_environment

    alice_original_key = get_owner_key(node, "alice")
    alice_original_authority = get_authority(alice_original_key)

    # thief changes alice recovery agent
    wallet_thief.api.change_recovery_account("alice", "thief")

    node.wait_number_of_blocks(OWNER_AUTH_RECOVERY_PERIOD//2)

    # the thief changes the owner key of alice's account
    thief_owner_key_to_alice_account = tt.Account("thief").public_key
    thief_owner_authority_to_alice_account = get_authority(thief_owner_key_to_alice_account)
    wallet_thief.api.update_account("alice", "{}",
                                    thief_owner_key_to_alice_account,
                                    thief_owner_key_to_alice_account,
                                    thief_owner_key_to_alice_account,
                                    thief_owner_key_to_alice_account,
                                    )
    assert get_owner_key(node, "alice") == thief_owner_key_to_alice_account

    # alice realizes she has lost the key and uses her recovery agent to change the owner key
    alice_new_key = tt.Account("alice", secret="new_key").public_key
    alice_new_authority = get_authority(alice_new_key)
    wallet_alice.api.import_key(tt.Account("alice", secret="new_key").private_key)
    wallet_alice_agent.api.request_account_recovery("alice.agent", "alice", alice_new_authority)
    wallet_alice.api.recover_account("alice", alice_original_authority, alice_new_authority)
    assert get_owner_key(node, "alice") == alice_new_key

    get_recovery_agent(node, "alice", wait_for_agent="thief") # wait for the recovery agent to change

    # thief recovers the alice account
    thief_key = tt.Account("thief", secret="it_is_thief").public_key
    wallet_thief.api.import_key(tt.Account("thief", secret="it_is_thief").private_key)
    thief_authority = get_authority(thief_key)
    wallet_thief.api.request_account_recovery("thief", "alice", thief_authority)
    assert len(node.api.database.find_account_recovery_requests(accounts=["alice"])["requests"]) == 1

    wallet_thief.api.recover_account("alice", thief_owner_authority_to_alice_account, thief_authority)
    assert get_owner_key(node, "alice") == thief_key


def test_steal_account_scenario_start_from_change_recovery_agent_by_thief_4(prepare_environment):
    """
    Timeline:
        0d                                15d                                30d                             45d
                                          │‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│
        │‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│                               │
    ────■─────────────────────────────────●──────────────●────────────────●──■───────────────────────────●───●──────>[t]
        ↓                                 ↓              ↓                ↓                              ↓
    thief changes agent         thief changes       alice-agent         alice - accept recovery         thief "recover"
    for alice account           owner authority     create account      request and delete change       alice account
    (duration 30d)              to alice account    recovery request    recovery agent request          - unsuccessfully
                                (duration 30d)                          - account is safe
    """
    node, wallet_alice, wallet_thief, wallet_alice_agent, wallet_thief_agent = prepare_environment

    alice_original_owner_key = get_owner_key(node, "alice")
    alice_original_authority = get_authority(alice_original_owner_key)

    # thief changes alice recovery agent
    wallet_thief.api.change_recovery_account("alice", "thief")

    node.wait_number_of_blocks(OWNER_AUTH_RECOVERY_PERIOD//2)

    # the thief changes the owner key of alice's account
    thief_owner_key_to_alice_account = tt.Account("thief").public_key
    wallet_thief.api.update_account("alice", "{}",
                                    thief_owner_key_to_alice_account,
                                    thief_owner_key_to_alice_account,
                                    thief_owner_key_to_alice_account,
                                    thief_owner_key_to_alice_account,
                                    )
    assert get_owner_key(node, "alice") == thief_owner_key_to_alice_account

    # alice realizes she has lost the key and uses her recovery agent to change the owner key
    alice_new_key = tt.Account("alice", secret="new_key").public_key
    alice_new_authority = get_authority(alice_new_key)
    wallet_alice.api.import_key(tt.Account("alice", secret="new_key").private_key)
    wallet_alice_agent.api.request_account_recovery("alice.agent", "alice", alice_new_authority)
    wallet_alice.api.recover_account("alice", alice_original_authority, alice_new_authority)
    assert get_owner_key(node, "alice") == alice_new_key

    # alice deletes the agent change request created by the thief
    wallet_alice.api.change_recovery_account("alice", "alice.agent")
    assert len(node.api.database.find_change_recovery_account_requests(accounts=["alice"])["requests"]) == 0
    assert get_recovery_agent(node, account_name="alice") == "alice.agent"

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        # thief tries to recover "alice" account
        thief_account_key = tt.Account("thief").public_key
        thief_authority = get_authority(thief_account_key)

        wallet_thief.api.request_account_recovery("thief", "alice", thief_authority)
    assert (
        "Cannot recover an account that does not have you as their recovery partner."
        in exception.value.response["error"]["message"]
    )
