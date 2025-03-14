from __future__ import annotations

from typing import TYPE_CHECKING

import pytest
from pydantic import ValidationError
from helpy._interfaces.wax import WaxOperationFailedError
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
            "Input too large: `too-long-account-name` (21) for fixed size string: (16)",
        ),
        (
            "to",
            "Account name \'to\' is too short. Use at least 3 characters.",
        ),
        (
            "...",
            "Account name \'...\' is not valid. Please follow the RFC 1035 rules",
        ),
    ],
)
def test_creation_of_account_with_invalid_name(
    wallet: tt.Wallet, account_name: str, expected_error_message: str
) -> None:
    with pytest.raises(WaxOperationFailedError) as exception:
        wallet.api.create_account("initminer", account_name, "{}")

    assert expected_error_message in str(exception.value)
