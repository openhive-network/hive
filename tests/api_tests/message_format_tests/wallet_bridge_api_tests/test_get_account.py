import pytest

import test_tools as tt

from .local_tools import as_string, run_for


ACCOUNT = 'initminer'

CORRECT_VALUES = [
    ACCOUNT,
    'non-exist-acc',
    '',
    100,
    True,
]


@pytest.mark.parametrize(
    'account', [
        *CORRECT_VALUES,
        *as_string(CORRECT_VALUES),
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_account_correct_value(prepared_node, should_prepare, account):
    prepared_node.api.wallet_bridge.get_account(account)


@pytest.mark.parametrize(
    'account', [
        ['example_array']
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_account_incorrect_type_of_argument(prepared_node, account):
    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.get_account(account)
