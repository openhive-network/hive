from __future__ import annotations

import asyncio
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from clive.__private.core.beekeeper import Beekeeper
    from clive_local_tools.data.models import WalletInfo


async def test_api_set_timeout(beekeeper: Beekeeper, wallet: WalletInfo) -> None:
    """Test test_api_set_timeout will test beekeeper_api.set_timeout api call."""
    # ARRANGE
    await beekeeper.api.open(wallet_name=wallet.name)
    await beekeeper.api.unlock(wallet_name=wallet.name, password=wallet.password)
    bk_wallet = (await beekeeper.api.list_wallets()).wallets[0]
    assert bk_wallet.unlocked is True, "Wallet should be unlocked."

    # ACT
    await beekeeper.api.set_timeout(seconds=1)
    await asyncio.sleep(1.5)

    # ASSERT
    bk_wallet = (await beekeeper.api.list_wallets()).wallets[0]
    assert bk_wallet.unlocked is False, "Wallet after timeout should be locked."
