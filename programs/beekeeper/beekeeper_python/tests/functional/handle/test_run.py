from __future__ import annotations

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from beekeepy import BeekeeperHandle


def x_test_smoke_run_beekeeper(beekeeper: BeekeeperHandle) -> None:
    return
    beekeeper.run()
    beekeeper.api.beekeeper.list_wallets()
    beekeeper.close()
