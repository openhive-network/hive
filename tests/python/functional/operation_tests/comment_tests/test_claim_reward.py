from __future__ import annotations

import datetime
from typing import Literal

import pytest

import test_tools as tt
from hive_local_tools.functional.python.operation import (
    get_reward_hbd_balance,
    get_reward_hive_balance,
    get_reward_vesting_balance,
    publish_feeds_with_confirmation,
)
from hive_local_tools.functional.python.operation.comment import Comment, CommentAccount, Vote


@pytest.fixture()
def node():
    witnesses_required_for_hf06_and_later = [f"witness-{witness_num}" for witness_num in range(20)]

    init_node = tt.InitNode()

    for witness in witnesses_required_for_hf06_and_later:
        init_node.config.witness.append(witness)

    init_node.config.plugin.append("account_history_api")

    init_node.run(
        time_control=tt.SpeedUpRateTimeControl(speed_up_rate=10),
        alternate_chain_specs=tt.AlternateChainSpecs(
            genesis_time=int(tt.Time.now(serialize=False).timestamp()),
            hardfork_schedule=[tt.HardforkSchedule(hardfork=28, block_num=1)],
            init_witnesses=witnesses_required_for_hf06_and_later,
            hbd_init_supply=100000000000,
            init_supply=20000000000,
            initial_vesting=tt.InitialVesting(hive_amount=10000000000, vests_per_hive=1800),
        ),
    )
    init_node.wait_number_of_blocks(100)

    return init_node


@pytest.fixture()
def wallet(node):
    return tt.Wallet(attach_to=node)


def prepare_account_with_reward_balance(
    node: tt.InitNode, wallet: tt.Wallet, balance_assets: Literal["hive, hbd, vest", "vest"]
) -> CommentAccount:
    comment_0 = Comment(node, wallet)

    if balance_assets == "vest":
        comment_0.send(reply_type="no_reply", percent_hbd=0)
    elif balance_assets == "hive, hbd, vest":
        comment_0.send(reply_type="no_reply")

    vote_0 = Vote(comment_0, "random")
    vote_0.vote(100)

    start_time = node.get_head_block_time() + datetime.timedelta(seconds=40 * 60)
    node.restart(time_control=tt.StartTimeControl(start_time=start_time, speed_up_rate=5))
    time_before_publish_feed = node.get_head_block_time()
    publish_feeds_with_confirmation(node, wallet, 1, 4)

    start_time = time_before_publish_feed + datetime.timedelta(seconds=20 * 60)
    node.restart(time_control=tt.StartTimeControl(start_time=start_time, speed_up_rate=5))

    assert get_reward_hbd_balance(node, comment_0.author) == tt.Asset.Tbd(0)
    assert get_reward_vesting_balance(node, comment_0.author) != tt.Asset.Vest(0)

    if balance_assets == "vest":
        assert get_reward_hive_balance(node, comment_0.author) == tt.Asset.Hive(0)
        return comment_0.author_obj

    assert get_reward_hive_balance(node, comment_0.author) != tt.Asset.Hive(0)

    comment_1 = Comment(node, wallet, parent=comment_0)
    comment_1.send(reply_type="reply_own_comment")
    vote_1 = Vote(comment_1, "random")
    vote_1.vote(90)

    start_time = node.get_head_block_time() + datetime.timedelta(seconds=40 * 60)
    node.restart(time_control=tt.StartTimeControl(start_time=start_time, speed_up_rate=5))

    # To receive a reward in HIVE instead of HBD, you must publish new feed and therefore change the median price.
    time_before_publish_feed = node.get_head_block_time()
    publish_feeds_with_confirmation(node, wallet, 25, 1)

    start_time = time_before_publish_feed + datetime.timedelta(seconds=20 * 60)
    node.restart(time_control=tt.StartTimeControl(start_time=start_time))

    assert get_reward_hive_balance(node, comment_0.author) != tt.Asset.Hive(0)
    assert get_reward_hbd_balance(node, comment_0.author) != tt.Asset.Tbd(0)
    assert get_reward_vesting_balance(node, comment_0.author) != tt.Asset.Vest(0)
    return comment_0.author_obj


@pytest.mark.parametrize(
    "verifying_reward_asset",
    [
        ("reward_hive",),
        ("reward_hbd",),
        ("reward_hive", "reward_hbd"),
        ("reward_hive", "reward_hbd", "reward_vests"),
    ],
)
def test_claim_all_calculated_reward(node: tt.InitNode, wallet: tt.Wallet, verifying_reward_asset: tuple):
    """
    Test case 1, 2, 7, 9 from issue: https://gitlab.syncad.com/hive/hive/-/issues/518
    """
    account = prepare_account_with_reward_balance(node, wallet, "hive, hbd, vest")

    rewards = {}
    for reward_mode in verifying_reward_asset:
        rewards[reward_mode] = account.get_reward_balance(reward_mode)

    account.claim_reward_balance(**rewards)

    account.assert_balance_after_claiming_available_rewards()
    account.assert_reward_balance_after_claiming_available_rewards()
    account.assert_is_rc_mana_decreased_after_claiming_available_rewards()


@pytest.mark.parametrize("reward_vests", [tt.Asset.Vest(10), "all"])
def test_claim_all_calculated_vests_reward(node, wallet, reward_vests):
    """
    Test case 3, 6 from issue: https://gitlab.syncad.com/hive/hive/-/issues/518
    """
    account = prepare_account_with_reward_balance(node, wallet, "vest")

    account.claim_reward_balance(reward_vests=reward_vests)

    account.assert_balance_after_claiming_available_rewards()
    account.assert_reward_balance_after_claiming_available_rewards()
    account.assert_is_rc_mana_decreased_after_claiming_available_rewards()
    account.assert_governance_vote_power_increase_after_claiming_available_rewards()


@pytest.mark.parametrize(
    ("hive", "hbd", "vests"),
    [
        (tt.Asset.Test(1), tt.Asset.Tbd(0), tt.Asset.Vest(0)),
        (tt.Asset.Test(0), tt.Asset.Tbd(1), tt.Asset.Vest(0)),
        (tt.Asset.Test(1), tt.Asset.Tbd(1), tt.Asset.Vest(0)),
        (tt.Asset.Test(1), tt.Asset.Tbd(1), tt.Asset.Vest(1)),
    ],
)
def test_claim_part_of_calculated_reward(node, wallet, hive, hbd, vests):
    """
    Test case 4, 5, 8, 10 from issue: https://gitlab.syncad.com/hive/hive/-/issues/518
    """
    account = prepare_account_with_reward_balance(node, wallet, "hive, hbd, vest")
    account.claim_reward_balance(hive, hbd, vests)

    account.assert_balance_after_claiming_available_rewards()
    account.assert_reward_balance_after_claiming_available_rewards()
    account.assert_is_rc_mana_decreased_after_claiming_available_rewards()
