from __future__ import annotations

from typing import TYPE_CHECKING

import pytest

from clive.exceptions import CommunicationError
from clive_local_tools.data.models import WalletInfo

if TYPE_CHECKING:
    from clive.__private.core.beekeeper import Beekeeper


async def test_api_close(beekeeper: Beekeeper, wallet: WalletInfo) -> None:
    """Test test_api_close will test beekeeper_api.close api call."""
    # ARRANGE
    assert len((await beekeeper.api.list_wallets()).wallets) == 1

    # ACT
    await beekeeper.api.close(wallet_name=wallet.name)

    # ASSERT
    assert (
        len((await beekeeper.api.list_wallets()).wallets) == 0
    ), "After close, there should be no wallets hold by beekeeper."


async def test_api_close_import_key_to_closed_wallet(beekeeper: Beekeeper, wallet_key_to_import: WalletInfo) -> None:
    """Test test_api_close_import_key_to_closed_wallet will test possibility of importing key into the closed wallet."""
    # ARRANGE
    await beekeeper.api.close(wallet_name=wallet_key_to_import.name)

    # ACT & ASSERT
    with pytest.raises(CommunicationError, match=f"Wallet not found: {wallet_key_to_import.name}"):
        await beekeeper.api.import_key(
            wif_key=wallet_key_to_import.keys.pairs[0].private_key.value, wallet_name=wallet_key_to_import.name
        )


async def test_api_close_double_close(beekeeper: Beekeeper, wallet_key_to_import: WalletInfo) -> None:
    """Test test_api_close_double_close will test possibility of double closing wallet."""
    # ARRANGE
    await beekeeper.api.import_key(
        wif_key=wallet_key_to_import.keys.pairs[0].private_key.value, wallet_name=wallet_key_to_import.name
    )

    # ACT
    await beekeeper.api.close(wallet_name=wallet_key_to_import.name)

    # ASSERT
    with pytest.raises(CommunicationError, match=f"Wallet not found: {wallet_key_to_import.name}"):
        await beekeeper.api.close(wallet_name=wallet_key_to_import.name)


async def test_api_close_not_existing_wallet(beekeeper: Beekeeper) -> None:
    """Test test_api_close will test possibility of closing not exzisting wallet."""
    # ARRANGE
    wallet = WalletInfo(password="password", name="name")

    # ACT & ASSERT
    with pytest.raises(CommunicationError, match=f"Wallet not found: {wallet.name}"):
        await beekeeper.api.close(wallet_name=wallet.name)
