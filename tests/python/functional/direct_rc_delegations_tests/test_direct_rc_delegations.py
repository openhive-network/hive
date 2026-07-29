from __future__ import annotations

import test_tools as tt
from hive_local_tools.functional.python.operation import Account

# RC amounts have to clear the chain's min_delegation - (account_creation_fee / 3) * vesting_share_price, see
# libraries/chain/rc/rc_utility.cpp - which is around 3600 RC on a default testnet. They are kept well above
# it, so that a change of the fee or of the vest price cannot quietly invalidate them, and well below what the
# delegators power up below, since delegatable RC comes from own vesting shares only (rc_adjustment and
# received delegations do not count, see get_maximum_rc in account_object.hpp).
DELEGATION = 100_000


def test_direct_rc_delegations(node: tt.InitNode, wallet: tt.Wallet) -> None:
    creator = "initminer"
    delegator = "delegator"
    receiver = "receiver"
    receiver2 = "zzz"
    wallet.api.create_account(creator, delegator, "{}")
    wallet.api.create_account(creator, receiver, "{}")
    wallet.api.create_account(creator, receiver2, "{}")
    wallet.api.transfer(creator, receiver, tt.Asset.from_legacy("10.000 TESTS"), "", "true")
    wallet.api.transfer_to_vesting(creator, delegator, tt.Asset.from_legacy("100.000 TESTS"), "true")

    rc_receiver = wallet.api.find_rc_accounts([receiver])[0]
    rc_receiver2 = wallet.api.find_rc_accounts([receiver2])[0]
    rc_delegator = wallet.api.find_rc_accounts([delegator])[0]
    rc_delegator_before = rc_delegator
    # tracks the delegator's manabar across the operations below: it takes the operation cost from the
    # broadcast response and projects the stored manabar onto the transaction's block, so both the cost and
    # the regeneration that happened in the meantime are accounted for
    delegator_account = Account(delegator, node, wallet)
    # receiver does no operations and ends with no net delegation, so its current_mana stays at the
    # base reserve a freshly created account has (rc_adjustment).
    receiver_base_mana = rc_receiver["rc_manabar"]["current_mana"]

    assert rc_receiver["delegated_rc"] == 0
    assert rc_receiver["received_delegated_rc"] == 0
    assert rc_delegator["delegated_rc"] == 0
    assert rc_delegator["received_delegated_rc"] == 0
    assert rc_receiver2["delegated_rc"] == 0
    assert rc_receiver2["received_delegated_rc"] == 0

    print(f"Delegating rc to {receiver} and {receiver2}")
    transaction = wallet.api.delegate_rc(delegator, [receiver, receiver2], DELEGATION, "true")

    rc_receiver = wallet.api.find_rc_accounts([receiver])[0]
    rc_receiver2 = wallet.api.find_rc_accounts([receiver2])[0]
    rc_delegator = wallet.api.find_rc_accounts([delegator])[0]

    assert rc_receiver["delegated_rc"] == 0
    assert rc_receiver["received_delegated_rc"] == DELEGATION
    assert rc_receiver2["delegated_rc"] == 0
    assert rc_receiver2["received_delegated_rc"] == DELEGATION
    assert rc_delegator["delegated_rc"] == 2 * DELEGATION
    assert rc_delegator["received_delegated_rc"] == 0
    # both delegations are created at once, so the delegator parts with two of them
    delegator_account.rc_manabar.assert_rc_current_mana_is_reduced(transaction, additional_rc_cost=2 * DELEGATION)
    delegator_account.rc_manabar.update()

    print("testing list direct delegations api")

    delegation_from_to = wallet.api.list_rc_direct_delegations([delegator, receiver], 1000)[0]

    assert delegation_from_to["from"] == delegator
    assert delegation_from_to["to"] == receiver
    assert delegation_from_to["delegated_rc"] == DELEGATION

    print(f"Increasing the delegation to {5 * DELEGATION} to {receiver}")
    transaction = wallet.api.delegate_rc(delegator, [receiver], 5 * DELEGATION, "true")

    rc_receiver = wallet.api.find_rc_accounts([receiver])[0]
    rc_delegator = wallet.api.find_rc_accounts([delegator])[0]

    assert rc_receiver["delegated_rc"] == 0
    assert rc_receiver["received_delegated_rc"] == 5 * DELEGATION
    assert rc_delegator["delegated_rc"] == 6 * DELEGATION
    assert rc_delegator["received_delegated_rc"] == 0
    # the delegation to receiver grows from one to five, so four more leave the delegator
    delegator_account.rc_manabar.assert_rc_current_mana_is_reduced(transaction, additional_rc_cost=4 * DELEGATION)
    delegator_account.rc_manabar.update()

    print(f"Reducing the delegation to {2 * DELEGATION} to {receiver}")
    transaction = wallet.api.delegate_rc(delegator, [receiver], 2 * DELEGATION, "true")

    rc_receiver = wallet.api.find_rc_accounts([receiver])[0]
    rc_delegator = wallet.api.find_rc_accounts([delegator])[0]

    assert rc_receiver["delegated_rc"] == 0
    assert rc_receiver["received_delegated_rc"] == 2 * DELEGATION
    assert rc_delegator["delegated_rc"] == 3 * DELEGATION
    assert rc_delegator["received_delegated_rc"] == 0
    # reducing a delegation does not give the current rc back, so only the operation itself is paid for
    delegator_account.rc_manabar.assert_rc_current_mana_is_reduced(transaction)
    delegator_account.rc_manabar.update()

    print(f"deleting the delegation to {receiver}")
    wallet.api.delegate_rc(delegator, [receiver], 0, "true")

    rc_receiver = wallet.api.find_rc_accounts([receiver])[0]
    rc_delegator = wallet.api.find_rc_accounts([delegator])[0]
    delegation = wallet.api.list_rc_direct_delegations([delegator, receiver], 1000)

    assert rc_receiver["delegated_rc"] == 0
    assert len(delegation) == 1
    assert rc_receiver["received_delegated_rc"] == 0
    assert rc_delegator["delegated_rc"] == DELEGATION
    assert rc_delegator["received_delegated_rc"] == 0
    assert rc_delegator["max_rc"] == rc_delegator_before["max_rc"] - DELEGATION

    print("testing list_rc_accounts")
    accounts = wallet.api.list_rc_accounts("delegator", 3)
    rc_delegator = wallet.api.find_rc_accounts([delegator])[0]
    assert len(accounts) == 3
    assert accounts[0]["account"] == "delegator"
    assert accounts[0]["rc_manabar"]["current_mana"] == rc_delegator["rc_manabar"]["current_mana"]
    assert accounts[0]["rc_manabar"]["last_update_time"] == rc_delegator["rc_manabar"]["last_update_time"]
    assert accounts[0]["max_rc"] == rc_delegator["max_rc"]
    assert accounts[0]["delegated_rc"] == DELEGATION
    assert accounts[0]["received_delegated_rc"] == 0
    assert accounts[1]["account"] == "hive.fund"

    accounts_all = wallet.api.list_rc_accounts("aaa", 100)
    assert (
        len(accounts_all) == 10
    )  # miners, null, steem.dao, hive.fund, temp, initminer, steem, delegator, receiver, zzz

    accounts_offset = wallet.api.list_rc_accounts("receiver", 3)
    assert len(accounts_offset) == 3
    assert accounts_offset[0]["account"] == "receiver"
    assert accounts_offset[0]["rc_manabar"]["current_mana"] == receiver_base_mana
    assert accounts_offset[1]["account"] == "steem"
    assert accounts_offset[2]["account"] == "steem.dao"
