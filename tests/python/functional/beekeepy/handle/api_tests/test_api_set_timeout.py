from __future__ import annotations

import time
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from beekeepy._handle import Beekeeper

    from hive_local_tools.beekeeper.models import WalletInfo


def test_api_set_timeout(beekeeper: Beekeeper, wallet: WalletInfo) -> None:
    """Test test_api_set_timeout will test beekeeper_api.set_timeout api call."""
    # ARRANGE
    beekeeper.api.beekeeper.open(wallet_name=wallet.name)
    beekeeper.api.beekeeper.unlock(wallet_name=wallet.name, password=wallet.password)
    bk_wallet = (beekeeper.api.beekeeper.list_wallets()).wallets[0]
    assert bk_wallet.unlocked is True, "Wallet should be unlocked."

    # ACT
    beekeeper.api.beekeeper.set_timeout(seconds=1)
    time.sleep(1.5)

    # ASSERT
    bk_wallet = (beekeeper.api.beekeeper.list_wallets()).wallets[0]
    assert bk_wallet.unlocked is False, "Wallet after timeout should be locked."
