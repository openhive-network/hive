import pytest

import test_tools as tt

from hive_local_tools import run_for
from hive_local_tools.api.message_format import as_string

ACCOUNT = "alice"

CORRECT_VALUES = [
    # RC ACCOUNT
    (ACCOUNT, 100),
    ("non-exist-acc", 100),
    ("true", 100),
    ("", 100),
    (100, 100),
    (True, 100),
    # LIMIT
    (ACCOUNT, 1),
    (ACCOUNT, 1000),
]


@pytest.mark.parametrize(
    "rc_account, limit",
    [
        *CORRECT_VALUES,
        *as_string(CORRECT_VALUES),
        (ACCOUNT, True),  # bool is treated like numeric (0:1)
    ],
)
@run_for("testnet")
def test_list_rc_accounts_with_correct_values(node, wallet, rc_account, limit):
    wallet.create_account(ACCOUNT)
    node.api.condenser.list_rc_accounts(rc_account, limit)


@pytest.mark.parametrize(
    "rc_account, limit",
    [
        # LIMIT
        (ACCOUNT, -1),
        (ACCOUNT, 0),
        (ACCOUNT, 1001),
    ],
)
@run_for("testnet")
def test_list_rc_accounts_with_incorrect_values(node, wallet, rc_account, limit):
    wallet.create_account(ACCOUNT)
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.condenser.list_rc_accounts(rc_account, limit)


@pytest.mark.parametrize(
    "rc_account, limit",
    [
        # WITNESS
        (["example-array"], 100),
        # LIMIT
        (ACCOUNT, "incorrect_string_argument"),
        (ACCOUNT, [100]),
        (ACCOUNT, "true"),
    ],
)
@run_for("testnet")
def test_list_rc_accounts_with_incorrect_type_of_arguments(node, wallet, rc_account, limit):
    wallet.create_account(ACCOUNT)
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.condenser.list_rc_accounts(rc_account, limit)


@run_for("testnet")
def test_list_rc_account_with_additional_argument(node, wallet):
    wallet.create_account(ACCOUNT)
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.condenser.list_rc_accounts(ACCOUNT, 100, "additional-argument")


@run_for("testnet")
def test_list_rc_account_with_missing_argument(node, wallet):
    wallet.create_account(ACCOUNT)
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.condenser.list_rc_accounts(ACCOUNT)
