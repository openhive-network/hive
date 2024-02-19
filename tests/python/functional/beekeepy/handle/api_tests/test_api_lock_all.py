from __future__ import annotations

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from beekeepy._handle import Beekeeper

    from python.functional.beekeepy.handle.wallet import WalletsGeneratorT


def test_api_lock_all(beekeeper: Beekeeper, setup_wallets: WalletsGeneratorT) -> None:
    """Test test_api_lock_all will test beekeeper_api.lock_all api call."""
    # ARRANGE
    wallets = setup_wallets(5, import_keys=False, keys_per_wallet=0)
    for wallet in wallets:
        beekeeper.api.beekeeper.open(wallet_name=wallet.name)
        beekeeper.api.beekeeper.unlock(wallet_name=wallet.name, password=wallet.password)
    for bk_wallet in (beekeeper.api.beekeeper.list_wallets()).wallets:
        assert bk_wallet.unlocked is True, "All created wallets should be unlocked."

    # ACT
    beekeeper.api.beekeeper.lock_all()

    # ASSERT
    for bk_wallet in (beekeeper.api.beekeeper.list_wallets()).wallets:
        assert bk_wallet.unlocked is False, "All created wallets should be locked."
