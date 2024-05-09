from __future__ import annotations

import pytest

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.api.message_format import as_string
from hive_local_tools.api.message_format.wallet_bridge_api.constants import ACCOUNTS

MAINNET_ACCOUNTS = [
    "initminer",
    "gtg",
]

CORRECT_VALUES = [
    # FROM_, TO
    (ACCOUNTS[0], ACCOUNTS[1], 100),
    (ACCOUNTS[0], "", 100),
    # LIMIT
    (ACCOUNTS[0], ACCOUNTS[1], 1),
    (ACCOUNTS[0], ACCOUNTS[1], 1000),
]


@pytest.mark.skip(reason="Unimplemented functionality in wallet")
@pytest.mark.parametrize(
    ("from_", "to", "limit"),
    [
        *CORRECT_VALUES,
        *as_string(CORRECT_VALUES),
        (ACCOUNTS[0], ACCOUNTS[1], True),  # bool is treated like numeric (0:1)
    ],
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_list_rc_direct_delegations_with_correct_value(
    node: tt.InitNode | tt.RemoteNode, should_prepare: bool, from_: str, to: str, limit: bool | int
) -> None:
    if isinstance(node, tt.RemoteNode):
        from_ = MAINNET_ACCOUNTS[0]
        to = MAINNET_ACCOUNTS[1]
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        create_accounts_and_delegate_rc_from_account0_to_account1(wallet, accounts=ACCOUNTS[0:2])

    node.api.wallet_bridge.list_rc_direct_delegations([from_, to], limit)


@pytest.mark.skip(reason="Unimplemented functionality in wallet")
@pytest.mark.parametrize(
    ("from_", "to", "limit"),
    [
        # FROM_
        ("", "", 100),
        ("non-exist-acc", "", 100),
        ("true", "", 100),
        # TO
        (ACCOUNTS[0], "true", 100),
        (ACCOUNTS[0], "non-exist-acc", 100),
        # LIMIT
        (ACCOUNTS[0], ACCOUNTS[1], -1),
        (ACCOUNTS[0], ACCOUNTS[1], 0),
        (ACCOUNTS[0], "", 1001),
    ],
)
@run_for("testnet")
def test_list_rc_direct_delegations_with_incorrect_value_in_testnet(
    node: tt.InitNode, from_: str, to: str, limit: int
) -> None:
    wallet = tt.Wallet(attach_to=node)
    create_accounts_and_delegate_rc_from_account0_to_account1(wallet, accounts=ACCOUNTS[0:2])

    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.list_rc_direct_delegations([from_, to], limit)


@pytest.mark.skip(reason="Unimplemented functionality in wallet")
@pytest.mark.parametrize(
    ("from_", "to", "limit"),
    [
        # FROM_
        ("", "", 100),
        ("non-exist-acc", "", 100),
        # TO
        (MAINNET_ACCOUNTS[0], "non-exist-acc", 100),
        # LIMIT
        (MAINNET_ACCOUNTS[0], MAINNET_ACCOUNTS[1], -1),
        (MAINNET_ACCOUNTS[0], MAINNET_ACCOUNTS[1], 0),
        (MAINNET_ACCOUNTS[0], "", 1001),
    ],
)
@run_for("mainnet_5m", "live_mainnet")
def test_list_rc_direct_delegations_with_incorrect_value_in_mainnet(
    node: tt.RemoteNode, from_: str, to: str, limit: int
) -> None:
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.list_rc_direct_delegations([from_, to], limit)


@pytest.mark.skip(reason="Unimplemented functionality in wallet")
@pytest.mark.parametrize(
    ("from_", "to", "limit"),
    [
        # FROM_
        ("100", ACCOUNTS[1], 100),
        (100, ACCOUNTS[1], 100),
        (True, ACCOUNTS[1], 100),
        ("", ACCOUNTS[1], 100),
        ([ACCOUNTS[0]], ACCOUNTS[1], 100),
        # TO
        (ACCOUNTS[0], 100, 100),
        (ACCOUNTS[0], "100", 100),
        (ACCOUNTS[0], True, 100),
        (ACCOUNTS[0], [ACCOUNTS[1]], 100),
        # LIMIT
        (ACCOUNTS[0], ACCOUNTS[1], "incorrect_string_argument"),
        (ACCOUNTS[0], ACCOUNTS[1], "true"),
        (ACCOUNTS[0], ACCOUNTS[1], [100]),
    ],
)
@run_for("testnet")
def test_list_rc_direct_delegations_with_incorrect_type_of_arguments_in_testnet(
    node: tt.InitNode, from_: bool | int | str, to: bool | list | int | str, limit: int | list | str
) -> None:
    wallet = tt.Wallet(attach_to=node)
    create_accounts_and_delegate_rc_from_account0_to_account1(wallet, accounts=ACCOUNTS)

    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.list_rc_direct_delegations([from_, to], limit)


@pytest.mark.skip(reason="Unimplemented functionality in wallet")
@pytest.mark.parametrize(
    ("from_", "to", "limit"),
    [
        # FROM_
        ("100", MAINNET_ACCOUNTS[1], 100),
        (100, MAINNET_ACCOUNTS[1], 100),
        ("", MAINNET_ACCOUNTS[1], 100),
        ([MAINNET_ACCOUNTS[0]], MAINNET_ACCOUNTS[1], 100),
        # TO
        (MAINNET_ACCOUNTS[0], 100, 100),
        (MAINNET_ACCOUNTS[0], "100", 100),
        (MAINNET_ACCOUNTS[0], [MAINNET_ACCOUNTS[1]], 100),
        # LIMIT
        (MAINNET_ACCOUNTS[0], MAINNET_ACCOUNTS[1], "incorrect_string_argument"),
        (MAINNET_ACCOUNTS[0], MAINNET_ACCOUNTS[1], "true"),
        (MAINNET_ACCOUNTS[0], MAINNET_ACCOUNTS[1], [100]),
    ],
)
@run_for("mainnet_5m", "live_mainnet")
def test_list_rc_direct_delegations_with_incorrect_type_of_arguments_in_mainnet(
    node: tt.RemoteNode, from_: int | list | str, to: int | list | str, limit: int | list | str
) -> None:
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.list_rc_direct_delegations([from_, to], limit)


def create_accounts_and_delegate_rc_from_account0_to_account1(wallet: tt.Wallet, accounts: list) -> None:
    wallet.create_accounts(len(accounts))
    wallet.api.transfer_to_vesting("initminer", accounts[0], tt.Asset.Test(0.1))
    wallet.api.delegate_rc(accounts[0], [accounts[1]], 5)
