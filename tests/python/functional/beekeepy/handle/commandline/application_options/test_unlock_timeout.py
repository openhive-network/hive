from __future__ import annotations

import time
from typing import TYPE_CHECKING

import pytest
from beekeepy._executable.arguments.beekeeper_arguments import BeekeeperArguments

if TYPE_CHECKING:
    from beekeepy._handle.beekeeper import Beekeeper


def check_wallet_lock(beekeeper: Beekeeper, required_status: bool) -> None:
    """Check if wallets are have required unlock status."""
    response_list_wallets = beekeeper.api.list_wallets()
    for wallet in response_list_wallets.wallets:
        assert wallet.unlocked == required_status


@pytest.mark.parametrize("unlock_timeout", [2, 3, 4])
def test_unlock_time(beekeeper_not_started: Beekeeper, unlock_timeout: int) -> None:
    """Test will check command line flag --unlock-time."""
    # ARRANGE
    beekeeper_not_started.run(additional_cli_arguments=BeekeeperArguments(unlock_timeout=unlock_timeout))
    beekeeper_not_started.api.create(wallet_name="wallet_name")
    check_wallet_lock(beekeeper_not_started, True)

    # ACT
    time.sleep(unlock_timeout)

    # ASSERT
    check_wallet_lock(beekeeper_not_started, False)
