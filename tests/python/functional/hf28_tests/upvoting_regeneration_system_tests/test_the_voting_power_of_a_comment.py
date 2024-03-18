from __future__ import annotations

import operator

import pytest

import test_tools as tt

from .block_log.generate_block_log import NUMBER_OF_REPLIES_TO_POST


@pytest.mark.parametrize("hardfork_version", [27])
@pytest.mark.parametrize(("weight", "op"), [(100, [operator.gt, operator.gt]), (-100, [operator.eq, operator.lt])])
def test_voting_power_of_a_comment_on_hf27(
    prepare_environment: tuple[tt.InitNode, tt.Wallet], hardfork_version: str, weight: int, op: list[operator]
) -> None:
    """
    Behavior on hardfork 27:
    Upvote comment:
    - The value of the comment voting power `rshares` is calculated based on the current_mana of voting_manabar,
    - 100% current_mana of voting_manabar value = 100% comment voting power value,
    - Reducing current_mana results in decreasing comment voting power (rshares).
    Downvote comment:
    - current_mana of downvote_manabar is enough for a full power vote thirteen times (current_mana of voting_manabar are full),
    - vote (13) using both downvote_manabar and voting_manabar, is also of maximum power of vote, but current_mana of voting_manabar are reduced
    - Each subsequent new vote has a lower voting power (rshares).
    """
    node, wallet = prepare_environment

    vote_absorbing_both_mana_bars = 13
    previous_vote_power = 0
    for comment_num in range(NUMBER_OF_REPLIES_TO_POST):
        wallet.api.vote("alice", "bob", f"comment-{comment_num}", weight)
        logging_comment_manabar(node, "alice")

        if comment_num == 0:
            previous_vote_power = get_rshares(node, f"comment-{comment_num}")
            continue

        operator_ = op[0] if comment_num < vote_absorbing_both_mana_bars else op[1]
        actual_vote_power = get_rshares(node, f"comment-{comment_num}")

        err = f"comment-{comment_num} should {previous_vote_power=} should be {operator_.__name__} {actual_vote_power=}"
        assert operator_(previous_vote_power, actual_vote_power), err

        previous_vote_power = actual_vote_power


@pytest.mark.parametrize("hardfork_version", ["current"])
@pytest.mark.parametrize("weight", [100, -100])
@pytest.mark.parametrize("bonus_iteration", list(range(100)))
def test_voting_power_of_a_comment_on_current_hardfork(
    prepare_environment: tuple[tt.InitNode, tt.Wallet], hardfork_version: str, weight: int, bonus_iteration: int
) -> None:
    """
    Behavior on hardfork 28+:
    Upvote comment:
    - Account can cast valid comment votes as long as current_mana of voting_manabar is available,
    - Comment voting power is constant regardless of the value of current_mana of voting_manabar
    Downvote comment:
    - Account can downvote for comment as long as current_mana of voting_manabar and downvote_manabar is available,
    - Comment voting power is constant regardless of the value of current_mana of voting_manabar.
    """
    node, wallet = prepare_environment

    maximum_vote_power = None
    for comment_num in range(NUMBER_OF_REPLIES_TO_POST):
        try:
            vote = wallet.api.vote("alice", "bob", f"comment-{comment_num}", weight)
            tt.logger.info(f"Vote-{comment_num} - entered the block: {vote} ")
            get_vote_from_account_history_api(node, comment_num)
        except tt.exceptions.CommunicationError as error:
            message = str(error)
            assert "Account does not have enough mana" in message, "Not found expected error message."
            return

        actual_vote_power = get_rshares(node, f"comment-{comment_num}")
        tt.logger.info(f"Vote for comment-{comment_num}")
        logging_comment_manabar(node, "alice")
        if comment_num == 0:
            maximum_vote_power = actual_vote_power
            continue

        assert maximum_vote_power == actual_vote_power, (
            "The account cast a comment vote with a power other than the maximum. "
            f"Maximum vote power: {maximum_vote_power}, current_vote_power: {actual_vote_power}"
        )
    raise AssertionError("Account `alice` mana has been not exhausted.")


def logging_comment_manabar(node: tt.InitNode, name: str) -> None:
    acc_obj = node.api.database.find_accounts(accounts=[name]).accounts[0]

    post_voting_power = int(acc_obj.post_voting_power.amount)
    voting_current_mana = acc_obj.voting_manabar.current_mana

    downvote_pool_percent = node.api.database.get_dynamic_global_properties().downvote_pool_percent / 10000
    downvote_max_mana = int(post_voting_power * downvote_pool_percent)
    downvote_current_mana = acc_obj.downvote_manabar.current_mana

    tt.logger.info(
        f"Voting mana: {voting_current_mana} ( {voting_current_mana / post_voting_power * 100}% ), "
        f"Downvote mana: {downvote_current_mana} ( {downvote_current_mana / downvote_max_mana * 100}% )."
    )


def get_vote_from_account_history_api(node: tt.InitNode, comment_num):
    vote = node.api.account_history.get_account_history(
        account="alice", include_reversible=True, start=-1, limit=1000, operation_filter_low=1
    )
    assert vote.history[-1][1].op.value.permlink == f"comment-{comment_num}"
    assert len(vote.history) == comment_num + 1
    tt.logger.info(f"Comment-{comment_num} are located in account history.")


def get_rshares(node: tt.InitNode, permlink: str) -> int:
    vop = (
        node.api.account_history.get_account_history(
            account="alice", include_reversible=True, start=-1, limit=1, operation_filter_high=256
        )
        .history[0][1]
        .op
    )

    assert vop.type == "effective_comment_vote_operation"
    assert vop.value.permlink == permlink
    return vop.value.rshares
