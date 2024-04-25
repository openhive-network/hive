from __future__ import annotations

import asyncio

import pytest

from clive.__private.core.beekeeper import Beekeeper


async def check_wallet_lock(beekeeper: Beekeeper, required_status: bool) -> None:
    """Check if wallets are have required unlock status."""
    response_list_wallets = await beekeeper.api.list_wallets()
    for wallet in response_list_wallets.wallets:
        assert wallet.unlocked == required_status


@pytest.mark.parametrize("unlock_timeout", [2, 3, 4])
async def test_unlock_time(unlock_timeout: int) -> None:
    """Test will check command line flag --unlock-time."""
    # ARRANGE
    async with await Beekeeper().launch(unlock_timeout=unlock_timeout) as beekeeper:
        await beekeeper.api.create(wallet_name="wallet_name")
        await check_wallet_lock(beekeeper, True)

        # ACT
        await asyncio.sleep(int(unlock_timeout))

        # ASSERT
        await check_wallet_lock(beekeeper, False)
