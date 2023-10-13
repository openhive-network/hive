"""Tests - operation in Hive - account witness vote operation - https://gitlab.syncad.com/hive/hive/-/issues/494"""
from __future__ import annotations

import pytest

import test_tools as tt
from hive_local_tools.constants import HIVE_GOVERNANCE_VOTE_EXPIRATION_PERIOD
from hive_local_tools.functional.python.operation import jump_to_date
from hive_local_tools.functional.python.operation.witness_vote import WitnessVote


@pytest.mark.testnet()
def test_witness_vote(node_with_custom_witnesses, wallet, voter):
    """User votes for the witness X and witness Y."""
    vote_for_witness_x = WitnessVote(node_with_custom_witnesses, wallet, voter.name, "witness-x")
    voter.rc_manabar.assert_rc_current_mana_is_reduced(
        operation_rc_cost=vote_for_witness_x.rc_cost, operation_timestamp=vote_for_witness_x.timestamp
    )
    assert vote_for_witness_x.get_number_of_votes() == 1
    voter.update_account_info()

    vote_for_witness_y = WitnessVote(node_with_custom_witnesses, wallet, voter.name, "witness-y")
    voter.rc_manabar.assert_rc_current_mana_is_reduced(
        operation_rc_cost=vote_for_witness_y.rc_cost, operation_timestamp=vote_for_witness_y.timestamp
    )
    assert vote_for_witness_y.get_number_of_votes() == 1
    assert vote_for_witness_x.get_number_of_votes() == 1


@pytest.mark.testnet()
def test_remove_vote_for_witness(node_with_custom_witnesses, wallet, voter):
    """User removes the vote."""
    vote_for_witness_x = WitnessVote(node_with_custom_witnesses, wallet, voter.name, "witness-x")
    voter.rc_manabar.assert_rc_current_mana_is_reduced(
        operation_rc_cost=vote_for_witness_x.rc_cost, operation_timestamp=vote_for_witness_x.timestamp
    )
    assert vote_for_witness_x.get_number_of_votes() == 1
    voter.update_account_info()

    vote_for_witness_y = WitnessVote(node_with_custom_witnesses, wallet, voter.name, "witness-y")
    voter.rc_manabar.assert_rc_current_mana_is_reduced(
        operation_rc_cost=vote_for_witness_y.rc_cost, operation_timestamp=vote_for_witness_y.timestamp
    )
    voter.update_account_info()
    assert vote_for_witness_y.get_number_of_votes() == 1
    assert vote_for_witness_x.get_number_of_votes() == 1

    vote_for_witness_x.delete_vote()
    voter.rc_manabar.assert_rc_current_mana_is_reduced(
        operation_rc_cost=vote_for_witness_x.rc_cost, operation_timestamp=vote_for_witness_x.timestamp
    )
    assert vote_for_witness_x.get_number_of_votes() == 0
    assert vote_for_witness_y.get_number_of_votes() == 1


@pytest.mark.testnet()
def test_vote_for_witnesses_and_wait_for_its_expiration(node_with_custom_witnesses, wallet, voter):
    """User hasn't voted for a HIVE_GOVERNANCE_VOTE_EXPIRATION_PERIOD"""
    vote_for_witness_x = WitnessVote(node_with_custom_witnesses, wallet, voter.name, "witness-x")
    assert vote_for_witness_x.get_number_of_votes() == 1
    voter.rc_manabar.assert_rc_current_mana_is_reduced(
        operation_rc_cost=vote_for_witness_x.rc_cost, operation_timestamp=vote_for_witness_x.timestamp
    )
    voter.update_account_info()

    vote_for_witness_y = WitnessVote(node_with_custom_witnesses, wallet, voter.name, "witness-y")
    assert vote_for_witness_y.get_number_of_votes() == 1
    voter.rc_manabar.assert_rc_current_mana_is_reduced(
        operation_rc_cost=vote_for_witness_y.rc_cost, operation_timestamp=vote_for_witness_y.timestamp
    )
    voter.update_account_info()

    timestamp = node_with_custom_witnesses.get_head_block_time()
    jump_to_date(node_with_custom_witnesses, timestamp + tt.Time.days(HIVE_GOVERNANCE_VOTE_EXPIRATION_PERIOD))
    assert vote_for_witness_x.get_number_of_votes() == 0
    assert vote_for_witness_y.get_number_of_votes() == 0
