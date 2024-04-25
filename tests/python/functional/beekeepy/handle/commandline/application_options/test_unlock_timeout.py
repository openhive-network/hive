from __future__ import annotations

import time
from typing import TYPE_CHECKING

import pytest

if TYPE_CHECKING:
    from beekeepy._handle.beekeeper import Beekeeper


def check_wallet_lock(beekeeper: Beekeeper, required_status: bool) -> None:
    """Check if wallets are have required unlock status."""
    response_list_wallets = beekeeper.api.beekeeper.list_wallets()
    for wallet in response_list_wallets.wallets:
        assert wallet.unlocked == required_status


@pytest.mark.parametrize("unlock_timeout", [2, 3, 4])
def test_unlock_time(beekeeper_not_started: Beekeeper, unlock_timeout: int) -> None:
    """Test will check command line flag --unlock-time."""
    # ARRANGE
    beekeeper_not_started.config.unlock_timeout = unlock_timeout
    with beekeeper_not_started:
        beekeeper_not_started.api.beekeeper.create(wallet_name="wallet_name")
        check_wallet_lock(beekeeper_not_started, True)

        # ACT
        time.sleep(unlock_timeout)

        # ASSERT
        check_wallet_lock(beekeeper_not_started, False)
