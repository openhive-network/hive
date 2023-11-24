from __future__ import annotations

import pytest

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.api.message_format import as_string


@run_for("testnet", "live_mainnet")
def test_get_reward_fund_with_correct_value(node: tt.InitNode | tt.RemoteNode) -> None:
    # Testing is only 'post' because it is the only reward fund in HF26
    node.api.wallet_bridge.get_reward_fund("post")


INCORRECT_VALUES = [
    "command",
    "post0",
    "post1",
    "post2",
    "",
    100,
    True,
]


@pytest.mark.parametrize(
    "reward_fund_name",
    [
        *INCORRECT_VALUES,
        *as_string(INCORRECT_VALUES),
    ],
)
@run_for("testnet", "live_mainnet")
def test_get_reward_fund_with_incorrect_value(
    node: tt.InitNode | tt.RemoteNode, reward_fund_name: bool | int | str
) -> None:
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.get_reward_fund(reward_fund_name)


@pytest.mark.parametrize("reward_fund_name", [["post"]])
@run_for("testnet", "live_mainnet")
def test_get_reward_fund_with_incorrect_type_of_argument(
    node: tt.InitNode | tt.RemoteNode, reward_fund_name: list
) -> None:
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.get_reward_fund(reward_fund_name)
