from __future__ import annotations

from typing import TYPE_CHECKING

import pytest

from clive.exceptions import CommunicationError

if TYPE_CHECKING:
    from clive.__private.core.beekeeper import Beekeeper
    from clive_local_tools.data.models import WalletInfo
    from clive_local_tools.data.types import WalletsGeneratorT


async def test_api_lock(beekeeper: Beekeeper, wallet: WalletInfo) -> None:
    """Test test_api_lock will test beekeeper_api.lock."""
    # ARRANGE
    bk_wallet = (await beekeeper.api.list_wallets()).wallets[0]
    assert bk_wallet.unlocked is True, "Target wallet should be unlocked."

    # ACT
    await beekeeper.api.lock(wallet_name=wallet.name)

    # ASSERT
    bk_wallet = (await beekeeper.api.list_wallets()).wallets[0]
    assert bk_wallet.unlocked is False, "Target wallet should be locked."


async def test_api_lock_locked_wallet(beekeeper: Beekeeper, wallet: WalletInfo) -> None:
    """Test test_api_unlock_unknown_wallet will try to lock already locked wallet."""
    # ARRANGE
    bk_wallet = (await beekeeper.api.list_wallets()).wallets[0]
    assert bk_wallet.unlocked is True, "Target wallet should be unlocked."

    # ACT
    await beekeeper.api.lock(wallet_name=wallet.name)

    # ASSERT
    with pytest.raises(CommunicationError, match="Unable to lock a locked wallet"):
        await beekeeper.api.lock(wallet_name=wallet.name)


async def test_api_lock_unknown_wallet(beekeeper: Beekeeper) -> None:
    """Test test_api_lock_unknown_wallet will try to lock unknown wallet."""
    # ARRANGE & ACT & ASSERT
    with pytest.raises(CommunicationError, match="Wallet not found"):
        await beekeeper.api.lock(wallet_name="name")


async def test_api_lock_target_wallet(beekeeper: Beekeeper, setup_wallets: WalletsGeneratorT) -> None:
    """Test test_api_lock_target_wallet will test locking of target wallet from the pool."""
    # ARRANGE
    wallets = await setup_wallets(5)
    wallet_to_lock = wallets[3]
    for wallet in wallets:
        await beekeeper.api.open(wallet_name=wallet.name)
        await beekeeper.api.unlock(wallet_name=wallet.name, password=wallet.password)
    for bk_wallet in (await beekeeper.api.list_wallets()).wallets:
        assert bk_wallet.unlocked is True, "All wallets should be unlocked."

    # ACT
    await beekeeper.api.lock(wallet_name=wallet_to_lock.name)

    # ASSERT
    for bk_wallet in (await beekeeper.api.list_wallets()).wallets:
        if bk_wallet.name == wallet_to_lock.name:
            assert bk_wallet.unlocked is False, "Target wallet should be locked."
        else:
            assert bk_wallet.unlocked is True, "Rest of wallets should be unlocked."
