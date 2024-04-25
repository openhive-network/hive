from __future__ import annotations

import pytest

from clive.__private.core.beekeeper import Beekeeper
from clive_local_tools.beekeeper import checkers


@pytest.mark.parametrize("webserver_thread_pool_size", [1, 2, 4, 8])
async def test_webserver_thread_pool_size(webserver_thread_pool_size: int) -> None:
    """Test will check command line flag --webserver-thread-pool-size."""
    # ARRANGE & ACT
    async with await Beekeeper().launch(webserver_thread_pool_size=webserver_thread_pool_size) as beekeeper:
        # ASSERT
        assert checkers.check_for_pattern_in_file(
            beekeeper.get_wallet_dir() / "stderr.log", f"configured with {webserver_thread_pool_size} thread pool size"
        )
