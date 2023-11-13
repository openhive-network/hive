from __future__ import annotations

import test_tools as tt


def logging_comment_manabar(node: tt.InitNode, name: str) -> None:
    acc_obj = node.api.database.find_accounts(accounts=[name])["accounts"][0]

    post_voting_power = int(acc_obj["post_voting_power"]["amount"])
    voting_current_mana = acc_obj["voting_manabar"]["current_mana"]

    downvote_pool_percent = node.api.database.get_dynamic_global_properties()["downvote_pool_percent"] / 10000
    downvote_max_mana = int(post_voting_power * downvote_pool_percent)
    downvote_current_mana = acc_obj["downvote_manabar"]["current_mana"]

    tt.logger.info(
        f"Voting mana: {voting_current_mana} ( {voting_current_mana / post_voting_power * 100}% ), "
        f"Downvote mana: {downvote_current_mana} ( {downvote_current_mana / downvote_max_mana * 100}% )."
    )


def get_rshares(node: tt.InitNode, permlink: str) -> int:
    vop = node.api.account_history.get_account_history(
        account="alice", include_reversible=True, start=-1, limit=1, operation_filter_high=256
    )["history"][0][1]["op"]

    assert vop["type"] == "effective_comment_vote_operation"
    assert vop["value"]["permlink"] == permlink
    return vop["value"]["rshares"]
