from __future__ import annotations

import pytest

from clive.__private.core.beekeeper import Beekeeper
from clive_local_tools.beekeeper import checkers


@pytest.mark.parametrize("backtrace", ["yes", "no"])
async def test_backtrace(backtrace: str) -> None:
    """Test will check command line flag --backtrace."""
    # ARRAGNE & ACT
    async with await Beekeeper().launch(backtrace=backtrace) as beekeeper:
        # ASSERT
        assert checkers.check_for_pattern_in_file(
            beekeeper.get_wallet_dir() / "stderr.log", "Backtrace on segfault is enabled."
        ) is (backtrace == "yes")
