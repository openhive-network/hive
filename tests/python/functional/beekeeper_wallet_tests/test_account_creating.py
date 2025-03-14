from __future__ import annotations

from typing import TYPE_CHECKING

import pytest

from clive.__private.core.iwax import WaxOperationFailedError

if TYPE_CHECKING:
    import test_tools as tt


def test_account_creation(wallet: tt.Wallet) -> None:
    wallet.api.create_account("initminer", "alice", "{}")
    assert wallet.api.list_accounts("a", 1) == ["alice"]


@pytest.mark.parametrize(
    ("account_name", "expected_error_message"),
    [
        (
            "too-long-account-name",
            "ensure this value has at most 16 characters",
        ),
        (
            "to",
            "ensure this value has at least 3 characters",
        ),
        (
            "...",
            "string does not match regex",
        ),
    ],
)
def test_creation_of_account_with_invalid_name(
    wallet: tt.Wallet, account_name: str, expected_error_message: str
) -> None:
    with pytest.raises(WaxOperationFailedError) as exception:
        wallet.api.create_account("initminer", account_name, "{}")

    assert expected_error_message in exception.value.json()
