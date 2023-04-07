import pytest

import test_tools as tt
from hive_local_tools.constants import OWNER_AUTH_RECOVERY_PERIOD
from hive_local_tools.functional.python.recovery import get_authority, get_recovery_agent, get_owner_key


def test_steal_account_scenario_0(prepare_environment):
    """
    Timeline:
        0d                                 15d                                 30d                              45d
                                           │‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│
        │‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│                                │
    ────●─────────────────────────────────■─────────●────────────────●─────────●────────────────────────────────■─●─>[t]
        ↓                                 ↓         ↓                ↓                                            ↓
    thief changes             thief changes       alice-agent        alice-accept                       thief `recovers`
    owner authority           agent for alice     create account     recovery request,                  alice's account.
    for alice account         account (duration   recovery request   but forgets to                     Alice completely
    (duration 30d)            30 days)                               check her agent                    loses access to
                                                                                                        the account
    """
    node, wallet_alice, wallet_thief, wallet_alice_agent, wallet_thief_agent = prepare_environment

    alice_original_owner_key = get_owner_key(node, "alice")
    alice_original_authority = get_authority(alice_original_owner_key)

    # the thief changes the owner key of alice's account
    thief_owner_key_to_alice_account = tt.Account("thief", secret="thief_key").public_key
    wallet_thief.api.import_key(tt.Account("thief", secret="thief_key").private_key)
    thief_owner_authority_to_alice_account = get_authority(thief_owner_key_to_alice_account)
    wallet_thief.api.update_account("alice", "{}",
                                    thief_owner_key_to_alice_account,
                                    thief_owner_key_to_alice_account,
                                    thief_owner_key_to_alice_account,
                                    thief_owner_key_to_alice_account,
                                    )
    assert get_owner_key(node, "alice") == thief_owner_key_to_alice_account

    node.wait_number_of_blocks(OWNER_AUTH_RECOVERY_PERIOD//2)

    # alice realizes she has lost the key and uses her recovery agent to change the owner key
    alice_new_key = tt.Account("alice", secret="new_key").public_key
    alice_new_authority = get_authority(alice_new_key)
    wallet_alice.api.import_key(tt.Account("alice", secret="new_key").private_key)
    wallet_alice_agent.api.request_account_recovery("alice.agent", "alice", alice_new_authority)

    # thief changes alice recovery agent
    wallet_thief.api.change_recovery_account("alice", "thief")

    # alice confirms the recovery request
    wallet_alice.api.recover_account("alice", alice_original_authority, alice_new_authority)
    assert get_owner_key(node, "alice") == alice_new_key

    get_recovery_agent(node, "alice", wait_for_agent="thief")

    # thief recovers the alice account
    thief_key = tt.Account("thief", secret="it_is_thief").public_key
    wallet_thief.api.import_key(tt.Account("thief", secret="it_is_thief").private_key)
    thief_authority = get_authority(thief_key)
    wallet_thief.api.request_account_recovery("thief", "alice", thief_authority)
    assert len(node.api.database.find_account_recovery_requests(accounts=["alice"])["requests"]) == 1

    wallet_thief.api.recover_account("alice", thief_owner_authority_to_alice_account, thief_authority)
    assert get_owner_key(node, "alice") == thief_key


def test_steal_account_scenario_1(prepare_environment):
    """
    Timeline:
        0d                                 15d                                 30d                              45d
                                           │‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│
        │‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│                                │
    ────●───────────────────●──────────────■─────────────────────────●─────────●────────────────────────────────■─●──>[t]
        ↓                   ↓              ↓                         ↓                                            ↓
    thief changes      alice - agent      thief changes         alice-accept recovery request            the thief loses
    owner authority    create account     agent for alice       and clear change recovery                access to the
    for alice account  recovery request   account (duration     agent request (account safe)             alice account
    (duration 30d)                        30 days)
    """
    node, wallet_alice, wallet_thief, wallet_alice_agent, wallet_thief_agent = prepare_environment

    alice_original_owner_key = get_owner_key(node, "alice")
    alice_original_authority = get_authority(alice_original_owner_key)

    # the thief changes the owner key of alice's account
    thief_owner_key_to_alice_account = tt.Account("thief", secret="thief_key").public_key
    wallet_thief.api.import_key(tt.Account("thief", secret="thief_key").private_key)
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

    # thief changes alice recovery agent
    wallet_thief.api.change_recovery_account("alice", "thief")

    # alice confirms the recovery request
    wallet_alice.api.recover_account("alice", alice_original_authority, alice_new_authority)
    assert get_owner_key(node, "alice") == alice_new_key

    # alice cancel change recovery agent request
    wallet_alice.api.change_recovery_account("alice", "alice.agent")
    assert len(node.api.database.find_account_recovery_requests(accounts=["alice"])["requests"]) == 0
    assert get_recovery_agent(node, "alice") == "alice.agent"

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        wallet_thief.api.request_account_recovery("thief", "alice", get_authority(tt.Account("thief").public_key))
    assert (
            "Cannot recover an account that does not have you as their recovery partner."
            in exception.value.response["error"]["message"]
    )


def test_steal_account_scenario_2(prepare_environment):
    """
    Timeline:
        0d                                 15d                                 30d                              45d
                                           │‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│
        │‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│                                │
    ────●──────────────────────────────────■─────────────────────────●─────────●──────────────●─────────────────■───>[t]
        ↓                                  ↓                         ↓                        ↓
    thief changes                thief changes recovery       alice-agent create        alice - can't
    owner authority              agent for alice account      account recovery          accept account
    for alice account            (duration 30 days)           request                   recovery request
    (duration 30d)
    """
    node, wallet_alice, wallet_thief, wallet_alice_agent, wallet_thief_agent = prepare_environment

    alice_original_owner_key = get_owner_key(node, "alice")
    alice_original_authority = get_authority(alice_original_owner_key)

    # the thief changes the owner key of alice's account
    thief_owner_key_to_alice_account = tt.Account("thief", secret="thief_key").public_key
    wallet_thief.api.import_key(tt.Account("thief", secret="thief_key").private_key)
    wallet_thief.api.update_account("alice", "{}",
                                    thief_owner_key_to_alice_account,
                                    thief_owner_key_to_alice_account,
                                    thief_owner_key_to_alice_account,
                                    thief_owner_key_to_alice_account,
                                    )
    assert get_owner_key(node, "alice") == thief_owner_key_to_alice_account

    node.wait_number_of_blocks(OWNER_AUTH_RECOVERY_PERIOD - 3)

    # thief changes alice recovery agent
    wallet_thief.api.change_recovery_account("alice", "thief")

    # alice realizes she has lost the key and uses her recovery agent to change the owner key
    alice_new_key = tt.Account("alice", secret="new_key").public_key
    alice_new_authority = get_authority(alice_new_key)
    wallet_alice.api.import_key(tt.Account("alice", secret="new_key").private_key)
    wallet_alice_agent.api.request_account_recovery("alice.agent", "alice", alice_new_authority)

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        wallet_alice.api.recover_account("alice", alice_original_authority, alice_new_authority)
    assert "Recent authority not found in authority history." in exception.value.response["error"]["message"]
