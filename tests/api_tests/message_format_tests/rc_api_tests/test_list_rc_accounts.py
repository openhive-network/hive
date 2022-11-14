import pytest

import test_tools as tt

from ..local_tools import run_for, as_string

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
        (ACCOUNT, 0),
        (ACCOUNT, 1000),
]


@pytest.fixture
def ready_node(prepared_node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        wallet.create_account(ACCOUNT)
    return prepared_node


@pytest.mark.parametrize(
    "rc_account, limit", [
        *CORRECT_VALUES,
        *as_string(CORRECT_VALUES),
        (ACCOUNT, True),  # bool is treated like numeric (0:1)
    ]
)
@run_for("testnet", "mainnet_5m", "mainnet_64m")
def test_list_rc_accounts_with_correct_values(ready_node, rc_account, limit):
    ready_node.api.rc.list_rc_accounts(start=rc_account, limit=limit)


@pytest.mark.parametrize(
    "rc_account, limit", [
        # LIMIT
        (ACCOUNT, -1),
        (ACCOUNT, 1001),
    ]
)
@run_for("testnet", "mainnet_5m", "mainnet_64m")
def test_list_rc_accounts_with_incorrect_values(ready_node, rc_account, limit):
    with pytest.raises(tt.exceptions.CommunicationError):
        ready_node.api.rc.list_rc_accounts(start=rc_account, limit=limit)


@pytest.mark.parametrize(
    "rc_account, limit", [
        # WITNESS
        (["example-array"], 100),

        # LIMIT
        (ACCOUNT, "incorrect_string_argument"),
        (ACCOUNT, [100]),
        (ACCOUNT, "true"),
    ]
)
@run_for("testnet", "mainnet_5m", "mainnet_64m")
def test_list_rc_accounts_with_incorrect_type_of_arguments(ready_node, rc_account, limit):
    with pytest.raises(tt.exceptions.CommunicationError):
        ready_node.api.rc.list_rc_accounts(start=rc_account, limit=limit)


@run_for("testnet", "mainnet_5m", "mainnet_64m")
def test_list_rc_account_with_additional_argument(ready_node):
    ready_node.api.rc.list_rc_accounts(start=ACCOUNT, limit=100, additional_argument="additional_argument")


@run_for("testnet", "mainnet_5m", "mainnet_64m")
def test_list_rc_account_with_missing_argument(ready_node):
    with pytest.raises(tt.exceptions.CommunicationError):
        ready_node.api.rc.list_rc_accounts(start=ACCOUNT)
