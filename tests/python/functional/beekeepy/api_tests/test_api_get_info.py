from __future__ import annotations

from datetime import timedelta
from typing import TYPE_CHECKING

from clive.__private.core.beekeeper.defaults import BeekeeperDefaults

if TYPE_CHECKING:
    from clive.__private.core.beekeeper import Beekeeper


async def test_api_get_info(beekeeper: Beekeeper) -> None:
    """Test test_api_get_info will test beekeeper_api.get_info api call."""
    # ARRANGE
    unlock_timeout = BeekeeperDefaults.DEFAULT_UNLOCK_TIMEOUT
    tolerance_secs = 1

    # ACT
    get_info = await beekeeper.api.get_info()

    # ASSERT
    upper_bound = get_info.now + timedelta(seconds=(unlock_timeout + tolerance_secs))
    lower_bound = get_info.now + timedelta(seconds=(unlock_timeout - tolerance_secs))
    message = f"Difference between get_info.now and get_info.timeout_time should be equal {unlock_timeout} (+/- {tolerance_secs}s)"
    assert lower_bound <= get_info.timeout_time <= upper_bound, message
