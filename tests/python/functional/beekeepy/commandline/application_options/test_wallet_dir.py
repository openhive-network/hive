from __future__ import annotations

from typing import TYPE_CHECKING

import pytest

from clive.__private.core.beekeeper import Beekeeper

if TYPE_CHECKING:
    from pathlib import Path


async def check_wallets_size(beekeeper: Beekeeper, required_size: int) -> None:
    """Check if wallet size."""
    response_list_wallets = await beekeeper.api.list_wallets()
    assert len(response_list_wallets.wallets) == required_size


@pytest.mark.parametrize("wallet_name", ["wallet0", "wallet1", "wallet2"])
async def test_wallet_dir(tmp_path: Path, wallet_name: str) -> None:
    """Test will check command line flag --wallet-dir."""
    # ARRANGE
    tempdir = tmp_path / "test_log_json_rpc"
    tempdir.mkdir()
    async with await Beekeeper().launch(wallet_dir=tempdir) as beekeeper:
        # ACT
        await check_wallets_size(beekeeper, 0)
        await beekeeper.api.create(wallet_name=wallet_name)
        await check_wallets_size(beekeeper, 1)

    # Start and check if created wallet exists.
    async with await Beekeeper().launch(wallet_dir=tempdir) as beekeeper:
        await check_wallets_size(beekeeper, 0)

        # ASSERT
        await beekeeper.api.open(wallet_name=wallet_name)
        await check_wallets_size(beekeeper, 1)
