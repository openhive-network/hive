from __future__ import annotations

from typing import TYPE_CHECKING

import pytest
from helpy.exceptions import RequestError

from hive_local_tools.beekeeper.models import WalletInfo

if TYPE_CHECKING:
    from beekeepy._handle import Beekeeper

    import test_tools as tt


def test_api_close(beekeeper: Beekeeper, wallet: WalletInfo) -> None:
    """Test test_api_close will test beekeeper_api.close api call."""
    # ARRANGE
    assert len((beekeeper.api.list_wallets()).wallets) == 1

    # ACT
    beekeeper.api.close(wallet_name=wallet.name)

    # ASSERT
    assert (
        len((beekeeper.api.list_wallets()).wallets) == 0
    ), "After close, there should be no wallets hold by beekeeper."


def test_api_close_import_key_to_closed_wallet(
    beekeeper: Beekeeper, wallet: WalletInfo, keys_to_import: list[tt.Account]
) -> None:
    """Test test_api_close_import_key_to_closed_wallet will test possibility of importing key into the closed wallet."""
    # ARRANGE
    beekeeper.api.close(wallet_name=wallet.name)

    # ACT & ASSERT
    with pytest.raises(RequestError, match=f"Wallet not found: {wallet.name}"):
        beekeeper.api.import_key(wif_key=keys_to_import[0].private_key, wallet_name=wallet.name)


def test_api_close_double_close(beekeeper: Beekeeper, wallet: WalletInfo, keys_to_import: list[tt.Account]) -> None:
    """Test test_api_close_double_close will test possibility of double closing wallet."""
    # ARRANGE
    beekeeper.api.import_key(wif_key=keys_to_import[0].private_key, wallet_name=wallet.name)

    # ACT
    beekeeper.api.close(wallet_name=wallet.name)

    # ASSERT
    with pytest.raises(RequestError, match=f"Wallet not found: {wallet.name}"):
        beekeeper.api.close(wallet_name=wallet.name)


def test_api_close_not_existing_wallet(beekeeper: Beekeeper) -> None:
    """Test test_api_close will test possibility of closing not exzisting wallet."""
    # ARRANGE
    wallet = WalletInfo(password="password", name="name")

    # ACT & ASSERT
    with pytest.raises(RequestError, match=f"Wallet not found: {wallet.name}"):
        beekeeper.api.close(wallet_name=wallet.name)
