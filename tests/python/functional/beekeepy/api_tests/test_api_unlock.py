from __future__ import annotations

from typing import TYPE_CHECKING

import pytest

from clive.exceptions import CommunicationError

if TYPE_CHECKING:
    from clive.__private.core.beekeeper import Beekeeper
    from clive_local_tools.data.models import WalletInfo
    from clive_local_tools.data.types import WalletsGeneratorT


async def test_api_unlock(beekeeper: Beekeeper, wallet: WalletInfo) -> None:
    """Test test_api_unlock will test beekeeper_api.unlock."""
    # ARRANGE
    await beekeeper.api.open(wallet_name=wallet.name)

    # ACT
    await beekeeper.api.unlock(wallet_name=wallet.name, password=wallet.password)

    # ASSERT
    bk_wallet = (await beekeeper.api.list_wallets()).wallets[0]
    assert bk_wallet.unlocked is True, "Wallet should be unlocked."


async def test_api_unlock_already_unclocked_wallet(beekeeper: Beekeeper, wallet: WalletInfo) -> None:
    """Test test_api_unlock_already_unclocked_wallet will try to unlock already unlocked wallet."""
    # ARRANGE
    await beekeeper.api.open(wallet_name=wallet.name)
    await beekeeper.api.unlock(wallet_name=wallet.name, password=wallet.password)

    bk_wallet = (await beekeeper.api.list_wallets()).wallets[0]
    assert bk_wallet.unlocked is True, "Wallet should be unlocked."

    # ACT & ASSERT
    with pytest.raises(CommunicationError, match=f"Wallet is already unlocked: {wallet.name}"):
        await beekeeper.api.unlock(wallet_name=wallet.name, password=wallet.password)


async def test_api_unlock_created_but_closed_wallet(beekeeper: Beekeeper, wallet: WalletInfo) -> None:
    """Test test_api_unlock_created_but_closed_wallet will try to unlock closed wallet that was one created."""
    # ARRANGE
    await beekeeper.api.close(wallet_name=wallet.name)
    assert len((await beekeeper.api.list_wallets()).wallets) == 0

    # ACT
    await beekeeper.api.unlock(wallet_name=wallet.name, password=wallet.password)
    bk_wallet = (await beekeeper.api.list_wallets()).wallets[0]

    # ASSERT
    assert bk_wallet.unlocked is True, "Wallet should be unlocked."


async def test_api_unlock_unknown_wallet(beekeeper: Beekeeper) -> None:
    """Test test_api_unlock_unknown_wallet will try to unlock unknown wallet."""
    # ARRANGE & ACT & ASSERT
    with pytest.raises(CommunicationError, match="Unable to open file"):
        await beekeeper.api.unlock(wallet_name="name", password="password")


async def test_api_unlock_one_wallet_at_the_time(beekeeper: Beekeeper, setup_wallets: WalletsGeneratorT) -> None:
    """Test test_api_unlock_one_wallet_at_the_time will try one wallet at the time."""
    # ARRANGE
    wallets = await setup_wallets(5, import_keys=False, keys_per_wallet=0)
    wallet_to_lock = wallets[3]
    for wallet in wallets:
        await beekeeper.api.open(wallet_name=wallet.name)

    for bk_wallet in (await beekeeper.api.list_wallets()).wallets:
        assert bk_wallet.unlocked is False, "All wallets should be locked."

    # ACT
    await beekeeper.api.unlock(wallet_name=wallet_to_lock.name, password=wallet_to_lock.password)

    # ASSERT
    for bk_wallet in (await beekeeper.api.list_wallets()).wallets:
        if bk_wallet.name == wallet_to_lock.name:
            assert bk_wallet.unlocked is True, "Target wallet should be unlocked."
        else:
            assert bk_wallet.unlocked is False, "Remaining wallets should be closed."
