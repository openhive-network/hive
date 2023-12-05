from __future__ import annotations

import pytest

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.api.message_format import as_string

ACCOUNTS = [f"account-{i}" for i in range(3)]

CORRECT_VALUES = [
    [""],
    ["non-exist-acc"],
    [ACCOUNTS[0]],
    ACCOUNTS,
    [100],
    [True],
]


@pytest.mark.parametrize(
    "rc_accounts",
    [
        *CORRECT_VALUES,
        *as_string(CORRECT_VALUES),
    ],
)
@run_for("testnet")
def test_find_rc_accounts_with_correct_value(
    node: tt.InitNode, wallet: tt.Wallet, rc_accounts: bool | int | list | str
) -> None:
    wallet.create_accounts(len(ACCOUNTS))
    node.api.condenser.find_rc_accounts(rc_accounts)


@pytest.mark.parametrize(
    "rc_accounts",
    [
        "['non-exist-acc']",
        True,
        100,
        "100",
        "incorrect_string_argument",
    ],
)
@run_for("testnet")
def test_find_rc_accounts_with_incorrect_type_of_argument(
    node: tt.InitNode, rc_accounts: bool | int | list | str
) -> None:
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.condenser.find_rc_accounts(rc_accounts)


@run_for("testnet")
def test_find_rc_accounts_with_missing_argument(node: tt.InitNode) -> None:
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.condenser.find_rc_accounts()


@run_for("testnet")
def test_find_rc_accounts_with_additional_argument(node: tt.InitNode) -> None:
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.condenser.find_rc_accounts([ACCOUNTS[0]], "additional_argument")
