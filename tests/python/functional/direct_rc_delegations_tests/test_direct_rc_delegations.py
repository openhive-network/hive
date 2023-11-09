from __future__ import annotations

import pytest

import test_tools as tt


def test_direct_rc_delegations(wallet: tt.Wallet) -> None:
    creator = "initminer"
    delegator = "delegator"
    receiver = "receiver"
    receiver2 = "zzz"
    wallet.api.create_account(creator, delegator, "{}")
    wallet.api.create_account(creator, receiver, "{}")
    wallet.api.create_account(creator, receiver2, "{}")
    wallet.api.transfer(creator, receiver, "10.000 TESTS", "", "true")
    wallet.api.transfer_to_vesting(creator, delegator, "0.010 TESTS", "true")
    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        wallet.api.transfer(receiver, receiver, "0.001 TESTS", "", "true")
    assert "receiver has 0 RC, needs 1 RC. Please wait to transact" in exception.value.error

    rc_receiver = wallet.api.find_rc_accounts([receiver])[0]
    rc_receiver2 = wallet.api.find_rc_accounts([receiver2])[0]
    rc_delegator = wallet.api.find_rc_accounts([delegator])[0]
    rc_delegator_before = rc_delegator

    assert rc_receiver["delegated_rc"] == 0
    assert rc_receiver["received_delegated_rc"] == 0
    assert rc_delegator["delegated_rc"] == 0
    assert rc_delegator["received_delegated_rc"] == 0
    assert rc_receiver2["delegated_rc"] == 0
    assert rc_receiver2["received_delegated_rc"] == 0

    print(f"Delegating rc to {receiver} and {receiver2}")
    wallet.api.delegate_rc(delegator, [receiver, receiver2], 10, "true")

    rc_receiver = wallet.api.find_rc_accounts([receiver])[0]
    rc_receiver2 = wallet.api.find_rc_accounts([receiver2])[0]
    rc_delegator = wallet.api.find_rc_accounts([delegator])[0]

    assert rc_receiver["delegated_rc"] == 0
    assert rc_receiver["received_delegated_rc"] == 10
    assert rc_receiver2["delegated_rc"] == 0
    assert rc_receiver2["received_delegated_rc"] == 10
    assert rc_delegator["delegated_rc"] == 20
    assert rc_delegator["received_delegated_rc"] == 0
    assert (
        rc_delegator["rc_manabar"]["current_mana"] == rc_delegator_before["rc_manabar"]["current_mana"] - 21
    )  # 20 + 1, 1 is the base cost of an rc delegation op, 20 is the amount delegated

    print("testing list direct delegations api")

    delegation_from_to = wallet.api.list_rc_direct_delegations([delegator, receiver], 1000)[0]

    assert delegation_from_to["from"] == delegator
    assert delegation_from_to["to"] == receiver
    assert delegation_from_to["delegated_rc"] == 10

    print(f"Increasing the delegation to 50 to {receiver}")
    wallet.api.delegate_rc(delegator, [receiver], 50, "true")

    rc_receiver = wallet.api.find_rc_accounts([receiver])[0]
    rc_delegator = wallet.api.find_rc_accounts([delegator])[0]

    assert rc_receiver["delegated_rc"] == 0
    assert rc_receiver["received_delegated_rc"] == 50
    assert rc_delegator["delegated_rc"] == 60
    assert rc_delegator["received_delegated_rc"] == 0
    assert (
        rc_delegator["rc_manabar"]["current_mana"] == rc_delegator_before["rc_manabar"]["current_mana"] - 62
    )  # 60 + 1 x 2, 1 is the base cost of an rc delegation op, 60 is the amount delegated

    print(f"Reducing the delegation to 20 to {receiver}")
    wallet.api.delegate_rc(delegator, [receiver], 20, "true")

    rc_receiver = wallet.api.find_rc_accounts([receiver])[0]
    rc_delegator = wallet.api.find_rc_accounts([delegator])[0]

    assert rc_receiver["delegated_rc"] == 0
    assert rc_receiver["received_delegated_rc"] == 20
    assert rc_delegator["delegated_rc"] == 30
    assert rc_delegator["received_delegated_rc"] == 0
    assert (
        rc_delegator["rc_manabar"]["current_mana"] == rc_delegator_before["rc_manabar"]["current_mana"] - 63
    )  # amount remains the same - 1 because current rc is not given back from reducing the delegation, and we paid 1 for the extra op

    print(f"deleting the delegation to {receiver}")
    wallet.api.delegate_rc(delegator, [receiver], 0, "true")

    rc_receiver = wallet.api.find_rc_accounts([receiver])[0]
    rc_delegator = wallet.api.find_rc_accounts([delegator])[0]
    delegation = wallet.api.list_rc_direct_delegations([delegator, receiver], 1000)

    assert rc_receiver["delegated_rc"] == 0
    assert len(delegation) == 1
    assert rc_receiver["received_delegated_rc"] == 0
    assert rc_delegator["delegated_rc"] == 10
    assert rc_delegator["received_delegated_rc"] == 0
    assert rc_delegator["max_rc"] == rc_delegator_before["max_rc"] - 10

    print("testing list_rc_accounts")
    accounts = wallet.api.list_rc_accounts("delegator", 3)
    rc_delegator = wallet.api.find_rc_accounts([delegator])[0]
    assert len(accounts) == 3
    assert accounts[0]["account"] == "delegator"
    assert accounts[0]["rc_manabar"]["current_mana"] == rc_delegator["rc_manabar"]["current_mana"]
    assert accounts[0]["rc_manabar"]["last_update_time"] == rc_delegator["rc_manabar"]["last_update_time"]
    assert accounts[0]["max_rc"] == rc_delegator["max_rc"]
    assert accounts[0]["delegated_rc"] == 10
    assert accounts[0]["received_delegated_rc"] == 0
    assert accounts[1]["account"] == "hive.fund"

    accounts_all = wallet.api.list_rc_accounts("aaa", 100)
    assert (
        len(accounts_all) == 10
    )  # miners, null, steem.dao, hive.fund, temp, initminer, steem, delegator, receiver, zzz

    accounts_offset = wallet.api.list_rc_accounts("receiver", 3)
    assert len(accounts_offset) == 3
    assert accounts_offset[0]["account"] == "receiver"
    assert accounts_offset[0]["rc_manabar"]["current_mana"] == 0
    assert accounts_offset[1]["account"] == "steem"
    assert accounts_offset[2]["account"] == "steem.dao"
