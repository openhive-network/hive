import pytest

import test_tools as tt

from .local_tools import as_string, run_for


# Testing is only 'post' because it is the only reward fund in HF17
@run_for('testnet', 'mainnet_64m')
def test_get_reward_fund_with_correct_value(prepared_node):
    prepared_node.api.wallet_bridge.get_reward_fund('post')


INCORRECT_VALUES = [
    'command',
    'post0',
    'post1',
    'post2',
    '',
    100,
    True,
]


@pytest.mark.parametrize(
    'reward_fund_name', [
        *INCORRECT_VALUES,
        *as_string(INCORRECT_VALUES),
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_reward_fund_with_incorrect_value(prepared_node, reward_fund_name):
    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.get_reward_fund(reward_fund_name)


@pytest.mark.parametrize(
    'reward_fund_name', [
        ['post']
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_reward_fund_with_incorrect_type_of_argument(prepared_node, reward_fund_name):
    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.get_reward_fund(reward_fund_name)
