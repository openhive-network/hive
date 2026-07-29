from __future__ import annotations

import pytest

import test_tools as tt

# Both amounts have to clear the chain's min_delegation - (account_creation_fee / 3) * vesting_share_price,
# see libraries/chain/rc/rc_utility.cpp - which is around 3600 RC on a default testnet. They are kept several
# orders of magnitude above it, so that a change of the fee or of the vest price cannot quietly invalidate
# them. The two differ only so that the delegations can be told apart in the listed results.
BOB_DELEGATION = 100_000
CAROL_DELEGATION = 200_000

TO_BOB = {"from": "alice", "to": "bob", "delegated_rc": BOB_DELEGATION}
TO_CAROL = {"from": "alice", "to": "carol", "delegated_rc": CAROL_DELEGATION}


@pytest.mark.parametrize(
    ("from_", "to", "expected_delegations"),
    [
        ("alice", "bob", [TO_BOB, TO_CAROL]),
        ("alice", "carol", [TO_CAROL]),
        # Empty 'to' parameter allow list all delegations from specific account.
        ("alice", "", [TO_BOB, TO_CAROL]),
        # Alice as 'to' parameter allow list delegations to bob and carol, because id of alice is less then id
        # bob's and carol's.
        ("alice", "alice", [TO_BOB, TO_CAROL]),
        # Initminer as 'to' parameter allow list delegations to bob and carol, because id of initminer is less
        # then id bob's and carol's (initminer was created first).
        ("alice", "initminer", [TO_BOB, TO_CAROL]),
        # This case isn't return any delegations, because id of dan is bigger then id bob's and carol's.
        ("alice", "dan", []),
        # Those cases aren't return any delegations, because weren't any delegations from bob, carol and initminer.
        ("bob", "alice", []),
        ("bob", "bob", []),
        ("bob", "carol", []),
        ("bob", "dan", []),
        ("bob", "initminer", []),
        ("carol", "alice", []),
        ("dan", "alice", []),
        ("initminer", "alice", []),
    ],
)
def test_list_rc_direct_delegations(wallet: tt.Wallet, from_: str, to: str, expected_delegations: list) -> None:
    # 'to' parameter is name of account, but accounts are listing by id involved with account, NOT alphabetically
    with wallet.in_single_transaction():
        wallet.api.create_account("initminer", "alice", "{}")
        wallet.api.create_account("initminer", "bob", "{}")
        wallet.api.create_account("initminer", "carol", "{}")
        wallet.api.create_account("initminer", "dan", "{}")

    # delegatable RC comes from own vesting shares only - rc_adjustment and received delegations do not count,
    # see get_maximum_rc in account_object.hpp - so alice has to power up enough to cover both delegations
    wallet.api.transfer_to_vesting("initminer", "alice", tt.Asset.Test(10))

    with wallet.in_single_transaction():
        wallet.api.delegate_rc("alice", ["bob"], BOB_DELEGATION)
        wallet.api.delegate_rc("alice", ["carol"], CAROL_DELEGATION)

    delegations = wallet.api.list_rc_direct_delegations([from_, to], 100)
    assert len(delegations) == len(expected_delegations)

    for delegation, expected_delegation in zip(delegations, expected_delegations):
        assert delegation.dict() == expected_delegation
