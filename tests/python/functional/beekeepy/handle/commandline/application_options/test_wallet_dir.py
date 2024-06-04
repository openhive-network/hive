from __future__ import annotations

from typing import TYPE_CHECKING

import pytest

if TYPE_CHECKING:
    from beekeepy._handle.beekeeper import Beekeeper


def check_wallets_size(beekeeper: Beekeeper, required_size: int) -> None:
    """Check if wallet size."""
    response_list_wallets = beekeeper.api.list_wallets()
    assert len(response_list_wallets.wallets) == required_size


@pytest.mark.parametrize("wallet_name", ["wallet0", "wallet1", "wallet2"])
def test_wallet_dir(beekeeper_not_started: Beekeeper, wallet_name: str) -> None:
    """Test will check command line flag --wallet-dir."""
    # ARRANGE
    beekeeper = beekeeper_not_started  # alias
    with beekeeper:
        # ACT
        check_wallets_size(beekeeper, 0)
        beekeeper.api.create(wallet_name=wallet_name)
        check_wallets_size(beekeeper, 1)

    # Start and check if created wallet exists.
    with beekeeper:
        check_wallets_size(beekeeper, 0)

        # ASSERT
        beekeeper.api.open(wallet_name=wallet_name)
        check_wallets_size(beekeeper, 1)
