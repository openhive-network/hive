from __future__ import annotations

import pytest

from clive.__private.core.beekeeper import Beekeeper
from clive.core.url import Url


@pytest.mark.parametrize(
    "notifications_endpoint",
    [Url(proto="http", host="0.0.0.0", port=None), Url(proto="http", host="127.0.0.1", port=None)],
)
async def test_notifications_endpoint(notifications_endpoint: Url) -> None:
    """
    Test will check command line flag --notifications-endpoint.

    In this test we will re-use built-in notification server. We will not pass a port here,
    because built-in notification server will get free port and use it.

    In order to test flag notifications-endpoint we will pass this flag, and internal
    we will pass port that already has been taken by notification http server to beekeeper
    executable.
    """
    # 1 pass notification flag
    # 2 inside start function there is special if, that will check if we have explicitly pass
    #   notification-endpoint flag, and append to it already taken port. This way we will
    #   point beekeeper where to send notifications.

    # ARRANGE & ACT & ASSERT
    async with await Beekeeper().launch(notifications_endpoint=notifications_endpoint):
        pass
