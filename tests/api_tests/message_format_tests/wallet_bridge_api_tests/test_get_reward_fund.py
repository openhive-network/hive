import pytest

import test_tools as tt
from hive_local_tools.api.message_format import as_string


def test_get_reward_fund_with_correct_value(node):
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
def test_get_reward_fund_with_incorrect_value(node, reward_fund_name):
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.get_reward_fund(reward_fund_name)


@pytest.mark.parametrize("reward_fund_name", [["post"]])
def test_get_reward_fund_with_incorrect_type_of_argument(node, reward_fund_name):
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.get_reward_fund(reward_fund_name)
