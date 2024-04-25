from __future__ import annotations

from typing import TYPE_CHECKING

import pytest

from clive.exceptions import CommunicationError

if TYPE_CHECKING:
    from clive.__private.core.beekeeper import Beekeeper
    from clive_local_tools.data.models import WalletInfo
    from clive_local_tools.data.types import WalletsGeneratorT


async def test_api_import_key(beekeeper: Beekeeper, setup_wallets: WalletsGeneratorT) -> None:
    """Test test_api_import_key will test beekeeper_api.import_key api call."""
    # ARRANGE
    wallets = await setup_wallets(1, import_keys=False, keys_per_wallet=5)
    wallet = wallets[0]

    # ACT & ASSERT
    for pair in wallet.keys.pairs:
        assert (
            pair.public_key
            == (await beekeeper.api.import_key(wallet_name=wallet.name, wif_key=pair.private_key.value)).public_key
        ), "Public key of imported wif should match."


async def test_api_import_key_to_locked(beekeeper: Beekeeper, wallet_key_to_import: WalletInfo) -> None:
    """Test test_api_import_key_to_locked will test beekeeper_api.import_key to the locked wallet."""
    # ARRANGE & ACT
    await beekeeper.api.lock(wallet_name=wallet_key_to_import.name)

    # ASSERT
    with pytest.raises(CommunicationError, match=f"Wallet is locked: {wallet_key_to_import.name}"):
        await beekeeper.api.import_key(
            wallet_name=wallet_key_to_import.name, wif_key=wallet_key_to_import.keys.pairs[0].private_key.value
        )


async def test_api_import_key_to_closed(beekeeper: Beekeeper, wallet_key_to_import: WalletInfo) -> None:
    """Test test_api_import_key_to_closed will test beekeeper_api.import_key to the closed wallet."""
    # ARRANGE & ACT
    await beekeeper.api.close(wallet_name=wallet_key_to_import.name)

    # ASSERT
    with pytest.raises(CommunicationError, match=f"Wallet not found: {wallet_key_to_import.name}"):
        await beekeeper.api.import_key(
            wallet_name=wallet_key_to_import.name, wif_key=wallet_key_to_import.keys.pairs[0].private_key.value
        )
