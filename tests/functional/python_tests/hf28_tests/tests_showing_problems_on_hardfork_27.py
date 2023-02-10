"""
This file contains tests showing abnormalities on hardfork 27, behaviors shown in these tests need to be fixed.
Each of the tests included here contains an equivalent enabled on the current hardfork has the same name without
the `_on_hf_27` suffix. e.g:
    test_decline_voting_rights_more_than_once_on_hf_27 -> test showing an abnormality
    test_decline_voting_rights_more_than_once -> should pass after repair

Tests enabled on the current hardfork are omitted. After the repairs, the `@pytest.mark.skip` marker should be removed
Related issue: https://gitlab.syncad.com/hive/hive/-/issues/441
"""
from hive_local_tools import run_for
from hive_local_tools.constants import TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS
from hive_local_tools.functional.python.hf28 import create_proposal
from hive_local_tools.functional.python.hf28.constants import VOTER_ACCOUNT


@run_for("testnet")
def test_decline_voting_rights_more_than_once_on_hf_27(prepare_environment_on_hf_27):
    node, wallet = prepare_environment_on_hf_27

    wallet.api.decline_voting_rights(VOTER_ACCOUNT, True)
    node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)
    assert node.api.database.find_accounts(accounts=[VOTER_ACCOUNT])['accounts'][0]["can_vote"] == False

    wallet.api.decline_voting_rights(VOTER_ACCOUNT, True)


@run_for("testnet")
def test_if_proposal_votes_were_removed_after_declining_voting_rights_on_hf_27(prepare_environment_on_hf_27):
    node, wallet = prepare_environment_on_hf_27
    create_proposal(wallet)

    # vote for proposal -> decline voting rights -> wait 30 days (60s in testnet) for approval decline voting rights
    wallet.api.update_proposal_votes(VOTER_ACCOUNT, [0], True)
    wallet.api.decline_voting_rights(VOTER_ACCOUNT, True)
    node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)

    assert len(node.api.condenser.list_proposal_votes([""], 1000, "by_voter_proposal", "ascending", "all")) == 1


@run_for("testnet")
def test_vote_for_proposal_from_account_that_has_declined_its_voting_rights_on_hf_27(prepare_environment_on_hf_27):
    node, wallet = prepare_environment_on_hf_27
    create_proposal(wallet)

    # decline voting rights -> wait 30 days (60s in testnet) for approval decline voting rights -> vote for proposal
    wallet.api.decline_voting_rights(VOTER_ACCOUNT, True)
    node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)

    wallet.api.update_proposal_votes(VOTER_ACCOUNT, [0], True)


@run_for("testnet")
def test_vote_for_proposal_when_decline_voting_rights_request_is_being_executed_on_hf_27(prepare_environment_on_hf_27):
    node, wallet = prepare_environment_on_hf_27
    create_proposal(wallet)

    # decline voting rights -> vote for proposal (before approving the waiver of voting rights) -> wait remaining time
    head_block_number = wallet.api.decline_voting_rights(VOTER_ACCOUNT, True)["block_num"]
    node.wait_for_block_with_number(head_block_number + (TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS // 2))
    wallet.api.update_proposal_votes(VOTER_ACCOUNT, [0], True)
    node.wait_for_block_with_number(head_block_number + TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)

    assert len(node.api.condenser.list_proposal_votes([""], 1000, "by_voter_proposal", "ascending", "all")) == 1
