from __future__ import annotations

import operator
from typing import TYPE_CHECKING

import pytest

import test_tools as tt

from .block_log.generate_block_log import NUMBER_OF_REPLIES_TO_POST

if TYPE_CHECKING:
    from .conftest import Account


@pytest.mark.parametrize("hardfork_version", [27])
@pytest.mark.parametrize(("weight", "operator_"), [(100, operator.gt), (-100, operator.lt)])
def test_voting_power_of_a_comment_on_hf27(
    prepare_environment: tuple[tt.InitNode, tt.Wallet],
    hardfork_version: str,
    weight: int,
    operator_: operator,
    alice: Account,
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
    voting_mana_history = []
    downvoting_mana_history = []
    vote_absorbing_both_mana_bars = 13

    node, wallet = prepare_environment

    for comment_num in range(NUMBER_OF_REPLIES_TO_POST):
        wallet.api.vote(alice.name, "bob", f"comment-{comment_num}", weight)
        alice.update_account_info()
        voting_mana_history.append(alice.vote_manabar.current_mana)
        downvoting_mana_history.append(alice.downvote_manabar.current_mana)
        logging_comment_manabar(node, alice.name)

    rshares_history = get_rshares_history(node)

    for num, (rshares, voting_mana, downvote_mana) in enumerate(
        zip(rshares_history, voting_mana_history, downvoting_mana_history)
    ):
        previous_rshares_value = rshares_history[num - 1]
        previous_voting_mana_value = voting_mana_history[num - 1]
        previous_downvoting_mana_value = downvoting_mana_history[num - 1]
        if num == 0:
            continue

        if num < vote_absorbing_both_mana_bars and weight < 0:
            assert rshares == previous_rshares_value, "rshares should not change during the first 13 downvotes."
            assert downvote_mana < previous_downvoting_mana_value, "The account did not incur a downvote mana cost."
        else:
            err = f"comment-{num} should {previous_rshares_value} should be {operator_.__name__} {rshares}"
            assert operator_(previous_rshares_value, rshares), err
            assert downvote_mana == previous_downvoting_mana_value, "Downvote mana should not change at this stage."
            assert voting_mana < previous_voting_mana_value, "The account did not incur a vote mana cost."


@pytest.mark.parametrize("hardfork_version", ["current"])
@pytest.mark.parametrize("weight", [100, -100])
def test_voting_power_of_a_comment_on_current_hardfork(
    prepare_environment: tuple[tt.InitNode, tt.Wallet],
    hardfork_version: str,
    weight: int,
    alice: Account,
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

    voting_mana_history = []
    downvoting_mana_history = []
    for comment_num in range(NUMBER_OF_REPLIES_TO_POST):
        try:
            wallet.api.vote(alice.name, "bob", f"comment-{comment_num}", weight)
        except tt.exceptions.CommunicationError as error:
            message = str(error)
            assert "Account does not have enough mana" in message, "Not found expected error message."

            rshares_history = get_rshares_history(node)
            assert all(rs == rshares_history[0] for rs in rshares_history), "Votes should have the same power value."

            for num, (voting_mana, downvote) in enumerate(zip(voting_mana_history, downvoting_mana_history)):
                if num == 0:
                    continue

                previous_voting_mana_value = voting_mana_history[num - 1]
                previous_downvoting_mana_value = downvoting_mana_history[num - 1]

                if weight < 0:
                    assert comment_num == 62, "Incorrect maximum number of votes and downvotes made with maximum power."
                    if num < 12:
                        assert downvote < previous_downvoting_mana_value, "Downvote mana did not change after voting."
                        err_msg = "Upvote mana decreased unexpectedly during downvote."
                        assert voting_mana == alice.rc_manabar.max_mana, err_msg
                    else:
                        err_msg = "When downvoting with 0% downvote mana, the upvote mana was not correctly retrieved."
                        assert voting_mana < previous_voting_mana_value, err_msg
                else:
                    assert comment_num == 50, "Incorrect maximum number of votes made with maximum power."
                    assert voting_mana < previous_voting_mana_value, "Upvote mana did not change after voting."
                    err_msg = "Downvote mana changed unexpectedly during upvote."
                    assert all(d_mana == downvoting_mana_history[0] for d_mana in downvoting_mana_history), err_msg
            return

        alice.update_account_info()
        voting_mana_history.append(alice.vote_manabar.current_mana)
        downvoting_mana_history.append(alice.downvote_manabar.current_mana)

        tt.logger.info(f"Vote for comment-{comment_num}")
        logging_comment_manabar(node, alice.name)

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


def get_rshares_history(node: tt.InitNode) -> list[int]:
    return [
        o[1].op.value.rshares
        for o in node.api.account_history.get_account_history(
            account="alice",
            include_reversible=True,
            start=-1,
            limit=1000,
            operation_filter_high=256,
        ).history
    ]
