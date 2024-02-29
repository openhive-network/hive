"""Tests account witness proxy operation - https://gitlab.syncad.com/hive/hive/-/issues/497"""
from __future__ import annotations

from typing import TYPE_CHECKING

import pytest

import test_tools as tt
from hive_local_tools.constants import HIVE_GOVERNANCE_VOTE_EXPIRATION_PERIOD
from hive_local_tools.functional.python.operation import get_transaction_timestamp, get_virtual_operations
from hive_local_tools.functional.python.operation.account_witness_proxy import AccountWitnessProxy
from schemas.operations.virtual import ExpiredAccountNotificationOperation, ProxyClearedOperation

if TYPE_CHECKING:
    from .conftest import Account, Proposal


@pytest.mark.testnet()
def test_set_account_witness_proxy(
    node: tt.InitNode, wallet: tt.Wallet, alice: Account, bob: Account, proposal_x: Proposal, proposal_y: Proposal
) -> None:
    """1. User has not voted for witness or proposal and sets proxy."""
    # There is an account A, who votes for witness X and proposal X.
    wallet.api.vote_for_witness(alice.name, "witness-x", True)
    wallet.api.update_proposal_votes(alice.name, [proposal_x.id], True)
    alice.update_account_info()
    bob.update_account_info()

    assert get_number_of_proposal_votes(node) == 1, "Vote for the proposal has not been created."
    assert len(node.api.database.list_witness_votes(start=["", ""], limit=10, order="by_account_witness").votes) == 1

    # User B sets a proxy: account A.
    set_proxy = AccountWitnessProxy(node, wallet, bob.name, alice.name)

    assert bob.get_proxy() == alice.name
    bob.rc_manabar.assert_rc_current_mana_is_reduced(set_proxy.rc_cost, set_proxy.timestamp)

    move_to_next_maintenance_time(node)
    alice.update_account_info()
    [proposal.update_proposal_info() for proposal in [proposal_x, proposal_y]]

    assert proposal_x.total_votes == alice.get_governance_vote_power() == alice.vest + bob.vest
    assert proposal_y.total_votes == tt.Asset.Vest(0)

    assert get_witness_votes(node, "witness-x") == alice.get_governance_vote_power() == alice.vest + bob.vest
    assert get_witness_votes(node, "witness-y") == tt.Asset.Vest(0)


@pytest.mark.testnet()
def test_remove_account_witness_proxy(
    node: tt.InitNode, wallet: tt.Wallet, alice: Account, bob: Account, proposal_x: Proposal, proposal_y: Proposal
) -> None:
    """2. User removes a proxy and before setting user did not vote."""
    test_set_account_witness_proxy(node, wallet, alice, bob, proposal_x, proposal_y)
    alice.update_account_info()
    bob.update_account_info()

    # User B removes a proxy: account A.
    remove_proxy = AccountWitnessProxy(node, wallet, bob.name, "")
    assert bob.get_proxy() == ""
    assert len(get_virtual_operations(node, ProxyClearedOperation)) == 1
    bob.rc_manabar.assert_rc_current_mana_is_reduced(remove_proxy.rc_cost, remove_proxy.timestamp)

    move_to_next_maintenance_time(node)
    [proposal.update_proposal_info() for proposal in [proposal_x, proposal_y]]

    alice.update_account_info()
    assert proposal_x.total_votes == alice.get_governance_vote_power() == alice.vest
    assert proposal_y.total_votes == tt.Asset.Vest(0)

    assert get_witness_votes(node, "witness-x") == alice.get_governance_vote_power() == alice.vest
    assert get_witness_votes(node, "witness-y") == tt.Asset.Vest(0)


@pytest.mark.testnet()
def test_account_witness_proxy(
    node: tt.InitNode,
    wallet: tt.Wallet,
    alice: Account,
    bob: Account,
    proposal_x: Proposal,
    proposal_y: Proposal,
    proposal_z: Proposal,
) -> None:
    """3. User voted for witness and proposal and now sets proxy."""
    wallet.api.vote_for_witness(alice.name, "witness-x", True)
    wallet.api.update_proposal_votes(alice.name, [proposal_x.id], True)
    alice.update_account_info()
    bob.update_account_info()

    # User B votes for witness-z.
    bob_vote_for_witness = wallet.api.vote_for_witness(bob.name, "witness-z", True)
    assert len(node.api.database.list_witness_votes(start=["", ""], limit=10, order="by_account_witness")["votes"]) == 2
    bob.rc_manabar.assert_rc_current_mana_is_reduced(
        operation_rc_cost=bob_vote_for_witness["rc_cost"],
        operation_timestamp=get_transaction_timestamp(node, bob_vote_for_witness),
    )
    bob.update_account_info()

    # User votes for proposal-z.
    bob_vote_for_proposal_z = wallet.api.update_proposal_votes(bob.name, [proposal_z.id], True)
    bob.rc_manabar.assert_rc_current_mana_is_reduced(
        operation_rc_cost=bob_vote_for_proposal_z["rc_cost"],
        operation_timestamp=get_transaction_timestamp(node, bob_vote_for_proposal_z),
    )

    assert get_number_of_proposal_votes(node) == 2

    move_to_next_maintenance_time(node)
    [proposal.update_proposal_info() for proposal in [proposal_x, proposal_y, proposal_z]]
    bob.update_account_info()

    assert proposal_x.total_votes == alice.get_governance_vote_power() == alice.vest
    assert proposal_y.total_votes == tt.Asset.Vest(0)
    assert proposal_z.total_votes == bob.get_governance_vote_power() == bob.vest

    # User B sets a proxy: account A.
    set_proxy = AccountWitnessProxy(node, wallet, bob.name, alice.name)

    assert bob.get_proxy() == alice.name
    bob.rc_manabar.assert_rc_current_mana_is_reduced(
        operation_rc_cost=set_proxy.rc_cost,
        operation_timestamp=set_proxy.timestamp,
    )
    move_to_next_maintenance_time(node)
    [proposal.update_proposal_info() for proposal in [proposal_x, proposal_y, proposal_z]]

    assert (
        proposal_x.total_votes
        == alice.get_governance_vote_power() + bob.get_governance_vote_power()
        == alice.vest + bob.vest
    )
    assert proposal_y.total_votes == tt.Asset.Vest(0)
    assert proposal_z.total_votes == tt.Asset.Vest(0)

    assert (
        get_witness_votes(node, "witness-x")
        == alice.get_governance_vote_power() + bob.get_governance_vote_power()
        == alice.vest + bob.vest
    )
    assert get_witness_votes(node, "witness-y") == tt.Asset.Vest(0)
    assert get_witness_votes(node, "witness-z") == tt.Asset.Vest(0)


def test_remove_proxy_with_active_votes(
    node: tt.InitNode,
    wallet: tt.Wallet,
    alice: Account,
    bob: Account,
    proposal_x: Proposal,
    proposal_y: Proposal,
    proposal_z: Proposal,
) -> None:
    """4. User removes a proxy and before setting user voted."""
    test_account_witness_proxy(node, wallet, alice, bob, proposal_x, proposal_y, proposal_z)

    # User B removes a proxy: account A.
    remove_proxy = AccountWitnessProxy(node, wallet, bob.name, "")

    assert bob.get_proxy() == ""
    assert len(get_virtual_operations(node, ProxyClearedOperation)) == 1
    bob.rc_manabar.assert_rc_current_mana_is_reduced(
        operation_rc_cost=remove_proxy.rc_cost,
        operation_timestamp=remove_proxy.timestamp,
    )

    move_to_next_maintenance_time(node)
    [proposal.update_proposal_info() for proposal in [proposal_x, proposal_y, proposal_z]]
    alice.update_account_info()
    bob.update_account_info()

    assert proposal_x.total_votes == alice.get_governance_vote_power() == alice.vest
    assert proposal_y.total_votes == tt.Asset.Vest(0)
    assert proposal_z.total_votes == bob.get_governance_vote_power() == bob.vest

    assert get_witness_votes(node, "witness-x") == alice.get_governance_vote_power() == alice.vest
    assert get_witness_votes(node, "witness-y") == tt.Asset.Vest(0)
    assert get_witness_votes(node, "witness-z") == tt.Asset.Vest(0)


def test_account_witness_proxy_after_expiration(
    node: tt.InitNode, wallet: tt.Wallet, alice: Account, bob: Account, proposal_x: Proposal, proposal_y: Proposal
) -> None:
    """5. User hasn't voted for a HIVE_GOVERNANCE_VOTE_EXPIRATION_PERIOD."""
    test_set_account_witness_proxy(node, wallet, alice, bob, proposal_x, proposal_y)

    # Shift the time by an HIVE_GOVERNANCE_VOTE_EXPIRATION_PERIOD
    node.restart(
        time_control=tt.Time.serialize(
            node.get_head_block_time() + tt.Time.seconds(HIVE_GOVERNANCE_VOTE_EXPIRATION_PERIOD),
            format_=tt.TimeFormats.TIME_OFFSET_FORMAT,
        )
    )

    move_to_next_maintenance_time(node)
    bob.update_account_info()
    [proposal.update_proposal_info() for proposal in [proposal_x, proposal_y]]

    assert bob.get_proxy() == ""
    assert len(get_virtual_operations(node, ProxyClearedOperation)) == 1
    assert len(get_virtual_operations(node, ExpiredAccountNotificationOperation)) == 2

    assert proposal_x.total_votes == tt.Asset.Vest(0)
    assert proposal_y.total_votes == tt.Asset.Vest(0)

    assert get_witness_votes(node, "witness-x") == tt.Asset.Vest(0)
    assert get_witness_votes(node, "witness-y") == tt.Asset.Vest(0)


def get_number_of_proposal_votes(node: tt.InitNode) -> int:
    return len(
        node.api.database.list_proposal_votes(
            start=[""], limit=1000, order="by_voter_proposal", order_direction="ascending", status="all"
        ).proposal_votes
    )


def get_witness_votes(node: tt.InitNode, witness_name: str) -> tt.Asset.Vest:
    return tt.Asset.from_nai(
        {
            "amount": node.api.database.list_witnesses(start=witness_name, limit=1, order="by_name").witnesses[0].votes,
            "precision": 6,
            "nai": "@@000000037",
        }
    )


def move_to_next_maintenance_time(node: tt.InitNode) -> None:
    next_maintenance_time = tt.Time.parse(node.api.database.get_dynamic_global_properties().next_maintenance_time)
    node.restart(time_control=tt.Time.serialize(next_maintenance_time, format_=tt.TimeFormats.TIME_OFFSET_FORMAT))
    assert node.get_head_block_time() >= next_maintenance_time
