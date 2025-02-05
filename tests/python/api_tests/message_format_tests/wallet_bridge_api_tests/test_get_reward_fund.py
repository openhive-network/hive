from __future__ import annotations

from typing import TYPE_CHECKING

import pytest
from helpy.exceptions import ErrorInResponseError

from hive_local_tools import run_for
from hive_local_tools.api.message_format import as_string

if TYPE_CHECKING:
    import test_tools as tt


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
    with pytest.raises(ErrorInResponseError):
        node.api.wallet_bridge.get_reward_fund(reward_fund_name)


@pytest.mark.parametrize("reward_fund_name", [["post"]])
@run_for("testnet", "live_mainnet")
def test_get_reward_fund_with_incorrect_type_of_argument(
    node: tt.InitNode | tt.RemoteNode, reward_fund_name: list
) -> None:
    with pytest.raises(ErrorInResponseError):
        node.api.wallet_bridge.get_reward_fund(reward_fund_name)
