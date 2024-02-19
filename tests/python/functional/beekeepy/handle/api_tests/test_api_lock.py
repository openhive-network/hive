from __future__ import annotations

from typing import TYPE_CHECKING

import pytest
from helpy.exceptions import RequestError

if TYPE_CHECKING:
    from beekeepy._handle import Beekeeper

    from hive_local_tools.beekeeper.models import WalletInfo, WalletsGeneratorT


def test_api_lock(beekeeper: Beekeeper, wallet: WalletInfo) -> None:
    """Test test_api_lock will test beekeeper_api.lock."""
    # ARRANGE
    bk_wallet = (beekeeper.api.beekeeper.list_wallets()).wallets[0]
    assert bk_wallet.unlocked is True, "Target wallet should be unlocked."

    # ACT
    beekeeper.api.beekeeper.lock(wallet_name=wallet.name)

    # ASSERT
    bk_wallet = (beekeeper.api.beekeeper.list_wallets()).wallets[0]
    assert bk_wallet.unlocked is False, "Target wallet should be locked."


def test_api_lock_locked_wallet(beekeeper: Beekeeper, wallet: WalletInfo) -> None:
    """Test test_api_unlock_unknown_wallet will try to lock already locked wallet."""
    # ARRANGE
    bk_wallet = (beekeeper.api.beekeeper.list_wallets()).wallets[0]
    assert bk_wallet.unlocked is True, "Target wallet should be unlocked."

    # ACT
    beekeeper.api.beekeeper.lock(wallet_name=wallet.name)

    # ASSERT
    with pytest.raises(RequestError, match="Unable to lock a locked wallet"):
        beekeeper.api.beekeeper.lock(wallet_name=wallet.name)


def test_api_lock_unknown_wallet(beekeeper: Beekeeper) -> None:
    """Test test_api_lock_unknown_wallet will try to lock unknown wallet."""
    # ARRANGE & ACT & ASSERT
    with pytest.raises(RequestError, match="Wallet not found"):
        beekeeper.api.beekeeper.lock(wallet_name="name")


def test_api_lock_target_wallet(beekeeper: Beekeeper, setup_wallets: WalletsGeneratorT) -> None:
    """Test test_api_lock_target_wallet will test locking of target wallet from the pool."""
    # ARRANGE
    wallets = setup_wallets(5)
    wallet_to_lock = wallets[3]
    for wallet in wallets:
        beekeeper.api.beekeeper.open(wallet_name=wallet.name)
        beekeeper.api.beekeeper.unlock(wallet_name=wallet.name, password=wallet.password)
    for bk_wallet in (beekeeper.api.beekeeper.list_wallets()).wallets:
        assert bk_wallet.unlocked is True, "All wallets should be unlocked."

    # ACT
    beekeeper.api.beekeeper.lock(wallet_name=wallet_to_lock.name)

    # ASSERT
    for bk_wallet in (beekeeper.api.beekeeper.list_wallets()).wallets:
        if bk_wallet.name == wallet_to_lock.name:
            assert bk_wallet.unlocked is False, "Target wallet should be locked."
        else:
            assert bk_wallet.unlocked is True, "Rest of wallets should be unlocked."
