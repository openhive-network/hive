import pytest

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.api.message_format import as_string

ACCOUNTS = [f'account-{i}' for i in range(10)]

CORRECT_VALUES = [
    ACCOUNTS[0],
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
@run_for("testnet")
def test_get_account_correct_value_testnet(node, account):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_accounts(len(ACCOUNTS))
    node.api.wallet_bridge.get_account(account)


@pytest.mark.parametrize(
    'account', [
        ['example_array']
    ]
)
@run_for("testnet")
def test_get_account_incorrect_type_of_argument(node, account):
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.get_account(account)
