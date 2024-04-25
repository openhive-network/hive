from __future__ import annotations

from datetime import timedelta
from typing import TYPE_CHECKING

from hive_local_tools.beekeeper.constants import DEFAULT_UNLOCK_TIMEOUT

if TYPE_CHECKING:
    from beekeepy._handle import Beekeeper


def test_api_get_info(beekeeper: Beekeeper) -> None:
    """Test test_api_get_info will test beekeeper_api.get_info api call."""
    # ARRANGE
    unlock_timeout = DEFAULT_UNLOCK_TIMEOUT
    tolerance_secs = 1

    # ACT
    get_info = beekeeper.api.get_info()

    # ASSERT
    upper_bound = get_info.now + timedelta(seconds=(unlock_timeout + tolerance_secs))
    lower_bound = get_info.now + timedelta(seconds=(unlock_timeout - tolerance_secs))
    message = (
        f"Difference between get_info.now and get_info.timeout_time should be equal {unlock_timeout} (+/-"
        f" {tolerance_secs}s)"
    )
    assert lower_bound <= get_info.timeout_time <= upper_bound, message
