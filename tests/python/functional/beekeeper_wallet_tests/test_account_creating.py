from __future__ import annotations

from typing import TYPE_CHECKING

import pytest

from msgspec import ValidationError

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
            "Expected `str` of length <= 16",
        ),
        (
            "to",
            "Expected `str` of length >= 3",
        ),
        (
            "...",
            "Expected `str` matching regex",
        ),
    ],
)
def test_creation_of_account_with_invalid_name(
    wallet: tt.Wallet, account_name: str, expected_error_message: str
) -> None:
    with pytest.raises(ValidationError) as exception:
        wallet.api.create_account("initminer", account_name, "{}")

    assert expected_error_message in str(exception.value)
