from __future__ import annotations

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from clive.__private.core.beekeeper import Beekeeper
    from clive_local_tools.data.types import WalletsGeneratorT


async def test_api_lock_all(beekeeper: Beekeeper, setup_wallets: WalletsGeneratorT) -> None:
    """Test test_api_lock_all will test beekeeper_api.lock_all api call."""
    # ARRANGE
    wallets = await setup_wallets(5, import_keys=False, keys_per_wallet=0)
    for wallet in wallets:
        await beekeeper.api.open(wallet_name=wallet.name)
        await beekeeper.api.unlock(wallet_name=wallet.name, password=wallet.password)
    for bk_wallet in (await beekeeper.api.list_wallets()).wallets:
        assert bk_wallet.unlocked is True, "All created wallets should be unlocked."

    # ACT
    await beekeeper.api.lock_all()

    # ASSERT
    for bk_wallet in (await beekeeper.api.list_wallets()).wallets:
        assert bk_wallet.unlocked is False, "All created wallets should be locked."
