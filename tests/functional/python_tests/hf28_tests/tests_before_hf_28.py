"""
Tests showing bugs. Each test has an equivalent after fix - there is currently a skip.
After the repair, remove the @pytest.mark.skip marks.
"""
from hive_local_tools import run_for
from hive_local_tools.functional.python.hf28 import create_proposal
from hive_local_tools.functional.python.hf28.constants import VOTER_ACCOUNT, TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS


@run_for("testnet")
def test_if_proposal_votes_were_removed_after_declining_voting_rights_before_hf_28(prepare_environment):
    node, wallet = prepare_environment
    create_proposal(wallet)

    # vote for proposal -> decline voting rights -> wait 30 days (60s in testnet) for approval decline voting rights
    wallet.api.update_proposal_votes(VOTER_ACCOUNT, [0], True)
    wallet.api.decline_voting_rights(VOTER_ACCOUNT, True)
    node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)

    assert len(node.api.condenser.list_proposal_votes([""], 1000, "by_voter_proposal", "ascending", "all")) == 1


@run_for("testnet")
def test_update_proposal_votes_with_the_account_that_has_declining_voting_rights_before_hf_28(prepare_environment):
    node, wallet = prepare_environment
    create_proposal(wallet)

    # decline voting rights -> wait 30 days (60s in testnet) for approval decline voting rights -> vote for proposal
    wallet.api.decline_voting_rights(VOTER_ACCOUNT, True)
    node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)

    wallet.api.update_proposal_votes(VOTER_ACCOUNT, [0], True)


@run_for("testnet")
def test_vote_for_proposal_when_decline_voting_rights_is_being_executed_before_before_hf_28(prepare_environment):
    node, wallet = prepare_environment
    create_proposal(wallet)

    # decline voting rights -> vote for proposal (before approving the waiver of voting rights) -> wait remaining time
    head_block_number = wallet.api.decline_voting_rights(VOTER_ACCOUNT, True)["block_num"]
    node.wait_for_block_with_number(head_block_number + (TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS // 2))
    wallet.api.update_proposal_votes(VOTER_ACCOUNT, [0], True)
    node.wait_for_block_with_number(head_block_number + TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)

    assert len(node.api.condenser.list_proposal_votes([""], 1000, "by_voter_proposal", "ascending", "all")) == 1


@run_for("testnet")
def test_declining_voting_rights_more_than_once_before_hf_28(prepare_environment):
    node, wallet = prepare_environment

    wallet.api.decline_voting_rights(VOTER_ACCOUNT, True)
    node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)
    assert node.api.database.find_accounts(accounts=[VOTER_ACCOUNT])['accounts'][0]["can_vote"] == False

    wallet.api.decline_voting_rights(VOTER_ACCOUNT, True)
